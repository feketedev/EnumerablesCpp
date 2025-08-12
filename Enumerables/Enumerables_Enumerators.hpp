#ifndef ENUMERABLES_ENUMERATORS_HPP
#define ENUMERABLES_ENUMERATORS_HPP

	/*  Part of Enumerables for C++.														  *
	 *																						  *
	 *  This file contains the algorithm cores of sequence generation and manipulation steps  *
	 *  - in the form of Enumerator classes - as well as the simple mechanism to adapt them	  *
	 *  for usage with range-based for loops (thus making Enumerable an iterable).			  */


#include "Enumerables_InterfaceTypes.hpp"
#include "Enumerables_TypeHelpers.hpp"
#include <algorithm>



namespace Enumerables::Def {

	using namespace TypeHelpers;

	// debug-only error texts
	constexpr char DepletedError[]    = "Enumerator has reached the end.";
	constexpr char MissedFetchError[] = "No element has been fetched yet.";
	constexpr char NotFetchedError[]  = "No element fetched successfuly (reached the end or missed FetchNext call).";


#pragma region Type helpers - Enumerator specific

	/// Result of Mapper function supplied with a V input.
	template <class V, class Mapper>
	using MappedT	= decltype(declval<Mapper>() (declval<V>()));

	/// Result of 2 param Mapper function supplied with a (V1, V2) input.
	template <class V1, class V2, class Combiner>
	using CombinedT	= decltype(declval<Combiner>() (declval<V1>(), declval<V2>()));



	#pragma region Iterator checks

	/// Whether the container has an available GetSize implementation.
	template <class C>
	class HasQueryableSize {

		template <class CC = C>
		static auto Check(const CC& c) -> decltype(GetSize(c));

		static void Check(...);

	public:
		using					SizeType = decltype(Check(declval<C>()));
		static constexpr bool	value	 = !is_void_v<SizeType>;
	};



	/// Whether two iterators have queriable distance.
	template <class From, class To = From>
	class HasQueryableDistance {

		template <class F = From>
		static auto CalcDiff(const F& a, const To& b) -> decltype(b - a);

		static void CalcDiff(...);

	public:
		using					DiffType = decltype(CalcDiff(declval<From>(), declval<To>()));
		static constexpr bool	value	 = !is_void_v<DiffType>;
	};



	/// Actual iterator distance, if available.
	template <class From, class To>
	SizeInfo TryGetIterDistance(const From& s, const To& e)
	{
		if constexpr (!HasQueryableDistance<From, To>::value) {
			// NOTE: Assuming that iterators belong to containers -> which are finite
			return { Boundedness::Bounded };
		}
		else {
			auto diff = e - s;

			static_assert (std::numeric_limits<decltype(diff)>::digits <= std::numeric_limits<size_t>::digits
						   && std::is_integral_v<decltype(diff)>,
						   "Unsafe cast to size_t.");

			// infrequent call, unknown types, rather make sure
			if (diff >= 0)
				return { Boundedness::Exact, static_cast<size_t>(diff) };

			// quite improbably an internal bug
			ENUMERABLES_CLIENT_BREAK ("Negative iterator distance??");
			throw LogicException("Negative iterator distance.");
		}
	}

	#pragma endregion



	#pragma region Nested iterable handling (e.g. Flatten)

	template <class TBegin, class TEnd = TBegin, class ForcedResult = void>   class IteratorEnumerator;


	/// Provides an Enumerator to traverse any container or AutoEnumerable in a uniform way (internally in Enumerators).
	/// @remarks
	///		Avoids circular dependency and temporary factory creation, as opposed to calling Enumerate(container).
	///		CONSIDER: handling iterators directly might reduce overheads, but would require some code duplication.
	template <class T>
	struct UniformEnumerationDeducer {

		template <class Iterable>
		static auto GetEnumerator(Iterable& src)
		{
			if constexpr (IsAutoEnumerable<Iterable>::value)
				return src.GetEnumerator();
			else
				return IteratorEnumerator { AdlBegin(src), AdlEnd(src) };
		}

		using TEnumerator = decltype(GetEnumerator(declval<T&>()));
	};

	#pragma endregion



	#pragma region Collection config helpers

	template <class V, class... Args>
	using AdjustedSet = AdjustedContainerT<SetOperations, V, Args...>;

	template <class V, class... Args>
	using AdjustedList = AdjustedContainerT<ListOperations, V, Args...>;


	template <class V, class Alloc = None>
	auto BracedInitWithOptionalAlloc(std::initializer_list<V> init, const Alloc& alloc = {})
	{
		if constexpr (IsNone<Alloc>)
			return BracedInitOperations::Init(init);
		else
			return BracedInitOperations::Init(init, alloc);
	}

	#pragma endregion

#pragma endregion




#pragma region Fundamentals

	/// Main enumerator interface, also represents a 'concept'.
	template <class T>
	class IEnumerator {
	public:
		using TElem = T;

		/// @returns:	A next element is available.
		virtual bool		FetchNext()		  = 0;

		/// Current element if FetchNext returned true.
		virtual T			Current()		  = 0;

		/// Try to inform about the remaining element count.
		/// @remarks
		///		Designed to be called before any FetchNext.
		///		Mid-sequence the behaviour is defined, but the result may degrade to Unknown.
		///		Lightweight: shouldn't rely on heavier datastructures used by FetchNext.
		virtual SizeInfo	Measure()	const = 0;

		virtual ~IEnumerator();

	protected:
		// Copy is undesired, we have factories and a bit different logic then iterators.
		IEnumerator(const IEnumerator<T>&) = delete;
		IEnumerator()					   = default;
	};

	template <class T>
	IEnumerator<T>::~IEnumerator() = default;



	/// An Enumerator to be used on interface boundary. Abstracts the level underneeth
	/// by calling it via the generic interface (thus dispatching virtual calls).
	template <class T>
	class InterfacedEnumerator final : public IEnumerator<T> {
		IEnumerator<T>*			ptr;

#	if ENUMERABLES_INTERFACED_ETOR_INLINE_SIZE > 0
		// NOTE: alignas requirement is not carried by decltype(buffer)!
		// void*: minimum for vtable (provided by "ptr" field anyway)
		static constexpr size_t buffAlign = alignof(void*);


		alignas(buffAlign) char	fixBuffer[ENUMERABLES_INTERFACED_ETOR_INLINE_SIZE];


		bool IsOnHeap() const
		{
			return static_cast<const void*>(ptr) <  fixBuffer
				|| static_cast<const void*>(ptr) >= fixBuffer + ENUMERABLES_INTERFACED_ETOR_INLINE_SIZE;
		}


		template <class Factory>
		void* InlineTarget()
		{
			using Etor = InvokeResultT<Factory>;
			void* aligned = AlignFor<Etor>(fixBuffer);
			ENUMERABLES_INTERNAL_ASSERT (fixBuffer + sizeof(fixBuffer) >= reinterpret_cast<char*>(aligned) + sizeof(Etor));
			return aligned;
		}


