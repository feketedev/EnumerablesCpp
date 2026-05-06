#ifndef ENUMERABLES_IMPLEMENTATION_HPP
#define ENUMERABLES_IMPLEMENTATION_HPP

	/*  Part of Enumerables for C++.										 *
	 *																		 *
	 *  This file contains hidden code directly related to interface files.  *
	 *																		 *
	 *  This is the ONE to include to instantiate the library -				 *
	 *  probably in your own configuration file (like Enumerables.hpp)		 */



// Could be conditionally defined altogether, but they are used throughout this
// library to be more concise - hopefully push_macro is supported widely enough
// to hide them just outside.
#ifdef ENUMERABLES_NO_FUN
#	pragma push_macro("FUN")
#	pragma push_macro("FUN1")
#	pragma push_macro("FUN2")
#	pragma push_macro("FUN3")
#	pragma push_macro("FUNV")
#	pragma push_macro("FUNV1")
#	pragma push_macro("FUNV2")
#	pragma push_macro("FUNV3")
#endif

#include "Enumerables_Interface.hpp"


#define ENUMERABLES_STRINGIFY_EVALD(x)		#x
#define ENUMERABLES_STRINGIFY(x)			ENUMERABLES_STRINGIFY_EVALD(x)



namespace Enumerables {


#pragma region InterfaceTypes

	inline bool			SizeInfo::ProovesDifferent(const SizeInfo& other) const
	{
		bool capped = kind == Boundedness::KnownBound;
		bool otherCapped = other.kind == Boundedness::KnownBound;

		return IsExact() && other.IsExact() && value != other.value
			|| IsExact() && otherCapped && value > other.value
			|| capped && other.IsExact() && value < other.value
			|| IsUnbounded() != other.IsUnbounded();
	}


	inline SizeInfo		SizeInfo::Add(const SizeInfo& other) const
	{
		Boundedness bnd = (kind == other.kind) ? kind
			: (HasValue() && other.HasValue()) ? Boundedness::KnownBound
			: (IsBounded() && other.IsBounded()) ? Boundedness::Bounded
			: (IsUnbounded() || other.IsUnbounded()) ? Boundedness::Unbounded
			: Boundedness::Unknown;

		return { bnd, value + other.value };
	}


	inline SizeInfo		SizeInfo::Limit(size_t max) const
	{
		return   HasValue() ? SizeInfo { kind, std::min(value, max) }
			: IsUnbounded() ? SizeInfo { Boundedness::Exact, max }
			: SizeInfo { Boundedness::KnownBound, max };
	}


	inline SizeInfo		SizeInfo::Limit(const SizeInfo& other) const
	{
		if (IsExact())
			return other.Limit(value);
		if (other.IsExact())
			return Limit(other.value);

		if (HasValue())
			return *this;
		if (other.HasValue())
			return other;

		return (IsBounded() || other.IsBounded()) ? Boundedness::Bounded
			: (IsUnbounded() && other.IsUnbounded()) ? Boundedness::Unbounded
			: Boundedness::Unknown;
	}


	inline SizeInfo		SizeInfo::Subtract(size_t elems) const
	{
		size_t subTill0 = (value >= elems) ? value - elems : 0;
		return { kind, subTill0 };
	}


	inline SizeInfo		SizeInfo::Filtered(bool terminable) const
	{
		Boundedness bnd = HasValue() ? Boundedness::KnownBound
			: (IsUnbounded() && terminable) ? Boundedness::Unknown
			: kind;

		return { bnd, value };
	}

#pragma endregion



	namespace TypeHelpers {
	namespace InitListSupport {

		template <class Cont, class TForced = void>
		struct WeakEnumerableElem {
			using type = EnumerableT<decltype(Enumerate<TForced>(declval<Cont>()))>;
		};
		template <class I>
		struct WeakEnumerableElem<std::initializer_list<I*>, void> {
			using type = MaybeToDereference<I*>;
		};
		template <class I, class F>
		struct WeakEnumerableElem<std::initializer_list<I*>, F> {
			using type = F;

			static_assert (is_convertible<I*, F>::value || is_convertible<I&, F>::value,
						   "No possible way to yield the requested type. (Internal error?)");
		};


		/// Given any iterable type, deduces the possible TElem of the result of Enumerate(...).
		/// For init-lists that carry ambiguous meaning, result is marked as MaybeToDereference.
		/// @remarks
		///		This workaround is for Concat(...) and similar, where the intention of some init-list of pointers
		///		can be clarified by some other relevant sequence (based on it containing & or * elements).
		template <class Cont, class TForced = void>
		using WeakElemT = typename WeakEnumerableElem<Cont, TForced>::type;

	}	// namespace InitListSupport
	}	// namespace TypeHelpers

}	// namespace Enumerables




namespace Enumerables {
namespace Def {


#pragma region Free helpers

