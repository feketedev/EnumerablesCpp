#ifndef ENUMERABLES_GENERICSTORAGE_HPP
#define ENUMERABLES_GENERICSTORAGE_HPP

	/*  Part of Enumerables for C++.						  *
	 *														  *
	 *  This file contains utilities to enable types lacking  *
	 *  certain capabilities to be managed in a "normal" way  *
	 *  - thus facilitate writing generic code.				  *
	 *  													  *
	 *  Particularly: 										  *
	 *  	- store references (in containers/unions)		  *
	 *  	- overwrite immutable objects as a whole		  *
	 *  	- emplace uncopiable (or even unmovable) objects  */


#include "Enumerables_TypeHelperBasics.hpp"
#include <new>



namespace Enumerables::TypeHelpers {


#pragma region StorableT

	/// Holds a pointer bypassing a reference for unified storage/access in generic code.
	/// Convertible to and std::hashable as the referred lvalue.
	/// @remarks
	///		The pointer is semantically const.
	///		- RefHolder must be assignable (swappable) as a whole for container algorithms.
	///		- Any other assignments are forbidden, for being rather ambiguous!
	///		- Otherwise the conversion to T& and comparisons are provided.
	template <class T>
	class RefHolder {
		static_assert (!is_reference<T>(), "Use with (qualified) decayed type.");

		T* /*const*/ ptr;

	public:
		RefHolder(T& ref) noexcept : ptr { &ref }
		{
		}

		RefHolder(const RefHolder&)				= default;
		RefHolder& operator =(const RefHolder&)	= default;		// OK to swap refs by algorithms
		RefHolder& operator =(const T&)			= delete;		// would be ambiguous!


		T&		 Get() const noexcept	{ return *ptr; }
		operator T&()  const noexcept	{ return *ptr; }


		// let's have full transparency with equality-checks
		template <class RH = T>
		bool operator ==(const RH& rhs)				const	 noexcept(noexcept(Get() == rhs))		{ return Get() == rhs; }

		template <class RH = T>
		bool operator !=(const RH& rhs)				const	 noexcept(noexcept(Get() != rhs))		{ return Get() != rhs; }

		template <class RT>
		bool operator ==(const RefHolder<RT>& rhs)	const	 noexcept(noexcept(Get() == rhs.Get()))	{ return Get() == rhs.Get(); }

		template <class RT>
		bool operator !=(const RefHolder<RT>& rhs)	const	 noexcept(noexcept(Get() != rhs.Get()))	{ return Get() != rhs.Get(); }


		// also support default ordering -> be usable in tree-sets
		template <class RH = T>
		bool operator <(const RH& rhs)				const	 noexcept(noexcept(Get() < rhs))		{ return Get() < rhs; }

		template <class RT>
		bool operator <(const RefHolder<RT>& rhs)	const	 noexcept(noexcept(Get() < rhs.Get()))	{ return Get() < rhs.Get(); }
	};

	template <class T, class LH = T>
	bool operator ==(const LH& lhs, const RefHolder<T>& ref) noexcept(noexcept(lhs == ref.Get()))	{ return lhs == ref.Get(); }

	template <class T, class LH = T>
	bool operator !=(const LH& lhs, const RefHolder<T>& ref) noexcept(noexcept(lhs != ref.Get()))	{ return lhs != ref.Get(); }

	template <class T, class LH = T>
	bool operator <(const LH& lhs, const RefHolder<T>& ref)	 noexcept(noexcept(lhs < ref.Get()))	{ return lhs < ref.Get(); }



	/// Trait to restore a RefHolder's pointed type. [Expects non-ref, non-volatile RefHolder.]
	template <class T>
	struct RestoredRef						{ using type = T;  };
	template <class T>
	struct RestoredRef<RefHolder<T>>		{ using type = T&; };
	template <class T>
	struct RestoredRef<const RefHolder<T>>	{ using type = T&; };



	/// Temporary storage for arbitrary T to be stored in a container/union by generic code - supports (l-value) references and values.
	/// @remarks
	///	  The input element, as a source Enumerator's return value can be:
	///	 	* lvalue ref  -> its address is available, and we assume it is sustained during the entire enumeration (constness preserved)
	///	 	* prvalue	  -> temporary / mapped result, must be stored if needed later (T left as is, no overhead)
	///	 	* xvalue (&&) -> to be avoided in general; still, decaying it makes most sense, as it probably won't survive the next Fetch
	template <class T>
	using StorableT = conditional_t< is_lvalue_reference_v<T>,
										RefHolder<remove_reference_t<T>>,
										remove_reference_t<T>			 >;


	/// Elem type restorable from a temporary container - i.e. resolve StorableT.
	/// [ignores ref, strips it from an unwrapped T&]
	template <class T>
	using RestorableT = typename RestoredRef<remove_reference_t<T>>::type;