		template <class Et>
		static constexpr bool SureFitsInline()
		{
			static_assert (alignof(Et) % buffAlign == 0, "Alignment not power of 2?");

			size_t req = alignof(Et);
			size_t slack = (req <= buffAlign) ? 0 : req - buffAlign;
			return slack + sizeof(Et) <= sizeof(fixBuffer);
		}
#	else
		bool					IsOnHeap() const	{ return true; }

		template <class Nested>
		static constexpr bool	SureFitsInline()	{ return false; }
#	endif

	public:
		bool				FetchNext()		override	{ return ptr->FetchNext(); }
		T					Current()		override	{ return ptr->Current();   }
		SizeInfo			Measure() const	override	{ return ptr->Measure();   }

		template <class Et>	Et*	TryCast()				{ return dynamic_cast<Et*>(ptr); }
		IEnumerator<T>&			WrappedInterface()		{ return *ptr; }


		~InterfacedEnumerator() override
		{
			if (IsOnHeap())
				delete ptr;
			else
				ptr->~IEnumerator();

			ptr = nullptr;
		}


		template <class NestedFactory>
		InterfacedEnumerator(NestedFactory&& fact)
		{
			if constexpr (SureFitsInline<InvokeResultT<NestedFactory>>())
				ptr = (new (InlineTarget<NestedFactory>()) RvoEmplacer<NestedFactory> { fact })->GetPtr();
			else
				ptr = new InvokeResultT<NestedFactory> { fact() };
		}
	};

}	// namespace Enumerables::Def


namespace Enumerables::TypeHelpers {

	template <class V>
	struct IsInterfacedEnumerator<Def::InterfacedEnumerator<V>>
	{
		static constexpr bool value = true;
	};

}


namespace Enumerables::Def {

	/// Represents the end() against any EnumeratorAdapter.
	struct EnumeratorAdapterEnd {};


	/// Adapts an Enumerator for usage inside range-based for (mimicking a legacy iterator).
	template <class TEnumerator>
	class EnumeratorAdapter {
		TEnumerator		enumerator;
		bool			hasCurrent;

	public:
		decltype(auto)	operator  *()	{ return enumerator.Current(); }
		void			operator ++()	{ hasCurrent = enumerator.FetchNext(); }

		bool operator !=(EnumeratorAdapterEnd) const
		{ 
			return hasCurrent;
		}

		template <class Fact>
		EnumeratorAdapter(const Fact& factory) : enumerator { factory() }
		{
			hasCurrent = enumerator.FetchNext();
		}

		// This type should only exist temporarily in for ( : ). Not even a move should be called.
		EnumeratorAdapter(EnumeratorAdapter&&) = delete;
	};

#pragma endregion




#pragma region Refined Enumerator Concepts

	#pragma region Caching operations Fundamentals

	/// Abstract Enumerator for multi-pass algorithms run deferred on first Fetch.
	/// Results container as a whole can be obtained as an optimization.
	/// @remarks
	///		Implementors and ObtainCachedResults calls should use the same alias:
	///		- CachingEnumerator<T, ListType<T>> vs
	///		  CachingEnumerator<T, ListOperations::Container<T>>
	///		have different RTTI.
	template <class V, class Cache>
	class CachingEnumerator : public IEnumerator<V> {
		
		// TODO: Eliminate redundancy introcuded by bound Cache type having replaced template Container.
		static_assert (is_same<StorableT<V>&, IterableT<Cache&>&>(), "Value type <-> cached item mismatch. "
																	 "Has StorableT been correctly applied?");

		struct State {
			Cache					results;
			IteratorT<Cache>		current;
			const IteratorT<Cache>	end;

			bool HasMore() const	{ return current != end; }

			State(CachingEnumerator& owner) : results { owner.CalcResults() }, current { AdlBegin(results) }, end { AdlEnd(results) }
			{
			}
		};
		Deferred<State>  fetchState;

	public:
		using TCache = Cache;


		/// Calculate cached results as a whole.
		virtual Cache CalcResults() = 0;


		// CONSIDER: Switching to ConsumeCurrent() -- this method prevents move-only TElem here - despite no algorithmic reason.
		//			 Enumerators already save the value of Current whenever needed to prevent duplicated calculations.
		//			 How much would it hurt to lose this iterator-like access in custom algorithms (like mergeing sequences)?
		//			 The main point of keeping Current separate instead of "Optional<T> FetchNext()", I think, is to provide RVO.

		V    Current()	 override final
		{
			ENUMERABLES_ETOR_USAGE_ASSERT (fetchState.IsInitialized() && fetchState->HasMore(), NotFetchedError);
			return Revive(*fetchState->current);
		}


		bool FetchNext() override final
		{
			if (fetchState.IsInitialized() && fetchState->HasMore())
				++fetchState->current;
			else
				fetchState.Reconstruct(*this);

			return fetchState->HasMore();
		}

		~CachingEnumerator() override;
	};

	template <class V, class Cache>
	CachingEnumerator<V, Cache>::~CachingEnumerator() = default;



	/// SFINAE helper for CachingEnumerator to obtain its cache.
	/// @remarks
	///		Possible scenarios to support:
	///			* moving exact container
	///			* converting move, primarily	 SmallList<V, N> <-> List<V>
	///			* move by element, e.g.			 Set<V>  -> List<V>
	///			* converting move elements, e.g. List<V> -> List<V2>
	///			  - main use-case: ToList<TDecayed>()    =>  Cache<RefHolder<T>> -> List<T>
	///				advantege: size is often unknown ex-ante
	///
	template <class Enumerator, class TrgContainer, class TrgElem>
	class HasConvertibleCache {
		static_assert (!is_reference<TrgElem>(), "Use StorableT for reference targets!");

		// NOTE: Might be nicer to use explicit specialization - but conversion from any concrete type to interface is wanted!
		template <class V, class C>
		static auto GetCache(CachingEnumerator<V, C>&& source) -> decltype(source.CalcResults()) { return source.CalcResults(); }
		static None GetCache(...)																 { return None {}; }

	public:
		using TCache = decltype(GetCache(declval<Enumerator>()));

		static constexpr bool asWhole		= is_constructible_v<TrgContainer, TCache&&>;
		static constexpr bool byElement		= is_constructible_v<TrgElem, EnumeratedT<Enumerator>> && !IsNone<TCache>;
		static constexpr bool byElementOnly = !asWhole && byElement;
	};



