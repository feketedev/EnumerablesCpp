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



namespace Enumerables {
namespace Def {

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

	/// Whether the container has a registered GetSize in our Enumerables namespace.
	template <class C>
	class HasQueryableSize {

		template <class CC = C>
		static auto Check(const CC& c) -> decltype(GetSize(c));

		static void Check(...);

	public:
		using					SizeType = decltype(Check(declval<C>()));
		static constexpr bool	value	 = !is_void<SizeType>::value;
	};



	/// Whether two iterators have queriable distance.
	template <class It>
	class HasQueryableDistance {

		template <class I = It>
		static auto CalcDiff(const I& a, const I& b) -> decltype(b - a);

		static void CalcDiff(...);

	public:
		using					DiffType = decltype(CalcDiff(declval<It>(), declval<It>()));
		static constexpr bool	value	 = !is_void<DiffType>::value;
	};


	template <class Container>
	constexpr bool HasIteratorDifference = HasQueryableDistance<IteratorT<Container>>::value;



	/// Actual iterator distance, if available.
	template <class It>
	auto TryGetIterDistance(const It& s, const It& e) -> enable_if_t<HasQueryableDistance<It>::value, SizeInfo>
	{
		auto diff = e - s;

		static_assert (std::numeric_limits<decltype(diff)>::digits <= std::numeric_limits<size_t>::digits
					   && std::is_integral<decltype(diff)>::value,
					   "Unsafe cast to size_t.");

		// infrequent call, unknown types, rather make sure
		if (diff >= 0)
			return { Boundedness::Exact, static_cast<size_t>(diff) };

		// quite improbably an internal bug
		ENUMERABLES_CLIENT_BREAK ("Negative iterator distance??");
		throw LogicException("Negative iterator distance.");
	}


	template <class It>
	auto TryGetIterDistance(const It&, const It&) -> enable_if_t<!HasQueryableDistance<It>::value, SizeInfo>
	{
		// NOTE: Assuming that iterators belong to containers -> which are finite
		return { Boundedness::Bounded };
	}

	#pragma endregion



	#pragma region Nested iterable handling (e.g. Flatten)

	template <class TIter, class ForcedResult = void>	class IteratorEnumerator;


	/// Provides an Enumerator to traverse any container or AutoEnumerable in a uniform way (internally in Enumerators).
	/// @remarks
	///		Avoids circular dependency and temporary factory creation, as opposed to calling Enumerate(container).
	///		CONSIDER: handling iterators directly might reduce overheads, but would require some code duplication.
	template <class T>
	struct UniformEnumerationDeducer {

		template <class Eb>
		static auto GetEnumerator(Eb& enumerable) -> enable_if_t<IsAutoEnumerable<Eb>::value, decltype(enumerable.GetEnumerator())>
		{
			return enumerable.GetEnumerator();
		}

		template <class C>
		static auto GetEnumerator(C& collection) -> enable_if_t<!IsAutoEnumerable<C>::value, IteratorEnumerator<decltype(AdlBegin(collection))>>
		{
			return { AdlBegin(collection), AdlEnd(collection) };
		}

		using TEnumerator = decltype(GetEnumerator(declval<T&>()));
	};

	#pragma endregion

#pragma endregion




#pragma region Fundamentals

	/// Main enumerator interface, also represents a 'concept'.
	template <class T>
	class IEnumerator {
	public:
		using TElem = T;

		/// @returns:	A next element is available.
		virtual bool			FetchNext()		  = 0;

		/// Current element if FetchNext returned true.
		virtual T				Current()		  = 0;

		/// Try to inform about the remaining element count.
		/// @remarks
		///		Designed to be called before any FetchNext.
		///		Mid-sequence the behaviour is defined, but the result may degrade to Unknown.
		///		Lightweight: shouldn't rely on heavier datastructures used by FetchNext.
		virtual SizeInfo		Measure()	const = 0;

		// Technical virtual move ctor, related to inline buffer handling.
		virtual IEnumerator<T>* MoveTo(void* mem) = 0;

		virtual ~IEnumerator();

		// Copy is undesired, we have factories and a bit different logic then iterators.
		IEnumerator(const IEnumerator<T>&) = delete;
		IEnumerator(IEnumerator<T>&&)	   = default;
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
		alignas(void*) char		fixBuffer[ENUMERABLES_INTERFACED_ETOR_INLINE_SIZE];


		bool IsOnHeap() const
		{
			return static_cast<const void*>(ptr) <  fixBuffer
				|| static_cast<const void*>(ptr) >= fixBuffer + ENUMERABLES_INTERFACED_ETOR_INLINE_SIZE;
		}


