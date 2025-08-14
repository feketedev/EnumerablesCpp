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
	 *    - in implementation of multipass algorithms								  *
	 *    - as backing buffer of "Materialized" Enumerables							  *
	 *    - debug buffer for natvis													  *
	 *  																			  *
	 *  ----------------------------------------------------------------------------  *
	 *  This file contains the default configuration and type bindings -> for STL.	  *
	 *  																			  *
	 *  Options laid out in this file should be changed BEFORE including the whole	  *
	 *  library via Enumerables_Implementation.hpp !	   Do NOT change this file!	  *
	 *  																			  *
	 *  Recommended is to have a customized Enumerables.hpp for the project with	  *
	 *  the customized settings, and including implementation at the end.			  *
	 *  																			  *
	 *  To use a custom container type:												  *
	 *   * create the appropriate "Binding" following the simple patterns below		  *
	 *   * point it by the corresponding define in your Enumerables.hpp				  *
	 *   * then including Enumerables_Implementation.hpp which pulls this file		  *
	 *   * don't modify this header													  *
	 *  [Alternatively: do the find&replace in your own copy :)]					  *
	 *  ----------------------------------------------------------------------------  */


#include "Enumerables_TypeHelperBasics.hpp"


namespace Enumerables {
	enum class StopReason : char;

	template <class V>	class OptResult;
}




// ==== Macros ==============================================================================================

#ifndef ENUMERABLES_NOINLINE
#	if defined(_MSC_VER) && !defined(__clang__)
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




// ==== Behavioural settings ================================================================================

// Use standard rebind mechanism (via std::allocator_traits)
// for allocators passed down to containers, when used with a changed type.
// Set false if the project's allocator-system is value-unaware (e.g. EASTL).
//
// When enabled, container bindings should define:
//	- AllocatedValueT<[K], V, Args...>
//		Type to be used instead of plain V as value_type.	[optional]
//	- AllocatorOptionIdx
//		The allocator's position within the ellipsis of the Container<[K,] V, [n,] Options...> alias.
//
// This is only for internal type-changes, the client is still responsible for correctly binding allocators
// to observable types. When T& is observable, a convention to bind T* externally is enforced.
#ifndef ENUMERABLES_STANDARD_REBIND_ALLOCATORS
#	define ENUMERABLES_STANDARD_REBIND_ALLOCATORS	true
#endif


// NOTE on Allocators & rebinding:
// When using standard allocators, another approach can be to completely ignore their value_type
// as received in parameters, and always rebind them appropriately for the container to use.
// This could facilitate comfortable usage without manually binding allocators for every usage.
//
// The implementation should be simple via config:
//	- set ENUMERABLES_STANDARD_REBIND_ALLOCATORS false
//  - define Container<...> aliases in bindings so that they rebind any incoming allocator.




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


#ifndef ENUMERABLES_LIST_BINDING
#define ENUMERABLES_LIST_BINDING  StlBinding::ListOperations

	namespace StlBinding {
		struct ListOperations {

			// NOTE: This little helper is necessitated by a clang limitation (bug?) in using ellipsis with template aliases.
			//		 Is it standard? (Resembles Core lang issue #1430, but we don't expand packs into non-pack alias params!)
			//		 Interestingly though, the same problem is not presented with unordered_map (SetOperations),
			//		 nor is it reproducible at all using MSVC or GCC.
			//
			//		 Rationale of using ellipsis:
			//			- keep library headers generic, any required customization in config
			//			- automatically use default (type) arguments of the given container itself
			//
			//		In case of further errors, the same trcick is applicable to other bindings.
			template <class V, class... Options>
			struct BindHelper  { using type = std::vector<V, Options...>; };

			// Argument deduction won't work through a dependent type, only with a direct alias.
			// Fortunately, deduced arguments don't trigger the (supposed) clang bug.
			template <class V, class... Options>
			using DeducibleContainer = std::vector<V, Options...>;


		// --- From here manifests the "Container Binding" concept ---

			/// Custom container type with arbitrary user-provided type-arguments.
			template <class V, class... Options>
			using Container = typename BindHelper<V, Options...>::type;

			/// Allocator's position within Container's "Options..."
			static constexpr unsigned AllocatorOptionIdx = 0;


			/// Create a new Container, preallocating the given capacity if supported.
			template <class TContainer, class... Opts>
			static TContainer	Init(size_t capacity, const Opts&... options)
			{
				TContainer list (options...);
				list.reserve(capacity);
				return list;
			}


			template <class V, class... Opts, class Vin>
			static void		Add(DeducibleContainer<V, Opts...>& l, Vin&& val)	{ l.push_back(std::forward<Vin>(val)); }

			template <class V, class... Opts>
			static void		Clear(DeducibleContainer<V, Opts...>& l)			{ l.clear();	/* keep capacity! */   }

			template <class V, class... Opts>
			static V&		Access(DeducibleContainer<V, Opts...>& l, size_t i)	{ return l[i];}
		};
	}	// namespace StlBinding

	template <class... Args>
	size_t GetSize(const std::vector<Args...>& v)  { return v.size(); }

