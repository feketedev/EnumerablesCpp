#ifndef ENUMERABLES_CONFIGDEFAULTS_HPP
#define ENUMERABLES_CONFIGDEFAULTS_HPP

	/*  ----------------------------------------------------------------------------  *
	 *  Part of Enumerables for to C++.												  *
	 *  																			  *
	 *  This library depends solely on standard headers, but is intended to allow	  *
	 *  usage with your favourite (or the project's mandatory) container types.		  *
	 *  																			  *
	 *  To eliminate the need for "find&replace" to apply Enumerables in any project  *
	 *  a preprocessor-helped binding system is in place.							  *
	 *  																			  *
	 *  [More template parameters could present an alternative (basically with the    *
	 *   very same descriptor classes), but using the preprocessor is a lighter so-	  *
	 *   lution, as long as no multiple bindings needed within a compliation unit.]   *
	 *  																			  *
	 *  Note that some container types (particularly lists) play internal roles too:  *
	 *	  - in implementation of multipass algorithms								  *
	 *	  - as backing buffer of "Materialized" Enumerables							  *
	 *	  - debug buffer for natvis													  *
	 *  																			  *
	 *  ----------------------------------------------------------------------------  *
	 *  This file contains the default configuration and type bindings -> for STL.	  *
	 *  																			  *
	 *  Options in this file should be changed BEFORE including the whole library	  *
	 *	via Enumerables_Implementation.hpp !										  *
	 *																				  *
	 * 	Recommended is to have a customized Enumerables.hpp for the project with	  *
	 * 	the customized settings, and including implementation at the end.			  *
	 * 																				  *
	 *  To use a custom container type:												  *
	 *	 * create the appropriate "Binding" following the simple patterns below		  *
	 *   * point it by the corresponding define in your Enumerables.hpp				  *
	 *	 * then including Enumerables_Implementation.hpp which pulls this file		  *
	 *	 * don't modify this header													  *
	 *  [Alternatively: do the find&replace in your own copy :)]					  *
	 *  ----------------------------------------------------------------------------  */



namespace Enumerables {
	enum class StopReason : char;

	template <class V>	class OptResult;
}




// ==== Macros ==============================================================================================

#ifndef ENUMERABLES_NOINLINE
#	ifdef _MSC_VER
#		define ENUMERABLES_NOINLINE __declspec(noinline)
#	else
#		define ENUMERABLES_NOINLINE __attribute__((noinline))
#	endif
#endif


// To hide FUN, FUNV:
//   define ENUMERABLES_NO_FUN
// before including Enumerables_Implementation.hpp




// ==== Debugging facilities ================================================================================

// Define a debug buffer inside each [Auto]Enumerable to reveal results of evaluation.
#ifndef ENUMERABLES_USE_RESULTSVIEW
#	ifdef _DEBUG
#		define ENUMERABLES_USE_RESULTSVIEW			true
#	else
#		define ENUMERABLES_USE_RESULTSVIEW			false
#	endif
#endif


// ResultsView evaluation mode if ENUMERABLES_USE_RESULTSVIEW is enabled.
//		0 - manual only [Test() or Print() from immediate window]
//		1 - on l-value usage: when a derived query is "forked" or GetEnumerator called
//			[includes terminal operations except the container-creator ones]
//		2 - right after creation
//			[most convinient, but each chained operation will allocate its ResultView!]
#ifndef ENUMERABLES_RESULTSVIEW_AUTO_EVAL
#	define ENUMERABLES_RESULTSVIEW_AUTO_EVAL		1
#endif


#ifndef ENUMERABLES_RESULTSVIEW_MAX_ELEMS
#	define ENUMERABLES_RESULTSVIEW_MAX_ELEMS		100
#endif



// ---- Assertions ------------------------------------------------------------------------------------------

// Debug notification before throwing for an input unexpected by user code (by calling .Single, .First etc.)
#ifndef ENUMERABLES_CLIENT_BREAK
#	if defined(_MSC_VER) && defined(_DEBUG)
#		define ENUMERABLES_CLIENT_BREAK(msg)		_RPT0(_CRT_ASSERT, msg)
#	else
#		define ENUMERABLES_CLIENT_BREAK(msg)		(void)msg
#	endif
#endif


// Debug notification for incorrect usage of Enumerators (accessing Current without FetchNext was true)
// - if no enumerators accessed directly, can be considered an internal check.
#ifndef ENUMERABLES_ETOR_USAGE_ASSERT
#	ifdef _DEBUG
#		define ENUMERABLES_ETOR_USAGE_ASSERT(cond, text)	if (!(cond)) ENUMERABLES_CLIENT_BREAK(text)
#	else
#		define ENUMERABLES_ETOR_USAGE_ASSERT(cond, text) 
#	endif
#endif