	/// Two enumerables (or contianers) have equal values as per operator ==.
	template <class A, class B>
	bool AreEqual(A&& lhs, B&& rhs)
	{
		// support comparison to containers too
		auto ebLeft  = Enumerate(lhs);
		auto ebRight = Enumerate(rhs);

		auto left  = ebLeft.GetEnumerator();
		auto right = ebRight.GetEnumerator();

		if (left.Measure().ProovesDifferent(right.Measure()))
			return false;

		bool moreLeft, moreRight;
		do {
			moreLeft  = left.FetchNext();
			moreRight = right.FetchNext();
		}
		while (moreLeft && moreRight && left.Current() == right.Current());

		return !moreLeft && !moreRight;
	}

	template <class I, class B>
	bool AreEqual(initializer_list<I>&& lhs, B&& rhs)
	{
		return AreEqual(lhs, rhs);
	}

	template <class I, class A>
	bool AreEqual(A&& lhs, initializer_list<I>&& rhs)
	{
		return AreEqual(lhs, rhs);
	}


	/// Bypasses tedious braced-init overloads of Enumerate when both the
	/// explicit target type and a deduced init-list is readily given.
	/// @remarks
	///		Using Enumerate directly in such situation can cause compilation error
	///		if ENUMERABLES_ADD_STRINGLIST_OVERLOADS is enabled and the specified
	///		target elem doesn't correspont to the init-list item (including qualifiers).
	///
	///		Init-list deduction occurs directly on top-level anyway, thus it
	///		is reasonable to bypass such Enumerate overloads when forwarding.
	template <class TForced = void, class C>
	auto EnumerateForwarded(C&& container)
	{
		return Enumerate<TForced>(forward<C>(container));
	}
	template <class TForced, class I, enable_if_t<!is_void<TForced>::value, int> = 0>
	auto EnumerateForwarded(initializer_list<I>&& initToSave)
	{
		return InitEnumerable<TForced>(move(initToSave), None {});
		// TODO: Handle allocator param when there's a supporting caller.
	}


	template <class TForced, class TCommon0, class C1, class C2>
	auto ConcatInternal(C1&& cont1, C2&& cont2)
	{
		using E1 = InitListSupport::WeakElemT<C1, TForced>;
		using E2 = InitListSupport::WeakElemT<C2, TForced>;

		using THead		  = OverrideT<TCommon0, E1>;
		using TCommon1	  = typename ConcatTypeDeducer<THead, E1>::TCommon;
		using TCommonWeak = typename ConcatTypeDeducer<TCommon1, E2>::TCommon;
		using TCommon	  = InitListSupport::ResolveContextEndedT<TCommonWeak>;

		return EnumerateForwarded<TCommon>(forward<C1>(cont1))
				.Concat(EnumerateForwarded<TCommon>(forward<C2>(cont2)));
	}


	// CONSIDER: Forbids less then 2 parameters - Should it be even supported for generic programming?
	template <class TForced, class TCommon0, class C1, class C2, class... CMore>
	auto ConcatInternal(C1&& cont1, C2&& cont2, CMore&&... tailConts)
	{
		using E1 = InitListSupport::WeakElemT<C1, TForced>;

		using THead	   = OverrideT<TCommon0, E1>;
		using TCommon1 = typename ConcatTypeDeducer<THead, E1>::TCommon;

		// must look ahead too for final consensus!
		using EbTail  = decltype(ConcatInternal<TForced, TCommon1>(forward<C2>(cont2), forward<CMore>(tailConts)...));
		using TCommon = typename EbTail::TElem;

		return EnumerateForwarded<TCommon>(forward<C1>(cont1))
				.Concat(ConcatInternal<TCommon>(forward<C2>(cont2), forward<CMore>(tailConts)...));
	}


	// Build a Dictionary via 2 mapper functions, use Cache if available.
	// (Core idea follows ObtainCachedResults.)
	template <class K, class V, class Source, class KeyMapper, class ValMapper, class... Options>
	enable_if_t<HasConvertibleCache<Source, void, V>::byElement,
				DictionaryType<K, V, Options...>>
	BuildDictObtainCache(Source& etor, size_t /*hint*/, const KeyMapper& toKey, const ValMapper& toValue, const Options&... opts)
	{
		auto cache = etor.CalcResults();

		auto res = DictOperations::Init<DictionaryType<K, V, Options...>>(GetSize(cache), opts...);
		for (auto& elem : cache) {
			K key = toKey(Revive(elem));
			DictOperations::Add(res, move(key), toValue(PassRevived(elem)));
		}
		return res;
	}