	/// Obtain results in the desired container type - by the least copy/conversion possible, expecting a potential CachingEnumerator as Source.
	/// @param  hint:			for manual hints (e.g. .ToList(n))
	/// @tparam ReqContainer:	bound container type to create or obtain
	template <class ContainerOps, class R, class ReqContainer, class... ContArgs, class Source>
	ReqContainer ObtainCachedResults(Source& etor, size_t hint, const ContArgs&... args)
	{
		if constexpr (HasConvertibleCache<Source, ReqContainer, R>::asWhole) {
			// CachingEnumerator:  Direct convert / pass results if possible
			return etor.CalcResults();
		}
		else if constexpr (HasConvertibleCache<Source, ReqContainer, R>::byElementOnly) {
			// CachingEnumerator:  Convert / pass by element if available cache is unconvertible
			auto cached = etor.CalcResults();
			size_t size = GetSize(cached);

			auto res = ContainerOps::template Init<ReqContainer>(size, args...);
			for (auto& e : cached)
				ContainerOps::Add(res, PassRevived(e));		// StorableT!

			return res;
		}
		else {
			// Not a CachingEnumerator: enumerate to create new container
			static_assert (!HasConvertibleCache<Source, ReqContainer, R>::byElement, "HasConvertibleCache internal error.");

			SizeInfo si  = etor.Measure();
			size_t   cap = (si.IsExact() && hint < si) ? si.value : hint;

			auto res = ContainerOps::template Init<ReqContainer>(cap, args...);
			while (etor.FetchNext())
				ContainerOps::Add(res, etor.Current());

			return res;
		}
	}


	/// Wrapper overload for legacy calling format: more concise, less flexible.
	/// @tparam R:		expected cached TElem
	/// @tparam N...:	[0..1] number, inline buffer size only for "Small" container types
	template < class ContainerOps, class R, size_t... N, class... ContOptions,
		class Cont = typename ContainerOps::template Container<StorableT<R>, N..., ContOptions...>,
		class Source >
	Cont ObtainCachedResults(Source& etor, size_t hint, const ContOptions&... options)
	{
		return ObtainCachedResults<ContainerOps, R, Cont>(etor, hint, options...);
	}


#if ENUMERABLES_EMPLOY_DYNAMICCAST

	template <class ContainerOps, class R, class ReqContainer, class... ContArgs, class T = R>
	ReqContainer   ObtainCachedResults(InterfacedEnumerator<T>& etor, size_t hint, const ContArgs&... args)
	{
		// NOTE: - Currently only List-caching enumerators exist.
		//		 - Obtaining any cache is beneficial here to avoid repeated virtual calls during enumeration.
		//		   - Moving items would be possible with ConsumeCurrent() on IEnumerable too.
		//		 - Revise if any CachingEnumerator<T, SmallList> or CachingEnumerator<T, Set>
		//		   or default allocator option will exist.
		using AimedCache = ListOperations::Container<StorableT<T>>;

		using ET = CachingEnumerator<T, AimedCache>;
		ET* caching = etor.template TryCast<ET>();
		if (caching == nullptr)
			return ObtainCachedResults<ContainerOps, R, ReqContainer>(etor.WrappedInterface(), hint, args...);
		else
			return ObtainCachedResults<ContainerOps, R, ReqContainer>(*caching, hint, args...);
	}

#endif

	#pragma endregion

#pragma endregion




#pragma region Sources

	#pragma region Generators

	template <class T>
	class EmptyEnumerator : public IEnumerator<T> {
	public:
		bool		FetchNext()	      override	{ return false; }
		T			Current()	      override	{ ENUMERABLES_CLIENT_BREAK(EmptyError);  throw LogicException(EmptyError); }
		SizeInfo	Measure()	const override	{ return { Boundedness::Exact, 0 }; }
	};



	/// Repeats a single value indefinitely.
	template <class V, class Result = void>
	class RepeaterEnumerator : public IEnumerator<OverrideT<Result, V>> {
		const V&	value;

	public:
		using typename RepeaterEnumerator::IEnumerator::TElem;

		bool		FetchNext()	      override	{ return true; }
		TElem		Current()	      override	{ return value; }
		SizeInfo	Measure()	const override	{ return Boundedness::Unbounded; }

		RepeaterEnumerator(const V& val) : value { val } {}
	};



	/// Generates infinite (unchecked) sequence. Requires termination from outside.
	template <class V, class Stepper, class Result = void>
	class SequenceEnumerator final : public IEnumerator<InterimElemAccessT<Result, V>> {
		Reassignable<V>  curr;
		const Stepper&	 step;
		bool			 firstFetched = false;

	public:
		using typename SequenceEnumerator::IEnumerator::TElem;

		TElem	Current()	override
		{
			ENUMERABLES_ETOR_USAGE_ASSERT (firstFetched, MissedFetchError);
			return curr;
		}


		bool	FetchNext()	override
		{
			// CONSIDER: are non-movable types useful? Would require 2 alternating accumulators to do RVO.
			//			 e.g. curr2.AcceptRvo([this]() -> decltype(auto) { return step(*curr); });
			//			 Check also: ScannerBase

			if (firstFetched)
				curr = step(*curr);
			else
				firstFetched = true;

			return true;
		}

		SizeInfo  Measure()	const override	{ return Boundedness::Unbounded; }

		SequenceEnumerator(const V& start, const Stepper& step) : curr { ForwardParams, start }, step { step }  {}
	};

	#pragma endregion



	#pragma region Container Enumerators

	/// An Enumerator to wrap legacy C++ iterators.
	template <class TBegin, class TEnd/* = TBegin*/, class ForcedResult/* = void*/>
	class IteratorEnumerator final : public IEnumerator<OverrideT<ForcedResult, PointedT<TBegin>>> {
		const TEnd	end;
		TBegin		curr;
		bool		active = false;

	public:
		using typename IteratorEnumerator::IEnumerator::TElem;

		static_assert (!is_reference<ForcedResult>() || HasConstValue<ForcedResult> || !HasConstValue<PointedT<TBegin>>,
					   "The explicitly requested reference type loses const qualifier.");

		static_assert (!is_reference<ForcedResult>() || IsRefCompatible<ForcedResult, PointedT<TBegin>>,
					   "The explicitly requested type is not reference-compatible with elements - could return reference to a temporary.");

		bool	FetchNext() override
		{
			if (active)
				++curr;

			return active = (curr != end);
		}


		TElem	Current() override
		{
			ENUMERABLES_ETOR_USAGE_ASSERT (active, curr != end ? MissedFetchError : DepletedError);
			return *curr;
		}

		SizeInfo  Measure() const override	{ return TryGetIterDistance(curr, end); }

		IteratorEnumerator(const TBegin& beg, const TEnd& end) : curr { beg }, end { end }  {}
	};

	template <class TBegin, class TEnd = TBegin>
	IteratorEnumerator(const TBegin&, const TEnd&) -> IteratorEnumerator<TBegin, TEnd>;