		template <class Et>
		static constexpr bool SureFitsInline()
		{
			using Buff = decltype(InterfacedEnumerator::fixBuffer);

			static_assert (alignof(Et) % alignof(Buff) == 0, "Alignment not power of 2?");

			// size_t req = alignof(Et);
			// size_t sat = alignof(Buff);
			// size_t slack = req - sat;
			// return slack + sizeof(Et) <= sizeof(Buff);

			return (alignof(Buff) >= alignof(Et))
				? (sizeof(Buff) >= sizeof(Et))
				: (alignof(Et) - alignof(Buff) + sizeof(Et) <= sizeof(Buff));
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
		IEnumerator<T>*		MoveTo(void*)   override	{ throw LogicException("Internal error: directly nested InterfacedEnumerators??"); }

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
		InterfacedEnumerator(NestedFactory&& fact, enable_if_t<SureFitsInline<InvokeResultT<NestedFactory>>()>* = nullptr)
			: ptr { (new (fixBuffer) TypeHelpers::RvoEmplacer<NestedFactory> { fact })->GetPtr() }
		{
		}

		template <class NestedFactory>
		InterfacedEnumerator(NestedFactory&& fact, enable_if_t<!SureFitsInline<InvokeResultT<NestedFactory>>(), int> = 0)
			: ptr { new InvokeResultT<NestedFactory> { fact() } }
		{
		}

		InterfacedEnumerator(InterfacedEnumerator&& src)
			: ptr { src.IsOnHeap() ? src.ptr : src.ptr->MoveTo(fixBuffer) }
		{
			if (src.IsOnHeap())
				src.ptr = nullptr;
		}
	};

}	// namespace Def
namespace TypeHelpers {

	template <class V>
	struct IsInterfacedEnumerator<Def::InterfacedEnumerator<V>>
	{
		static constexpr bool value = true;
	};

}
namespace Def {


	/// Adapts an Enumerator for usage inside range-based for (mimicking a legacy iterator).
	template <class TEnumerator>
	class EnumeratorAdapter {
		// TODO 17: optional initialization is required until c++17, as begin(), end() must have the same type.
		union {
			TEnumerator enumerator;
		};
		const bool isEmptyMock;
		bool	   hasNext;

	public:
		decltype(auto)	operator  *()	{ return enumerator.Current(); }
		void			operator ++()	{ hasNext = enumerator.FetchNext(); }

		/// Hack, as value of 'end' is don't care.
		/// Assuming end != beg won't get called (in line with the standard), end() returns a Null-Object.
		bool operator !=(const EnumeratorAdapter<TEnumerator>& end)
		{
			ENUMERABLES_ETOR_USAGE_ASSERT (end.isEmptyMock, "EnumeratorAdapter is Intended for range-based for only!");
			(void)end;
			return hasNext;
		}

		/// Represent an empty enumeration for end().
		///	Unfortunately a separate type cannot be used until C++17.
		EnumeratorAdapter() : isEmptyMock { true }, hasNext { false }
		{
		}

		template <class Fact>
		EnumeratorAdapter(const Fact& factory) : enumerator { factory() }, isEmptyMock { false }
		{
			// fresh enumerators point ahead the first element
			hasNext = enumerator.FetchNext();
		}

		// This type should only exist temporarily in for ( : ).
		// Not even a move should be called, but its implementation is mandatory until c++17.
		EnumeratorAdapter(const EnumeratorAdapter&) = delete;

		EnumeratorAdapter(EnumeratorAdapter&& src) : isEmptyMock { src.isEmptyMock }, hasNext { src.hasNext }
		{
			if (!isEmptyMock)
				new (&enumerator) TEnumerator { std::move(src.enumerator) };
		}

		~EnumeratorAdapter()
		{
			if (!isEmptyMock)
				enumerator.~TEnumerator();
		}
	};

#pragma endregion




#pragma region Refined Enumerator Concepts

	#pragma region Caching operations Fundamentals

	/// Abstract Enumerator for multi-pass algorithms run deffered on first Fetch.
	/// Results container as a whole can be obtained as an optimization.
	/// @remarks
	///		Implementors and ObtainCachedResults calls should use the same alias:
	///		- CachingEnumerator<T, ListType> vs
	///		  CachingEnumerator<T, ListOperations::Container>
	///		have different RTTI.
	template <class V, template <class> class Container>
	class CachingEnumerator : public IEnumerator<V> {
	protected:
		using TCache = Container<StorableT<V>>;

	private:
		struct State {
			TCache					results;
			IteratorT<TCache>		current;
			const IteratorT<TCache>	end;

			bool HasMore() const	{ return current != end; }

			State(CachingEnumerator& owner) : results { owner.CalcResults() }, current { AdlBegin(results) }, end { AdlEnd(results) }
			{
			}
		};

		Deferred<State>		fetchState;

	public:
		virtual TCache CalcResults() = 0;

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