	template <class K, class V, class Source, class KeyMapper, class ValMapper, class... Options>
	enable_if_t<!HasConvertibleCache<Source, void, V>::byElement,
				DictionaryType<K, V, Options...>>
	BuildDictObtainCache(Source& etor, size_t hint, const KeyMapper& toKey, const ValMapper& toValue, const Options&... opts)
	{
		SizeInfo si  = etor.Measure();
		size_t   cap = (si.IsExact() && hint < si) ? si.value : hint;

		auto res = DictOperations::Init<DictionaryType<K, V, Options...>>(cap, opts...);
		while (etor.FetchNext()) {
			auto&& elem = etor.Current();
			K      key  = toKey(elem);
			DictOperations::Add(res, move(key), toValue(forward<decltype(elem)>(elem)));
		}
		return res;
	}


#if ENUMERABLES_EMPLOY_DYNAMICCAST

	template <class K, class V, class KeyMapper, class ValMapper, class T, class... Options>
	DictionaryType<K, V, Options...>   BuildDictObtainCache(InterfacedEnumerator<T>& etor, size_t hint,
															const KeyMapper& toKey, const ValMapper& toValue,
															const Options&...							opts)
	{
		// Only List-Cachings exist so far -- see ObtainCachedResults notes
		using AimedCache = ListOperations::Container<StorableT<T>>;
		using ET		 = CachingEnumerator<AimedCache>;

		ET* caching = etor.template TryCast<ET>();
		if (caching == nullptr)
			return BuildDictObtainCache<K, V>(etor.WrappedInterface(), hint, toKey, toValue, opts...);
		else
			return BuildDictObtainCache<K, V>(*caching, hint, toKey, toValue, opts...);
	}

#endif



	template <class T, class... Os>
	SetType<RefHolder<T>> InitRefholderSet(const initializer_list<T*>& elems, const Os&... opts)
	{
		static_assert (!is_scalar<T>::value,
					   "Capturing set elements by reference is only meant to save copies - no sense for scalars. "
					   "Since the set is formed eagerly in this case, referred elements can't mutate until query!");

		auto set = SetOperations::Init<SetType<RefHolder<T>>>(elems.size(), opts...);
		for (auto* e : elems)
			SetOperations::Add(set, *e);
		return set;
	}

#pragma endregion




#pragma region AutoEnumerable Terminal functions

	// This fancy emplacing construction mostly makes sense if
	// the result itself is in a context eligible for RVO/NRVO.
	// -> C++17 unmovables; otherwise just 1 less move
	template <class T, class Enumerator>
	Optional<T> CurrentAsOptional(Enumerator& et)
	{
		return OptionalOperations::FromCurrent<T>(et);
	}


	template <class T>
	Optional<T> NoValue(StopReason code)
	{
		return OptionalOperations::NoValue<T>(code);
	}



	template <class TFactory>
	auto AutoEnumerable<TFactory>::First() const -> TElem
	{
		auto et = GetEnumerator();

		if (!et.FetchNext()) {
			ENUMERABLES_CLIENT_BREAK (EmptyError);
			throw LogicException(EmptyError);
		}
		return et.Current();
	}


	template <class TFactory>
	auto AutoEnumerable<TFactory>::Single() const -> TElem
	{
		auto et = GetEnumerator();

		if (!et.FetchNext()) {
			ENUMERABLES_CLIENT_BREAK (EmptyError);
			throw LogicException(EmptyError);
		}

		// NOTE: && is not in focus currently, result should not become dangling at end!
		TElem result = et.Current();

		if (et.FetchNext()) {
			ENUMERABLES_CLIENT_BREAK (AmbiguityError);
			throw LogicException(AmbiguityError);
		}

		return result;
	}


	template <class TFactory>
	auto AutoEnumerable<TFactory>::FirstIfAny() const -> Optional<TElem>
	{
		auto et = GetEnumerator();
		if (et.FetchNext())
			return CurrentAsOptional<TElem>(et);
		else
			return NoValue<TElem>(StopReason::Empty);
	}


	// enables RVO for SingleIfAny
	template <class Et>
	auto SingleCurrent(Et& etor)
	{
		auto result = CurrentAsOptional<EnumeratedT<Et>>(etor);
		if (!etor.FetchNext())
			return result;

		ENUMERABLES_CLIENT_BREAK (AmbiguityError);
		throw LogicException(AmbiguityError);
	}


	template <class TFactory>
	auto AutoEnumerable<TFactory>::SingleIfAny() const -> Optional<TElem>
	{
		auto et = GetEnumerator();

		if (et.FetchNext())
			return SingleCurrent(et);
		else
			return NoValue<TElem>(StopReason::Empty);
	}


	// CONSIDER: No way for RVO (thus C++17 unmovables) here unless OptResult becomes clearable
	template <class TFactory>
	auto AutoEnumerable<TFactory>::SingleOrNone() const -> Optional<TElem>
	{
		auto et = GetEnumerator();

		if (!et.FetchNext())
			return NoValue<TElem>(StopReason::Empty);

		auto result = CurrentAsOptional<TElem>(et);
		if (!et.FetchNext())
			return result;

		return NoValue<TElem>(StopReason::Ambiguous);
	}