	/// Variation of IteratorEnumerator for non-randomaccess iterators when the container is sizeable.
	template <class Container, class ForcedResult = void>
	class ContainerEnumerator final : public IEnumerator<OverrideT<ForcedResult, IterableT<Container>>> {
		Container&						subject;
		Deferred<IteratorT<Container>>	it;
		bool							unended = true;

		static_assert (!HasQueryableDistance<IteratorT<Container>>::value, "Should instantiate simple IteratorEnumerator!");
		static_assert (HasQueryableSize<Container>::value,				   "Should instantiate simple IteratorEnumerator!");

	public:
		using typename ContainerEnumerator::IEnumerator::TElem;

		static_assert (!is_reference<ForcedResult>() || IsRefCompatible<ForcedResult, IterableT<Container>>,
					   "The explicitly requested type is not reference-compatible with elements - could return reference to a temporary.");


		bool	FetchNext() override
		{
			if (it.IsInitialized() && unended)
				++(*it);
			else if (!it.IsInitialized())
				it = AdlBegin(subject);

			return unended = (*it != AdlEnd(subject));
		}


		TElem	Current() override
		{
			ENUMERABLES_ETOR_USAGE_ASSERT (it.IsInitialized(),  MissedFetchError);
			ENUMERABLES_ETOR_USAGE_ASSERT (*it != AdlEnd(subject), DepletedError);
			return **it;
		}


		SizeInfo  Measure() const override
		{
			return {
				!it.IsInitialized() ? Boundedness::Exact : Boundedness::KnownBound,
				GetSize(subject)
			};
		}

		ContainerEnumerator(Container& subject) : subject { subject }  {}
	};

	template <class C>
	ContainerEnumerator(C&) -> ContainerEnumerator<C>;

	#pragma endregion

#pragma endregion




#pragma region Chainable Manipulators

	// Adhere to the parameter-layout convention of ChainTool!
	//
	// Template arguments:		<TSource  [, TSource2], storage Args..., TypeonlyArgs...>
	//												                     ^
	//												                     |-- can default, can be required either
	//												 					 v
	// Ctor param list :		(Factory1 [, Factory2], deduced Args..., FixParams...)


	#pragma region Filtrations + Truncations

	enum class FilterMode : short { TakeWhile, SkipUntil, ReleaseBy };



	template <class Source, class TPred>
	class FilterEnumerator final : public IEnumerator<EnumeratedT<Source>> {
		Source			source;
		const TPred&	pred;

	public:
		using typename FilterEnumerator::IEnumerator::TElem;

		bool	  FetchNext()	  override
		{
			bool any = source.FetchNext();
			while (any && !pred(source.Current()))
				any = source.FetchNext();

			return any;
		}

		TElem	  Current()		  override	{ return source.Current(); }
		SizeInfo  Measure() const override	{ return source.Measure().Filtered(); }

		template <class Factory>
		FilterEnumerator(Factory&& getSource, const TPred& pred) : source { getSource() }, pred { pred }  {}
	};



	template <class Source, class TPred>
	class FilterUntilEnumerator final : public IEnumerator<EnumeratedT<Source>> {
		Source				source;
		const TPred&		pred;
		const FilterMode	mode;
		bool				active		= true;
		bool				dbgReleased = false;		// asserts only - fits well in padding

	public:
		using typename FilterUntilEnumerator::IEnumerator::TElem;

		bool	FetchNext() override
		{
			if (!active) {
				switch (mode) {
					case FilterMode::SkipUntil:
						return source.FetchNext();

					case FilterMode::ReleaseBy:
						dbgReleased = true;
						return false;

					case FilterMode::TakeWhile:
						return false;

					default:
						throw LogicException("Invalid parameter");
				}
			}
			ENUMERABLES_INTERNAL_ASSERT (active);

			bool any;
			bool accepted;
			do {
				any		 = source.FetchNext();
				accepted = any && pred(source.Current());
			}
			while (mode == FilterMode::SkipUntil && any && !accepted);
			// advance as needed for Skip - otherwise 1 step

			switch (mode) {
				case FilterMode::SkipUntil:
					ENUMERABLES_INTERNAL_ASSERT (!any || accepted);
					active = false;
					return any;

				case FilterMode::TakeWhile:
					return active = accepted;

				case FilterMode::ReleaseBy:
					active = !accepted;
					return any;

				default:
					throw LogicException("Invalid parameter");
			}
		}


		TElem	Current() override
		{
			ENUMERABLES_ETOR_USAGE_ASSERT (mode != FilterMode::TakeWhile || active,	DepletedError);
			ENUMERABLES_ETOR_USAGE_ASSERT (!dbgReleased,							DepletedError);
			return source.Current();
		}


		SizeInfo	Measure() const override
		{
			return source.Measure().Filtered(mode != FilterMode::SkipUntil); 
		}


		template <class Factory>
		FilterUntilEnumerator(Factory&& getSource, const TPred& pred, FilterMode mode)
			: source { getSource() }, pred { pred }, mode { mode }  {}
	};


	
	// for cases when the operand can't be readily captured as a SetType<T>
	template <class Source, class OpSource, class... SetOptions>
	class SetFilterEnumerator final : public IEnumerator<EnumeratedT<Source>> {

		// CONSIDER: this decaying is not transparent currently, if Options has an allocator, that must follow it. Hint added to static assert.
		using S	   = StorableT<DecayIfScalarT<EnumeratedT<OpSource>>>;
		using TSet = AdjustedSet<S, SetOptions...>;

		Source		source;
		TSet		operand;
		const bool	intersect;	// == !subtract


		static TSet   CreateOperand(OpSource&& etor, const SetOptions&... opts)
		{
			// NOTE: S won't always match CalcResults' type, but getting a Set CachingEnumerator here would be quite lucky anyway
			return ObtainCachedResults<SetOperations, S, TSet>(etor, 0, opts...);
		}

		// NOTE: Avoids MSVC 2015 bug: expanding SetOptions... within ctor initializer-block results in C2226.
		static TSet   CreateOperandDefaultOpts(OpSource&& etor)
		{
			return ObtainCachedResults<SetOperations, S, TSet>(etor, 0, SetOptions {}...);
		}

	public:
		using typename SetFilterEnumerator::IEnumerator::TElem;
		
		TElem	Current()	override	{ return source.Current(); }

		bool	FetchNext()	override
		{
			while (source.FetchNext()) {
				bool inOper = SetOperations::Contains<S>(operand, source.Current());
				if (inOper == intersect)
					return true;
			}
			return false;
		}


		SizeInfo  Measure()	const override
		{
			SizeInfo s = source.Measure();
			return intersect
				? s.Filtered().Limit(GetSize(operand))
				: s.Filtered();
		}


		template <class SrcFactory, class OpFactory>
		SetFilterEnumerator(SrcFactory&& getSource, OpFactory&& getOpSource, const SetOptions&... opts, bool intersect) :
			source    { getSource() },
			intersect { intersect },
			operand   { CreateOperand(getOpSource(), opts...) }
		{
		}

