#ifndef ENUMERABLES_TYPEHELPERBASICS_HPP
#define ENUMERABLES_TYPEHELPERBASICS_HPP

	/*  Part of Enumerables for C++.					*
	 *													*
	 *  This file contains basic and generic utilities  *
	 *  that extend the standard type traits.			*
	 *  Tools here should aim for the general case.		*/


#include <type_traits>



namespace Enumerables::TypeHelpers {

	using std::move;
	using std::forward;
	using std::declval;
	using std::decay_t;
	using std::enable_if_t;
	using std::conditional_t;
	using std::remove_pointer_t;
	using std::remove_reference_t;
	using std::is_abstract;
	using std::is_abstract_v;
	using std::is_assignable;
	using std::is_assignable_v;
	using std::is_const;
	using std::is_const_v;
	using std::is_constructible;
	using std::is_constructible_v;
	using std::is_copy_constructible;
	using std::is_copy_constructible_v;
	using std::is_convertible;
	using std::is_convertible_v;
	using std::is_void;
	using std::is_void_v;
	using std::is_void;
	using std::is_void_v;
	using std::is_reference;
	using std::is_reference_v;
	using std::is_lvalue_reference;
	using std::is_lvalue_reference_v;
	using std::is_rvalue_reference;
	using std::is_rvalue_reference_v;
	using std::is_member_object_pointer;
	using std::is_member_object_pointer_v;
	using std::is_member_pointer;
	using std::is_member_pointer_v;
	using std::is_member_function_pointer;
	using std::is_member_function_pointer_v;
	using std::is_pointer;
	using std::is_pointer_v;
	using std::is_same;
	using std::is_same_v;
	using std::is_scalar;
	using std::is_scalar_v;
	using std::is_class;
	using std::is_class_v;



	// ===== Container detection ======================================================================================

	// Required for complete ADL
	using std::begin;
	using std::end;
	using std::size;


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

	/// Obtain size if container supports std::size or has its own ADL overload.
	template <class C>
	auto AdlSize(const C& cont) -> decltype(size(cont))
	{
		return size(cont);
	}


	/// Iterator of a C++ container
	template <class Container>
	using IteratorT    = decltype(begin(declval<Container>()));

	/// End iterator of a C++ container (may or may not differ from IteratorT)
	template <class Container>
	using EndIteratorT = decltype(end(declval<Container>()));

	/// Iterated type of a legacy C++ iterator
	template <class Iter>
	using PointedT  = decltype(*declval<Iter>());

	/// Iterated type of a C++ container
	template <class Container>
	using IterableT = PointedT<IteratorT<Container>>;


	/// Any container-like entity, candidate to be wrapped by an Enumerable.
	template <class C>
	concept RangeIterable = requires (IteratorT<C>& begin, EndIteratorT<C>& end)
	{ 
		++begin;
		* begin;
		{ begin != end } -> std::convertible_to<bool>;
	};



	// ===== Type parameter deduction adjustments =====================================================================

	/// Sometimes 'void' needs an instantiable alternative
	struct None {};

	template <class T>
	constexpr bool IsNone = is_same_v<T, None>;


	/// For optionally specified 1st type arguments: any non-void type overrides Default.
	template <class Override, class Default>
	using OverrideT = conditional_t<is_void_v<Override> || IsNone<Override>, Default, Override>;

	/// Intentionally prevent implicit deduction of a function argument.
	template <class T>
	using NoDeduce = OverrideT<T, void>;



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
	using RemoveRvalueRefT = conditional_t<is_rvalue_reference_v<T>, remove_reference_t<T>, T>;

	/// Drop ref if pointer underlies.  [T*& --> T*;   T& --> T&]
	template <class T>
	using RemovePtrRefT	   = conditional_t<is_pointer_v<remove_reference_t<T>>, remove_reference_t<T>, T>;

	/// Remove references and cv from scalars.
	template <class T>
	using DecayIfScalarT   = conditional_t<is_scalar_v<remove_reference_t<T>>, BaseT<T>, T>;


	/// Finds the innermost pointed/referenced type (arrays left in place), qualifiers included.
	template <class T>
	struct RootPointed {
		using Type = T;
		static_assert (!is_pointer<T>() && !is_reference<T>(), "Internal error.");
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
		conditional_t< is_pointer_v<T> && !is_reference_v<T>,	std::add_const_t<remove_pointer_t<remove_reference_t<T>>>*,
		conditional_t< is_lvalue_reference_v<T>,				std::add_const_t<remove_reference_t<T>>&,
		conditional_t< is_rvalue_reference_v<T>,				std::add_const_t<remove_reference_t<T>>&&,
																std::add_const_t<T>										>>>;