#endif



#ifndef ENUMERABLES_SET_BINDING
#define ENUMERABLES_SET_BINDING   StlBinding::SetOperations

	namespace StlBinding {
		struct SetOperations {

			template <class V, class... Options>
			using Container = std::unordered_set<V, Options...>;

			static constexpr unsigned AllocatorOptionIdx = 2;


			template <class TContainer, class... Opts>
			static TContainer	Init(size_t capacity, const Opts&... options)
			{
				TContainer set (0u, options...);
				set.reserve(capacity);
				return set;
			}


			template <class V, class... Opts>
			static bool			Contains(const Container<V, Opts...>& s, const V& elem)	{ return s.find(elem) != s.end(); }

			template <class V, class... Opts, class Vin>
			static void			Add(Container<V, Opts...>& s, Vin&& elem)				{ s.insert(std::forward<Vin>(elem)); }
		};
	}

	template <class... Args>
	size_t GetSize(const std::unordered_set<Args...>& s)  { return s.size(); }

#endif



#ifndef ENUMERABLES_DICTIONARY_BINDING
#define ENUMERABLES_DICTIONARY_BINDING	StlBinding::DictionaryOperations

	namespace StlBinding {
		struct DictionaryOperations {

			template <class K, class V, class... Options>
			using Container = std::unordered_map<K, V, Options...>;

			static constexpr unsigned AllocatorOptionIdx = 2;

			template <class K, class V, class... Options>
			using AllocatedValueT = std::pair<const K, V>;


			template <class TContainer, class... Opts>
			static TContainer	Init(size_t capacity, const Opts&... options)
			{
				TContainer dict (/*buckets:*/ 0u, options...);
				dict.reserve(capacity);
				return dict;
			}


			template <class K, class V, class... Opts>
			static bool		Contains(const Container<K, V, Opts...>& d, const K& key)	{ return d.find(key) != d.end(); }

			template <class K, class V, class... Opts, class Kin, class Vin>
			static void		Add(Container<K, V, Opts...>& d, Kin&& key, Vin&& val)		{ d.emplace(std::forward<Kin>(key), std::forward<Vin>(val)); }

			//template <class K, class V, class... Opts, class Kin, class Vin>
			//static void	Upsert(Container<K, V, Opts...>& d, K&& key, Vin&& val)		{ d.insert_or_assign(std::forward<Kin>(key), std::forward<Vin>(val)); }

			template <class K, class V, class... Opts>
			static V&		Access(Container<K, V, Opts...>& d, const K& key)			{ return d[key]; }
		};
	}

	template <class... Args>
	size_t GetSize(const std::unordered_map<Args...>& d)  { return d.size(); }

#endif



#if defined(_MSC_VER) && defined(_OPTIONAL_) || defined(_GLIBCXX_OPTIONAL)

	// Suggested way of using std::optional if OptResult is undesired.
	namespace StlBinding {
		struct OptionalOperations {
			template <class V>	using Container = std::optional<std::decay_t<V>>;

			template <class T, class Enumerator>
			static Container<T>	FromCurrent(Enumerator& et)		{ return Container<T>{ et.Current() }; }

			template <class T>
			static Container<T>	NoValue(StopReason)				{ return std::nullopt; }
		};
	}

	template <class T>
	bool HasValue(const std::optional<T>& o)  { return o.has_value(); }

#endif