		template <class SrcFactory, class OpFactory, class = typename IfMultipleTypes<OpFactory, SetOptions...>::type>
		SetFilterEnumerator(SrcFactory&& getSource, OpFactory&& getOpSource, bool intersect) :
			source    { getSource() },
			intersect { intersect },
			operand   { CreateOperandDefaultOpts(getOpSource()) }
		{
		}
	};



	template <class Source>
	class CounterEnumerator final : public IEnumerator<EnumeratedT<Source>> {
		Source				source;
		size_t				counter;
		const FilterMode	mode;
		bool				dbgDepleted = false;	// asserts only - fits well in padding

	public:
		using typename CounterEnumerator::IEnumerator::TElem;


		bool FetchNext() override
		{
			if (mode == FilterMode::TakeWhile && counter == 0) {
				dbgDepleted = true;
				return false;
			}

			bool any = source.FetchNext();

			if (mode == FilterMode::SkipUntil) {
				while (any && counter > 0) {
					any = source.FetchNext();
					--counter;
				}
			}
			else {
				--counter;
			}

			return any;
		}


		TElem Current() override
		{
			ENUMERABLES_ETOR_USAGE_ASSERT (mode != FilterMode::SkipUntil || counter == 0, MissedFetchError);
			ENUMERABLES_ETOR_USAGE_ASSERT (mode != FilterMode::TakeWhile || !dbgDepleted, DepletedError);

			return source.Current();
		}


		SizeInfo Measure() const override
		{
			SizeInfo s0 = source.Measure();

			return (mode == FilterMode::TakeWhile)
				? s0.Limit(counter)
				: s0.Subtract(counter);
		}


		template <class Factory>
		CounterEnumerator(Factory&& getSource, FilterMode mode, size_t count) : source { getSource() }, mode { mode }, counter { count }
		{
			ENUMERABLES_INTERNAL_ASSERT (mode == FilterMode::TakeWhile || mode == FilterMode::SkipUntil);
		}
	};



	template <class Source, class TWanted>
	class TypeFilterEnumerator final : public IEnumerator<TWanted> {
		using E = PointedOrRefdT<typename Source::TElem>;
		using W = PointedOrRefdT<TWanted>;

		Source	source;
		W*		current = nullptr;

	public:
		static_assert (is_pointer<typename Source::TElem>() || is_reference<typename Source::TElem>(),
					   "Only pointer or reference elements can hide a different dynamic type!");
		
		static_assert (is_pointer<TWanted>() || is_reference<TWanted>(),
					   "Please specify the desired pointer or reference type exactly for readability.");

		bool	FetchNext()	override
		{
			current = nullptr;
			while (current == nullptr && source.FetchNext()) {
				E* elem = PointerOrRef<E*>::Translate(source.Current());
				current = dynamic_cast<W*>(elem);
			}
			return current != nullptr;
		}

		TWanted	Current()	override
		{
			ENUMERABLES_ETOR_USAGE_ASSERT (current != nullptr, NotFetchedError);
			return PointerOrRef<TWanted>::Translate(current);
		}

		SizeInfo Measure()	const override
		{ 
			return source.Measure().Filtered();
		}

		template <class Factory>
		TypeFilterEnumerator(Factory&& getSource) : source { getSource() }  {}
	};

	#pragma endregion



	#pragma region Element Mappings

	// NOTE: Mapper could be used equivalently, but is 8 bytes larger and .As is a frequent implicit operation, so keep this!
	template <class Source, class TConverted>
	class ConverterEnumeratorBase : public IEnumerator<TConverted> {
	protected:
		Source source;

	public:
		static_assert (!is_reference<TConverted>() || is_reference<typename Source::TElem>(),
					   "Source elements are prvalues, requested references would become dangling!");

		bool		FetchNext()		  override final	{ return source.FetchNext(); }
		SizeInfo	Measure()   const override final	{ return source.Measure(); }

		template <class Factory>
		ConverterEnumeratorBase(Factory&& getSource) : source { getSource() }  {}
	};


	template <class Source, class TResult>
	class ConverterEnumerator final : public ConverterEnumeratorBase<Source, TResult> {
	public:
		using ConverterEnumeratorBase<Source, TResult>::ConverterEnumeratorBase;

		static_assert (!is_reference<TResult>() || IsRefCompatible<TResult, typename Source::TElem>,
					   "Requested type is not reference-compatible with source element - could return reference to a temporary.");

		TResult	 Current() override  { return this->source.Current(); }
	};


	template <class Source, class TCasted>
	class CastingEnumerator final : public ConverterEnumeratorBase<Source, TCasted> {
	public:
		using ConverterEnumeratorBase<Source, TCasted>::ConverterEnumeratorBase;

		static_assert (!is_reference<TCasted>() || AreRefCompatible<TCasted, typename Source::TElem>,
					   "No reference-compatibility between source and target type - could return reference to a temporary.");

		TCasted  Current() override  { return static_cast<TCasted>(this->source.Current()); }
	};


	template <class Source, class TCasted>
	class DynCastingEnumerator final : public ConverterEnumeratorBase<Source, TCasted> {
	public:
		using ConverterEnumeratorBase<Source, TCasted>::ConverterEnumeratorBase;

		TCasted  Current() override  { return dynamic_cast<TCasted>(this->source.Current()); }
	};



	template <class Source, class Mapper>
	class MapperEnumerator final : public IEnumerator<MappedT<EnumeratedT<Source>, Mapper>> {
		Source			source;
		const Mapper&	map;

	public:
		using typename MapperEnumerator::IEnumerator::TElem;

		bool		FetchNext()		  override	{ return source.FetchNext(); }
		TElem		Current()		  override	{ return map(source.Current()); }
		SizeInfo	Measure()	const override	{ return source.Measure(); }

		template <class Factory>
		MapperEnumerator(Factory&& getSource, const Mapper& map) : source { getSource() }, map { map }  {}
	};

	

	template <class Source>
	class IndexerEnumerator final : public IEnumerator<Indexed<EnumeratedT<Source>>> {
		Source	source;
		size_t	index	= SIZE_MAX;

	public:
		using typename IndexerEnumerator::IEnumerator::TElem;

		bool	  FetchNext()	  override
		{
			++index;					// defined overflow
			return source.FetchNext();
		}

		TElem	  Current()		  override	{ return { index, source.Current() }; }
		SizeInfo  Measure()	const override	{ return source.Measure(); }

		template <class Factory>
		IndexerEnumerator(Factory&& getSource) : source { getSource() }  {}
	};



	// NOTE: Could be replaced with ScannerEnumerator  -  Acc = (curr, prev * curr),
	//		 But:  for Current() TElem would need to be copyable.
	template <class Source, class Combiner>
	class CombinerEnumerator final : public IEnumerator<CombinedT<EnumeratedT<Source>, EnumeratedT<Source>, Combiner>> {		
		using V = EnumeratedT<Source>;