	template <class TFactory>
	auto AutoEnumerable<TFactory>::Last() const -> TElem
	{
		auto et = GetEnumerator();

		if (!et.FetchNext()) {
			ENUMERABLES_CLIENT_BREAK (EmptyError);
			throw LogicException(EmptyError);
		}

		while (true) {
			TElem last = et.Current();
			if (!et.FetchNext())
				return last;
		}
	}


	template <class TFactory>
	auto AutoEnumerable<TFactory>::LastIfAny() const -> Optional<TElem>
	{
		auto et = GetEnumerator();

		if (!et.FetchNext())
			return NoValue<TElem>(StopReason::Empty);

		while (true) {
			auto last = CurrentAsOptional<TElem>(et);
			if (!et.FetchNext())
				return last;
		}
	}


	template <class TFactory>
	size_t AutoEnumerable<TFactory>::Count() const
	{
		size_t count = 0;
		auto et = GetEnumerator();

		SizeInfo si = et.Measure();
		if (si.IsExact())
			return si;

		while (et.FetchNext())
			count++;

		return count;
	}


	template <class TFactory>
	bool AutoEnumerable<TFactory>::AllEqual() const
	{
		auto et = GetEnumerator();
		if (!et.FetchNext())
			return true;

		TElem first = et.Current();

		while (et.FetchNext()) {
			if (!(first == et.Current()))
				return false;
		}
		return true;
	}


	// https://en.wikipedia.org/wiki/2Sum
	//template <class S>
	//void  Sum2(S& sum, const S& n, S& error)
	//{
	//	const S s0 = sum;
	//	sum += n;
	//	const S diff1 = s0 - (sum - n);
	//	const S diff2 = n - (sum - s0);
	//	error += diff1 + diff2;
	//}


	// https://en.wikipedia.org/wiki/Kahan_summation_algorithm
	// Simple Kahan isn't appropriate since we have no assumptions about magnitudes!
	// Neurmaier seems to perform slightly better on a current CPU, while slightly worse on an old one.
	// Neither gives perfect results - Neumaier is the current choice.
	template <class S>
	void   NeumaierSum2(S& sum, const S& b, S& error)
	{
		const S s0 = sum;
		sum += b;

		// for growing sums, first branch should be continuously predicted with success
		if (abs(s0) >= abs(b)) {
			const S diffB = b - (sum - s0);
			error += diffB;
		}
		else {
			const S diffA = s0 - (sum - b);
			error += diffA;
		}
	}


	template <class S, class Et, enable_if_t<std::is_floating_point<S>::value, int> = 0>
	S SumEnumerated(Et& etor)
	{
		S		sum {};
		S		err {};
		while (etor.FetchNext())
			NeumaierSum2(sum, etor.Current(), err);

		return sum + err;
	}


	template <class S, class Et,
			  enable_if_t<!std::is_floating_point<S>::value && IsAddAssignable<S>::value, int> = 0>
	S SumEnumerated(Et& etor)
	{
		S sum {};
		while (etor.FetchNext())
			sum += etor.Current();

		return sum;
	}


	// fallback method of summation for immutable types
	template <class S, class Et, enable_if_t<!IsAddAssignable<S>::value, int> = 0>
	S SumEnumerated(Et& etor)
	{
		Reassignable<S> sum = S {};
		while (etor.FetchNext())
			sum = *sum + etor.Current();

		return sum.PassValue();

		// CONSIDER: has no RVO. Tail-recursion maybe? - Then also for anything else consistently?
	}


	template <class TFactory>
	template <class S>
	S	AutoEnumerable<TFactory>::Sum() const
	{
		auto et = GetEnumerator();
		return SumEnumerated<S>(et);
	}


	// uses compensated sum - not sure if Avg needs it though
	template <class TFactory>
	template <class S>
	Optional<S>   AutoEnumerable<TFactory>::Avg() const
	{
		static_assert (std::is_floating_point<S>::value, "Intended for floating-point operations.");

		auto	enumerator = GetEnumerator();
		size_t	count = 0;
		S		sum {};
		S		err {};
		while (enumerator.FetchNext()) {
			++count;
			NeumaierSum2(sum, static_cast<S>(enumerator.Current()), err);
		}
		if (count) {
			return sum / static_cast<S>(count)
				 + err / static_cast<S>(count);
		}
		return NoValue<S>(StopReason::Empty);
	}



	// Formerly: ToReferenced().Minimums().First(), but let's be more lightweigth.
	template<class TFactory>
	template<class Comp>
	auto AutoEnumerable<TFactory>::Min(const Comp& isLess) const -> Optional<TElemDecayed>
	{
		const auto& isLessLambda = BinPred(isLess);

		auto et = GetEnumerator();
		if (!et.FetchNext())
			return NoValue<TElemDecayed>(StopReason::Empty);

		Reassignable<TElem> min = et.Current();

		while (et.FetchNext()) {
			TElem curr = et.Current();
			if (isLessLambda(curr, *min))
				min.AssignHeadMoved(curr);
		}
		return min.PassValue();
	}


