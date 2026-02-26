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



namespace Enumerables {
namespace TypeHelpers {


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
		static_assert (!is_reference<T>::value, "Use with (qualified) decayed type.");

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
	using StorableT = conditional_t< is_lvalue_reference<T>::value,
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

		static_assert (!is_reference<R>::value, "Expecting a prvalue from factory.");

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
		static_assert (!is_reference<T>::value, "Only for prvalues.");

		T	obj;

		template <class Factory>
		Emplacer(FactoryInvokeSelector, Factory&& create) noexcept(noexcept(create()))
			: obj { create() }
		{
			static_assert (is_same<decltype(create()), T>::value, "Factory returns mismatching type.");
		}

		template <class... Args>
		Emplacer(ForcedBracesSelector, Args&&... args) noexcept(noexcept(T { forward<Args>(args)... }))
			: obj { forward<Args>(args)... }
		{
		}

		template <class... Args>
		Emplacer(Args&&... args) noexcept(noexcept(T (forward<Args>(args)...)))
			: obj(forward<Args>(args)...)
		{
		}
	};

	template <class T>
	using EmplacableT = conditional_t< is_lvalue_reference<T>::value,	RefHolder<remove_reference_t<T>>,
																		Emplacer<remove_reference_t<T>>	 >;

	// Provide access homologous to RefHolder:

	template <class V>	const V&	Revive(const Emplacer<V>& stored)		noexcept  { return stored.obj; }
	template <class V>	V&			Revive(Emplacer<V>& stored)				noexcept  { return stored.obj; }

	template <class V>	const V&	ReviveConst(const Emplacer<V>& stored)	noexcept  { return stored.obj; }
	template <class V>	const V&	ReviveConst(Emplacer<V>& stored)		noexcept  { return stored.obj; }

	template <class V>	V&&			PassRevived(Emplacer<V>& stored)		noexcept  { return move(stored.obj); }

#pragma endregion




#pragma region GenericStorage

	template <class>	class BytesStorage;
	template <class>	class UntrackedUnionStorage;
	template <class>	class TrackedUnionStorage;


	/// Generalized temporary storage for potentially any type (refs/immutables included).
	/// Defines all supposable operations, but leaves their management - including lifetime handling - to the user/inheritor.
	/// @tparam SupportReconstruct:  in-place reconstruction support (with possibly const members) is needed
	/// @tparam TrackLiveness:		 even when storing a pointer is not required by standard, store a bool to track whether T is available
	/// @remarks
	///		The realizations changable based on SupportReconstruct are to avoid char[] casting + extra pointer when possible.
	///		Optional tracking is added to avoid needing an extra bool when the extra pointer can inform us about the same.
	///		C++17 basically renders both of these obsolete with std::launder.
	template <class T, bool SupportReconstruct = true, bool TrackLiveness = false>
	class GenericStorage {

		using Emp   = EmplacableT<T>;

		// NOTE: for a P1971R0 compliant compiler, this ceremony and can be omitted. It is safe to just use UnionStorage, no bool param.
		static constexpr bool ResortToBytes = SupportReconstruct && !is_reference<T>::value && !is_scalar<T>::value;
		using Store = conditional_t<ResortToBytes, BytesStorage<Emp>,
					  conditional_t<TrackLiveness, TrackedUnionStorage<Emp>, UntrackedUnionStorage<Emp>>>;

		static_assert (alignof(Store) >= alignof(StorableT<T>), "Missed alignment?");

		Store  storage;

	public:
		// NOTE: Old clang crashes on auto&/auto* return types, hence need to augment it.
		using Ptr      = remove_reference_t<T>*;
		using ConstPtr = const remove_reference_t<T>*;

		static constexpr bool IsConstructTracked = ResortToBytes || TrackLiveness;


		// ---- Access ----

		const T&	Value()			const&	noexcept  { return Revive(storage.Get()); }
		T&			Value()				 &	noexcept  { return Revive(storage.Get()); }
		T&&			Value()				&&	noexcept  { return PassRevived(storage.Get()); }
		T&&			PassValue()				noexcept  { return PassRevived(storage.Get()); }

		const T&	operator *()	const&	noexcept  { return Value(); }
		T&			operator *()		 &	noexcept  { return Value(); }
		T&&			operator *()		&&	noexcept  { return move(*this).Value(); }