		Source				source;
		const Combiner&		binop;
		Deferred<V>			prev;
		bool				nextFetched = false;

	public:
		using typename CombinerEnumerator::IEnumerator::TElem;

		bool	FetchNext()	override
		{
			if (nextFetched) {
				prev.AssignCurrent(source);
				return nextFetched = source.FetchNext();
			}

			if (!prev.IsInitialized() && source.FetchNext()) {
				prev.AssignCurrent(source);
				return nextFetched = source.FetchNext();
			}

			ENUMERABLES_INTERNAL_ASSERT (!nextFetched);
			return false;
		}


		TElem	Current()	override
		{
			ENUMERABLES_ETOR_USAGE_ASSERT (nextFetched, NotFetchedError);

			return binop(*prev, source.Current());
		}


		// NOTE: This implementation is non-monotonic in conjunction with ContainerEnumerator (Exact, N - 1) -> (Bound, N)
		//		 but Measure() during enumeration is just a nice to have.
		SizeInfo  Measure()	const override
		{
			return source.Measure().Subtract(!prev.IsInitialized());
		}

		template <class Factory>
		CombinerEnumerator(Factory&& getSource, const Combiner& binop) : source { getSource() }, binop { binop }  {}
	};



	template <class Source1, class Source2, class Zipper, class Result = void>
	class ZipperEnumerator final : public IEnumerator<OverrideT<Result, CombinedT<EnumeratedT<Source1>, EnumeratedT<Source2>, Zipper>>> {
		Source1			source1;
		Source2			source2;
		const Zipper&	zip;

	public:
		using typename ZipperEnumerator::IEnumerator::TElem;

		TElem		Current()	      override	{ return zip(source1.Current(), source2.Current());  }
		bool		FetchNext()	      override	{ return source1.FetchNext() && source2.FetchNext(); }
		SizeInfo	Measure()	const override	{ return source1.Measure().Limit(source2.Measure()); }

		template <class Fact1, class Fact2>
		ZipperEnumerator(Fact1&& getSource1, Fact2&& getSource2, const Zipper& zip) :
			source1 { getSource1() },
			source2 { getSource2() },
			zip		{ zip }
		{
		}
	};

	#pragma endregion



	#pragma region Concatenations
	
	template <class Source, class ContinuationSource>
	class ConcatEnumerator final : public IEnumerator<EnumeratedT<Source>> {
		static_assert (is_convertible<EnumeratedT<ContinuationSource>, EnumeratedT<Source>>(), "Concat: Incompatible continuation.");

		Source				source;
		ContinuationSource	continuation;
		bool				sourceDepleted = false;

	public:
		using typename ConcatEnumerator::IEnumerator::TElem;

		bool	  FetchNext()	  override
		{
			sourceDepleted = sourceDepleted || !source.FetchNext();
			return			!sourceDepleted || continuation.FetchNext();
		}

		TElem	  Current()		  override	{ return sourceDepleted ? continuation.Current() : source.Current(); }
		SizeInfo  Measure()	const override	{ return source.Measure().Add(continuation.Measure()); }

		template <class SrcFact, class ContFact>
		ConcatEnumerator(SrcFact&& getSource, ContFact&& getContinuation) : source { getSource() }, continuation { getContinuation() }  {}
	};



	template <class Source>
	class FlattenerEnumerator final : public IEnumerator<IterableT<EnumeratedT<Source>>> {

		using NestedEb = EnumeratedT<Source>;
		using Deducer  = UniformEnumerationDeducer<NestedEb>;
		using NestedEt = typename Deducer::TEnumerator;

		Source				ebSource;
		Deferred<NestedEb>	nested;				// lifetime might be tied to it! (e.g. ToMaterialized())
		Deferred<NestedEt>	nestedEnumerator;

	public:
		using typename FlattenerEnumerator::IEnumerator::TElem;

		bool	  FetchNext()	  override
		{
			// try to advance outer enumerator as needed
			while (!nestedEnumerator.IsInitialized() || !nestedEnumerator->FetchNext()) {
				if (!ebSource.FetchNext())
					return false;

				// avoid move requirements on returns
				nested.AssignCurrent(ebSource);
				nestedEnumerator.AcceptRvo([this]() { return Deducer::GetEnumerator(*nested); });
			}

			return true;
		}

		TElem	  Current()		  override	{ return nestedEnumerator->Current(); }
		SizeInfo  Measure()	const override	{ return { Boundedness::Unknown }; }

		template <class Factory>
		FlattenerEnumerator(Factory&& getSource) : ebSource { getSource() }  {}
	};



	// TODO: alloc-free Deferred<T> version for N = 1
	template <class Source>
	class ReplayEnumerator final : public IEnumerator<EnumeratedT<Source>> {
	public:
		using typename ReplayEnumerator::IEnumerator::TElem;

	private:
		Source								source;
		size_t								counter;
		SmallListType<StorableT<TElem>, 1>	headElems;
		bool								inReplay = false;

	public:
		bool FetchNext() override
		{
			if (!inReplay) {
				bool srcNext = source.FetchNext();
				if (srcNext && 0 < counter) {
					SmallListOperations::Add(headElems, source.Current());
					--counter;
					return true;
				}
				inReplay = !srcNext;
				return srcNext || GetSize(headElems) > 0;
			}

			if (counter != GetSize(headElems))
				++counter;

			return counter != GetSize(headElems);
		}


		TElem Current() override
		{
			if (!inReplay)
				return source.Current();

			ENUMERABLES_ETOR_USAGE_ASSERT (counter < GetSize(headElems), DepletedError);

			StorableT<TElem>& r = SmallListOperations::Access(headElems, counter);
			return Revive(r);
		}


		SizeInfo Measure() const override
		{
			SizeInfo toGo     = source.Measure();
			size_t   toRepeat = GetSize(headElems) + std::min(counter, toGo.value);
			return { toGo.kind, toGo.value + toRepeat };
		}


		template <class Factory>
		ReplayEnumerator(Factory&& getSource, size_t n) :
			source    { getSource() },
			counter   { n },
			headElems { SmallListOperations::Init<decltype(headElems)>(n) }
		{
		}
	};

	#pragma endregion



	#pragma region Scan (folding) operations

	/// For Scan operations - like foldl / Aggregate, but return each partial result.
	template <class Source, class Combiner, class TAcc, template <class> class Storage>
	class ScannerBase : public IEnumerator<TAcc> {

		const Combiner& combine;

		using InElem = typename Source::TElem;

		static_assert (IsCallable<Combiner, TAcc, InElem>::value,
					   "Aggregator function is not callable with (TAcc, TElem).");