// No internal debugging by default
#ifndef ENUMERABLES_INTERNAL_ASSERT
#	define ENUMERABLES_INTERNAL_ASSERT(cond)	
#endif




// ==== Optimization settings ===============================================================================

// Inline buffer size for type-erased Enumerators
// - that is where IEnumerator<V> descendant can be emplaced on Enumerable<V>::GetEnumerator() call
// - used as the space for algorithm-state during iteration, usually requires more then its factory
// - note that the wrapped factory in Enumerable<V> is unrelated to this value and can use heap,
//   that depends on std::function's own inline optimization which is typically for 16..48 bytes
#ifndef ENUMERABLES_INTERFACED_ETOR_INLINE_SIZE
#	define ENUMERABLES_INTERFACED_ETOR_INLINE_SIZE	112
#endif


// Enable looking for optimal shortcuts even when an interfaced Enumerable<T> is chained
// - such as passing the result list as a whole through .Order().ToInterfaced().ToList()
#ifndef ENUMERABLES_EMPLOY_DYNAMICCAST
#	define ENUMERABLES_EMPLOY_DYNAMICCAST			true
#endif




// ==== Define bindings to STL if needed ====================================================================

#ifndef ENUMERABLES_LIST_BINDING
#	include <vector>
#endif

#ifndef ENUMERABLES_SET_BINDING
#	include <unordered_set>
#endif

#ifndef ENUMERABLES_DICTIONARY_BINDING
#	include <unordered_map>
#endif

#ifndef ENUMERATIONS_EXCEPTION_TYPE
#	include <stdexcept>
#	define ENUMERATIONS_EXCEPTION_TYPE   std::logic_error
#endif



namespace Enumerables {
namespace StlBinding {


#ifndef ENUMERABLES_LIST_BINDING
#define ENUMERABLES_LIST_BINDING  StlBinding::ListOperations

	struct ListOperations {
		template <class V>	using Container = std::vector<V>;

		template <class V>
		static Container<V>	Init(size_t capacity)
		{
			Container<V> l;
			l.reserve(capacity);
			return l;
		}

		template <class V, class Vin>
		static void			Add(Container<V>& l, Vin&& val)		{ l.push_back(std::forward<Vin>(val)); }

		template <class V>
		static void			Clear(Container<V>& l)				{ l.clear();	/* keep capacity! */   }

		template <class V>
		static size_t		GetSize(const Container<V>& l)		{ return l.size(); }

		template <class V>
		static V&			Access(Container<V>& l, size_t i)	{ return l[i];}
	};

#endif



#ifndef ENUMERABLES_SET_BINDING
#define ENUMERABLES_SET_BINDING   StlBinding::SetOperations

	struct SetOperations {
		template <class V>	using Container = std::unordered_set<V>;

		template <class V>
		static Container<V>	Init(size_t capacity)
		{
			Container<V> s;
			s.reserve(capacity);
			return s;
		}

		template <class V>
		static size_t		GetSize(const Container<V>& s)					{ return s.size(); }

		template <class V>
		static bool			Contains(const Container<V>& s, const V& elem)	{ return s.find(elem) != s.end(); }

		template <class V, class Vin>
		static void			Add(Container<V>& s, Vin&& elem)				{ s.insert(std::forward<Vin>(elem)); }
	};

#endif



#ifndef ENUMERABLES_DICTIONARY_BINDING
#define ENUMERABLES_DICTIONARY_BINDING	StlBinding::DictionaryOperations

	struct DictionaryOperations {
		template <class K, class V>		using Container = std::unordered_map<K, V>;

		template <class K, class V>
		static Container<K, V> Init(size_t capacity)
		{
			Container<K, V> d;
			d.reserve(capacity);
			return d;
		}

		template <class K, class V>
		static size_t	GetSize(const Container<K, V>& d)					{ return d.size(); }

		template <class K, class V>
		static bool		Contains(const Container<K, V>& d, const K& key)	{ return d.find(key) != d.end(); }

		template <class K, class V, class Kin, class Vin>
		static void		Add(Container<K, V>& d, Kin&& key, Vin&& val)		{ d.emplace(std::forward<Kin>(key), std::forward<Vin>(val)); }

		//template <class K, class V, class Kin, class Vin>
		//static void	Upsert(Container<K, V>& d, K&& key, Vin&& val)		{ d.insert_or_assign(std::forward<Kin>(key), std::forward<Vin>(val)); }
	};

#endif



#if defined(_MSC_VER) && defined(_OPTIONAL_)

	// Suggested way of using std::optional if OptResult is undesired.
	struct OptionalOperations {
		template <class V>	using Container = std::optional<std::decay_t<V>>;