	/// Implementation of DeepConstT.
	template <class T>
	struct DeepConst {
		using Type = T;
		static_assert (!is_pointer<T>() && !is_reference<T>() && !std::is_array<T>(), "Internal error.");
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
	constexpr bool HasConstValue	= is_same_v<ConstValueT<OverrideT<T, int>>, T>;

	template <class Trg, class Src>
	constexpr bool IsHeadAssignable	= !is_reference_v<Trg> && is_assignable_v<Trg&, Src>;

	template <class Trg, class Src>
	constexpr bool IsRefCompatible	= is_convertible_v<remove_reference_t<Src>*, remove_reference_t<Trg>*>;

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
	struct CommonOrVoid<T, U, std::void_t<std::common_type_t<T, U>>> {
		using Type = std::common_type_t<T, U>;
	};


	/// Helper: std::common_type augmented to preserve top-level qualifiers too -- still strips off &!
	template <class R, class Q>
	using QualifiedCommonT = remove_pointer_t<typename CommonOrVoid< remove_reference_t<R>*,
																	 remove_reference_t<Q>* >::Type>;


	/// Proposable deduced result type to combine 2 presumed compatible sources - adhering to the first one in case a
	/// value-conversion/copy is needed. [&+& and &&+&& yield the common &/&& type respectively; void if incompatible]
	template <class Pri, class Sec>
	using CompatResultT = conditional_t< is_lvalue_reference_v<Pri> && is_lvalue_reference_v<Sec> && HaveRefcompatibleRoots<Pri, Sec>,
										 std::add_lvalue_reference_t<QualifiedCommonT<Pri, Sec>>,
						  conditional_t< is_rvalue_reference_v<Pri> && is_rvalue_reference_v<Sec> && HaveRefcompatibleRoots<Pri, Sec>,
										 std::add_rvalue_reference_t<QualifiedCommonT<Pri, Sec>>,
						  conditional_t< is_pointer_v<remove_reference_t<Pri>> && is_pointer_v<remove_reference_t<Sec>>,
										 typename CommonOrVoid<Pri, Sec>::Type,
						  conditional_t< is_convertible_v<Sec, BaseT<Pri>>,
										 BaseT<Pri>,
						  void >>>>;



	// ===== Function detections ======================================================================================

	template <class Func, class... Args>
	using InvokeResultT = std::invoke_result_t<Func, Args...>;



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

		template <class Obj = T, enable_if_t<!is_pointer_v<remove_reference_t<Obj>>, int> = 0>
		static constexpr bool Check(decay_t<decltype((declval<Obj>().*declval<Mptr>()) (declval<Args>()...))>*)
		{
			return true;
		}

		template <class Obj = T, enable_if_t<is_pointer_v<remove_reference_t<Obj>>, int> = 0>
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

	template <class T, class... Args>
	concept BraceConstructible = IsBraceConstructible<T, Args...>::value;


	/// Constructible either via {} as a struct, or possibly narrowing via ().
	template <class T, class... Args>
	constexpr bool IsConstructibleAnyway = IsBraceConstructible<T, Args...>::value
										|| is_constructible_v<T, Args...>;



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

	template <class T>
	using IfVoid		= enable_if_t<is_void_v<T>>;

	template <class T, class S = T>
	using IfNonvoidVal	= enable_if_t<std::is_object_v<T>, S>;

	template <class T>
	concept NonvoidValue = std::is_object_v<T>;


	template <class T, class S = T>
	using IfPRValue	  = enable_if_t<!is_reference_v<T>, S>;

	template <class T, class S = T>
	using IfReference = enable_if_t<is_reference_v<T>, S>;

	template <class T>
	concept Reference = is_reference_v<T>;

	template <class T, class S = T>
	using IfMutRVal	= enable_if_t< !is_const_v<remove_reference_t<T>> && (is_rvalue_reference_v<T> || !is_reference_v<T>),
								   S >;

	template <class T, class S = T>
	using IfLVal	= enable_if_t< is_lvalue_reference_v<T>, S>;

	template <class T, class S = T>
	using IfMutLVal	= enable_if_t< !is_const_v<remove_reference_t<T>> && is_lvalue_reference_v<T>,
								   S >;


	template <class T, class S = T>
	using IfConst	= enable_if_t<is_const_v<remove_reference_t<T>>, S>;

	template <class T, class S = T>
	using IfMutable	= enable_if_t<!is_const_v<remove_reference_t<T>>, S>;

	template <class T, class S = T>
	using IfValueOrMutable = enable_if_t< !is_const_v<remove_reference_t<T>> || !is_reference_v<T>,
										  S >;

}		// namespace Enumerables::TypeHelpers

#endif	// ENUMERABLES_TYPEHELPERBASICS_HPP
