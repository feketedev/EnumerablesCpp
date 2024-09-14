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
	template <class T>
	class RefHolder {
		static_assert (!is_reference<T>::value, "Use with (qualified) decayed type.");

		T* /*const*/ ptr;

	public:
		template <class Ref, class = enable_if_t<is_same<const T, const remove_reference_t<Ref>>::value>>
		RefHolder(Ref&& ref) : ptr { &ref }
		{
			static_assert (is_lvalue_reference<Ref>::value, "Reference stored from rvalue soon becomes dangling!");
		}

		T&		 Get() const	{ return *ptr; }
		operator T&()  const	{ return *ptr; }
	};



	/// Temporary storage for arbitrary T input in generic code - supports both (l-value) references and values.
	/// @remarks
	///	  The input element, as a source Enumerator's return value can be:
	///	 	* lvalue ref  -> its address is available, and we assume it is sustained during the entire enumeration (constness preserved)
	///	 	* prvalue	  -> temporary / mapped result, must be stored if needed later (T left as is, no overhead)
	///	 	* xvalue (&&) -> to be avoided in general; still, decaying it makes most sense, as it probably won't survive the enumeration
	template <class T>
	using StorableT = conditional_t< is_lvalue_reference<T>::value,
										RefHolder<remove_reference_t<T>>,
										remove_reference_t<T>			 >;


	/// Access stored instance as lvalue
	template <class V>	V&			Revive(const RefHolder<V>& stored)		{ return stored.Get(); }
	template <class V>	V&			Revive(RefHolder<V>& stored)			{ return stored.Get(); }
	template <class V>	V&			Revive(V& stored)						{ return stored; }

	/// Access stored instance forcing constness even on referenced object
	template <class V>	const V&	ReviveConst(const RefHolder<V>& stored)	{ return stored.Get(); }
	template <class V>	const V&	ReviveConst(RefHolder<V>& stored)		{ return stored.Get(); }
	template <class V>  const V&	ReviveConst(const V& stored)			{ return stored; }

	/// Get final access to the stored entity, if possible as an rvalue
	template <class V>	V&			PassRevived(RefHolder<V>& stored)		{ return stored.Get(); }
	template <class V>	V&&			PassRevived(V& stored)					{ return move(stored); }

#pragma endregion