	template<class TFactory>
	template<class Comp>
	auto AutoEnumerable<TFactory>::Max(const Comp& isLess) const -> Optional<TElemDecayed>
	{
		auto isLessRef = RefLambda(isLess);

		return Min(SwappedBinop(BinPred(isLessRef)));
	}



	template <class TFactory>
	bool AutoEnumerable<TFactory>::Any() const
	{
		auto et = GetEnumerator();

		// prevent running complex algorithms - such as OrderBy
		SizeInfo si = et.Measure();
		if (si.IsUnbounded() || si.IsExact())
			return si.IsUnbounded() || si.value > 0;

		return et.FetchNext();
	}


	template <class TFactory>
	template <class Pred>
	bool AutoEnumerable<TFactory>::All(const Pred& pred) const
	{
		auto et = GetEnumerator();
		while (et.FetchNext()) {
			if (!pred(et.Current()))
				return false;
		}
		return true;
	}


	template <class TFactory>
	template <class... Options>
	auto AutoEnumerable<TFactory>::ToList(size_t sizeHint) const -> ListType<TElemDecayed, Options...>
	{
		TEnumerator etor = GetEnumeratorNoDebug();
		return ObtainCachedResults<ListOperations, TElemDecayed>(etor, sizeHint, Options {}...);
	}

	template <class TFactory>
	template <class... Options>
	auto AutoEnumerable<TFactory>::ToList(size_t sizeHint, const Options&... opts) const -> ListType<TElemDecayed, Options...>
	{
		TEnumerator etor = GetEnumeratorNoDebug();
		return ObtainCachedResults<ListOperations, TElemDecayed>(etor, sizeHint, opts...);
	}


	template <class TFactory>
	template <size_t N, class... Options>
	ENUMERABLES_WARN_FOR_SMALLLIST
	auto AutoEnumerable<TFactory>::ToList(size_t sizeHint) const -> SmallListType<TElemDecayed, N, Options...>
	{
		TEnumerator etor = GetEnumeratorNoDebug();
		return ObtainCachedResults<SmallListOperations, TElemDecayed, N>(etor, sizeHint, Options {}...);
	}

	template <class TFactory>
	template <size_t N, class... Options>
	ENUMERABLES_WARN_FOR_SMALLLIST
	auto AutoEnumerable<TFactory>::ToList(size_t sizeHint, const Options&... opts) const -> SmallListType<TElemDecayed, N, Options...>
	{
		TEnumerator etor = GetEnumeratorNoDebug();
		return ObtainCachedResults<SmallListOperations, TElemDecayed, N>(etor, sizeHint, opts...);
	}


	template <class TFactory>
	template <class... Options>
	auto AutoEnumerable<TFactory>::ToSet(size_t sizeHint) const -> SetType<TElemDecayed, Options...>
	{
		TEnumerator etor = GetEnumeratorNoDebug();
		return ObtainCachedResults<SetOperations, TElemDecayed>(etor, sizeHint, Options {}...);
	}

	template <class TFactory>
	template <class... Options>
	auto AutoEnumerable<TFactory>::ToSet(size_t sizeHint, const Options&... opts) const -> SetType<TElemDecayed, Options...>
	{
		TEnumerator etor = GetEnumeratorNoDebug();
		return ObtainCachedResults<SetOperations, TElemDecayed>(etor, sizeHint, opts...);
	}


	template<class TFactory>
	template<class... Options, class TKeyMapper>
	auto AutoEnumerable<TFactory>::ToDictionary(const TKeyMapper& toKey, size_t hint) const
		-> DictionaryType<DecayedResultLV<TKeyMapper>, TElemDecayed, Options...>
	{
		// copy-pasted "stateful-options" overload to avoid recursion without more enable_if!
		using Key = DecayedResultLV<TKeyMapper>;

		Forwarder<TElem> fwdValue;

		TEnumerator etor = GetEnumeratorNoDebug();
		return BuildDictObtainCache<Key, TElemDecayed>(etor, hint, KeyMapper(toKey), fwdValue, Options {}...);
	}

	template<class TFactory>
	template<class... Options, class TKeyMapper>
	auto AutoEnumerable<TFactory>::ToDictionary(const TKeyMapper& toKey, size_t hint, const Options&... opts) const
		-> DictionaryType<DecayedResultLV<TKeyMapper>, TElemDecayed, Options...>
	{
		using Key = DecayedResultLV<TKeyMapper>;

		Forwarder<TElem> fwdValue;

		TEnumerator etor = GetEnumeratorNoDebug();
		return BuildDictObtainCache<Key, TElemDecayed>(etor, hint, KeyMapper(toKey), fwdValue, opts...);
	}


