#ifndef ENUMERABLES_TYPEHELPERS_HPP
#define ENUMERABLES_TYPEHELPERS_HPP

	/*  Part of Enumerables for C++.											 *
	 *																			 *
	 *  This file contains template utilities that provide various abstractions  *
	 *  over the C++ type system, mainly to support a more functional-style and  *
	 *  less verbose code; as well as deduction helpers for Enumerable creation. */


#include "Enumerables_GenericStorage.hpp"



namespace Enumerables {
namespace TypeHelpers {


#pragma region Enumerable conversion helpers

	/// Enumerated type of an Enumerable (an Enumerator factory)
	template <class Eb>
	using EnumerableT = decltype(declval<Eb>().GetEnumerator().Current());

	/// Enumerated type of an Enumerator
	template <class Et>
	using EnumeratedT = decltype(declval<Et>().Current());



	/// Et is InterfacedEnumerator<T>.
	template <class Et>
	struct IsInterfacedEnumerator;

	template <class Et>
	struct IsInterfacedEnumerator {
		static constexpr bool value = false;
	};



	/// Eb is any kind of AutoEnumerable<T> -- including Enumerable<T>.
	template <class Eb>
	struct IsAutoEnumerable;

	template <class Eb>
	struct IsAutoEnumerable {
		static constexpr bool value = false;
	};



	/// C is a special container and is NOT eligible for automatic wrapping (e.g. initializer lists or Enumerables themselves).
	template <class C>
	struct IsSpeciallyTreatedContainer;

	template <class T>
	struct IsSpeciallyTreatedContainer<const std::initializer_list<T>> {
		static constexpr bool value = true;
	};

	template <class T>
	struct IsSpeciallyTreatedContainer<std::initializer_list<T>> {
		static constexpr bool value = true;
	};

	template <class Other>
	struct IsSpeciallyTreatedContainer {
		static constexpr bool value = IsAutoEnumerable<Other>::value;
	};


	/// Regulates implicit conversions from arbitrary AutoEnumerables to interfaced Enumerable<T>.
	/// @tparam FromEt:	Enumerator type of the source AutoEnumerable
	/// @tparam ToEt:	targeted InterfacedEnumerator<T> of Enumerable<T>
	template <class FromEt, class ToEt>
	class IsInterfacedConversion {
		using From = EnumeratedT<FromEt>;
		using To   = EnumeratedT<ToEt>;

		static constexpr bool toInterfaced = IsInterfacedEnumerator<ToEt>::value;
		static constexpr bool promoting    = toInterfaced && !IsInterfacedEnumerator<FromEt>::value;
		static constexpr bool converting   = !is_same<To, From>::value;

	public:
		static constexpr bool trivial = promoting && !converting;

		static constexpr bool asConst =  toInterfaced
									  && converting
									  && is_same<To, ConstValueT<From>>::value;

		static constexpr bool asDecayed =  toInterfaced
										&& is_reference<From>::value
										&& is_same<To, BaseT<From>>::value
										&& !is_abstract<To>::value;

		static constexpr bool any = trivial || asConst || asDecayed;
	};

#pragma endregion




#pragma region Automatic deduction helpers

	/// Type-checking helper to store a single-value seed into an enumeration.
	/// E.g. for Repeat, Sequence - in contrast with containers.
	template <class UniversalInit, class RequestedOutput = void>
	struct SeededEnumerationTypes {

		/// Stored single-value type for the user-given universal reference.
		using SeedStorage = conditional_t< is_lvalue_reference<UniversalInit>::value,
											UniversalInit,
											BaseT<UniversalInit> >;

							// BaseT removes const for the Enumerable to stay moveable!

		/// Element type of the resulting enumerable.
		using Output = OverrideT<RequestedOutput, SeedStorage>;

		// NOTE: Defaulting to const& (in favour of more complex types) would be somewhat analogous to Enumerate(Container&&),
		//		 but self-contained const& Enumerables have more caveats, and seem less intuitive here!

		static_assert (!is_rvalue_reference<SeedStorage>::value, "Enumerables internal error.");

		static_assert (is_reference<SeedStorage>::value || !is_reference<Output>::value || HasConstValue<Output>,
					   "Mutable ref output requested, but that requires (l-value) ref initialization! "
					   "Initializing with r-value results in materialized enumerable (having const& / pr-value support).");

		static_assert (is_reference<SeedStorage>::value || !is_reference<Output>::value || is_convertible<BaseT<SeedStorage>*, BaseT<Output>*>::value,
					   "Const & output for a materialized enumerable is possible only with compatible types!");
	};