		// Note: &-collapsing suffice for Value(), but * needs auto	 -> or explicit help
		Ptr			operator ->()			noexcept  { return &Value(); }
		ConstPtr	operator ->()	const	noexcept  { return &Value(); }

		operator	const T&()		const&	noexcept  { return Value(); }
		operator	T&()				 &	noexcept  { return Value(); }
		operator	T&&()				&&	noexcept  { return move(*this).Value(); }

		template <bool tracked = IsConstructTracked>
		enable_if_t<tracked, bool>  IsConstructed() const noexcept
		{
			return storage.IsConstructed();
		}


		// ---- Outsourced lifetime management (!) ----

		GenericStorage()  = default;
		~GenericStorage() = default;

		void Destroy()  noexcept(std::is_nothrow_destructible<T>::value)
		{
			storage.Destroy();
		}


		// copy/move manually if appropriate!
		GenericStorage(const GenericStorage& src) = delete;

		/// if only @p src is initialized!
		void MoveFrom(GenericStorage& src)		  noexcept(is_nothrow_move_constructible<T>::value)
		{
			storage.Construct(src.PassValue());
		}

		/// if only @p src is initialized!
		void CopyFrom(const GenericStorage& src)  noexcept(is_nothrow_copy_constructible<T>::value)
		{
			storage.Construct(src.Value());
		}


		// ---- Construction/assignment ops ----

		template <class... Args>
		enable_if_t<!is_reference<AsDependentT<T, Args...>>::value>		// guard needed for RefHolder
		ConstructBraced(Args&&... ctorArgs)  noexcept(noexcept(T { forward<Args>(ctorArgs)... }))
		{
			storage.Construct(TypeHelpers::ConstructBraced, forward<Args>(ctorArgs)...);
		}

		template <class Trg>
		enable_if_t<is_reference<AsDependentT<T, Trg>>::value>			// RefHolder
		ConstructBraced(Trg&& referred)  noexcept
		{
			storage.Construct(forward<Trg>(referred)...);
		}


		template <class... Args>
		void ConstructParens(Args&&... ctorArgs)  noexcept(noexcept(T (forward<Args>(ctorArgs)...)))
		{
			storage.Construct(forward<Args>(ctorArgs)...);
		}


		template <class... Args>
		enable_if_t<is_constructible<T, Args...>::value>
		ConstructParensPreferred(Args&&... ctorArgs)  noexcept(noexcept(T (forward<Args>(ctorArgs)...)))
		{
			ConstructParens(forward<Args>(ctorArgs)...);
		}

		template <class... Args>
		enable_if_t<IsBraceConstructible<T, Args...>::value && !is_constructible<T, Args...>::value>
		ConstructParensPreferred(Args&&... ctorArgs)  noexcept(noexcept(T { forward<Args>(ctorArgs)... }))
		{
			ConstructBraced(forward<Args>(ctorArgs)...);
		}


		// Note: When expecting conversions, use Construct! That will need a temporary anyway.
		template <class Factory>
		auto InvokeFactory(Factory&& create)  noexcept(noexcept(create()))
			-> enable_if_t<is_same<Emp, Emplacer<decltype(create())>>::value>
		{
			storage.Construct(TypeHelpers::InvokeFactory, create);
		}

		// for RefHolder<T> or any mismatching prvalue that needs conversion
		template <class Factory, class Trg = T>
		auto InvokeFactory(Factory&& create)  noexcept(noexcept(create()))
			-> enable_if_t<!is_same<Emp, Emplacer<decltype(create())>>::value>
		{
			storage.Construct(create());
		}


		// Check with optimization: constexpr for unmatching types
		template <class Src>
		constexpr bool IsNotSelf(const Src&)   const noexcept	{ return true; }
		bool		   IsNotSelf(const T& src) const noexcept	{ return &src != &Value(); }


		/// Assign or Reconstruct depending on type capability.
		/// Only if already initialized!
		template <class Src>
		T& Reassign(Src&& src, enable_if_t<!IsHeadAssignable<T, Src> && is_constructible<T, Src>::value>* = nullptr)
		noexcept(is_nothrow_constructible<T, Src>::value)
		{
			static_assert (SupportReconstruct || is_reference<T>::value || is_scalar<T>::value,
						   "Stored type does not support this assignment, whilst Reconstruction support is not set!");

			if (IsNotSelf(src)) {
				Destroy();
				ConstructParens(forward<Src>(src));
			}
			return Value();
		}

