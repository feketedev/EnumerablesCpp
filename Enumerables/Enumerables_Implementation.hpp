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
		return HasValue() ? SizeInfo { kind, std::min(value, max) }
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
	bool AreEqual(std::initializer_list<I>&& lhs, B&& rhs)
	{
		return AreEqual(lhs, rhs);
	}

	template <class I, class A>
	bool AreEqual(A&& lhs, std::initializer_list<I>&& rhs)
	{
		return AreEqual(lhs, rhs);
	}


	template <class TForced, class TCommon0, class C1, class C2>
	auto ConcatInternal(C1&& cont1, C2&& cont2)
	{
		using Eb1 = decltype(Enumerate<TForced>(forward<C1>(cont1)));
		using Eb2 = decltype(Enumerate<TForced>(forward<C2>(cont2)));

		using THead	   = OverrideT<TCommon0, EnumerableT<Eb1>>;
		using TCommon1 = typename ConcatTypeDeducer<THead, EnumerableT<Eb1>>::TCommon;
		using TCommon  = typename ConcatTypeDeducer<TCommon1, EnumerableT<Eb2>>::TCommon;

		return Enumerate<TCommon>(forward<C1>(cont1))
				.Concat(Enumerate<TCommon>(forward<C2>(cont2)));
	}


	// Avoiding less then 2 parameters... Should it be even supported for generic programming?
	template <class TForced, class TCommon0, class C1, class C2, class... CMore>
	auto ConcatInternal(C1&& cont1, C2&& cont2, CMore&&... tailConts)
	{
		using Eb1      = decltype(Enumerate<TForced>(forward<C1>(cont1)));
		using Eb2      = decltype(Enumerate<TForced>(forward<C2>(cont2)));
		using THead	   = OverrideT<TCommon0, EnumerableT<Eb1>>;
		using TCommon2 = typename ConcatTypeDeducer<THead, EnumerableT<Eb2>>::TCommon;

		// must look ahead too for final consensus!
		using EbTail  = decltype(ConcatInternal<TForced, TCommon2>(forward<C2>(cont2), forward<CMore>(tailConts)...));
		using TCommon = typename ConcatTypeDeducer<TCommon2, EnumerableT<EbTail>>::TCommon;

		return Enumerate<TCommon>(forward<C1>(cont1))
				.Concat(ConcatInternal<TForced, TCommon>(forward<C2>(cont2), forward<CMore>(tailConts)...));
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

		return *move(sum);	
		
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
	auto AutoEnumerable<TFactory>::Min(Comp&& isLess) const -> Optional<TElemDecayed>
	{
		auto et = GetEnumerator();
		if (!et.FetchNext())
			return NoValue<TElemDecayed>(StopReason::Empty);

		Reassignable<TElem> min = et.Current();

		while (et.FetchNext()) {
			TElem curr = et.Current();
			if (isLess(curr, min))
				min.AssignMoved(curr);
		}
		return forward<TElem>(*min);
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
	template <class... ListOptions>
	auto AutoEnumerable<TFactory>::ToList(size_t sizeHint) const -> ListType<TElemDecayed, ListOptions...>
	{
		TEnumerator etor = GetEnumeratorNoDebug();
		return ObtainCachedResults<ListOperations, TElemDecayed>(etor, sizeHint, ListOptions {}...);
	}

	template <class TFactory>
	template <class... ListOptions>
	auto AutoEnumerable<TFactory>::ToList(size_t sizeHint, const ListOptions&... opts) const -> ListType<TElemDecayed, ListOptions...>
	{
		TEnumerator etor = GetEnumeratorNoDebug();
		return ObtainCachedResults<ListOperations, TElemDecayed>(etor, sizeHint, opts...);
	}


	template <class TFactory>
	template <size_t N, class... SmallListOptions>
	ENUMERABLES_WARN_FOR_SMALLLIST
	auto AutoEnumerable<TFactory>::ToList(size_t sizeHint) const -> SmallListType<TElemDecayed, N, SmallListOptions...>
	{
		TEnumerator etor = GetEnumeratorNoDebug();
		return ObtainCachedResults<SmallListOperations, TElemDecayed, N>(etor, sizeHint, SmallListOptions {}...);
	}

	template <class TFactory>
	template <size_t N, class... SmallListOptions>
	ENUMERABLES_WARN_FOR_SMALLLIST
	auto AutoEnumerable<TFactory>::ToList(size_t sizeHint, const SmallListOptions&... opts) const -> SmallListType<TElemDecayed, N, SmallListOptions...>
	{
		TEnumerator etor = GetEnumeratorNoDebug();
		return ObtainCachedResults<SmallListOperations, TElemDecayed, N>(etor, sizeHint, opts...);
	}


	template <class TFactory>
	template <class... SetOptions>
	auto AutoEnumerable<TFactory>::ToSet(size_t sizeHint) const -> SetType<TElemDecayed, SetOptions...>
	{
		TEnumerator etor = GetEnumeratorNoDebug();
		return ObtainCachedResults<SetOperations, TElemDecayed>(etor, sizeHint, SetOptions {}...);
	}

	template <class TFactory>
	template <class... SetOptions>
	auto AutoEnumerable<TFactory>::ToSet(size_t sizeHint, const SetOptions&... opts) const -> SetType<TElemDecayed, SetOptions...>
	{
		TEnumerator etor = GetEnumeratorNoDebug();
		return ObtainCachedResults<SetOperations, TElemDecayed>(etor, sizeHint, opts...);
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

		// Here Enumerate() serves:
		//	* as a way to uniformize calls where "second" is a direct Container (either & or &&)
		//	* as an appropriate closure in those cases (Container&& -> copy; Container& -> reference)
		//    Note that ChainedFactory itself does not store references currently.
		auto wrapped2    = Enumerate<TForced>(forward<Enumerable2>(second));
		auto factoryRefs = std::forward_as_tuple(factory, move(wrapped2.factory));
		auto nextFactory = JoinFactories<NextEnumerator, TypeArgs...>(factoryRefs, forward<Args>(args)...);

		return AutoEnumerable<decltype(nextFactory)> { move(nextFactory), isPure };
	}

	template <class TFactory>
	template <class Enumerable2, class TForced, template <class...> class NextEnumerator, class... TypeArgs, class... Args>
	auto AutoEnumerable<TFactory>::MvChainJoined(Enumerable2& second, Args&& ...args)
	{
		auto wrapped2    = Enumerate<TForced>(forward<Enumerable2>(second));
		auto factoryRefs = std::forward_as_tuple(move(factory), move(wrapped2.factory));
		auto nextFactory = JoinFactories<NextEnumerator, TypeArgs...>(factoryRefs, forward<Args>(args)...);

		return AutoEnumerable<decltype(nextFactory)> { move(nextFactory), isPure };
	}


	template<class TFactory>
	void AutoEnumerable<TFactory>::ViewTrigger() const
	{
#	if ENUMERABLES_USE_RESULTSVIEW && ENUMERABLES_RESULTSVIEW_AUTO_EVAL == 1
		ResultsView.Fill(factory, isPure, true);
#	endif
	}

#pragma endregion




#if ENUMERABLES_USE_RESULTSVIEW

	template <class T>
	template <class V, class Factory>
	void ResultBuffer<T>::Fill(Factory& getEnumerator, bool isPure, bool autoCall, enable_if_t<is_copy_constructible<V>::value>*)
	{
		if (autoCall && (GetSize(Elements) > 0 || Status[0] == 'E'))
			return;

		if (isPure) {
			auto	 et	 = getEnumerator();
			SizeInfo si  = et.Measure();
			size_t	 cap = si.IsExact() ? si.value : 0u;

			Elements = SmallListOperations::template Init<TDebugValue, 4>(cap);

			size_t count = 0;
			while (et.FetchNext() && count < ENUMERABLES_RESULTSVIEW_MAX_ELEMS) {
				SmallListOperations::Add(Elements, et.Current());
				++count;
			}
			Status = count < ENUMERABLES_RESULTSVIEW_MAX_ELEMS
				? "Evaluation successful."
				: "Showing first" ENUMERABLES_STRINGIFY(ENUMERABLES_RESULTSVIEW_MAX_ELEMS) "elements.";
		}
		else {
			Status = "Not available - enumeration is marked as having side-effects.";
			if (!autoCall)
				ENUMERABLES_CLIENT_BREAK ("Disabled for having side-effects - cannot build debug buffer.");
		}

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