		template <class T, class Enumerator>
		static Container<T>	FromCurrent(Enumerator& et)		{ return Container<T>{ et.Current() }; }

		template <class T>
		static Container<T>	NoValue(StopReason)				{ return std::nullopt; }
	};

#endif

}	// namespace StlBinding
}	// namespace Enumerables




// ==== Set the defined bindings (or fallbacks) =============================================================

namespace Enumerables {

	using LogicException = ENUMERATIONS_EXCEPTION_TYPE;

	using ListOperations = ENUMERABLES_LIST_BINDING;
	using SetOperations  = ENUMERABLES_SET_BINDING;
	using DictOperations = ENUMERABLES_DICTIONARY_BINDING;


	namespace DefaultBinding {

		// Recommended default for terminal operations of optional result.
		struct OptionalOperations {
			template <class T>	using Container = OptResult<T>;

			template <class T, class Enumerator>
			static Container<T>	FromCurrent(Enumerator& et)
			{
				return OptResult<T>::FromFactory([&]() -> decltype(auto) { return et.Current(); });
			}

			template <class T>
			static Container<T>	NoValue(StopReason rs)	{ return rs; }

			template <class T>
			static bool HasValue(const Container<T>& o) { return o.HasValue(); }
		};


		// STL doesn't have a small_vector (one with an inline buffer for initial elements, but being able to dynamically expand if needed)
		// - hence only a ListOp. fallback is provided, but the below code presents the way to utilize such type from your favorite library.
		struct FallbackSmallListOperations {
			template <class V, size_t InlineCap>
			using Container = ListOperations::Container<V>;

			template <class V, size_t N>
			static Container<V, N>	Init(size_t capacity = N)		{ return ListOperations::Init<V>(capacity); }

			template <class V, size_t N = 0>
			static size_t	GetSize(const Container<V, N>& l)		{ return ListOperations::GetSize(l); }

			template <class V, size_t N = 0, class Vin>
			static void		Add(Container<V, N>& l, Vin&& val)		{ ListOperations::Add(l, std::forward<Vin>(val)); }

			template <class V, size_t N = 0>
			static void		Clear(Container<V, N>& l)				{ ListOperations:::Clear(l); }

			template <class V, size_t N = 0>
			static V&		Access(Container<V, N>& l, size_t i)	{ return ListOperations::Access(l, i); }
		};

	}	// namespace DefaultBinding



#ifdef ENUMERABLES_OPTIONAL_BINDING
	using OptionalOperations = ENUMERABLES_OPTIONAL_BINDING;
#else
	using OptionalOperations = DefaultBinding::OptionalOperations;
#endif


#ifdef ENUMERABLES_SMALLLIST_BINDING
	using SmallListOperations = ENUMERABLES_SMALLARRAY_BINDING;

#	define ENUMERABLES_WARN_FOR_SMALLLIST 
#else
	using SmallListOperations = DefaultBinding::FallbackSmallListOperations;

	// Fallback is needed internally, but warn for usage on interface.
#	define ENUMERABLES_WARN_FOR_SMALLLIST	[[deprecated("No SmallList type defined, will use standard List type! Use ENUMERABLES_SMALLLIST_BINDING")]]
#endif



	// Some shortcuts
	template <class V>				using ListType		 = typename ListOperations::Container<V>;
	template <class V, size_t N>	using SmallListType  = typename SmallListOperations::Container<V, N>;
	template <class V>				using SetType		 = typename SetOperations::Container<V>;
	template <class K, class V>		using DictionaryType = typename DictOperations::Container<K, V>;
	template <class V>				using Optional		 = typename OptionalOperations::Container<V>;

	// "Import" GetSize's + HasValue defined in binding
	template <class V>				size_t GetSize(const ListType<V>& l)			{ return ListOperations::GetSize<V>(l);			}
	template <class V>				size_t GetSize(const SetType<V>&  s)			{ return SetOperations::GetSize<V>(s);			}
	template <class V, size_t N>	size_t GetSize(const SmallListType<V, N>& l)	{ return SmallListOperations::GetSize<V, N>(l); }
	template <class K, class V>		size_t GetSize(const DictionaryType<K, V>& d)	{ return DictOperations::GetSize<K, V>(d);		}

	template <class T>				bool HasValue(const Optional<T>& o)				{ return OptionalOperations::HasValue<T>(o);	}


	// NOTE: To improve performance by enabling size hints,
	//		 GetSize for further containers can be introduced by client code to namespace Enumerables.
	//		 Just like HasValue, to enable the .ValuesOnly() shorthand for multiple types. (Requires an operator* too.)
	// TODO 17: std::size


}	// namespace Enumerables


#endif	// ENUMERABLES_CONFIGDEFAULTS_HPP