// ==== Set the defined bindings (or fallbacks) =============================================================

	using LogicException = ENUMERATIONS_EXCEPTION_TYPE;

	// Set fundamental bindings: fallbacks below can rely on them
	using ListOperations = ENUMERABLES_LIST_BINDING;
	using SetOperations  = ENUMERABLES_SET_BINDING;
	using DictOperations = ENUMERABLES_DICTIONARY_BINDING;


	namespace DefaultBinding {

		// STL doesn't have a small_vector (one with an inline buffer for initial elements, but being able to dynamically expand if needed)
		// - hence only a ListOperations fallback is provided, but it presents the way to utilize such a type from your favourite library.
		struct FallbackSmallListOperations : public ListOperations {

			template <class V, size_t InlineCap, class... Options>
			using Container = ListOperations::Container<V, Options...>;

			template <class V, size_t InlineCap, class... Options>
			using AllocatedValueT = typename TypeHelpers::AsDependentT<V, ListOperations>::template AllocatedValueT<V, Options...>;
		};


		// Required solely by direct Enumerate({ a, b, c }) syntax, specifies the backing container of elements.
		// This default uses the specified LIST_BINDING, assuming initializer_list support.
		struct ListBracedInitOperations {

			/// @tparam Alloc:  optional user-given allocator - use default if void
			template <class V, class Alloc = void>
			using Container = TypeHelpers::OverriddenNthArgT<ListOperations::Container<V>,
															 ListOperations::AllocatorOptionIdx + 1,
															 Alloc								   >;

			/// Create backing container with default allocator.
			template <class V>
			static Container<V>		Init(std::initializer_list<V> initList)
			{
				return Container<V>(initList);
			}

			/// Create backing container with a user-specified allocator.
			template <class V, class A>
			static Container<V, A>	Init(std::initializer_list<V> initList, const A& allocator)
			{
				return Container<V, A>(initList, allocator);
			}
		};

		
		// Recommended default for terminal operations of optional result.
		struct OptionalOperations {
			template <class T>	using Container = OptResult<T>;		// this alias could adjust type (e.g. decay<T>)
																	// => not for automatic deduction

			template <class T, class Enumerator>
			static Container<T>	FromCurrent(Enumerator& et)
			{
				return OptResult<T>::FromFactory([&]() -> decltype(auto) { return et.Current(); });
			}

			template <class T>
			static Container<T>	NoValue(StopReason rs)	{ return rs; }
		};

	}	// namespace DefaultBinding

	template <class T>
	bool HasValue(const OptResult<T>& o)  { return o.HasValue(); }



#ifdef ENUMERABLES_SMALLLIST_BINDING
	using SmallListOperations = ENUMERABLES_SMALLLIST_BINDING;

#	define ENUMERABLES_WARN_FOR_SMALLLIST 
#else
	using SmallListOperations = DefaultBinding::FallbackSmallListOperations;

	// Fallback is needed internally, but warn for usage on interface.
#	define ENUMERABLES_WARN_FOR_SMALLLIST	[[deprecated("No SmallList type defined, will use standard List type! Use ENUMERABLES_SMALLLIST_BINDING")]]
#endif


#ifdef ENUMERABLES_BRACEDINIT_BINDING
	using BracedInitOperations = ENUMERABLES_BRACEDINIT_BINDING;
#else
	using BracedInitOperations = DefaultBinding::ListBracedInitOperations;
#endif


#ifdef ENUMERABLES_OPTIONAL_BINDING
	using OptionalOperations = ENUMERABLES_OPTIONAL_BINDING;
#else
	using OptionalOperations = DefaultBinding::OptionalOperations;
#endif


	// Establish container type aliases
	template <class V, class... Opts>			using ListType		 = typename ListOperations::Container<V, Opts...>;
	template <class V, size_t N, class... Opts>	using SmallListType  = typename SmallListOperations::Container<V, N, Opts...>;
	template <class V, class... Opts>			using SetType		 = typename SetOperations::Container<V, Opts...>;
	template <class K, class V, class... Opts>	using DictionaryType = typename DictOperations::Container<K, V, Opts...>;
	template <class V>							using Optional		 = typename OptionalOperations::Container<V>;


	// NOTE: These containers must have a corresponding GetSize/HasValue free-function accessible in the Enumerables namespace!
	//		 To improve performance by enabling size hints for any other container types used as input,
	//		 further overloads can also be introduced by client code to the Enumerables namespace.
	//		 HasValue overloads enables the .ValuesOnly() shorthand for optional-like types. (Requires an operator* too.)

	static_assert (std::is_same<size_t,	decltype(GetSize(std::declval<ListType<int>&>()))			>::value, "GetSize(List) is not defined!");
	static_assert (std::is_same<size_t,	decltype(GetSize(std::declval<SmallListType<int, 1>&>()))	>::value, "GetSize(SmallList) is not defined!");
	static_assert (std::is_same<size_t,	decltype(GetSize(std::declval<SetType<int>&>()))			>::value, "GetSize(Set) is not defined!");
	static_assert (std::is_same<size_t,	decltype(GetSize(std::declval<DictionaryType<int, int>&>()))>::value, "GetSize(Dictionary) is not defined!");
	static_assert (std::is_same<bool,	decltype(HasValue(std::declval<Optional<int>&>()))			>::value, "HasValue(Optional) is not defined!");

}	// namespace Enumerables


#endif	// ENUMERABLES_CONFIGDEFAULTS_HPP