	template<class TFactory>
	template<class K, class... Options>
	auto AutoEnumerable<TFactory>::ToDictionaryOf(LVOverloadTo<K> getKey, size_t hint) const
		-> DictionaryType<decay_t<K>, TElemDecayed, Options...>
	{
		// copy-pasted "stateful-options" overload to avoid recursion without more enable_if!
		Forwarder<TElem> fwdValue;

		TEnumerator etor = GetEnumeratorNoDebug();
		return BuildDictObtainCache<decay_t<K>, TElemDecayed>(etor, hint, getKey, fwdValue, Options {}...);
	}

	template<class TFactory>
	template<class K, class... Options>
	auto AutoEnumerable<TFactory>::ToDictionaryOf(LVOverloadTo<K> getKey, size_t hint, const Options&... opts) const
		-> DictionaryType<decay_t<K>, TElemDecayed, Options...>
	{
		Forwarder<TElem> fwdValue;

		TEnumerator etor = GetEnumeratorNoDebug();
		return BuildDictObtainCache<decay_t<K>, TElemDecayed>(etor, hint, getKey, fwdValue, opts...);
	}


	template<class TFactory>
	template<class... Options, class TKeyMapper, class TValueMapper, enable_if_t<!is_convertible<TValueMapper, size_t>::value, int>>
	auto AutoEnumerable<TFactory>::ToDictionary(const TKeyMapper& toKey, const TValueMapper& toValue, size_t hint) const
		-> DictionaryType<DecayedResultLV<TKeyMapper>,
						  DecayedResult<TValueMapper>,
						  Options...>
	{
		// copy-pasted "stateful-options" overload to avoid recursion without more enable_if!
		using Key   = DecayedResultLV<TKeyMapper>;
		using Value = DecayedResult<TValueMapper>;

		TEnumerator etor = GetEnumeratorNoDebug();
		return BuildDictObtainCache<Key, Value>(etor, hint, KeyMapper(toKey), ValueMapper(toValue), Options {}...);
	}

	template<class TFactory>
	template<class... Options, class TKeyMapper, class TValueMapper, enable_if_t<!is_convertible<TValueMapper, size_t>::value, int>>
	auto AutoEnumerable<TFactory>::ToDictionary(const TKeyMapper& toKey, const TValueMapper& toValue, size_t hint, const Options&... opts) const
		-> DictionaryType<DecayedResultLV<TKeyMapper>,
						  DecayedResult<TValueMapper>,
						  Options...>
	{
		using Key   = DecayedResultLV<TKeyMapper>;
		using Value = DecayedResult<TValueMapper>;

		TEnumerator etor = GetEnumeratorNoDebug();
		return BuildDictObtainCache<Key, Value>(etor, hint, KeyMapper(toKey), ValueMapper(toValue), opts...);
	}


	template<class TFactory>
	template<class K, class V, class... Options>
	auto AutoEnumerable<TFactory>::ToDictionaryOf(LVOverloadTo<K> getKey, OverloadTo<V> getValue, size_t hint) const
		-> DictionaryType<decay_t<K>, decay_t<V>, Options...>
	{
		TEnumerator etor = GetEnumeratorNoDebug();
		return BuildDictObtainCache<decay_t<K>, decay_t<V>>(etor, hint, getKey, getValue, Options {}...);
	}

	template<class TFactory>
	template<class K, class V, class... Options>
	auto AutoEnumerable<TFactory>::ToDictionaryOf(LVOverloadTo<K> getKey, OverloadTo<V> getValue, size_t hint, const Options&... opts) const
		-> DictionaryType<decay_t<K>, decay_t<V>, Options...>
	{
		TEnumerator etor = GetEnumeratorNoDebug();
		return BuildDictObtainCache<decay_t<K>, decay_t<V>>(etor, hint, getKey, getValue, opts...);
	}


	template <class TFactory>
	template <class V>
	auto AutoEnumerable<TFactory>::ToMaterialized() const
	{
		static_assert (!is_reference<V>::value || is_const<remove_reference_t<V>>::value,
					   "A materialized enumeration must be const - no access by nonconst ref.");
		static_assert (is_convertible<TElem, V>::value, "Incompatible output specified.");

		return Enumerate<V>(this->ToList());
	}


	template <class TFactory>
	auto AutoEnumerable<TFactory>::ToSnapshot() const
	{
		// actually this works for prvalues too, but is wasteful!
		static_assert (is_reference<TElem>::value, "For by-value enumerations use ToMaterialized instead!");

		using S = StorableT<TElem>;

		TEnumerator etor = GetEnumeratorNoDebug();
		auto		list = ObtainCachedResults<ListOperations, S>(etor, 0u);

		return Enumerate<const S&>(move(list))
				 .Map([](const S& sv) -> TElem { return Revive(sv); });
	}

#pragma endregion




#pragma region AutoEnumerable Convenience Overloads

	template<class TFactory>
	template<class ...Os>
	auto AutoEnumerable<TFactory>::Except(initializer_list<DeepConstT<TElemDecayed*>>&& elems, const Os & ...setOptions) const &
	{
		return Where([set = InitRefholderSet(elems, setOptions...)](TElemConstParam x) {
			return !SetOperations::Contains<RefHolder<const DeepConstT<TElemDecayed>>>(set, x);
		});
	}