	/// Type-checking helper to wrap containers.
	/// @remarks
	///		These restrictions don't take part in SFINAE overload selection, but give friendly errors.
	///		Their presence is still important!
	namespace ContainerWrapChecks
	{
		namespace Internals
		{
			template <class Iterated, class TElem>		void CoreChecks()
			{
				static_assert (is_reference<Iterated>::value || !is_reference<TElem>::value,
							   "Container iterates over pr-values. Can't enumerate them by reference directly!\n"
							   "V -> const V& conversion is possible by explicitly creating a snapshot of the container\n"
							   "like:  \"Enumerate(cont).ToMaterialized<const V&>()\"");

				static_assert (!is_reference<Iterated>::value || !is_reference<TElem>::value || !HasConstValue<Iterated> || HasConstValue<TElem>,
							   "No const V& -> V& conversion, existing objects cannot lose const qualifier!");

				static_assert (!is_reference<TElem>::value || is_convertible<Iterated, TElem>::value,
							   "Container iterates over a type that is not compatible with the enumerated interface.");
			}

			template <class Iterated, class TElem>		void ImplicitReinforcedChecks()
			{
				// (V& -> V)  copy is silently allowed
				// -- perf implications allowed silently, but prvalue enumerations aren't even used with costly copyable types.
				//    In contrast, conversion ctors seem far less expected to run under the hood...
				static_assert (is_reference<TElem>::value || is_same<BaseT<Iterated>, BaseT<TElem>>(),
							   "Conversion required to obtain the targeted element type.\n"
							   "For clarity, make it explicit as:	\"Enumerate<TargetedType>(cont)\"");
			}
		}


		template <class Iterated, class TElem>		void ByRef()
		{
			static_assert (is_reference<Iterated>::value || !is_reference<TElem>::value || HasConstValue<TElem>,
						   "Container iterates over pr-values. Can't enumerate them by non-const reference!\n"
						   "No V -> V& conversion!");

			Internals::CoreChecks<Iterated, TElem>();
		}


		template <class Iterated, class TElem>		void ByValue()
		{
			static_assert (!is_reference<TElem>::value || HasConstValue<TElem>,
						   "A self-contained (aka. Materialized) enumeration cannot use non-const & elements, "
						   "as an Enumerable itself must be immutable!");

			Internals::CoreChecks<Iterated, TElem>();
		}


		template <class Iterated, class TElem>
		void ByRefImplicit()
		{
			ByRef<Iterated, TElem>();
			Internals::ImplicitReinforcedChecks<Iterated, TElem>();
		}


		template <class Iterated, class TElem>
		void ByValueImplicit()
		{
			ByValue <Iterated, TElem>();
			Internals::ImplicitReinforcedChecks<Iterated, TElem>();
		}
	}



	// CONSIDER: Update to common_type (CompatResultT)?
	template <class Vote1, class Vote2>
	struct ConcatTypeDeducer {
		static_assert (is_reference<Vote1>::value == is_reference<Vote2>::value,
					   "Can't Concat references with pr-values implicitly. Specify forced element type for clarity!");

		static_assert (is_reference<Vote1>::value || is_pointer<Vote1>::value && is_pointer<Vote2>::value || is_same<const Vote1, const Vote2>::value,
					   "Concatenation requires element value conversion (if possible). Specify forced element type for clarity!");

		using T1 = BaseOrPointedT<Vote1>;
		using T2 = BaseOrPointedT<Vote2>;
		using CommonBaseVote = conditional_t< is_same<T1, T2>::value,			Vote1,
							   conditional_t< is_convertible<T2*, T1*>::value,	Vote1,
							   conditional_t< is_convertible<T1*, T2*>::value,	Vote2,
							   void >>>;

		static_assert (!is_void<CommonBaseVote>::value,
					   "Neither element type points/references a public base class of the other. "
					   "You may specify a common ancestor manually."							  );

		using TCommon = conditional_t< HasConstValue<Vote1> || HasConstValue<Vote2>,
										ConstValueT<CommonBaseVote>,
										CommonBaseVote >;

		static_assert (is_convertible<Vote1, TCommon>::value && is_convertible<Vote2, TCommon>::value,
					   "Enumerables::Concat internal error. Try to specify forced element type.");
	};

#pragma endregion




#pragma region Unified Projection

	/// Navigate to a field of an object (or get a derived value) via a member-pointer.
	/// @remarks
	///	  Supports:
	///		* pointer to Data Member for direct access
	///			-> preserving rvalueness of owner object
	///		* pointer to Member Function of no parameters (for typical getter calls)
	///			-> returning the exact type (!) defined by that function
	namespace MemberPointerHelpers
	{
		template <class Ptr>
		using IfMemberObject = enable_if_t<is_member_object_pointer<remove_reference_t<Ptr>>::value>;

		template <class Ptr>
		using IfMemberFunction = enable_if_t<is_member_function_pointer<remove_reference_t<Ptr>>::value>;

		// For references, need to enhance compiler behaviour (older clang does not propagate rvalueness at all!)
		template <class T, class Selected>
		decltype(auto) PropagateRval(Selected&& s)
		{
			using RvalPropagated = conditional_t<is_lvalue_reference<T>::value, Selected&, remove_reference_t<Selected>&&>;
			return static_cast<RvalPropagated>(s);
		}