		CachingEnumerator(CachingEnumerator&&) = default;
		CachingEnumerator() = default;

		~CachingEnumerator() override;
	};

	template <class V, template <class> class Container>
	CachingEnumerator<V, Container>::~CachingEnumerator() = default;



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
		static_assert (!is_reference<TrgElem>::value, "Use StorableT for reference targets!");

		// NOTE: Might be nicer to use explicit specialization - but conversion from any concrete type to interface is wanted!
		template <class V, template <class> class C>
		static auto GetCache(CachingEnumerator<V, C>&& source)	{ return source.CalcResults(); }
		static None GetCache(...)								{ return None {}; }

	public:
		using TCache = decltype(GetCache(declval<Enumerator>()));

		static constexpr bool asWhole		= is_constructible<TrgContainer, TCache&&>::value;
		static constexpr bool byElement		= is_constructible<TrgElem, EnumeratedT<Enumerator>>::value && !IsNone<TCache>;
		static constexpr bool byElementOnly = !asWhole && byElement;
	};



	/// Obtain results in the desired container type - by the least copy/conversion possible, expecting a potential CachingEnumerator as Source.
	/// @param  hint:	for manual hints (e.g. .ToList(n))
	/// @tparam N...:	[0..1] number, inline buffer size only for "Small" container types
	template <class ContainerOps, class R, size_t... N, class Cont = typename ContainerOps::template Container<R, N...>, class Source>
	Cont ObtainCachedResults(Source& etor, size_t /*hint*/, enable_if_t<HasConvertibleCache<Source, Cont, R>::asWhole, Cont>* = nullptr)
	{
		// CachingEnumerator:  Direct convert / pass results if possible
		return etor.CalcResults();
	}


	template <class ContainerOps, class R, size_t... N, class Cont = typename ContainerOps::template Container<R, N...>, class Source>
	Cont ObtainCachedResults(Source& etor, size_t /*hint*/, enable_if_t<HasConvertibleCache<Source, Cont, R>::byElementOnly, Cont>* = nullptr)
	{
		// CachingEnumerator:  Convert / pass by element if available cache is unconvertible
		auto cached = etor.CalcResults();
		size_t size = GetSize(cached);

		Cont res = ContainerOps::template Init<R, N...>(size);
		for (auto& e : cached)
			ContainerOps::Add(res, PassRevived(e));		// StorableT!

		return res;
	}


	template <class ContainerOps, class R, size_t... N, class Cont = typename ContainerOps::template Container<R, N...>, class Source>
	Cont ObtainCachedResults(Source& etor, size_t hint, enable_if_t<!HasConvertibleCache<Source, Cont, R>::byElement, Cont>* = nullptr)
	{
		// Not CachingEnumerator: create new list by enumerating
		SizeInfo si  = etor.Measure();
		size_t   cap = (si.IsExact() && hint < si) ? si.value : hint;

		Cont res = ContainerOps::template Init<R, N...>(cap);
		while (etor.FetchNext())
			ContainerOps::Add(res, etor.Current());

		return res;
	}


#if ENUMERABLES_EMPLOY_DYNAMICCAST

	// NOTE: SmallList<T, N> is not supported currently. Solve if any CachingEnumerator<T, SmallList> will exist.
	template <class ContainerOps, class R, template <class> class ContTempl = ContainerOps::template Container>
	ContTempl<R>   ObtainCachedResults(InterfacedEnumerator<IfPRValue<R>>& etor, size_t hint)
	{
		using ET = CachingEnumerator<R, ContTempl>;
		ET* caching = etor.template TryCast<ET>();
		if (caching == nullptr)
			return ObtainCachedResults<ContainerOps, R>(etor.WrappedInterface(), hint);
		else
			return ObtainCachedResults<ContainerOps, R>(*caching, hint);
	}

#endif

	#pragma endregion

#pragma endregion




#pragma region Sources

	#pragma region Generators

	template <class T>
	class EmptyEnumerator : public IEnumerator<T> {
	public:
		bool			FetchNext()		  override	{ return false; }
		T				Current()		  override	{ ENUMERABLES_CLIENT_BREAK(EmptyError);  throw LogicException(EmptyError); }
		SizeInfo		Measure()   const override	{ return { Boundedness::Exact, 0 }; }
		IEnumerator<T>* MoveTo(void* mem) override	{ return new (mem) EmptyEnumerator { std::move(*this) }; }
	};



	/// Repeats a single value indefinitely.
	template <class V, class Result = void>
	class RepeaterEnumerator : public IEnumerator<OverrideT<Result, V>> {
		const V&	value;

	public:
		using typename RepeaterEnumerator::IEnumerator::TElem;