		template <class Src>
		T& Reassign(Src&& src, enable_if_t<IsHeadAssignable<T, Src>>* = nullptr)
		noexcept(is_nothrow_assignable<T&, Src>::value)
		{
			static_assert (!is_reference<T>::value, "GenericStorage Internal error.");
			T& v = Value();
			v    = forward<Src>(src);
			return v;
		}
	};

#pragma endregion




#pragma region GenericStorage Strategies

	// The conventional way	- from c++17, having laundry, unions suffice.
	template <class T>
	class BytesStorage final {

		alignas(T) char  buffer[sizeof(T)];

		// NOTE: It is actually UB to reinterpret buffer after placement, the resulting ptr must be stored!
		T*  ptr = nullptr;

	public:
		bool		IsConstructed()	const noexcept  { return ptr != nullptr; }
		const T&	Get()			const noexcept  { return *ptr; }
		T&			Get()				  noexcept  { return *ptr; }

		BytesStorage()					  = default;
		BytesStorage(const BytesStorage&) = delete;

		template <class... Args>
		void Construct(Args&&... args)  noexcept(noexcept(T { forward<Args>(args)... }))
		{
			ptr = new (buffer) T { forward<Args>(args)... };
		}

		void Destroy() noexcept(std::is_nothrow_destructible<T>::value)
		{
			T& obj = Get();
			ptr = nullptr;		// lifetime ends by dtor start
			obj.~T();
		}
	};


	// Better for debugger, no casts, no extra pointer - but should not reconstruct a T having const fields without std::launder!
	// NOTE: P1971R0 alleviates restrictions retroactively, however that change dates to 2019.
	// Tracked- / Untracked- versions added to avoid duplicated data (extra bool +padding) in Deferred...
	template <class T>
	class UnionStorage {

		// avoid placing a "complete-const" type
		using NonConst = std::remove_const_t<T>;

		union { NonConst value; };

	public:
		const T&	Get() const	noexcept  { return value; }
		T&			Get()		noexcept  { return value; }

		~UnionStorage() {}			// user responsibility!
		UnionStorage()  noexcept {}

		template <class... Args>
		void Construct(Args&&... args)  noexcept(noexcept(T { forward<Args>(args)... }))
		{
			new (&value) NonConst { forward<Args>(args)... };
		}

		void Destroy() noexcept(std::is_nothrow_destructible<T>::value)
		{
			value.~NonConst();
		}
	};



	template <class T>
	class UntrackedUnionStorage final : public UnionStorage<T> {
	};



	template <class T>
	class TrackedUnionStorage final : public UnionStorage<T> {

		bool isConstructed = false;

	public:
		bool IsConstructed() const noexcept  { return isConstructed; }

		template <class... Args>
		void Construct(Args&&... args)  noexcept(noexcept(T { forward<Args>(args)... }))
		{
			UnionStorage<T>::Construct(forward<Args>(args)...);
			isConstructed = true;
		}

		void Destroy() noexcept(std::is_nothrow_destructible<T>::value)
		{
			isConstructed = false;		// lifetime ends by dtor start
			UnionStorage<T>::Destroy();
		}
	};

#pragma endregion


}		// namespace TypeHelpers
}		// namespace Enumerables





namespace std {


#pragma region StorableT

	template<class T>
	struct hash<Enumerables::TypeHelpers::RefHolder<T>> : hash<remove_const_t<T>>	// inherit disabledness
	{
		// SFINAE: don't define when disabled for referred type
		template<class TT = T, enable_if_t<is_same<TT, T>::value, int> = 0>
		size_t operator ()(const Enumerables::TypeHelpers::RefHolder<TT>& ref) const
		noexcept(noexcept(hash<remove_const_t<T>>::operator()(ref.Get())))
		{
			return hash<remove_const_t<T>>::operator()(ref.Get());
		}
	};

#pragma endregion


}		// namespace std


#endif	// ENUMERABLES_GENERICSTORAGE_HPP