		template <class T, class Selector>
		auto&		   Select(T*  obj, Selector p, IfMemberObject<Selector>* = nullptr)		{ return  obj->*p; }
		template <class T, class Selector>
		decltype(auto) Select(T&& obj, Selector p, IfMemberObject<Selector>* = nullptr)		{ return PropagateRval<T>(obj.*p); }

		template <class T, class Selector>
		decltype(auto) Select(T*  obj, Selector p, IfMemberFunction<Selector>* = nullptr)	{ return (obj->*p)(); }
		template <class T, class Selector>
		decltype(auto) Select(T&& obj, Selector p, IfMemberFunction<Selector>* = nullptr)	{ return (forward<T>(obj).*p)(); }
	}

	/// Result of applying a member pointer (either access or getter call) to an object of T.
	template <class T, class Mptr>
	using SelectedMemberT = decltype(MemberPointerHelpers::Select(declval<T>(), declval<Mptr>()));



	/// Helper to semi-implicitly select the applicable overload of a getter-like method given
	/// by pointer-to-memberfunction syntax, when it designates an overload-set. Costs +1 ptr.
	/// @tparam R:	Expected result of projection: Exact or Decayed return type of the getter. (!)
	/// @tparam T:	Qualified owner of getter method (i.e. AutoEnumerable<F>::TElem).
	/// @remarks
	///		Consider use-cases like:	intervals.Select<int&>(&Interval::GetStart)
	///									intervals.OrderBy<int&>(&Interval::GetStart)
	///									intervals.MapTo<int>(&Interval::CalcLength)
	///			need to materialize:	data.Map(&ToInterval)
	///										.Select<int>(&Interval::GetStart)
	///
	///		Note that if the pointer to member function is exactly specified (either casted
	///		or not having an overload-set) then there's no need for this helper, nor for an
	///		explicitly specified return type (if no conversion needed for the projection):
	///									intervals.Map(&Interval::CalcLength)
	///
	///		In these exact (non-overloadset ptr) cases the compiler will prefer a templated
	///		overload (providing exact match of parameter type) against an extra conversion,
	///		so there should be no adverse effect of providing method overloads that receive
	///		an OverloadResolver to solve the nonexact cases.
	///
	///		The helper narrows the acceptable overloads to those properly qualified based on
	///		T's &-ness and constness. Even then, the return type can't be deduced as a language
	///		limitation (nor even if it would be exact), hence it must be exactly known.
	///		However, working with rvalue sequences often necessitate the materialization of
	///		the results from a getter - leading to a target element type differring from the
	///		getter's type!
	///
	///		To stay consistent, in methods like Select<R>(...), MapTo<R>(...) the user shall
	///		specify the targeted element type after the projection, not the ref-qualified
	///		return type of the getter. This is supported up to the materialization of typical
	///		getter results. (This is minimal requirement, but can't introduce ambiguity.)
	///
	///			E.g. the getter			int&	Interval::GetStart();
	///			can be used either as	intervals.Select<int&>(&Interval.GetStart)
	///			or						intervals.Select<int>(&Interval.GetStart)
	///			but no other way.
	///			[ Workaround is to convert in a next step:
	///									intervals.Select<int&>(&Interval.GetStart).AsConst()
	///									intervals.Select<int>(&Interval.GetStart).As<long>() ]
	///
	///		This quadruplication here seems manageable.
	///
	///		To support unconstrained .MapTo<R>, no lifetime checks are in place here
	///		- .Select checks that separately.
	///
	///		NOTE: const && / volatile support is out of scope!
	///
	template <class T, class R>
	class OverloadResolver final {

		using DT = decay_t<T>;

		// qualified Object type - [const] & for [const] pointers
		using O  = conditional_t< is_class<DT>::value,  T,
				   conditional_t< is_pointer<DT>::value && is_class<remove_pointer_t<DT>>::value,  remove_pointer_t<DT>&,
				   None>>;

		// Decayed object type
		using OD = decay_t<O>;

		// basically the discriminator
		R (OverloadResolver::* const wrapper)(T&&) const;

		union {
			// ---- Acceptable Method pointers ----

			// Possible overload-sets of exact R return type

			R	(OD::* uFun)();
			R	(OD::* cFun)()	const;

			R	(OD::* lFun)()	&;
			R	(OD::* clFun)()	const &;
			R	(OD::* rFun)()	&&;


			// Auxiliary overloads to provide copy/materialization in most common situations
			//	-> only if R is a prvalue

			R&		 (OD::* uFunRef)();             //
			const R& (OD::* cFunRef)()	const;		// important need: getter called on rvalue object
			const R& (OD::* clFunRef)()	const &;	//
			R&&		 (OD::* rFunRRef)() &&;         //
			R&		 (OD::* lFunRef)() &;

			R&&		 (OD::* uFunRRef)();			// convenience extra: move-out pattern ".PassXY()"

			// Providing all viable combinations would cause ambiguity.
			// Fortunately left-outs are not all sensible, e.g.:
			//   R& Get() const		// obviously not part of the object, lifetime not bound