		bool				FetchNext()		  override	{ return true; }
		TElem				Current()		  override	{ return value; }
		SizeInfo			Measure()	const override	{ return Boundedness::Unbounded; }
		IEnumerator<TElem>*	MoveTo(void* mem) override	{ return new (mem) RepeaterEnumerator { std::move(*this) }; }

		RepeaterEnumerator(const V& val) : value { val } {}
		RepeaterEnumerator(RepeaterEnumerator&&) = default;
	};



	/// Generates infinite (unchecked) sequence. Requires termination from outside.
	template <class V, class Stepper, class Result = void>
	class SequenceEnumerator final : public IEnumerator<OverrideT<Result, V>> {
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
			// CONSIDER 17: are non-movable types useful? Would require 2 alternating accumulators to do RVO.
			//				e.g. curr2.AcceptRvo([this]() -> decltype(auto) { return step(*curr); });
			//				Check also: ScannerBase

			if (firstFetched)
				curr = step(*curr);
			else
				firstFetched = true;

			return true;
		}

		SizeInfo			Measure()   const override	{ return Boundedness::Unbounded; }
		IEnumerator<TElem>*	MoveTo(void* mem) override	{ return new (mem) SequenceEnumerator { std::move(*this) }; }

		SequenceEnumerator(const V& start, const Stepper& step) : curr { ForwardParams, start }, step { step }  {}
		SequenceEnumerator(SequenceEnumerator&&) = default;
	};

	#pragma endregion



	#pragma region Container Enumerators

	// TODO 17: Support for different TBegin and TEnd (instead of TIter)

	/// An Enumerator to wrap legacy C++ iterators.
	template <class TIter, class ForcedResult/* = void*/>
	class IteratorEnumerator final : public IEnumerator<OverrideT<ForcedResult, PointedT<TIter>>> {
		const TIter end;
		TIter		curr;
		bool		active = false;

	public:
		using typename IteratorEnumerator::IEnumerator::TElem;

		static_assert (!is_reference<ForcedResult>::value || IsRefCompatible<ForcedResult, PointedT<TIter>>,
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


		SizeInfo			Measure()   const override	{ return TryGetIterDistance(curr, end); }
		IEnumerator<TElem>*	MoveTo(void* mem) override	{ return new (mem) IteratorEnumerator { std::move(*this) }; }


		IteratorEnumerator(const TIter& beg, const TIter& end) : curr { beg },	end { end }  {}
		IteratorEnumerator(IteratorEnumerator&&) = default;
	};



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

		static_assert (!is_reference<ForcedResult>::value || IsRefCompatible<ForcedResult, IterableT<Container>>,
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


		IEnumerator<TElem>*  MoveTo(void* mem) override
		{
			return new (mem) ContainerEnumerator { std::move(*this) };
		}


		ContainerEnumerator(Container& subject) : subject { subject }  {}
		ContainerEnumerator(ContainerEnumerator&&) = default;
	};

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

		bool	FetchNext()	override
		{
			bool any = source.FetchNext();
			while (any && !pred(source.Current()))
				any = source.FetchNext();

			return any;
		}

		TElem				Current()		  override	{ return source.Current(); }
		SizeInfo			Measure()   const override	{ return source.Measure().Filtered(); }
		IEnumerator<TElem>*	MoveTo(void* mem) override	{ return new (mem) FilterEnumerator { std::move(*this) }; }

		template <class Factory>
		FilterEnumerator(Factory&& getSource, const TPred& pred) : source { getSource() }, pred { pred }  {}
		FilterEnumerator(FilterEnumerator&&) = default;
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

		TElem				Current()		  override
		{
			ENUMERABLES_ETOR_USAGE_ASSERT (mode != FilterMode::TakeWhile || active,	DepletedError);
			ENUMERABLES_ETOR_USAGE_ASSERT (!dbgReleased,							DepletedError);
			return source.Current();
		}
		SizeInfo			Measure()   const override	{ return source.Measure().Filtered(mode != FilterMode::SkipUntil); }
		IEnumerator<TElem>*	MoveTo(void* mem) override	{ return new (mem) FilterUntilEnumerator { std::move(*this) };     }

		template <class Factory>
		FilterUntilEnumerator(Factory&& getSource, const TPred& pred, FilterMode mode) : source { getSource() }, pred { pred }, mode { mode }  {}
		FilterUntilEnumerator(FilterUntilEnumerator&&) = default;
	};



	// for cases when the operand can't be readily captured as a SetType<T>
	template <class Source, class OpSource>
	class SetFilterEnumerator final : public IEnumerator<EnumeratedT<Source>> {

		using S = StorableT<RemoveScalarRefT<EnumeratedT<OpSource>>>;

		Source			source;
		SetType<S>		operand;
		const bool		intersect;	// == !subtract

	public:
		using typename SetFilterEnumerator::IEnumerator::TElem;