		// let narrowing through - it provides readable error in place
		static_assert (IsConstructibleAnyway<TAcc, CombinedT<TAcc, InElem, Combiner>>
					   || is_assignable_v<TAcc, CombinedT<TAcc, InElem, Combiner>>,
					   "Accumulator type is neither constructible nor assignable from "
					   "the result of the Aggregator function."						   );
	
	protected:
		Source				source;
		Storage<TAcc>		accumulator;

		bool CombineNext()
		{
			const bool any = source.FetchNext();
			if (any)
				accumulator = combine(std::forward<TAcc>(*accumulator), source.Current());

			return any;
		}

	public:
		// CONSIDER: ETORUSAGE_ASSERTs? Would require extra bytes here - prob. not worth it this case.
		TAcc		Current()		  override final	{ return accumulator; }
		SizeInfo	Measure()	const override final	{ return this->source.Measure(); }

		template <class Factory>
		ScannerBase(Factory&& getSource, const Combiner& combiner) :
			source		{ getSource() },
			combine		{ combiner }
		{
		}

		template <class Factory>
		ScannerBase(Factory&& getSource, const Combiner& combiner, const StorableT<TAcc>& init) :
			source		{ getSource() },
			combine		{ combiner },
			accumulator	{ ForwardParams, Revive(init) }
		{
		}
	};



	/// Scan operation with accumulator explicitly initialized.
	template <class Source, class Combiner, class TAcc>
	class ScannerEnumerator final : public ScannerBase<Source, Combiner, TAcc, Reassignable> {
	public:
		bool FetchNext() override	{ return this->CombineNext(); }

		using ScannerEnumerator::ScannerBase::ScannerBase;
	};



	/// Scan operation with accumulator initialized by the First fetched element.
	template <
		class Source,
		class Combiner,
		class InitAccMapper /*= None*/,		// NOTE: a bit clunky with Chain<...>
		class TAcc >
	class FetchFirstScannerEnumerator final : public ScannerBase<Source, Combiner, TAcc, Deferred> {

		using Base   = ScannerBase<Source, Combiner, TAcc, Deferred>;
		using InElem = typename Source::TElem;

		const InitAccMapper&	initAccumulator;


		void InitFromCurrent()
		{
			if constexpr (IsNone<InitAccMapper>) {
				if constexpr (is_same_v<InElem, TAcc>) {
					this->accumulator.AssignCurrent(this->source);
				}
				else {
					static_assert(IsBraceConstructible<TAcc, InElem>::value,
								  "Can't convert input element into TAcc. (Narrowing not supported.)"
								  " Please specify an Initial Accumulator Mapper."				     );

					this->accumulator = this->source.Current();
				}
			}
			else {
				if constexpr (is_same_v<MappedT<InElem, InitAccMapper>, TAcc>) {
					this->accumulator.AcceptRvo([this]() -> decltype(auto) {
						return initAccumulator(this->source.Current());
					});
				}
				else {
					static_assert(IsBraceConstructible<TAcc, MappedT<InElem, InitAccMapper>>::value,
								  "Can't convert input element into TAcc. (Narrowing not supported.)");

					this->accumulator = initAccumulator(this->source.Current());
				}
			}
		}

	public:
		bool FetchNext() override
		{
			if (this->accumulator.IsInitialized())
				return this->CombineNext();

			const bool any = this->source.FetchNext();
			if (any)
				InitFromCurrent();

			return any;
		}


		template <class Fact>
		FetchFirstScannerEnumerator(Fact&& getSource, const Combiner& combiner, const InitAccMapper& initializer = {}) :
			Base			{ getSource, combiner },
			initAccumulator	{ initializer }
		{
		}
	};

	#pragma endregion

#pragma endregion




#pragma region Chainable Caching Operations

	template <class Source, class Ordering>
	class SorterEnumerator final : public CachingEnumerator<EnumeratedT<Source>,
															ListOperations::Container<StorableT<EnumeratedT<Source>>>> {
		Source			 source;
		const Ordering&	 ordering;

	public:
		using typename SorterEnumerator::CachingEnumerator::TCache;
		using typename SorterEnumerator::CachingEnumerator::TElem;

		TCache	  CalcResults() override
		{
			TCache cache = ObtainCachedResults<ListOperations, StorableT<TElem>>(source, 0);
			std::sort(AdlBegin(cache), AdlEnd(cache), ordering);
			return cache;
		}

		SizeInfo  Measure() const override	{ return source.Measure(); }

		template <class Factory>
		SorterEnumerator(Factory&& getSource, const Ordering& ordering) : source { getSource() }, ordering { ordering }  {}
	};



	template <class Source, class Ordering>
	class MinSeekEnumerator final : public CachingEnumerator<EnumeratedT<Source>,
															 ListOperations::Container<StorableT<EnumeratedT<Source>>>> {
		Source			 source;
		const Ordering&	 isLess;

	public:
		using typename MinSeekEnumerator::CachingEnumerator::TCache;
		using typename MinSeekEnumerator::CachingEnumerator::TElem;

		// CONSIDER: Check perf of a separate alg for .MinimumsBy(field)
		TCache CalcResults() override
		{
			TCache minimums;
			if (!source.FetchNext())
				return minimums;

			ListOperations::Add(minimums, source.Current());
			bool more = true;
			while (more) {
				TElem& best = Revive(ListOperations::Access(minimums, 0));

				// nested loop is separate for slight perf increase
				while (more = source.FetchNext()) {
					TElem&& curr = source.Current();

					if (isLess(best, curr))
						continue;

					if (isLess(curr, best))
						ListOperations::Clear(minimums);

					ListOperations::Add(minimums, forward<TElem>(curr));
					break;
				}
			}
			return minimums;
		}


		SizeInfo Measure() const override
		{
			SizeInfo s = source.Measure();
			if (s.IsExact())
				s.kind = Boundedness::KnownBound;
			return s;
		}


		template <class Factory>
		MinSeekEnumerator(Factory&& getSource, const Ordering& ordering) : source { getSource() }, isLess { ordering }  {}
	};

#pragma endregion




#pragma region Construction helpers

	/// Choose Enumerator for Container based on available kinds of size info.
	template <class ForcedResult = void, class C>
	auto CreateEnumeratorFor(C& container)
	{
		using Beg = IteratorT<C&>;
		using End = EndIteratorT<C&>;
		constexpr bool containerSizeOnly = HasQueryableSize<C>::value && 
										  !HasQueryableDistance<Beg, End>::value;
		if constexpr (containerSizeOnly)
			return ContainerEnumerator<C, ForcedResult> { container };
		else
			return IteratorEnumerator<Beg, End, ForcedResult> { AdlBegin(container), AdlEnd(container) };
	}



	#pragma region Aggregate / Scan deductions