			// ---- Acceptable Free-function pointers ----

			R	(*tFreeFun)(T&&);		// exact ref param		- [const]& / rval -> &&
			R	(*vFreeFun)(DT);		// copy/move into param - if prval overload exists, it should be the only one -> no ambiguity
			R	(*cFreeFun)(const DT&);	// if T is nonconst

			const R&	(*cFreeFunRef)(const DT&);	// prob. the only important materializing free-fun. case
		};


		// NOTE: could be templated?   (Seems tolerable so far.)
		// Options for "wrapper".
		R	CallU(T&& p)		const	{ return MemberPointerHelpers::Select(p, uFun);		}
		R	CallCpyU(T&& p)		const	{ return MemberPointerHelpers::Select(p, uFunRef);	}
		R	CallMaterU(T&& p)	const	{ return MemberPointerHelpers::Select(p, uFunRRef);	}
		R	CallC(T&& p)		const	{ return MemberPointerHelpers::Select(p, cFun);		}
		R	CallCpyC(T&& p)		const	{ return MemberPointerHelpers::Select(p, cFunRef);	}
		R	CallL(T&& p)		const	{ return MemberPointerHelpers::Select(forward<T>(p), lFun);	}
		R	CallCpyL(T&& p)		const	{ return MemberPointerHelpers::Select(forward<T>(p), lFunRef);	}
		R	CallCL(T&& p)		const	{ return MemberPointerHelpers::Select(p, clFun);	}
		R	CallCpyCL(T&& p)	const	{ return MemberPointerHelpers::Select(p, clFunRef);	}
		R	CallR(T&& p)		const	{ return MemberPointerHelpers::Select(forward<T>(p), rFun);	 }
		R	CallMaterR(T&& p)	const	{ return MemberPointerHelpers::Select(forward<T>(p), rFunRRef); }

		R	CallFreeT(T&& p)		const	{ return tFreeFun(forward<T>(p)); }
		R	CallFreeV(T&& p)		const	{ return vFreeFun(forward<T>(p)); }
		R	CallFreeC(T&& p)		const	{ return cFreeFun(p);	 }
		R	CallMaterFreeC(T&& p)	const	{ return cFreeFunRef(p); }


		// internal helper ctors for when mptr's type has already been decided
		OverloadResolver(R (OD::* mptr)(),			void*)	: uFun { mptr }, wrapper { &OverloadResolver::CallU }	{}
		OverloadResolver(R (OD::* mptr)() const,	void*)	: cFun { mptr }, wrapper { &OverloadResolver::CallC }	{}

		OverloadResolver(R (OD::* mptr)() &,		void*)	: lFun  { mptr }, wrapper { &OverloadResolver::CallL }	{}
		OverloadResolver(R (OD::* mptr)() &&,		void*)	: rFun  { mptr }, wrapper { &OverloadResolver::CallR }	{}
		OverloadResolver(R (OD::* mptr)() const &,	void*)	: clFun { mptr }, wrapper { &OverloadResolver::CallCL }	{}


	public:
		/// Apply the projection.
		R operator ()(T&& obj) const	{ return (this->*wrapper)(forward<T>(obj)); }



		// ---- Const/Mutable overload selection - exact return type ----

		// On a mutable object, either const-qualified or nonqualified overload can be called (no enable_if narrowing),
		// but the nonqualified version should be selected. For that we utilize the language feature:
		// non-template overloads (with better parameter match) can hide templated ones - and that is not an ambiguity!
		using PreferredGetter = conditional_t< is_const<remove_reference_t<O>>::value,
											   R (OD::*)() const,
											   R (OD::*)()		 >;
		constexpr
		OverloadResolver(PreferredGetter mptr)					: OverloadResolver { mptr, nullptr } {}

		template <class OO = O> constexpr
		OverloadResolver(R (IfMutable<OO, OD>::* mptr)())		: uFun { mptr }, wrapper { &OverloadResolver::CallU }	{}

		template <class OO = O> constexpr						// template only to be unpreferred
		OverloadResolver(R (IfMutable<OO, OD>::* mptr)() const)	: cFun { mptr }, wrapper { &OverloadResolver::CallC }	{}



		// ---- Ref-qualified overload selection - exact return type ----

		using PreferredRefQualGetter = conditional_t<is_const<remove_reference_t<O>>::value, R (OD::*)() const &,
									   conditional_t<is_lvalue_reference<O>::value,			 R (OD::*)() &,
									   														 R (OD::*)() &&		>>;
		constexpr
		OverloadResolver(PreferredRefQualGetter mptr)			  : OverloadResolver { mptr, nullptr } {}

		template <class OO = O> constexpr
		OverloadResolver(R (IfMutLVal<OO, OD>::* mptr)() &)		  : lFun { mptr }, wrapper { &OverloadResolver::CallL }		{}

