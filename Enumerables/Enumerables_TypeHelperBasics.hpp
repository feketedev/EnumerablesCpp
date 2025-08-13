#ifndef ENUMERABLES_TYPEHELPERBASICS_HPP
#define ENUMERABLES_TYPEHELPERBASICS_HPP

	/*  Part of Enumerables for C++.					*
	 *													*
	 *  This file contains basic and generic utilities  *
	 *  that extend the standard type traits.			*
	 *  Tools here should aim for the general case.		*/


#include <type_traits>



namespace Enumerables {
namespace TypeHelpers {

	using std::move;
	using std::forward;
	using std::declval;
	using std::decay_t;
	using std::enable_if_t;
	using std::conditional_t;
	using std::remove_pointer_t;
	using std::remove_reference_t;
	using std::void_t;
	using std::is_abstract;
	using std::is_assignable;
	using std::is_const;
	using std::is_constructible;
	using std::is_copy_constructible;
	using std::is_convertible;
	using std::is_void;
	using std::is_reference;
	using std::is_lvalue_reference;
	using std::is_rvalue_reference;
	using std::is_member_object_pointer;
	using std::is_member_pointer;
	using std::is_member_function_pointer;
	using std::is_pointer;
	using std::is_same;
	using std::is_scalar;
	using std::is_class;



	// ===== Container detection ======================================================================================

	// Required for complete ADL
	using std::begin;
	using std::end;


	/// Enforce ADL - even inside Enumerable, which has its own begin/end members.
	template <class C>
	auto AdlBegin(C&& cont) -> decltype(begin(forward<C>(cont)))
	{
		return begin(forward<C>(cont));
	}

	template <class C>
	auto AdlEnd(C&& cont) -> decltype(end(forward<C>(cont)))
	{
		return end(forward<C>(cont));
	}


	/// Iterator of a C++ container
	template <class Container>
	using IteratorT = decltype(begin(declval<Container>()));

	/// Iterated type of a legacy C++ iterator
	template <class Iter>
	using PointedT = decltype(*declval<Iter>());

	/// Iterated type of a C++ container
	template <class Container>
	using IterableT = PointedT<IteratorT<Container>>;


	/// R if C seems like a generic container, candidate to be wrapped by an Enumerable.
	template <class C, class R = C>
	using IfContainerLike = enable_if_t<is_convertible<decltype(begin(declval<C>()) != end(declval<C>())), bool>::value, R>;



	// ===== Type parameter deduction adjustments =====================================================================

	/// Sometimes 'void' needs an instantiable alternative
	struct None {};

	template <class T>
	constexpr bool IsNone = is_same<T, None>::value;


	/// For optionally specified 1st type arguments: any non-void type overrides Default.
	template <class Override, class Default>
	using OverrideT = conditional_t<is_void<Override>::value || is_same<Override, None>::value, Default, Override>;

	// NOTE: utilizing IsNone here -----------------------------^
	//		 causes old MSVC to complain about (C2970) internal linkage
	//		 vs non-type argument when invoked under OverrideOp::Apply!
	
	template <class Override>
	struct OverrideOp {
		template <class Default>
		using Apply = OverrideT<Override, Default>;
	};


	/// Intentionally prevent implicit deduction of a function argument.
	template <class T>
	using NoDeduce = OverrideT<T, void>;


	/// Make T a dependent-type on some arbitrary template parameter to enable SFINAE.
	template <class FakeDep, class T>
	using AsDependentT = conditional_t<is_void<FakeDep>::value, T, T>;


	/// Provides ::type alias if receives at least 2 types. SFINAE helper to substitute sizeof...(Ts) > 0.
	/// @tparam H: Mandatory head elem to be aliased -> can be used to ensure an argument-dependent result!
	template <class H, class... Ts>
	struct IfMultipleTypes {};

	template <class H, class S, class... T>
	struct IfMultipleTypes<H, S, T...> {
		using type = H;
	};



	// ===== Ref/ptr tools ============================================================================================

