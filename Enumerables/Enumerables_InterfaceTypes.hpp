#ifndef ENUMERABLES_INTERFACETYPES_HPP
#define ENUMERABLES_INTERFACETYPES_HPP


	/*  Part of Enumerables for C++.									 *
	 *																	 *
	 *  This file contains helper classes that appear on the Interface.  */


#include "Enumerables_ConfigDefaults.hpp"
#include "Enumerables_TypeHelpers.hpp"



namespace Enumerables {

	// ==== Error messages =======================================================

	enum class StopReason : char	{ None = 0, Empty, Ambiguous };

	constexpr char EmptyError[]		= "Enumeration yielded no results!";
	constexpr char AmbiguityError[]	= "Enumeration yielded multiple results!";

	inline const char* ToString(StopReason code)
	{
		return code == StopReason::Empty	 ? EmptyError
			 : code == StopReason::Ambiguous ? AmbiguityError
			 : nullptr;
	}



	// ==== Size hint ============================================================

	enum class Boundedness {
		Unknown,
		Unbounded,
		Bounded		= 3,
		KnownBound	= 5,
		Exact		= 7
	};


	struct SizeInfo {
		Boundedness		kind;
		size_t			value;


		SizeInfo(Boundedness kind, size_t val = SIZE_MAX) :
			kind  { (kind == Boundedness::KnownBound) && (val == 0) ? Boundedness::Exact : kind },
			value { val }
		{
		}

		bool HasValue() const			// otherwise value is dontcare (garbage)
		{
			return kind == Boundedness::Exact
				|| kind == Boundedness::KnownBound;
		}

		bool IsBounded() const
		{
			return kind != Boundedness::Unknown
				&& kind != Boundedness::Unbounded;
		}

		bool IsUnbounded() const
		{
			return kind == Boundedness::Unbounded;
		}

		bool	IsExact() const
		{
			return kind == Boundedness::Exact;
		}

		operator size_t() const
		{
			ENUMERABLES_INTERNAL_ASSERT (HasValue());
			return value;
		}

		bool	 ProovesDifferent(const SizeInfo& other) const;

		SizeInfo Add  (const SizeInfo& other)		const;
		SizeInfo Limit(const SizeInfo& other)		const;
		SizeInfo Limit(size_t max)					const;
		SizeInfo Subtract(size_t elems)				const;
		SizeInfo Filtered(bool terminable = false)	const;
	};



	// ==== Indexed result =======================================================

	/// An element with its ordinal number attached.
	template <class T>
	struct Indexed {
		size_t	index;
		T		value;

		T&			operator *()		{ return value; }
		T*			operator ->()		{ return &value; }
		const T&	operator *()  const	{ return value; }
		const T*	operator ->() const	{ return &value; }

		bool operator ==(const Indexed& rhs) const { return index == rhs.index && value == rhs.value; }
		bool operator !=(const Indexed& rhs) const { return !operator==(rhs); }
	};



	// ==== Optional Result ======================================================

	/// A simplified, logically immutable optional type, offering & support and rich chaining/transformation features.
	template <class T>
	class OptResult final {
		const StopReason				error	= StopReason::None;
		TypeHelpers::GenericStorage<T>	storage;


		using TDecayed		= std::decay_t<T>;

		template <class F>
		using IfFallbackFun	= std::enable_if_t<std::is_same_v<TypeHelpers::InvokeResultT<F>, OptResult>>;

		template <class F>
		using IfDefaultFun	= std::enable_if_t<std::is_convertible_v<TypeHelpers::InvokeResultT<F>, T>>;

		template <class M, class TT = T>
		using MapResult		= decltype(TypeHelpers::LambdaCreators::CustomMapper<TT>(std::declval<M&&>()).operator()(std::declval<TT>()));

		// quickfix: this separate gate triggers substitution failure prior to static_asserts of CustomMapper - which would fire even in subsequently disqualified overloads
		template <class M, class TT = T>
		using IfMappable = std::enable_if_t<TypeHelpers::IsCallable<M&&, TT>::value || std::is_member_pointer_v<std::decay_t<M>>, int>;



		void CheckAccess() const
		{
			if (HasValue())
				return;

			ENUMERABLES_CLIENT_BREAK(ReasonOfMissText());
			throw LogicException(ReasonOfMissText());
		}