		bool		FetchNext() override
		{
			while (source.FetchNext()) {
				bool inOper = SetOperations::Contains(operand, source.Current());
				if (inOper == intersect)
					return true;
			}
			return false;
		}

		SizeInfo	Measure() const override
		{
			SizeInfo s = source.Measure();
			return intersect
				? s.Filtered().Limit(GetSize(operand))
				: s.Filtered();
		}

		TElem				Current()		  override	{ return source.Current(); }
		IEnumerator<TElem>*	MoveTo(void* mem) override	{ return new (mem) SetFilterEnumerator { std::move(*this) }; }

		template <class SrcFactory, class OpFactory>
		SetFilterEnumerator(SrcFactory&& getSource, OpFactory&& getOpSource, bool intersect) :
			source    { getSource() },
			intersect { intersect }
		{
			// NOTE: S won't always match CalcResults' type, but getting a Set CachingEnumerator here would be really extreme anyway
			auto etor = getOpSource();
			operand   = ObtainCachedResults<SetOperations, S>(etor, 0);
		}

		SetFilterEnumerator(SetFilterEnumerator&&) = default;
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


		SizeInfo	Measure() const override
		{
			SizeInfo s0 = source.Measure();

			return (mode == FilterMode::TakeWhile)
				? s0.Limit(counter)
				: s0.Subtract(counter);
		}


		IEnumerator<TElem>*	MoveTo(void* mem) override
		{
			return new (mem) CounterEnumerator { std::move(*this) };
		}


		template <class Factory>
		CounterEnumerator(Factory&& getSource, FilterMode mode, size_t count) : source { getSource() }, mode { mode }, counter { count }
		{
			ENUMERABLES_INTERNAL_ASSERT (mode == FilterMode::TakeWhile || mode == FilterMode::SkipUntil);
		}

		CounterEnumerator(CounterEnumerator&&) = default;
	};

	#pragma endregion



	#pragma region Element Mappings

	// NOTE: Mapper could be used equivalently, but is 8 bytes larger and .As is a frequent implicit operation, so keep this!
	template <class Source, class TConverted>
	class ConverterEnumerator final : public IEnumerator<TConverted> {
		Source source;

	public:
		static_assert (!is_reference<TConverted>::value || is_reference<typename Source::TElem>::value,
					   "Source elements are prvalues, requested references would become dangling!");

		static_assert (!is_reference<TConverted>::value || IsRefCompatible<TConverted, typename Source::TElem>,
					   "Requested type is not reference-compatible with source element - could return reference to a temporary.");

		bool						FetchNext()		  override	{ return source.FetchNext(); }
		TConverted					Current()		  override	{ return source.Current();   }
		SizeInfo					Measure()   const override	{ return source.Measure(); }
		IEnumerator<TConverted>*	MoveTo(void* mem) override	{ return new (mem) ConverterEnumerator { std::move(*this) }; }

		template <class Factory>
		ConverterEnumerator(Factory&& getSource) : source { getSource() }  {}
		ConverterEnumerator(ConverterEnumerator&&) = default;
	};



	template <class Source, class Mapper>
	class MapperEnumerator final : public IEnumerator<MappedT<EnumeratedT<Source>, Mapper>> {
		Source			source;
		const Mapper&	map;

	public:
		using typename MapperEnumerator::IEnumerator::TElem;

		bool				FetchNext()		  override	{ return source.FetchNext(); }
		TElem				Current()		  override	{ return map(source.Current()); }
		SizeInfo			Measure()   const override	{ return source.Measure(); }
		IEnumerator<TElem>*	MoveTo(void* mem) override	{ return new (mem) MapperEnumerator { std::move(*this) }; }

		template <class Factory>
		MapperEnumerator(Factory&& getSource, const Mapper& map) : source { getSource() }, map { map }  {}
		MapperEnumerator(MapperEnumerator&&) = default;
	};

	

	template <class Source>
	class IndexerEnumerator final : public IEnumerator<Indexed<EnumeratedT<Source>>> {
		Source	source;
		size_t	index	= SIZE_MAX;

	public:
		using typename IndexerEnumerator::IEnumerator::TElem;

		bool FetchNext() override
		{
			++index;					// defined overflow
			return source.FetchNext();
		}

		TElem				Current()		  override	{ return { index, source.Current() }; }
		SizeInfo			Measure()   const override	{ return source.Measure(); }
		IEnumerator<TElem>*	MoveTo(void* mem) override	{ return new (mem) IndexerEnumerator { std::move(*this) }; }

		template <class Factory>
		IndexerEnumerator(Factory&& getSource) : source { getSource() }  {}
		IndexerEnumerator(IndexerEnumerator&&) = default;
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

		bool	FetchNext()		override
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


		TElem	Current()		override
		{
			ENUMERABLES_ETOR_USAGE_ASSERT (nextFetched, NotFetchedError);

			return binop(*prev, source.Current());
		}