	/// Function of std::decay without conversions: just strip qualifiers and &
	template <class T>
	using BaseT			= std::remove_cv_t<remove_reference_t<T>>;

	/// Pointed or referenced type, unqualified.  [cv T&|T*|T*& --> T;  cv T** --> cv T*]
	template <class T>
	using BasePointedT	= std::remove_cv_t<remove_pointer_t<remove_reference_t<T>>>;

	/// Top-level pointed or referenced type, unqualified.  [cv T*|T& --> T;  T*& --> T*]
	template <class T>
	using PointedOrRefdBaseT = BaseT<remove_pointer_t<T>>;

	/// Top-level pointed or referenced type.  [T*|T& --> T;  T*& --> T*]
	template <class T>
	using PointedOrRefdT	 = remove_reference_t<remove_pointer_t<T>>;


	/// Helper to consume T* and T& in a uniform way.
	template <class>
	struct PointerOrRef;

	template <class T>
	struct PointerOrRef<T*> {
		static T*	Translate(T* ptr)	{ return ptr;  }
		static T*	Translate(T& ref)	{ return &ref; }
	};
	template <class T>
	struct PointerOrRef<T&> {
		static T&	Translate(T* ptr)	{ return *ptr; }
		static T&	Translate(T& ref)	{ return ref;  }
	};


	template <class T>
	using RemoveRvalueRefT = conditional_t<is_rvalue_reference<T>::value, remove_reference_t<T>, T>;

	/// Drop ref if pointer underlies.  [T*& --> T*;   T& --> T&]
	template <class T>
	using RemovePtrRefT	   = conditional_t<is_pointer<remove_reference_t<T>>::value, remove_reference_t<T>, T>;

	/// Remove references and cv from scalars.
	template <class T>
	using DecayIfScalarT   = conditional_t<is_scalar<remove_reference_t<T>>::value, BaseT<T>, T>;


	/// Finds the innermost pointed/referenced type (arrays left in place), qualifiers included.
	template <class T>
	struct RootPointed {
		using Type = T;
		static_assert (!is_pointer<T>::value && !is_reference<T>::value, "Internal error.");
	};
	template <class T>
	struct RootPointed<T*>					{ using Type = typename RootPointed<T>::Type; };
	template <class T>
	struct RootPointed<T* const>			{ using Type = typename RootPointed<T>::Type; };
	template <class T>
	struct RootPointed<T* const volatile>	{ using Type = typename RootPointed<T>::Type; };
	template <class T>
	struct RootPointed<T* volatile>			{ using Type = typename RootPointed<T>::Type; };
	template <class T>
	struct RootPointed<T&>					{ using Type = typename RootPointed<T>::Type; };
	template <class T>
	struct RootPointed<T&&>					{ using Type = typename RootPointed<T>::Type; };

	/// The innermost pointed/referenced type without qualifiers (arrays left in place).
	template <class T>
	using RootPointedBaseT = std::remove_cv_t<typename RootPointed<T>::Type>;


	/// Inject const inside top-level pointer/reference, or simply before value type. [T must not be void.]
	template <class T>
	using ConstValueT =
		conditional_t< is_pointer<T>::value && !is_reference<T>::value,	std::add_const_t<remove_pointer_t<remove_reference_t<T>>>*,
		conditional_t< is_lvalue_reference<T>::value,					std::add_const_t<remove_reference_t<T>>&,
		conditional_t< is_rvalue_reference<T>::value,					std::add_const_t<remove_reference_t<T>>&&,
																		std::add_const_t<T>										>>>;