		enum LateInitSelector { LateInit };
		OptResult(LateInitSelector) {}

	public:
		template <class... Args, class = std::enable_if_t<TypeHelpers::IsBraceConstructible<T, Args...>::value>>
		OptResult(Args&&... args)	: storage { TypeHelpers::ForwardParams, std::forward<Args>(args)... }
		{
		}

		OptResult(StopReason code)	: error	  { code }
		{
			ENUMERABLES_INTERNAL_ASSERT (code != StopReason::None);
		}


		template <class Factory>
		static OptResult FromFactory(Factory&& fact)
		{
			OptResult r { LateInit };
			r.storage.InvokeFactory(std::forward<Factory>(fact));
			return r;
		}


		OptResult(OptResult&& src)		: error { src.error }
		{
			if (src.HasValue())
				storage.MoveFrom(src.storage);
		}

		OptResult(const OptResult& src)	: error { src.error }
		{
			if (src.HasValue())
				storage.CopyFrom(src.storage);
		}


		template <class S>
		OptResult(OptResult<S>&& src)		: error { src.ReasonOfMiss() }
		{
			if (src.HasValue())
				storage.Construct(src.Value());
		}

		template <class S>
		OptResult(const OptResult<S>& src)	: error { src.ReasonOfMiss() }
		{
			if (src.HasValue())
				storage.Construct(src.Value());
		}

		template <class S>
		OptResult(OptResult<S>& src) : OptResult { const_cast<const OptResult<S>&>(src) }
		{
		}


		~OptResult()
		{
			if (HasValue())
				storage.Destroy();
		}


		bool		HasValue()					const	{ return error == StopReason::None; }
		StopReason	ReasonOfMiss()				const	{ return error; }
		const char* ReasonOfMissText()			const	{ return ToString(error); }


		bool		operator ==(std::nullptr_t)	const	{ return !HasValue(); }

		template <class Rhs>
		bool		operator ==(const Rhs& rhs)	const	{ return HasValue() && Value() == rhs; }

		template <class OptRhs>
		bool		operator ==(const OptResult<OptRhs>& rhs) const
		{
			return rhs.HasValue() ? operator==(rhs.Value()) : !HasValue();
		}


		template <class Rhs>
		bool		operator !=(const Rhs& rhs)				 const { return !operator==(rhs); }

		template <class OptRhs>		// only avoids ambiguity
		bool		operator !=(const OptResult<OptRhs>& rhs) const { return !operator==(rhs); }



		T&			Value()				&	{ CheckAccess(); return storage.Value(); }
		const T&	Value()		  const &	{ CheckAccess(); return storage.Value(); }
		T&&			Value()			   &&	{ CheckAccess(); return std::move(storage).Value(); }

		T&			operator *()		&	{ return Value(); }
		const T&	operator *()  const &	{ return Value(); }
		T&&			operator *()	   &&	{ return std::move(*this).Value(); }

		// Note: &-collapsing suffices for Value(), but * needs auto
		auto*		operator ->()			{ return &Value(); }
		auto*		operator ->() const		{ return &Value(); }


		/// If this has no value, chose the argument of the same optional type.
		/// [Choice made over references.]
		const OptResult&	OrFallback(const OptResult& b)  const &	{ return HasValue() ? *this : b; }
		OptResult&			OrFallback(OptResult& b)			  &	{ return HasValue() ? *this : b; }
		OptResult&&			OrFallback(OptResult&& b)			 &&	{ return HasValue() ? std::move(*this) : std::move(b); }		// <- possible to chain within 1 expression

		/// If this has no value, chose the argument of the same optional type.
		/// [Value return - move/copy as appropriate.]
		OptResult			OrFallback(const OptResult& b)		 &&	{ return HasValue() ? std::move(*this) : OptResult { b }; }		// <- expected asymmetric usage!
		OptResult			OrFallback(OptResult&& b)		const &	{ return HasValue() ? OptResult { *this } : std::move(b); }

		/// Only if this has no value, call the given function to produce a fallback value of the same optional type.
		/// [Value return - move/copy as appropriate.]
		template <class F, class = IfFallbackFun<F>>
		OptResult			OrFallback(F&& getFallback)		const &	{ return HasValue() ? *this : getFallback(); }