	template<class TFactory>
	template<class ...Os>
	auto AutoEnumerable<TFactory>::Except(initializer_list<DeepConstT<TElemDecayed*>>&& elems, const Os & ...setOptions) &&
	{
		return Move().Where([set = InitRefholderSet(elems, setOptions...)](TElemConstParam x) {
			return !SetOperations::Contains<RefHolder<const DeepConstT<TElemDecayed>>>(set, x);
		});
	}

	template<class TFactory>
	template<class ...Os>
	auto AutoEnumerable<TFactory>::Intersect(initializer_list<DeepConstT<TElemDecayed*>>&& elems, const Os & ...setOptions) const &
	{
		return Where([set = InitRefholderSet(elems, setOptions...)](TElemConstParam x) {
			return SetOperations::Contains<RefHolder<const DeepConstT<TElemDecayed>>>(set, x);
		});
	}

	template<class TFactory>
	template<class ...Os>
	auto AutoEnumerable<TFactory>::Intersect(initializer_list<DeepConstT<TElemDecayed*>>&& elems, const Os & ...setOptions) &&
	{
		return Move().Where([set = InitRefholderSet(elems, setOptions...)](TElemConstParam x) {
			return SetOperations::Contains<RefHolder<const DeepConstT<TElemDecayed>>>(set, x);
		});
	}


	template<class TFactory>
	template<class... SetOptions>
	auto AutoEnumerable<TFactory>::Except(initializer_list<DeepConstT<TElemDecayed*>>&& elems) const &
	{
		return Where([set = InitRefholderSet(elems, SetOptions {}...)](TElemConstParam x) {
			return !SetOperations::Contains<RefHolder<const DeepConstT<TElemDecayed>>>(set, x);
		});
	}

	template<class TFactory>
	template<class... SetOptions>
	auto AutoEnumerable<TFactory>::Except(initializer_list<DeepConstT<TElemDecayed*>>&& elems) &&
	{
 		return Move().Where([set = InitRefholderSet(elems, SetOptions {}...)](TElemConstParam x) {
				return !SetOperations::Contains<RefHolder<const DeepConstT<TElemDecayed>>>(set, x);
		});
	}

	template<class TFactory>
	template<class... SetOptions>
	auto AutoEnumerable<TFactory>::Intersect(initializer_list<DeepConstT<TElemDecayed*>>&& elems) const &
	{
 		return Where([set = InitRefholderSet(elems, SetOptions {}...)](TElemConstParam x) {
				return SetOperations::Contains<RefHolder<const DeepConstT<TElemDecayed>>>(set, x);
		});
	}

	template<class TFactory>
	template<class... SetOptions>
	auto AutoEnumerable<TFactory>::Intersect(initializer_list<DeepConstT<TElemDecayed*>>&& elems) &&
	{
 		return Move().Where([set = InitRefholderSet(elems, SetOptions {}...)](TElemConstParam x) {
				return SetOperations::Contains<RefHolder<const DeepConstT<TElemDecayed>>>(set, x);
		});
	}

#pragma endregion




#pragma region AutoEnumerable internals

	template <class TFactory>
	template <template <class...> class NextEnumerator, class... TypeArgs, class... Args>
	auto AutoEnumerable<TFactory>::Chain(Args&& ...args) const
	{
		ViewTrigger();
		auto nextFactory = ChainFactory<NextEnumerator, TypeArgs...>(factory, forward<Args>(args)...);

		return AutoEnumerable<decltype(nextFactory)> { move(nextFactory), isPure };
	}

	template <class TFactory>
	template <template <class...> class NextEnumerator, class... TypeArgs, class... Args>
	auto AutoEnumerable<TFactory>::MvChain(Args&& ...args)
	{
		auto nextFactory = ChainFactory<NextEnumerator, TypeArgs...>(move(factory), forward<Args>(args)...);

		return AutoEnumerable<decltype(nextFactory)> { move(nextFactory), isPure };
	}


	template <class TFactory>
	template <class Enumerable2, class TForced, template <class...> class NextEnumerator, class... TypeArgs, class... Args>
	auto AutoEnumerable<TFactory>::ChainJoined(Enumerable2& second, Args&& ...args) const
	{
		ViewTrigger();

		// Here Enumerate() /upd.: EnumerateForwarded/ serves:
		//	* as a way to uniformize calls where "second" is a direct Container (either & or &&)
		//	* as an appropriate closure in those cases (Container&& -> copy; Container& -> reference)
		//    Note that ChainedFactory itself does not store references currently.
		auto wrapped2    = EnumerateForwarded<TForced>(forward<Enumerable2>(second));
		auto factoryRefs = std::forward_as_tuple(factory, move(wrapped2.factory));
		auto nextFactory = JoinFactories<NextEnumerator, TypeArgs...>(factoryRefs, forward<Args>(args)...);

		return AutoEnumerable<decltype(nextFactory)> { move(nextFactory), isPure };
	}