	/// Implementation of DeepConstT.
	template <class T>
	struct DeepConst {
		using Type = T;
		static_assert (!is_pointer<T>::value && !is_reference<T>::value && !std::is_array<T>::value, "Internal error.");
	};
	template <class T>
	struct DeepConst<T*>				{ using Type = std::add_const_t<typename DeepConst<T>::Type> *; };
	template <class T>
	struct DeepConst<T* const>			{ using Type = std::add_const_t<typename DeepConst<T>::Type> * const; };
	template <class T>
	struct DeepConst<T* const volatile>	{ using Type = std::add_const_t<typename DeepConst<T>::Type> * const volatile; };
	template <class T>
	struct DeepConst<T* volatile>		{ using Type = std::add_const_t<typename DeepConst<T>::Type> * volatile; };
	template <class T>
	struct DeepConst<T&>				{ using Type = std::add_const_t<typename DeepConst<T>::Type> const &; };
	template <class T>		
	struct DeepConst<T&&>				{ using Type = std::add_const_t<typename DeepConst<T>::Type> const &&; };
	template <class T>		
	struct DeepConst<T[]>				{ using Type = typename DeepConst<T>::Type const []; };
	template <class T, size_t N>		
	struct DeepConst<T[N]>				{ using Type = typename DeepConst<T>::Type const [N]; };

	/// Inject const under every pointed / referenced level. Top qualifiers left intact!
	/// @remarks
	///		Any less const-qualified similar type should be convertible into the result.
	template <class T>
	using DeepConstT = typename DeepConst<T>::Type;


	// [void -> false]
	template <class T>
	constexpr bool HasConstValue	= is_same<ConstValueT<OverrideT<T, int>>, T>::value;

	template <class Trg, class Src>
	constexpr bool IsHeadAssignable	= !is_reference<Trg>::value && is_assignable<Trg&, Src>::value;

	template <class Trg, class Src>
	constexpr bool IsRefCompatible	= is_convertible<remove_reference_t<Src>*, remove_reference_t<Trg>*>::value;

	template <class T1, class T2>
	constexpr bool AreRefCompatible	= IsRefCompatible<T1, T2> || IsRefCompatible<T2, T1>;


	/// Innermost pointed/referenced type of Src is reference-compatible with Trg's innermost type - pointer depth ignored (!)
	template <class Trg, class Src>
	constexpr bool HasRefcompatibleRoot   = IsRefCompatible<RootPointedBaseT<Trg>, RootPointedBaseT<Src>>;

	/// Innermost pointed/referenced types are reference-compatible - pointer depth ignored (!)
	template <class T, class U>
	constexpr bool HaveRefcompatibleRoots = HasRefcompatibleRoot<T, U>
										 || HasRefcompatibleRoot<U, T>;


	/// Graceful variant of std::commontype: falling back to void instead of substitution failure.
	template <class T, class U, class = void>
	struct CommonOrVoid {
		using Type = void;
	};
	template <class T, class U>
	struct CommonOrVoid<T, U, void_t< decltype(false ? declval<T>() : declval<U>()),	// hardened SFINAE for old STL (VS2015)
									  std::common_type_t<T, U>						>> {
		using Type = std::common_type_t<T, U>;
	};


	/// Helper: std::common_type augmented to preserve top-level qualifiers too -- still strips off &!
	template <class R, class Q>
	using QualifiedCommonT = remove_pointer_t<typename CommonOrVoid< remove_reference_t<R>*,
																	 remove_reference_t<Q>* >::Type>;


	/// Proposable deduced result type to combine 2 presumed compatible sources - adhering to the first one in case a
	/// value-conversion/copy is needed. [&+& and &&+&& yield the common &/&& type respectively; void if incompatible]
	template <class Pri, class Sec>
	using CompatResultT = conditional_t< is_lvalue_reference<Pri>::value && is_lvalue_reference<Sec>::value && HaveRefcompatibleRoots<Pri, Sec>,
										 std::add_lvalue_reference_t<QualifiedCommonT<Pri, Sec>>,
						  conditional_t< is_rvalue_reference<Pri>::value && is_rvalue_reference<Sec>::value && HaveRefcompatibleRoots<Pri, Sec>,
										 std::add_rvalue_reference_t<QualifiedCommonT<Pri, Sec>>,
						  conditional_t< is_pointer<remove_reference_t<Pri>>::value && is_pointer<remove_reference_t<Sec>>::value,
										 typename CommonOrVoid<Pri, Sec>::Type,
						  conditional_t< is_convertible<Sec, BaseT<Pri>>::value,
										 BaseT<Pri>,
						  void >>>>;