		// NOTE: This implementation is non-monotonic in conjunction with ContainerEnumerator (Exact, N - 1) -> (Bound, N)
		//		 but Measure() during enumeration is just a nice to have.
		SizeInfo			Measure()	const override	{ return source.Measure().Subtract(!prev.IsInitialized()); }
		IEnumerator<TElem>*	MoveTo(void* mem) override	{ return new (mem) CombinerEnumerator { std::move(*this) }; }

		template <class Factory>
		CombinerEnumerator(Factory&& getSource, const Combiner& binop) : source { getSource() }, binop { binop }  {}
		CombinerEnumerator(CombinerEnumerator&&) = default;
	};



	template <class Source1, class Source2, class Zipper, class Result = void>
	class ZipperEnumerator final : public IEnumerator<OverrideT<Result, CombinedT<EnumeratedT<Source1>, EnumeratedT<Source2>, Zipper>>> {
		Source1			source1;
		Source2			source2;
		const Zipper&	zip;

	public:
		using typename ZipperEnumerator::IEnumerator::TElem;

		TElem				Current()		  override	{ return zip(source1.Current(), source2.Current());  }
		bool				FetchNext()		  override	{ return source1.FetchNext() && source2.FetchNext(); }
		SizeInfo			Measure()   const override	{ return source1.Measure().Limit(source2.Measure()); }
		IEnumerator<TElem>*	MoveTo(void* mem) override	{ return new (mem) ZipperEnumerator { std::move(*this) }; }

		template <class Fact1, class Fact2>
		ZipperEnumerator(Fact1&& getSource1, Fact2&& getSource2, const Zipper& zip) :
			source1 { getSource1() },
			source2 { getSource2() },
			zip		{ zip }
		{
		}

		ZipperEnumerator(ZipperEnumerator&&) = default;
	};

	#pragma endregion



	#pragma region Concatenations
	
	template <class Source, class ContinuationSource>
	class ConcatEnumerator final : public IEnumerator<EnumeratedT<Source>> {
		static_assert (is_convertible<EnumeratedT<ContinuationSource>, EnumeratedT<Source>>::value, "Concat: Incompatible continuation.");

		Source				source;
		ContinuationSource	continuation;
		bool				sourceDepleted = false;

	public:
		using typename ConcatEnumerator::IEnumerator::TElem;

		bool	FetchNext() override
		{
			sourceDepleted = sourceDepleted || !source.FetchNext();
			return			!sourceDepleted || continuation.FetchNext();
		}

		TElem				Current()		  override	{ return sourceDepleted ? continuation.Current() : source.Current(); }
		SizeInfo			Measure()   const override	{ return source.Measure().Add(continuation.Measure()); }
		IEnumerator<TElem>*	MoveTo(void* mem) override	{ return new (mem) ConcatEnumerator { std::move(*this) }; }

		template <class SrcFact, class ContFact>
		ConcatEnumerator(SrcFact&& getSource, ContFact&& getContinuation) : source { getSource() }, continuation { getContinuation() }  {}
		ConcatEnumerator(ConcatEnumerator&&) = default;
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

		bool		FetchNext() override
		{
			// try to advance outer enumerator as needed
			while (!nestedEnumerator.IsInitialized() || !nestedEnumerator->FetchNext()) {
				if (!ebSource.FetchNext())
					return false;

				nested.AssignCurrent(ebSource);

				// possibly useful in 17: avoid move requirement (along with MoveTo(m))
				nestedEnumerator.AcceptRvo([this]() { return Deducer::GetEnumerator(*nested); });
			}

			return true;
		}

		TElem				Current()		  override	{ return nestedEnumerator->Current(); }
		SizeInfo			Measure()   const override	{ return { Boundedness::Unknown }; }
		IEnumerator<TElem>*	MoveTo(void* mem) override	{ return new (mem) FlattenerEnumerator { std::move(*this) }; }

		template <class Factory>
		FlattenerEnumerator(Factory&& getSource) : ebSource { getSource() }  {}
		FlattenerEnumerator(FlattenerEnumerator&&) = default;
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


		IEnumerator<TElem>*	MoveTo(void* mem) override
		{
			return new (mem) ReplayEnumerator { std::move(*this) };
		}


		template <class Factory>
		ReplayEnumerator(Factory&& getSource, size_t n) :
			source    { getSource() },
			counter   { n },
			headElems { SmallListOperations::Init<StorableT<TElem>, 1>(n) }
		{
		}

		ReplayEnumerator(ReplayEnumerator&&) = default;
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
					   || is_assignable<TAcc, CombinedT<TAcc, InElem, Combiner>>::value,
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
		TAcc		Current()		  override final	{ return *accumulator; }
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