		template <class OO = O> constexpr						  // template only to be unpreferred
		OverloadResolver(R (IfMutable<OO, OD>::* mptr)() const &) : clFun { mptr }, wrapper { &OverloadResolver::CallCL }	{}

		template <class OO = O> constexpr
		OverloadResolver(R (IfMutRVal<OO, OD>::* mptr)() &&)	  : rFun { mptr }, wrapper { &OverloadResolver::CallR }		{}



		// ---- Member overload selection for materielizing the result ----

		template <class OO = O, class RR = R> constexpr
		OverloadResolver(const IfPRValue<RR>& (IfConst<OO, OD>::* mptr)() const)	: cFunRef { mptr },  wrapper { &OverloadResolver::CallCpyC }	{}

		template <class OO = O, class RR = R> constexpr
		OverloadResolver(const IfPRValue<RR>& (IfConst<OO, OD>::* mptr)() const &)	: clFunRef { mptr }, wrapper { &OverloadResolver::CallCpyCL }	{}

		template <class OO = O, class RR = R> constexpr
		OverloadResolver(IfPRValue<RR>& (IfMutLVal<OO, OD>::* mptr)() &)			: lFunRef { mptr },  wrapper { &OverloadResolver::CallCpyL }	{}

		template <class OO = O, class RR = R> constexpr
		OverloadResolver(IfPRValue<RR>& (IfMutable<OO, OD>::* mptr)())				: uFunRef { mptr },  wrapper { &OverloadResolver::CallCpyU }	{}

		template <class OO = O, class RR = R> constexpr
		OverloadResolver(IfPRValue<RR>&& (IfMutable<OO, OD>::* mptr)())				: uFunRRef { mptr }, wrapper { &OverloadResolver::CallMaterU }	{}

		template <class OO = O, class RR = R> constexpr
		OverloadResolver(IfPRValue<RR>&& (IfMutRVal<OO, OD>::* mptr)() &&)			: rFunRRef { mptr }, wrapper { &OverloadResolver::CallMaterR }	{}



		// ---- Free function selection ----

		constexpr OverloadResolver(R (*f)(T&&))				: tFreeFun { f }, wrapper { &OverloadResolver::CallFreeT }	{}
		constexpr OverloadResolver(R (*f)(DT))				: vFreeFun { f }, wrapper { &OverloadResolver::CallFreeV }	{}

		template <class TT = T> constexpr
		OverloadResolver(R (*f)(const IfValueOrMutable<TT, DT>&))	: cFreeFun { f }, wrapper { &OverloadResolver::CallFreeC }	{}

		// ----  Free function selection - materializing the result ----

		template <class RR = R> constexpr
		OverloadResolver(const IfPRValue<RR>& (*f)(const DT&))		: cFreeFunRef { f }, wrapper { &OverloadResolver::CallMaterFreeC }	{}
	};



	/// Potential type of a free Predicate function. Conventionally non-scalars are taken by const&.
	/// @remarks
	///		Used as the default type for the template argument of a parameter expecting a predicate lambda
	///		provides a more limited, but overhead-free and syntactically light alternative to OverloadResolver.
	///		Assumptions:
	///		- Predicate member-functions are typically just const, and should not have an overload set.
	///		- Free functions are often overloaded by type, but conventionally take either const& or decayed value depending on size.
	///		This way the usual cases are covered for free.
	template <class TDecayed>
	using FreePredicatePtr = conditional_t< is_scalar<TDecayed>::value,
											bool (*)(TDecayed),
											bool (*)(const TDecayed&) >;



	namespace LambdaCreators
	{
		// ==== Determine Safe + efficient parameter types to accept an element ===============

		// * don't copy prvalue to parameter
		// * but consider it movable ("Map" is the final call on it)
		template <class T>
		using MapParamT = conditional_t< is_reference<T>::value || is_pointer<T>::value,
										 T,
										 T&& >;

		// helper to std::forward some MapParamT
		template <class T>
		using MapForwardT = conditional_t< is_rvalue_reference<MapParamT<T>>::value,
										   remove_reference_t<T>,
										   T& >;

		// * don't copy, don't move
		// * const access only - even to pointee (for Predicates)
		template <class T>
		using ConstParamT = conditional_t< is_pointer<remove_reference_t<T>>::value,
										   const remove_pointer_t<remove_reference_t<T>>*,
										   const remove_reference_t<T>& >;



		// ==== Determine Safe return types ==================================================

		// Avoid returning xvalues with automatic deduction -> materialize them as prvalues.
		// NOTE: && can be forced explicitly, but general behaviour of Enumerable<T&&> is currently unspecified.
		template <class T>
		using NonExpiringT = conditional_t<is_rvalue_reference<T>::value, remove_reference_t<T>, T>;

		// correct targeted member type so that its lifetime is ensured after return
		template <class O, class M>
		using NonExpiringMemberT = conditional_t< is_lvalue_reference<O>::value || is_pointer<O>::value,
												  NonExpiringT<M>,
												  BaseT<M> >;



		// ==== SFINAE helpers ===============================================================