#pragma region GenericStorage

	// Optimization to accept function results - potentially avoid requiring move ctor from C++17
	template <class Factory>
	struct RvoEmplacer {
		using R = InvokeResultT<Factory>;

		static_assert (!is_reference<R>::value, "Expecting a prvalue from factory.");

		R	obj;
		R*	GetPtr()	{ return &obj; }

		RvoEmplacer(Factory& fact) : obj { fact() }
		{
			static_assert (sizeof(RvoEmplacer) == sizeof(R),   "Size mismatch??");
			static_assert (alignof(RvoEmplacer) == alignof(R), "Alignment mismatch??");
		}
	};


	// Constructor selectors
	enum FactoryInvokeSelector { InvokeFactory };
	enum ParamForwardSelector  { ForwardParams };


	/// Generalized temporary storage for potentially any type (refs/immutables included).
	/// Defines all supposable operations, but leaves their management (even lifetime handling!) to the user/inheritor.
	/// @remarks
	///		Strategies are just to avoid char[] casting when possible (e.g. for debugging without natvis; +avoid reinterpret anyway).
	///		C++17 basically renders them obsolete with std::launder.
	///	    [A direct storage strategy could also have been existed to enforce initialization, but found no use-case.]
	template <class T, template <class> class Storage>
	class GenericStorage {
		using S	= StorableT<T>;
		Storage<S> val;

	public:
		// NOTE: Old clang crashes on auto&/auto* return types, hence need to augment it.
		using Ptr      = remove_reference_t<T>*;
		using ConstPtr = const remove_reference_t<T>*;


		// ---- Access ----

		const T&	Value()			const&	{ return Revive(val.Get()); }
		T&			Value()				 &	{ return Revive(val.Get()); }
		T&&			Value()				&&	{ return PassRevived(val.Get()); }
		T&&			PassValue()				{ return PassRevived(val.Get()); }

		const T&	operator *()	const&	{ return Value(); }
		T&			operator *()		 &	{ return Value(); }
		T&&			operator *()		&&	{ return move(*this).Value(); }

		// Note: &-collapsing suffice for Value(), but * needs auto	 -> or explicit help
		Ptr			operator ->()			{ return &Value(); }
		ConstPtr	operator ->()	const	{ return &Value(); }

		operator	const T&()		const&	{ return Value(); }
		operator	T&()				 &	{ return Value(); }
		operator	T&&()				&&	{ return move(*this).Value(); }


		// ---- Outsourced lifetime management (!) ----

		GenericStorage()	{}
		~GenericStorage()	{}

		void Destroy()		{ val.Get().~S(); }


		// copy/move manually if appropriate!
		GenericStorage(const GenericStorage& src) = delete;

		/// if only @p src is initialized!
		void MoveFrom(GenericStorage& src)
		{
			new (val.GetBuffer()) S { src.PassValue() };
		}

		/// if only @p src is initialized!
		void CopyFrom(const GenericStorage& src)
		{
			new (val.GetBuffer()) S { src.Value() };
		}


		// ---- Construction/assignment ops ----

		template <class... Args>
		GenericStorage(ParamForwardSelector, Args&&... ctorArgs) : val { ForwardParams, forward<Args>(ctorArgs)... }
		{
		}

		template <class Fact>
		GenericStorage(FactoryInvokeSelector, Fact& create) : val { TypeHelpers::InvokeFactory, create }
		{
		}


		template <class... Args>
		void Construct(Args&&... ctorArgs)
		{
			new (val.GetBuffer()) S { forward<Args>(ctorArgs)... };
		}


		template <class Factory, class Trg = T>
		void InvokeFactory(Factory&& create, enable_if_t<is_same<S, Trg>::value>* = nullptr)
		{
			// Note: to allow conversions, use Construct! That will need a temporary anyway.
			static_assert (is_same<decltype(create()), T>::value, "Factory returns mismatching type.");
			static_assert (is_same<typename RvoEmplacer<Factory>::R, S>::value, "GenericStorage deduction error.");
			new (val.GetBuffer()) RvoEmplacer<Factory> { create };
		}

		// shortcut for S = RefHolder<T>
		template <class Factory, class Trg = T>
		void InvokeFactory(Factory&& create, enable_if_t<!is_same<S, Trg>::value>* = nullptr)
		{
			static_assert (is_same<decltype(create()), T>::value, "Factory returns mismatching type.");
			new (val.GetBuffer()) S { create() };
		}


		// Check with optimization: constexpr for unmatching types
		template <class Src>
		constexpr bool IsNotSelf(const Src&)   const	{ return true; }
		bool		   IsNotSelf(const T& src) const	{ return &src != &Value(); }


		/// only if already initialized!
		template <class Src, class Trg = T>
		T& Reassign(Src&& src, enable_if_t<!IsHeadAssignable<Trg, Src>>* = nullptr)
		{
			if (IsNotSelf(src)) {
				Destroy();
				Construct(forward<Src>(src));
			}
			return Value();
		}

		/// only if already initialized!
		template <class Src, class Trg = T>
		T& Reassign(Src&& src, enable_if_t<IsHeadAssignable<Trg, Src>>* = nullptr)
		{
			static_assert (!is_reference<T>::value, "GenericStorage Internal error.");
			T& v = Value();
			v    = forward<Src>(src);
			return v;
		}
	};

#pragma endregion




#pragma region GenericStorage Strategies

	// The conventional way			// TODO 17: No need from c++17
	template <class T>
	class BytesHolder final {

		alignas (T) char buffer[sizeof(T)];

	public:
		T*			GetBuffer()			{ return reinterpret_cast<T*>	   (&buffer[0]); }
		const T*	GetBuffer()	const	{ return reinterpret_cast<const T*>(&buffer[0]); }
		const T&	Get()		const	{ return *GetBuffer(); }
		T&			Get()				{ return *GetBuffer(); }

		BytesHolder() = default;

		template <class... Args>
		BytesHolder(ParamForwardSelector, Args&&... ctorArgs)
		{
			new (buffer) T { forward<Args>(ctorArgs)... };
		}

		template <class Fact>
		BytesHolder(FactoryInvokeSelector, Fact& create)
		{
			new (buffer) RvoEmplacer<Fact> { create };
		}
	};


	// Better for intellisense, no casts - but should not be reassigned without std::launder!
	template <class T>
	class UnionHolder final {

		union { T val; };

	public:
		const T&	Get() const		{ return val; }
		T&			Get()			{ return val; }
		T*			GetBuffer()		{ return &val;}

		~UnionHolder() {}			// user responsibility!
		UnionHolder()  {}

		template <class... Args>
		UnionHolder(ParamForwardSelector, Args&&... ctorArgs)	: val { forward<Args>(ctorArgs)... }
		{
		}

		template <class Fact>
		UnionHolder(FactoryInvokeSelector, Fact& create)		: val { create() }
		{
		}
	};

#pragma endregion


}		// namespace TypeHelpers
}		// namespace Enumerables

#endif	// ENUMERABLES_GENERICSTORAGE_HPP