		ScannerBase(ScannerBase&&) = default;
	};



	/// Scan operation with accumulator explicitly initialized.
	template <class Source, class Combiner, class TAcc>
	class ScannerEnumerator final : public ScannerBase<Source, Combiner, TAcc, Reassignable> {
	public:
		bool				FetchNext()		  override	{ return this->CombineNext(); }
		IEnumerator<TAcc>*	MoveTo(void* mem) override	{ return new (mem) ScannerEnumerator { std::move(*this) }; }

		using ScannerEnumerator::ScannerBase::ScannerBase;

		ScannerEnumerator(ScannerEnumerator&&) = default;
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


		template <class IM = InitAccMapper>
		void InitFromCurrent(enable_if_t<IsNone<IM> && is_same<InElem, TAcc>::value>* = nullptr)
		{
			this->accumulator.AssignCurrent(this->source);
		}

		template <class IM = InitAccMapper>
		void InitFromCurrent(enable_if_t<IsNone<IM> && !is_same<InElem, TAcc>::value>* = nullptr)
		{
			static_assert(IsBraceConstructible<TAcc, InElem>::value,
						  "Can't convert input element into TAcc. (Narrowing not supported.)"
						  " Please specify an Initial Accumulator Mapper."				     );

			this->accumulator = this->source.Current();
		}


		template <class IM = InitAccMapper>
		void InitFromCurrent(enable_if_t<!IsNone<IM> && is_same<MappedT<InElem, IM>, TAcc>::value>* = nullptr)
		{
			this->accumulator.AcceptRvo([this]() -> decltype(auto) {
				return initAccumulator(this->source.Current());
			});
		}

		template <class IM = InitAccMapper>
		void InitFromCurrent(enable_if_t<!IsNone<IM> && !is_same<MappedT<InElem, IM>, TAcc>::value>* = nullptr)
		{
			static_assert(IsBraceConstructible<TAcc, MappedT<InElem, InitAccMapper>>::value,
						  "Can't convert input element into TAcc. (Narrowing not supported.)");

			this->accumulator = initAccumulator(this->source.Current());
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

		IEnumerator<TAcc>*	MoveTo(void* mem) override	{ return new (mem) FetchFirstScannerEnumerator { std::move(*this) };		}

		template <class Fact>
		FetchFirstScannerEnumerator(Fact&& getSource, const Combiner& combiner, const InitAccMapper& initializer = {}) :
			Base			{ getSource, combiner },
			initAccumulator	{ initializer }
		{
		}

		FetchFirstScannerEnumerator(FetchFirstScannerEnumerator&&) = default;
	};

	#pragma endregion

#pragma endregion




#pragma region Chainable Caching Operations

	template <class Source, class Ordering>
	class SorterEnumerator final : public CachingEnumerator<EnumeratedT<Source>, ListOperations::Container> {
		Source			 source;
		const Ordering&	 ordering;

	public:
		using typename SorterEnumerator::CachingEnumerator::TCache;
		using typename SorterEnumerator::CachingEnumerator::TElem;

		TCache	CalcResults() override
		{
			TCache cache = ObtainCachedResults<ListOperations, StorableT<TElem>>(source, 0);
			std::sort(AdlBegin(cache), AdlEnd(cache), ordering);
			return cache;
		}

		SizeInfo				Measure()   const override	{ return source.Measure(); }
		IEnumerator<TElem>* 	MoveTo(void* mem) override	{ return new (mem) SorterEnumerator { std::move(*this) }; }


		template <class Factory>
		SorterEnumerator(Factory&& getSource, const Ordering& ordering) : source { getSource() }, ordering { ordering }  {}
		SorterEnumerator(SorterEnumerator&&) = default;
	};



	template <class Source, class Ordering>
	class MinSeekEnumerator final : public CachingEnumerator<EnumeratedT<Source>, ListOperations::Container> {
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


		IEnumerator<TElem>* MoveTo(void* mem) override
		{
			return new (mem) MinSeekEnumerator { std::move(*this) };
		}


		template <class Factory>
		MinSeekEnumerator(Factory&& getSource, const Ordering& ordering) : source { getSource() }, isLess { ordering }  {}
		MinSeekEnumerator(MinSeekEnumerator&&) = default;
	};

#pragma endregion




#pragma region Construction helpers

	#pragma region Container Enumerator choice

	template <class C>
	using IfContainerSizeOnly = enable_if_t< HasQueryableSize<C>::value &&
											!HasIteratorDifference<C>,
											void >;

	template <class C>
	using IfTrySizeIterators = enable_if_t< !HasQueryableSize<C>::value ||
											HasIteratorDifference<C>,
											void >;


	template <class ForcedResult = void, class C, class = IfTrySizeIterators<C&>>
	auto CreateEnumeratorFor(C& container) -> IteratorEnumerator<IteratorT<C&>, ForcedResult>
	{
		return { AdlBegin(container), AdlEnd(container) };
	}