		// L is not any specially treated case (roughly)
		template <class L>
		using IfPotentialLambda = enable_if_t< !is_member_pointer<L>::value &&
											   !is_pointer<L>::value &&
											   !std::is_function<L>::value >;

		// F is potentially a free function pointer
		template <class F>
		using IfPotentialFreeFunc = enable_if_t<is_pointer<F>::value || std::is_function<F>::value>;

		// M is a member object or function pointer
		template <class M>
		using IfMemberPointer = enable_if_t<is_member_pointer<M>::value>;



		// ==== Return type conversion helper ================================================

		// NOTE: This feature was part of multiple Enumerators (as they can define separate
		//		 return type anyway), but it's so common, that seemed better to extract.

		// In contrast to another lambda, this may preserve the original's id, in case it's relevant
		template <class L, class R>
		struct ReturnConverter {
			L lambda;

			// variadic: extended for binary ops (or anything more)
			template <class... P>
			R operator ()(P&&... in)		{ return lambda(forward<P>(in)...); }

			template <class... P>
			R operator ()(P&&... in) const	{ return lambda(forward<P>(in)...); }
		};


		template <class DeducedRes, class Trg, class L>
		auto&&	WrapIfConversionReqd(L&& lambda, enable_if_t<is_same<DeducedRes, Trg>::value>* = nullptr)
		{
			return forward<L>(lambda);
		}


		template <class DeducedRes, class Trg, class L>
		auto	WrapIfConversionReqd(L&& lambda, enable_if_t<!is_same<DeducedRes, Trg>::value>* = nullptr)
		{
			static_assert (!is_reference<Trg>::value || HasConstValue<Trg> || !HasConstValue<DeducedRes>,
						   "Requested result type loses const qualifier.");
			static_assert (is_convertible<DeducedRes, Trg>::value,
						   "Given function has incompatible return type.");
			static_assert (!is_void<DeducedRes>::value,
						   "HINT: Return type deduced to void. Can happen with unbound template-parameters for a function.");
			static_assert (is_reference<DeducedRes>::value && !is_rvalue_reference<DeducedRes>::value || !is_reference<Trg>::value,
						   "Function returns r-value, expected reference would become dangling!");

			return ReturnConverter<decay_t<L>, Trg> { forward<L>(lambda) };
		}



		// ==== Map via arbitrary function ===================================================

		template <class T, class R = void, class L>
		decltype(auto) CustomMapper(L&& lambda, IfPotentialLambda<remove_reference_t<L>>* = nullptr)
		{
			// will call a copy inside an Enumerator ==> should not find "&&" overload; but no constness required
			using AppliedL = decay_t<L>;
			static_assert (IsCallable<AppliedL, T>::value, "This method expects a unary mapper function or selector for: TElem -> TMapped."
														   " Check lambda's parameter type including constness!"						   );

			using DeducedRes = decltype(declval<AppliedL>().operator()(declval<T>()));
			using TargetRes	 = OverrideT<R, NonExpiringT<DeducedRes>>;

			return WrapIfConversionReqd<DeducedRes, TargetRes>(forward<L>(lambda));
		}


		template <class T, class R = void, class Mptr>
		auto CustomMapper(Mptr p, IfMemberPointer<Mptr>* = nullptr)
		{
			return CustomMapper<T, R>(
				[p](MapParamT<T> elem) -> decltype(auto)
				{
					return MemberPointerHelpers::Select(forward<MapForwardT<T>>(elem), p);
				}
			);
		}


		template <class T, class R = void, class Fptr = None>
		auto CustomMapper(Fptr p, IfPotentialFreeFunc<Fptr>* = nullptr)
		{
			return CustomMapper<T, R>(
				[p](MapParamT<T> elem) -> decltype(auto) { return (*p)(forward<MapForwardT<T>>(elem)); }
			);
		}



		// ==== Map as Projection - ensuring member lifetime =================================

		template <class T, class R = void, class L>
		decltype(auto) Selector(L&& lambda, IfPotentialLambda<remove_reference_t<L>>* = nullptr)
		{
			using AppliedL = decay_t<L>;
			static_assert (IsCallable<AppliedL, T>::value, "This method expects a unary projection function or selector for: TElem -> TSubobj."
														   " Check lambda's parameter type including constness!"							   );

			using DeducedRes	= decltype(declval<AppliedL>().operator()(declval<T>()));
			using CorrectedRes	= NonExpiringMemberT<T, DeducedRes>;
			using TargetRes		= OverrideT<R, CorrectedRes>;

			static_assert (is_lvalue_reference<T>::value || is_pointer<T>::value || !is_reference<TargetRes>::value,
						   "Selected member cannot be a reference if input object is an r-value!");

			return WrapIfConversionReqd<DeducedRes, TargetRes>(forward<L>(lambda));
		}