	/// Access stored instance as lvalue
	template <class V>	V&			Revive(const RefHolder<V>& stored)		noexcept  { return stored.Get(); }
	template <class V>	V&			Revive(RefHolder<V>& stored)			noexcept  { return stored.Get(); }
	template <class V>	V&			Revive(V& stored)						noexcept  { return stored; }

	/// Access stored instance forcing constness even on referenced object
	template <class V>	const V&	ReviveConst(const RefHolder<V>& stored)	noexcept  { return stored.Get(); }
	template <class V>	const V&	ReviveConst(RefHolder<V>& stored)		noexcept  { return stored.Get(); }
	template <class V>  const V&	ReviveConst(const V& stored)			noexcept  { return stored; }

	/// Get final access to the stored entity, if possible as an rvalue
	template <class V>	V&			PassRevived(RefHolder<V>& stored)		noexcept  { return stored.Get(); }
	template <class V>	V&&			PassRevived(V& stored)					noexcept  { return move(stored); }

#pragma endregion




#pragma region Emplacer

	// Constructor selectors
	enum FactoryInvokeSelector { InvokeFactory };
	enum ForcedBracesSelector  { ConstructBraced };


	// Optimization to accept function results - potentially avoid requiring move ctor from C++17
	template <class Factory>
	struct RvoEmplacer {
		using R = InvokeResultT<Factory>;

		static_assert (!is_reference<R>(), "Expecting a prvalue from factory.");

		R	obj;
		R*	GetPtr() noexcept	{ return &obj; }

		RvoEmplacer(Factory& fact) noexcept(noexcept(fact())) : obj { fact() }
		{
			static_assert (sizeof(RvoEmplacer) == sizeof(R),   "Size mismatch??");
			static_assert (alignof(RvoEmplacer) == alignof(R), "Alignment mismatch??");
		}
	};



	/// Helper to constract T in-place in the selected manner, including accepting function results
	/// with possible RVO - potentially avoid requiring move ctor from C++17
	template <class T>
	struct Emplacer {
		static_assert (!is_reference<T>(), "Only for prvalues.");

		T	obj;

		template <class Factory>
		Emplacer(FactoryInvokeSelector, Factory&& create)  noexcept(noexcept(create()))
			: obj { create() }
		{
			static_assert (is_same<decltype(create()), T>(), "Factory returns mismatching type.");
		}

		template <class... Args>
		Emplacer(ForcedBracesSelector, Args&&... args)  noexcept(IsNothrowBraceConstructible<T, Args...>::value)
			: obj { forward<Args>(args)... }
		{
		}

		template <class... Args>
		Emplacer(Args&&... args)  noexcept(is_nothrow_constructible_v<T, Args...>)
			: obj(forward<Args>(args)...)
		{
		}
	};

	template <class T>
	using EmplacableT = conditional_t< is_lvalue_reference_v<T>,	RefHolder<remove_reference_t<T>>,
																	Emplacer<remove_reference_t<T>>	 >;

	// Provide access homologous to RefHolder:

	template <class V>	const V&	Revive(const Emplacer<V>& stored)		noexcept  { return stored.obj; }
	template <class V>	V&			Revive(Emplacer<V>& stored)				noexcept  { return stored.obj; }

	template <class V>	const V&	ReviveConst(const Emplacer<V>& stored)	noexcept  { return stored.obj; }
	template <class V>	const V&	ReviveConst(Emplacer<V>& stored)		noexcept  { return stored.obj; }

	template <class V>	V&&			PassRevived(Emplacer<V>& stored)		noexcept  { return move(stored.obj); }

#pragma endregion




#pragma region GenericStorage

	/// Generalized temporary storage for potentially any type (refs/immutables included).
	/// Defines all supposable operations, but leaves their management (even lifetime handling!) to the user/inheritor.
	template <class T>
	class GenericStorage {
		using Emp = EmplacableT<T>;

		union { Emp val; };

		Emp&		Storage()				{ return *std::launder(&val); }
		const Emp&	Storage()		const	{ return *std::launder(&val); }

	public:
		// NOTE: Old clang crashes on auto&/auto* return types, hence need to augment it.
		using Ptr      = remove_reference_t<T>*;
		using ConstPtr = const remove_reference_t<T>*;


		// ---- Access ----

		const T&	Value()			const&	noexcept  { return Revive(Storage()); }
		T&			Value()				 &	noexcept  { return Revive(Storage()); }
		T&&			Value()				&&	noexcept  { return PassRevived(Storage()); }
		T&&			PassValue()				noexcept  { return PassRevived(Storage()); }

		const T&	operator *()	const&	noexcept  { return Value(); }
		T&			operator *()		 &	noexcept  { return Value(); }
		T&&			operator *()		&&	noexcept  { return PassValue(); }

		// &-collapsing suffice for Value(), but pointers need auto*  --> or explicit help
		Ptr			operator ->()			noexcept  { return &Value(); }
		ConstPtr	operator ->()	const	noexcept  { return &Value(); }