	template <class ForcedResult = void, class C, class = IfContainerSizeOnly<C&>>
	auto CreateEnumeratorFor(C& container) -> ContainerEnumerator<C, ForcedResult>
	{
		return { container };
	}

	#pragma endregion



	#pragma region Aggregate / Scan deductions

	/// Trait to chose between ScannerEnumerator and FetchFirstScannerEnumerator.
	/// @remarks
	///		A bit exploratory feature. The overloads could also be separated by finding good names...
	///		This class makes a light enable_if decision only, further checks should commence after the choice is made.
	template <class TElem, class ValOrMapper, class ForcedAcc = void>
	class IsAccuInit {
		template <class FA = ForcedAcc> static constexpr
		enable_if_t<is_void<FA>::value || !IsConstructibleAnyway<FA, ValOrMapper>, bool>
		IsMapper()
		{
			return IsCallable<ValOrMapper, TElem>::value;
			// Shouldn't try to deduce (possibly running into Error) on the other branch!
		}

		template <class FA = ForcedAcc> static constexpr
		enable_if_t<IsConstructibleAnyway<FA, ValOrMapper>, bool>
		IsMapper()
		{
			return false;
		}

	public:
		static constexpr bool byMapping = IsMapper();
		static constexpr bool byValue   = is_void<ForcedAcc>::value ? !IsMapper() : IsConstructibleAnyway<ForcedAcc, ValOrMapper>;

		static_assert (byValue || byMapping, "Bad argument to initialize the accumulator. This algorithm requires either:\n"
					   "  A) An initial Accumulator value [convertible to ForcedAcc type if that's specified]\n"
					   "  B) An initial Mapper : TElem -> Acc to be applied to the first element before the Combiner calls.");
	};



	/// Determines accumulator type for Scan/Aggregate and provides friendly errors.
	/// Member-Select semantics are the default, use explicit ForcedAcc to force &.
	template <class TElem>
	class AccuDeducer {

		template <class A, class E, class C>
		static auto TryCombine() -> enable_if_t<IsCallable<C, A, E>::value, CombinedT<A, E, C>>;

		template <class A, class E, class C>
		static auto TryCombine() -> enable_if_t<IsCallableMember<A, C, E>::value, AppliedMemberT<A, C, E>>;

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

			static_assert (is_constructible<Acc, Next>::value || is_assignable<Acc, Next>::value,
						   "Can't convert Combiner's result into the next accumulator value."	);

			using TAcc = Acc;
		};


		template <class Combiner, class ForcedAcc>
		struct FirstInitCheck {
			using TCallResult = decltype(TryCombine<TElem, TElem, Combiner>());
			using TImplicit	  = LambdaCreators::NonExpiringMemberT<TElem, TCallResult>;

			using TProposed   = OverrideT<ForcedAcc, TImplicit>;

			static_assert (!is_void<TCallResult>::value || !is_void<ForcedAcc>::value,
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

			static_assert (!is_void<ForcedAcc>::value || !is_void<TCompatible>::value, "Can't deduce common compatible type. Speficy Accumulator type explicitly!");

			// TODO: Vague but practical check for functions choosing between const&s (e.g. std::max) to prevent referring temporaries.
			//		 For now only known problematic cases get decayed. ForcedAcc is unconstrained.
			//		 Desired: LambdaCreators could track their exact wrapped function types -> rendering possible to check if converted temporaries were needed!
			using VElem	= BasePointedT<TElem>;
			using VInit	= BasePointedT<Init>;
			using TImplicit	= conditional_t< !HasConstValue<TNoExp>					// shouldn't have temporaries with normal design
										  || IsRefCompatible<TNoExp, TElem>			// probably just ref/ptr conversion to ancestor
										  || is_same<VInit, VElem>::value,			// can be a valid subobject selection, even if no ref-comp.
											 TNoExp,
											 decay_t<TNoExp> >;
			using TProposed = OverrideT<ForcedAcc, TImplicit>;

			// quick fix to allow memberfunction-pointers
			static constexpr bool isCallable = IsCallable<Combiner, TProposed, TElem>::value
											|| IsCallableMember<TProposed, Combiner, TElem>::value;

			static_assert ( is_void<ForcedAcc>::value || is_constructible<ForcedAcc, Init>::value, "SFINAE error.");
			static_assert ( is_void<ForcedAcc>::value || isCallable,	"Combiner function not callable with the specified accumulator.");
			static_assert (!is_void<ForcedAcc>::value || isCallable,	"Combiner function not callable with the deduced accumulator "
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


}		// namespace Def
}		// namespace Enumerables

#endif	// ENUMERABLES_ENUMERATORS_HPP