	/// Simplified, unchecked version of std::align.
	template <class T>
	void* AlignFor(void* trg)
	{
		size_t miss = reinterpret_cast<uintptr_t>(trg) % alignof(T);
		size_t offs = miss ? alignof(T) - miss : 0u;
		return static_cast<char*>(trg) + offs;
	}



	// ===== Container Binding helpers ================================================================================

	template <size_t... Values>
	struct SizeList {
		static constexpr size_t dim = sizeof...(Values);
	};


	template <class...>
	struct TypeList {
		static constexpr size_t size = 0;
	};
	template <class U>
	struct TypeList<U> {
		using first = U;
		using last  = U;
		static constexpr size_t size = 1;
	};
	template <class H, class S, class... Tail>
	struct TypeList<H, S, Tail...> {
		using first = H;
		using last  = typename TypeList<S, Tail...>::last;
		static constexpr size_t size = 2 + sizeof...(Tail);
	};


	template <class NewHead, class Tail>
	struct PrependType;
	template <class H, class... Ts>
	struct PrependType<H, TypeList<Ts...>> {
		using typeList = TypeList<H, Ts...>;
	};


	template <class List, template <class> class Mapper>
	struct MapTypeList;
	template <template <class> class Mapper>
	struct MapTypeList<TypeList<>, Mapper> {
		using typeList = TypeList<>;
	};
	template <class H, class... Ts, template <class> class Mapping>
	struct MapTypeList<TypeList<H, Ts...>, Mapping> {
		using Tail     = typename MapTypeList<TypeList<Ts...>, Mapping>::typeList;
		using typeList = typename PrependType<Mapping<H>, Tail>::typeList;
	};
	

	/// Implementation of BindChangingNthT / ChangedNthArgT.
	template <template <class...> class Trg, template <class> class Change, unsigned n, class ProcessedList, class... OrigArgs>
	struct BindChangingNthArg;

	// index reached -> change here! [Next is required to exist.]
	template <template <class...> class Trg, template <class> class Change, class... Intact, class Next, class... Left>
	struct BindChangingNthArg<Trg, Change, 0, TypeList<Intact...>, Next, Left...> {
		using type = Trg<Intact..., Change<Next>, Left...>;
	};
	// end reached -> overindexed, finish silently
	template <template <class...> class Trg, template <class> class Change, unsigned n, class... Intact>
	struct BindChangingNthArg<Trg, Change, n, TypeList<Intact...>> {
		using type = Trg<Intact...>;
	};
	// process next
	template <template <class...> class Trg, template <class> class Change, unsigned n, class... Intact, class Next, class... Left>
	struct BindChangingNthArg<Trg, Change, n, TypeList<Intact...>, Next, Left...> {
		using type = typename BindChangingNthArg<Trg, Change, n - 1, TypeList<Intact..., Next>, Left...>::type;
	};

	/// Bind Trg<Args...> but with the modification of a given type argument as Arg -> Change<Arg>.
	/// [Overindexing tolerated.]
	template <template <class...> class Trg, template <class> class Change, unsigned n, class... Args>
	using BindChangingNthT = typename BindChangingNthArg<Trg, Change, n, TypeList<>, Args...>::type;


	/// Helper for ChangedNthArgT.
	template <class Bound, unsigned n, template <class> class Change>
	struct ChangeNthArg {
		static_assert (n != n, "Unable to match the templated type. Have you given a bound template class as first argument?");
	};
	template <template <class...> class T, class... Originals, unsigned n, template <class> class Change>
	struct ChangeNthArg<T<Originals...>, n, Change> {
		static_assert (sizeof...(Originals) > n, "Index to change is larger than the last available parameter's!");

		using type = BindChangingNthT<T, Change, n, Originals...>;
	};

	/// Rebind the given template type T<class...> with the modification of a single type argument,
	/// i.e. Arg -> Change<Arg>.		[Overindexing disallowed; Non-type args unsupported.]
	template <class Bound, unsigned n, template <class> class Change>
	using ChangedNthArgT = typename ChangeNthArg<Bound, n, Change>::type;