	template <class TFactory>
	template <class Enumerable2, class TForced, template <class...> class NextEnumerator, class... TypeArgs, class... Args>
	auto AutoEnumerable<TFactory>::MvChainJoined(Enumerable2& second, Args&& ...args)
	{
		auto wrapped2    = EnumerateForwarded<TForced>(forward<Enumerable2>(second));
		auto factoryRefs = std::forward_as_tuple(move(factory), move(wrapped2.factory));
		auto nextFactory = JoinFactories<NextEnumerator, TypeArgs...>(factoryRefs, forward<Args>(args)...);

		return AutoEnumerable<decltype(nextFactory)> { move(nextFactory), isPure };
	}


	template<class TFactory>
	void AutoEnumerable<TFactory>::ViewTrigger() const
	{
#	if ENUMERABLES_USE_RESULTSVIEW && (ENUMERABLES_RESULTSVIEW_AUTO_EVAL & 1)
			ResultsView.Fill(factory, isPure, true);
#	endif
	}

#pragma endregion




#if ENUMERABLES_USE_RESULTSVIEW

	template <class T>
	template <class V, class Factory>
	void ResultBuffer<T>::Fill(Factory& getEnumerator, bool isPure, bool autoCall, enable_if_t<is_copy_constructible<V>::value>*)
	{
#	if !(ENUMERABLES_RESULTSVIEW_AUTO_EVAL & 4)
		if (autoCall && (GetSize(Elements) > 0 || Status[0] == 'E'))
			return;
#	endif

		if (!isPure) {
			Status = "Not available - enumeration is marked as having side-effects.";
			if (!autoCall)
				ENUMERABLES_CLIENT_BREAK ("Disabled for having side-effects - cannot build debug buffer.");
			return;
		}

		auto     et = getEnumerator();
		SizeInfo si = et.Measure();
		if (autoCall && si.IsUnbounded()) {
			Status = "Not evaluated by default - the sequence is Unbounded. "
					 "Such enumerations often rely on chained steps to terminate."
#				if ENUMERABLES_RESULTSVIEW_MANU_EVAL
					 "  Attempt Test() or Print() from Immediate window if safe to examine."
#				endif
					 ;
			return;
		}

		SizeInfo display = si.Limit(ENUMERABLES_RESULTSVIEW_MAX_ELEMS);
		size_t   cap     = display.IsExact() ? display.value : 0u;
		Elements = SmallListOperations::template Init<decltype(Elements)>(cap);

		size_t count = 0;
		while (et.FetchNext() && count < ENUMERABLES_RESULTSVIEW_MAX_ELEMS) {
			SmallListOperations::Add(Elements, et.Current());
			++count;
		}
		Status = count < ENUMERABLES_RESULTSVIEW_MAX_ELEMS
			? "Evaluation successful."
			: "Showing first " ENUMERABLES_STRINGIFY(ENUMERABLES_RESULTSVIEW_MAX_ELEMS) " elements.";
	}

	template <class T>
	template <class V, class Factory>
	void ResultBuffer<T>::Fill(Factory&, bool isPure, bool autoCall, enable_if_t<!is_copy_constructible<V>::value>*)
	{
		ENUMERABLES_INTERNAL_ASSERT (!isPure);
		Status = "Not available for this type.";
		if (!autoCall)
			ENUMERABLES_CLIENT_BREAK ("Unsupported type - cannot build debug buffer.");
	}


#	if ENUMERABLES_RESULTSVIEW_MANU_EVAL

	/// Fill the debug buffer with yielded values if possible. For immediate window.
	template<class TFactory>
	ENUMERABLES_NOINLINE
	const char* AutoEnumerable<TFactory>::Test() const
	{
		ResultsView.Fill(factory, isPure, false);
		return ResultsView.Status;
	}

	/// Print yielded values if possible. For immediate window.
	template<class TFactory>
	ENUMERABLES_NOINLINE
	auto AutoEnumerable<TFactory>::Print() const -> decltype(ResultsView.Elements)&
	{
		Test();
		return ResultsView.Elements;
	}

#	endif

#endif	// ENUMERABLES_USE_RESULTSVIEW


}	// namespace Def
}	// namespace Enumerables



#undef ENUMERABLES_STRINGIFY_EVALD
#undef ENUMERABLES_STRINGIFY

#ifdef ENUMERABLES_NO_FUN
#	pragma pop_macro("FUN")
#	pragma pop_macro("FUN1")
#	pragma pop_macro("FUN2")
#	pragma pop_macro("FUN3")
#	pragma pop_macro("FUNV")
#	pragma pop_macro("FUNV1")
#	pragma pop_macro("FUNV2")
#	pragma pop_macro("FUNV3")
#endif

#endif	// ENUMERABLES_IMPLEMENTATION_HPP