		template <class T, class R = void, class Mptr>
		auto Selector(Mptr p, IfMemberPointer<Mptr>* = nullptr)
		{
			using DeducedRes	= SelectedMemberT<T, Mptr>;
			using CorrectedRes	= NonExpiringMemberT<T, DeducedRes>;
			using TargetRes		= OverrideT<R, CorrectedRes>;

			return Selector<T, TargetRes>(
				[p](MapParamT<T> elem) -> TargetRes { return MemberPointerHelpers::Select(forward<MapForwardT<T>>(elem), p); }
			);
		}


		template <class T, class R = void, class Fptr>
		auto Selector(Fptr p, IfPotentialFreeFunc<Fptr>* = nullptr)
		{
			using DeducedRes	= decltype((*p)(forward<MapForwardT<T>>(declval<T>())) );
			using CorrectedRes	= NonExpiringMemberT<T, DeducedRes>;
			using TargetRes		= OverrideT<R, CorrectedRes>;

			return Selector<T, TargetRes>(
				[p](MapParamT<T> elem) -> TargetRes { return (*p)(forward<MapForwardT<T>>(elem)); }
			);
		}



		// ==== Map to bool (Predicate) ======================================================

		template <class T, class L>
		decltype(auto) Predicate(L&& lambda, IfPotentialLambda<remove_reference_t<L>>* = nullptr)
		{
			// will call a copy inside an Enumerator ==> should not find && overload; but no constness required
			using AppliedL = decay_t<L>;

			// CONSIDER: Enforce constness of T? Currently idempotence of lambda is user responsibility.
			static_assert (IsCallable<AppliedL, T>::value, "This method expects a unary predicate function or selector for: TElem -> bool."
														   " Check lambda's parameter type including constness!"						   );

			using DeducedRes = decltype(declval<AppliedL>().operator()(declval<T>()));

			static_assert (is_convertible<DeducedRes, bool>::value, "The predicate function must evaluate to bool!");

			return WrapIfConversionReqd<DeducedRes, bool>(forward<L>(lambda));
		}


		template <class T, class Mptr>
		auto Predicate(Mptr p, IfMemberPointer<Mptr>* = nullptr)
		{
			return Predicate<T>(
				[p](ConstParamT<T> elem) { return MemberPointerHelpers::Select(elem, p); }
			);
		}


		template <class T, class Fptr>
		auto Predicate(Fptr p, IfPotentialFreeFunc<Fptr>* = nullptr)
		{
			return Predicate<T>(
				[p](ConstParamT<T> elem) { return (*p)(elem); }
			);
		}
	}

#pragma endregion




#pragma region Unified Binary Ops

	namespace MemberPointerHelpers
	{
		template <class T, class BinOp, class R>
		decltype(auto) ApplyBinop(T*  obj, BinOp op, R&& r, IfMemberFunction<BinOp>* = nullptr)	{ return (obj->*op)(forward<R>(r)); }

		template <class T, class BinOp, class R>
		decltype(auto) ApplyBinop(T&& obj, BinOp op, R&& r, IfMemberFunction<BinOp>* = nullptr)	{ return (forward<T>(obj).*op)(forward<R>(r)); }
	}

	/// Result of applying a member pointer to an object of T with a single parameter of Rh.
	template <class T, class Mptr, class Rh>
	using AppliedMemberT = decltype(MemberPointerHelpers::ApplyBinop(declval<T>(), declval<Mptr>(), declval<Rh>()));



	namespace LambdaCreators
	{
		// ==== Map via binary function ======================================================

		template <class T1, class T2, class R = void, class L>
		decltype(auto) BinaryMapper(L&& lambda, IfPotentialLambda<remove_reference_t<L>>* = nullptr)
		{
			using AppliedL = decay_t<L>;
			static_assert (IsCallable<AppliedL, T1, T2>::value, "This method expects a binary operation: (TValue1, TValue2) -> TMapped."
																" Check lambda's parameter type including constness!"					);

			using DeducedRes = decltype(declval<AppliedL>().operator()(declval<T1>(), declval<T2>()));
			using TargetRes	 = OverrideT<R, NonExpiringT<DeducedRes>>;

			return WrapIfConversionReqd<DeducedRes, TargetRes>(forward<L>(lambda));
		}


		template <class T1, class T2, class R = void, class Mptr>
		auto BinaryMapper(Mptr p, IfMemberPointer<Mptr>* = nullptr)
		{
			using FW1 = MapForwardT<T1>;
			using FW2 = MapForwardT<T2>;
			return BinaryMapper<T1, T2, R>(
				[p](MapParamT<T1> lhs, MapParamT<T2> rhs) -> decltype(auto)
				{
					return MemberPointerHelpers::ApplyBinop(forward<FW1>(lhs), p, forward<FW2>(rhs));
				}
			);
		}


		template <class T1, class T2, class R = void, class Fptr = None>
		auto BinaryMapper(Fptr p, IfPotentialFreeFunc<Fptr>* = nullptr)
		{
			using FW1 = MapForwardT<T1>;
			using FW2 = MapForwardT<T2>;
			return BinaryMapper<T1, T2, R>(
				[p](MapParamT<T1> lhs, MapParamT<T2> rhs) -> decltype(auto) { return (*p)(forward<FW1>(lhs), forward<FW2>(rhs)); }
			);
		}
	}