	/// Rebind the given template type T<class...> replacing a single type argument with Override,
	/// provided it's not None or void.	[Overindexing disallowed; Non-type args unsupported.]
	template <class Bound, unsigned n, class Override = None>
	using OverriddenNthArgT = ChangedNthArgT<Bound, n, OverrideOp<Override>::template Apply>;




	// ===== Function detections ======================================================================================

	template <class Func, class... Args>
	using InvokeResultT = std::result_of_t<Func(Args...)>;



	template <class Func, class... Args>
	struct IsCallable {

		template <class F = Func>
		static constexpr bool Check(decay_t<decltype(declval<F>()(declval<Args>()...))>*)
		{
			return true;
		}

		static constexpr bool Check(...)
		{
			return false;
		}

		static constexpr bool value = Check(nullptr);
	};



	template <class T, class Mptr, class... Args>
	struct IsCallableMember {

		template <class Obj = T, enable_if_t<!is_pointer<remove_reference_t<Obj>>::value, int> = 0>
		static constexpr bool Check(decay_t<decltype((declval<Obj>().*declval<Mptr>()) (declval<Args>()...))>*)
		{
			return true;
		}

		template <class Obj = T, enable_if_t<is_pointer<remove_reference_t<Obj>>::value, int> = 0>
		static constexpr bool Check(decay_t<decltype((declval<Obj>()->*declval<Mptr>()) (declval<Args>()...))>*)
		{
			return true;
		}


		static constexpr bool Check(...)
		{
			return false;
		}

		static constexpr bool value = Check(nullptr);
	};



	template <class T, class... Args>
	struct IsBraceConstructible {

		template <class TT = T>
		static constexpr bool Check(decay_t<decltype(TT { declval<Args>()... })>*)
		{
			return true;
		}

		static constexpr bool Check(...)
		{
			return false;
		}

		static constexpr bool value = Check(nullptr);
	};



	/// Constructible either via {} as a struct, or possibly narrowing via ().
	template <class T, class... Args>
	constexpr bool IsConstructibleAnyway = IsBraceConstructible<T, Args...>::value
										|| is_constructible<T, Args...>::value;



	/// Checks wether T has operator +=
	template <class T, class Addend = T>
	struct IsAddAssignable {

		template <class TT = T>
		static constexpr bool Check(decay_t<decltype(declval<TT&>() += declval<Addend>())>*)
		{
			return true;
		}

		static constexpr bool Check(...)
		{
			return false;
		}

		static constexpr bool value = Check(nullptr);
	};



	// ===== Enable_if shorthands =====================================================================================

	template <class T, class S = T>
	using IfNonScalar = enable_if_t<!is_scalar<T>::value, S>;

	template <class T, class S = T>
	using IfPRValue = enable_if_t<!is_reference<T>::value, S>;

	template <class T, class S = T>
	using IfReference = enable_if_t<is_reference<T>::value, S>;

	template <class T, class S = T>
	using IfMutRVal = enable_if_t< !is_const<remove_reference_t<T>>::value &&
								   (is_rvalue_reference<T>::value || !is_reference<T>::value),
								   S >;

	template <class T, class S = T>
	using IfLVal = enable_if_t< is_lvalue_reference<T>::value, S>;

	template <class T, class S = T>
	using IfMutLVal = enable_if_t< !is_const<remove_reference_t<T>>::value &&
								   is_lvalue_reference<T>::value,
								   S >;


	template <class T, class S = T>
	using IfConst = enable_if_t<is_const<remove_reference_t<T>>::value, S>;

	template <class T, class S = T>
	using IfMutable = enable_if_t<!is_const<remove_reference_t<T>>::value, S>;

	template <class T, class S = T>
	using IfValueOrMutable = enable_if_t< !is_const<remove_reference_t<T>>::value
									   || !is_reference<T>::value,
										  S >;

}		// namespace TypeHelpers
}		// namespace Enumerables

#endif	// ENUMERABLES_TYPEHELPERBASICS_HPP