		template <class F, class = IfFallbackFun<F>>
		OptResult			OrFallback(F&& getFallback)			 &&	{ return HasValue() ? std::move(*this) : getFallback(); }



		/// If this has no value, fallback to a guaranteed value.
		/// [Value return - move/copy as appropriate.]
		template <class TT = T, std::enable_if_t<!std::is_reference_v<TT>, int> = 0>
		TDecayed			OrDefault(const TDecayed& def)		 && { return HasValue() ? std::move(*this).Value() : T { def }; }
		TDecayed			OrDefault(TDecayed&& def)			 && { return HasValue() ? std::move(*this).Value() : std::move(def); }
		TDecayed			OrDefault(TDecayed&& def)		const & { return HasValue() ? T { this->Value() }      : std::move(def); }

		/// If this has no value, fallback to a guaranteed value.
		/// [Choice made over payload references.]
		template <class TT = T, std::enable_if_t<std::is_same_v<TT, TDecayed&>, int> = 0>
		T&					OrDefault(T& def)				const & { return HasValue() ? Value() : def; }

		template <class TT = T, std::enable_if_t<std::is_same_v<TT, TDecayed> || std::is_same_v<TypeHelpers::ConstValueT<TT>, const TDecayed&>, int> = 0>
		const TDecayed&		OrDefault(const TDecayed& def)	const & { return HasValue() ? Value() : def; }

		template <class TT = T, std::enable_if_t<std::is_same_v<TT, TDecayed>, int> = 0>
		TDecayed&			OrDefault(TDecayed& def)			  & { return HasValue() ? Value() : def; }


		/// Only if this has no value, call the fallback function to produce a guaranteed value.
		/// [Exact payload type only.]
		template <class F, class = IfDefaultFun<F>>
		T					OrDefault(F&& getDefault)		const &	{ return HasValue() ? Value() : getDefault(); }

		template <class F, class = IfDefaultFun<F>>
		T					OrDefault(F&& getDefault)			 &&	{ return HasValue() ? std::move(*this).Value() : getDefault(); }



		/// If has value, apply a transformation function to it - otherwise forward error code.
		/// @param mapper:	Lambda, pointer to member, or exact [member-] function pointer to accept type T.
		template <class M>
		OptResult<MapResult<M, T>>			MapValue(M&& mapper)	&&
		{
			auto map = TypeHelpers::LambdaCreators::CustomMapper<T>(mapper);
			if (HasValue())
				return map(std::move(*this).Value());
			else
				return error;
		}

		template <class M, IfMappable<M, T&> = 0>
		OptResult<MapResult<M, T&>>			MapValue(M&& mapper)	&
		{
			auto map = TypeHelpers::LambdaCreators::CustomMapper<T&>(mapper);
			if (HasValue())
				return map(Value());
			else
				return error;
		}

		template <class M, IfMappable<M, const T&> = 0>
		OptResult<MapResult<M, const T&>>	MapValue(M&& mapper)	const &
		{
			auto map = TypeHelpers::LambdaCreators::CustomMapper<const T&>(mapper);
			if (HasValue())
				return map(Value());
			else
				return error;
		}



		/// If has value, resolve and apply a transformation function to it - otherwise forward error code.
		/// @tparam	R:		Result type of mapper function (mandatory for overload-set)
		/// @param	mapper:	Overload-set of [member-] functions to accept type T.
		template <class R>
		OptResult<R>   MapValueTo(TypeHelpers::OverloadResolver<T, R> mapper)		 &&
		{
			return std::move(*this).MapValue(mapper);
		}

		template <class R>
		OptResult<R>   MapValueTo(TypeHelpers::OverloadResolver<T&, R> mapper)		 &
		{
			return MapValue(mapper);
		}

		template <class R>
		OptResult<R>   MapValueTo(TypeHelpers::OverloadResolver<const T&, R> mapper) const &
		{
			return MapValue(mapper);
		}
	};



	template <class Lhs, class Rhs>
	bool operator ==(const Lhs& lhs, const OptResult<Rhs>& rhs)
	{
		return rhs == lhs;
	}

	template <class Lhs, class Rhs>
	bool operator !=(const Lhs& lhs, const OptResult<Rhs>& rhs)
	{
		return !rhs.operator==(lhs);
	}

}		// namespace Enumerables

#endif	// ENUMERABLES_INTERFACETYPES_HPP