	/// Trait to chose between ScannerEnumerator and FetchFirstScannerEnumerator.
	/// @remarks
	///		A bit exploratory feature. The overloads could also be separated by finding good names...
	///		This class makes a light enable_if decision only, further checks should commence after the choice is made.
	template <class TElem, class ValOrMapper, class ForcedAcc = void>
	class IsAccuInit {
		
		static constexpr bool IsMapper()
		{
			if constexpr (is_void_v<ForcedAcc> || !IsConstructibleAnyway<ForcedAcc, ValOrMapper>)
				return IsCallable<ValOrMapper, TElem>::value;
				// Shouldn't try to deduce (possibly running into Error) on the other branch!
			else
				return false;
		}

	public:
		static constexpr bool byMapping = IsMapper();
		static constexpr bool byValue   = is_void_v<ForcedAcc> ? !IsMapper() : IsConstructibleAnyway<ForcedAcc, ValOrMapper>;

		static_assert (byValue || byMapping, "Bad argument to initialize the accumulator. This algorithm requires either:\n"
					   "  A) An initial Accumulator value [convertible to ForcedAcc type if that's specified]\n"
					   "  B) An initial Mapper : TElem -> Acc to be applied to the first element before the Combiner calls.");
	};



	/// Determines accumulator type for Scan/Aggregate and provides friendly errors.
	/// Member-Select semantics are the default, use explicit ForcedAcc to force &.
	template <class TElem>
	class AccuDeducer {

		template <class A, class E, class C>
		static auto TryCombine() -> CombinedT<A, E, C>;

		template <class A, class E, class C>
		static auto TryCombine() -> AppliedMemberT<A, C, E>;

		template <class A, class E, class C>
		static auto TryCombine() -> enable_if_t<!IsCallable<C, A, E>::value && !IsCallableMember<A, C, E>::value, void>;

		template <class E, class M>
		static auto TryMap() -> MappedT<E, M>;

		template <class E, class M>
		static auto TryMap() -> enable_if_t<!IsCallable<M, E>::value, void>;



		// commons for all init
		template <class Combiner, class Acc>
		struct CombineCheck {
			// NOTE: repeated wrapping mechanism of AutoEnumerable::CombinerL for concise call...
			//		 Also TryCombine got augmented for memberpointers.
			using Wrapped = decay_t<decltype(LambdaCreators::BinaryMapper<Acc, TElem>(declval<Combiner>()))>;

			static_assert (IsCallable<Wrapped, Acc&&, TElem>::value,
						   "The Combiner object is not callable with (Acc, TElem).");

			using Next = CombinedT<Acc&&, TElem, Wrapped>;

			static_assert (is_constructible<Acc, Next>() || is_assignable<Acc, Next>(),
						   "Can't convert Combiner's result into the next accumulator value.");

			using TAcc = Acc;
		};


		template <class Combiner, class ForcedAcc>
		struct FirstInitCheck {
			using TCallResult = decltype(TryCombine<TElem, TElem, Combiner>());
			using TImplicit	  = LambdaCreators::NonExpiringMemberT<TElem, TCallResult>;

			using TProposed   = OverrideT<ForcedAcc, TImplicit>;

			static_assert (!is_void<TCallResult>() || !is_void<ForcedAcc>(),
						   "Aggregator function is not callable with (TElem, TElem)"
						   " or it results void. \n"
						   "Specify accumulator type or initial value explicitly!"	);

			using TAcc = typename CombineCheck<Combiner, TProposed>::TAcc;
		};


		template <class Init, class Combiner, class ForcedAcc>
		struct DirectInitCheck {

			using TCallResult = decltype(TryCombine<Init, TElem, Combiner>());

			using TCompatible = CompatResultT<Init, TCallResult>;
			using TNoExp	  = LambdaCreators::NonExpiringMemberT<TElem, TCompatible>;

			static_assert (!is_void<ForcedAcc>() || !is_void<TCompatible>(), "Can't deduce common compatible type. Speficy Accumulator type explicitly!");

			// TODO: Vague but practical check for functions choosing between const&s (e.g. std::max) to prevent referring temporaries.
			//		 For now only known problematic cases get decayed. ForcedAcc is unconstrained.
			//		 Desired: LambdaCreators could track their exact wrapped function types -> rendering possible to check if converted temporaries were needed!
			using VElem	= BasePointedT<TElem>;
			using VInit	= BasePointedT<Init>;
			using TImplicit	= conditional_t< !HasConstValue<TNoExp>				// shouldn't have temporaries with normal design
										  || IsRefCompatible<TNoExp, TElem>		// probably just ref/ptr conversion to ancestor
										  || is_same_v<VInit, VElem>,			// can be a valid subobject selection, even if no ref-comp.
											 TNoExp,
											 decay_t<TNoExp> >;
			using TProposed = OverrideT<ForcedAcc, TImplicit>;

			// quick fix to allow memberfunction-pointers
			static constexpr bool isCallable = IsCallable<Combiner, TProposed, TElem>::value
											|| IsCallableMember<TProposed, Combiner, TElem>::value;

			static_assert ( is_void<ForcedAcc>() || is_constructible<ForcedAcc, Init>(), "SFINAE error.");
			static_assert ( is_void<ForcedAcc>() || isCallable,	"Combiner function not callable with the specified accumulator.");
			static_assert (!is_void<ForcedAcc>() || isCallable,	"Combiner function not callable with the deduced accumulator "
																"(common reference for its result and Init or decayed Init)." );

			using TAcc = typename CombineCheck<Combiner, TProposed>::TAcc;
		};


		template <class Mapper, class Combiner, class ForcedAcc>
		struct MappingInitCheck {
			using MapResult = decltype(TryMap<TElem, Mapper>());
			using TImplicit = LambdaCreators::NonExpiringMemberT<TElem, MapResult>;
			using TProposed = OverrideT<ForcedAcc, TImplicit>;

			static_assert (IsBraceConstructible<TProposed, MapResult>::value,
						   "Accumulator can't be initialized with the initial mapper's result.\n"
						   "HINT: in case of ambiguity, specify Accumulator type explicitly to\n"
						   "prefer copy init over mapping with the object's operator()(TElem) ! ");

			using TAcc = typename CombineCheck<Combiner, TProposed>::TAcc;
		};

	public:
		template <class Combiner, class ForcedAcc = void>
		using ForFirstInit   = typename FirstInitCheck<Combiner, ForcedAcc>::TAcc;

		template <class Init, class Combiner, class ForcedAcc = void>
		using ForDirectInit  = typename DirectInitCheck<Init, Combiner, ForcedAcc>::TAcc;

		template <class InitMapper, class Combiner, class ForcedAcc = void>
		using ForMappingInit = typename MappingInitCheck<InitMapper, Combiner, ForcedAcc>::TAcc;
	};

	#pragma endregion

#pragma endregion


}		// namespace Enumerables::Def

#endif	// ENUMERABLES_ENUMERATORS_HPP