	// CONSIDER: No overload resolution for binary ops yet.

#pragma endregion




#pragma region Generalized Storage

	template <class T>
	using ReassignableStorageFor = conditional_t< IsHeadAssignable<StorableT<T>, StorableT<T>>,
													GenericStorage<T, UnionHolder>,
													GenericStorage<T, BytesHolder>			   >;


	/// Ensure assignment capability - even for an immutable class.
	template <class T>
	class Reassignable final : private ReassignableStorageFor<T> {
		using Storage = ReassignableStorageFor<T>;

	public:
		using Storage::Storage;		// ctor - "ReassignableStorageFor" had fun compiler differences around injected class name :D

		Reassignable() = delete;

		template <class TT = T, class = enable_if_t<IsBraceConstructible<T, TT>::value>>
		Reassignable(TT&& val) : Storage { ForwardParams, forward<TT>(val) }
		{
		}
		Reassignable(const Reassignable& src)	{ this->CopyFrom(src); }
		Reassignable(Reassignable&& src)		{ this->MoveFrom(src); }

		~Reassignable()							{ this->Destroy(); }

		using Storage::Value;
		using Storage::PassValue;
		using Storage::operator *;
		using Storage::operator ->;
		using Storage::operator T&;


		template <class S>
		T& operator =(S&& src)					{ return this->Reassign(forward<S>(src));  }

		T& operator =(Reassignable&& src)		{ return this->operator=(src.PassValue()); }
		T& operator =(const Reassignable& src)	{ return this->operator=(src.Value());	   }


		template <class Func>
		void AcceptRvo(Func&& creator)
		{
			this->Destroy();
			this->InvokeFactory(creator);
		}

		template <class... Args>
		void Reconstruct(Args&&... args)
		{
			this->Destroy();
			this->Construct(forward<Args>(args)...);
		}


		// allow generic code to move (without triggering dangling assignment checks inside)
		template <class TT = T>
		void AssignMoved(IfReference<TT> src)	{ Reconstruct(src); }

		template <class TT = T>
		void AssignMoved(IfPRValue<TT>& src)	{ Reconstruct(move(src)); }


		// shorthand specifically for enumerators (being an internal helper)
		template <class Et>
		void AssignCurrent(Et&& enumerator)
		{
			this->AcceptRvo([&enumerator]() -> decltype(enumerator.Current()) { return enumerator.Current(); });
		}
	};




	/// Value with deferred, possibly repeated initialization.
	template <class T>
	class Deferred final : private ReassignableStorageFor<T> {
		bool initialized = false;

	public:
		bool IsInitialized() const	{ return initialized; }

		Deferred() = default;

		Deferred(const Deferred& src) : initialized { src.initialized }
		{
			if (initialized)	this->CopyFrom(src);
		}

		Deferred(Deferred&& src)	  : initialized { src.initialized }
		{
			if (initialized)	this->MoveFrom(src);
		}

		~Deferred()
		{
			if (initialized)	this->Destroy();
		}


		// no asserts, not a public type
		using ReassignableStorageFor<T>::Value;
		using ReassignableStorageFor<T>::PassValue;
		using ReassignableStorageFor<T>::operator *;
		using ReassignableStorageFor<T>::operator ->;
		using ReassignableStorageFor<T>::operator T&;


		template <class S>
		T& operator =(S&& src)
		{
			if (initialized)	this->Reassign(forward<S>(src));
			else				this->Construct(forward<S>(src));

			initialized = true;
			return Value();
		}

		T& operator =(Deferred&& src)		{ return this->operator=(src.PassValue()); }
		T& operator =(const Deferred& src)	{ return this->operator=(src.Value());	   }

		template <class Func>
		void AcceptRvo(Func&& creator)
		{
			if (initialized)	this->Destroy();

			this->InvokeFactory(creator);
			initialized = true;
		}

		template <class... Args>
		void Reconstruct(Args&&... args)
		{
			if (initialized)	this->Destroy();

			this->Construct(forward<Args>(args)...);
			initialized = true;
		}


		// allow generic code to move (without triggering dangling assignment checks inside)
		template <class TT = T>
		void AssignMoved(IfReference<TT> src)	{ Reconstruct(src); }

		template <class TT = T>
		void AssignMoved(IfPRValue<TT>& src)	{ Reconstruct(move(src)); }


		// shorthand specifically for enumerators (Deferred being an internal helper)
		template <class Et>
		void AssignCurrent(Et&& enumerator)
		{
			this->AcceptRvo([&enumerator]() -> decltype(enumerator.Current()) { return enumerator.Current(); });
		}
	};

#pragma endregion


}		// namespace TypeHelpers
}		// namespace Enumerables

#endif	// ENUMERABLES_TYPEHELPERS_HPP