		operator	const T&()		const&	noexcept  { return Value(); }
		operator	T&()				 &	noexcept  { return Value(); }
		operator	T&&()				&&	noexcept  { return PassValue(); }



		// ---- Outsourced lifetime management (!) ----

		GenericStorage()	{}
		~GenericStorage()	{}

		void Destroy()  noexcept(is_nothrow_destructible_v<T>)
		{
			Storage().~Emp();
		}


		// copy/move manually if appropriate!
		GenericStorage(const GenericStorage& src) = delete;

		/// if only @p src is initialized!
		void MoveFrom(GenericStorage& src)		  noexcept(is_nothrow_move_constructible_v<T>)
		{
			new (&val) Emp { src.PassValue() };
		}

		/// if only @p src is initialized!
		void CopyFrom(const GenericStorage& src)  noexcept(is_nothrow_copy_constructible_v<T>)
		{
			new (&val) Emp { src.Value() };
		}


		// ---- Construction/assignment ops ----

		template <class... Args>
		enable_if_t<!is_reference_v<AsDependentT<T, Args...>>>		// guard needed against RefHolder
		ConstructBraced(Args&&... ctorArgs)  noexcept(IsNothrowBraceConstructible<T, Args...>::value)
		{
			new (&val) Emp { TypeHelpers::ConstructBraced, forward<Args>(ctorArgs)... };
		}

		template <class Trg>
		enable_if_t<is_reference_v<AsDependentT<T, Trg>>>			// if RefHolder
		ConstructBraced(Trg&& referred)  noexcept
		{
			new (&val) Emp { forward<Trg>(referred) };
		}


		template <class... Args>
		void ConstructParens(Args&&... ctorArgs)  noexcept(is_nothrow_constructible_v<T, Args...>)
		{
			new (&val) Emp { forward<Args>(ctorArgs)... };
		}


		template <class... Args>
		enable_if_t<is_constructible_v<T, Args...>>
		ConstructParensPreferred(Args&&... ctorArgs)  noexcept(is_nothrow_constructible_v<T, Args...>)
		{
			new (&val) Emp { forward<Args>(ctorArgs)... };
		}

		template <class... Args>
		enable_if_t<IsBraceConstructible<T, Args...>::value && !is_constructible_v<T, Args...>>
		ConstructParensPreferred(Args&&... ctorArgs)  noexcept(IsNothrowBraceConstructible<T, Args...>::value)
		{
			new (&val) Emp { TypeHelpers::ConstructBraced, forward<Args>(ctorArgs)... };
		}


		// Note: When expecting conversions, use Construct! That will need a temporary anyway.
		template <class Factory>
		void InvokeFactory(Factory&& create)  noexcept(noexcept(create()))
		{
			static_assert (is_same<T, decltype(create())>(), "To allow conversions, use Construct*() directly");

			if constexpr (is_same_v<Emp, Emplacer<decltype(create())>>) {
				new (&val) Emp { TypeHelpers::InvokeFactory, create };
			}
			else {
				new (&val) Emp { create() };

				static_assert (is_reference<T>());		// currently expecting RefHolder only
			}
		}


		// Check with optimization: constexpr for unmatching types
		template <class Src>
		constexpr bool IsNotSelf(const Src&)   const noexcept	{ return true; }
		bool		   IsNotSelf(const T& src) const noexcept	{ return &src != &Value(); }


		/// Assign or Reconstruct depending on type capability.
		/// Only if already initialized!
		template <class Src>
		T& Reassign(Src&& src)
		noexcept(IsHeadAssignable<T, Src> ? is_nothrow_assignable_v<T&, Src> : IsNothrowReconstructible<T, Src>)
		{
			if constexpr (IsHeadAssignable<T, Src>) {
				static_assert (!is_reference<T>(), "GenericStorage Internal error.");
				T& v = Value();
				v    = forward<Src>(src);
				return v;
			}
			else {
				if (IsNotSelf(src)) {
					Destroy();
					ConstructParens(forward<Src>(src));
				}
				return Value();
			}
		}
	};

#pragma endregion


}		// namespace Enumerables::TypeHelpers





namespace std {


#pragma region StorableT

	template<class T>
	struct hash<Enumerables::TypeHelpers::RefHolder<T>> : hash<remove_const_t<T>>	// inherit disabledness
	{
		// SFINAE: don't define when disabled for referred type
		template<class TT = T, enable_if_t<is_same_v<TT, T>, int> = 0>
		size_t operator ()(const Enumerables::TypeHelpers::RefHolder<TT>& ref) const
		noexcept(noexcept(hash<remove_const_t<T>>::operator()(ref.Get())))
		{
			return hash<remove_const_t<T>>::operator()(ref.Get());
		}
	};

#pragma endregion


}		// namespace std


#endif	// ENUMERABLES_GENERICSTORAGE_HPP
