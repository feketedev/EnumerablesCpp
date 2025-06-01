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
		struct VectorBindHelper
		{
			using type = std::vector<V, Options...>;
		};



		// Custom container type with arbitrary user-provided type-arguments.
		template <class V, class... Options>
		using Container = typename VectorBindHelper<V, Options...>::type;

		template <class V, class... Opts>
		static Container<V, Opts...>		Init(size_t capacity, const Opts&... opts)
		{
			Container<V, Opts...> l (opts...);
			l.reserve(capacity);
			return l;
		}



		// TODO: Could these initializations be simplified?
		//		 (Full benefit will come with builder-syntax for allocators.)

		// Similar to Init, but these overloads explicitly take an allocator designated for the container.
		//	* Should Container accept further parameters from the user in customizable contexts,
		//	  those are to be forwarded after capacity and alloc, as well.
		//	* A user-specified custom allocator should take precedence over the designated one.
		//	  [e.g.: ToList<int, MyAlloc>()]	This is solved by 2 overloads for std::vector.
		//	* The exact result type is not assumed at call places.
		template <class V, class Alloc>
		static Container<V, Alloc>			InitWithAllocator(const Alloc& alloc, size_t capacity)
		{
			return Init<V, Alloc>(capacity, alloc);
		}

		template <class V, class Alloc, class DirectAlloc>
		static Container<V, DirectAlloc>	InitWithAllocator(const Alloc&, size_t capacity, const DirectAlloc& alloc)
		{
			return Init<V, DirectAlloc>(capacity, alloc);
		}



		// Init_list overloads:
		// Required solely by direct Enumerate({ a, b, c }) syntax, only for List, no custom parameters.
		template <class V>
		static Container<V>			Init(std::initializer_list<V> initList)
		{
			return std::vector<V> (initList);
		}
		
		template <class V, class Alloc>
		static Container<V, Alloc>	InitWithAllocator(const Alloc& a, std::initializer_list<V> initList)
		{
			return { initList, a };
		}



		// Argument deduction won't work through a depentend type, only with a direct alias.
		// Fortunately, deduced arguments don't trigger the (supposed) clang bug.
		template <class V, class... Options>
		using DeducibleContainer = std::vector<V, Options...>;


		template <class V, class... Opts, class Vin>
		static void		Add(DeducibleContainer<V, Opts...>& l, Vin&& val)	{ l.push_back(std::forward<Vin>(val)); }

		template <class V, class... Opts>
		static void		Clear(DeducibleContainer<V, Opts...>& l)			{ l.clear();	/* keep capacity! */   }

		template <class V, class... Opts>
		static size_t	GetSize(const DeducibleContainer<V, Opts...>& l)	{ return l.size(); }

		template <class V, class... Opts>
		static V&		Access(DeducibleContainer<V, Opts...>& l, size_t i)	{ return l[i];}
	};

#endif



#ifndef ENUMERABLES_SET_BINDING
#define ENUMERABLES_SET_BINDING   StlBinding::SetOperations

	struct SetOperations {
		template <class V, class... Options>
		using Container = std::unordered_set<V, Options...>;

		template <class V, class... Opts>
		static Container<V, Opts...>	Init(size_t capacity, const Opts&... options)
		{
			Container<V, Opts...> s (0u, options...);
			s.reserve(capacity);
			return s;
		}

		template <class V, class... Opts>
		static size_t		GetSize(const Container<V, Opts...>& s)					{ return s.size(); }

		template <class V, class... Opts>
		static bool			Contains(const Container<V, Opts...>& s, const V& elem)	{ return s.find(elem) != s.end(); }

		template <class V, class... Opts, class Vin>
		static void			Add(Container<V, Opts...>& s, Vin&& elem)				{ s.insert(std::forward<Vin>(elem)); }
	};

#endif



#ifndef ENUMERABLES_DICTIONARY_BINDING
#define ENUMERABLES_DICTIONARY_BINDING	StlBinding::DictionaryOperations

	struct DictionaryOperations {
		template <class K, class V, class... Options>	using Container = std::unordered_map<K, V, Options...>;

		template <class K, class V, class... Options>
		static Container<K, V, Options...> Init(size_t capacity, const Options&... opts)
		{
			Container<K, V, Options...> d (/*buckets:*/ 0u, opts...);
			d.reserve(capacity);
			return d;
		}

		template <class K, class V, class... Opts>
		static size_t	GetSize(const Container<K, V, Opts...>& d)					{ return d.size(); }

		template <class K, class V, class... Opts>
		static bool		Contains(const Container<K, V, Opts...>& d, const K& key)	{ return d.find(key) != d.end(); }

		template <class K, class V, class... Opts, class Kin, class Vin>
		static void		Add(Container<K, V, Opts...>& d, Kin&& key, Vin&& val)		{ d.emplace(std::forward<Kin>(key), std::forward<Vin>(val)); }

		//template <class K, class V, class... Opts, class Kin, class Vin>
		//static void	Upsert(Container<K, V, Opts...>& d, K&& key, Vin&& val)		{ d.insert_or_assign(std::forward<Kin>(key), std::forward<Vin>(val)); }

		template <class K, class V, class... Opts>
		static V&		Access(Container<K, V, Opts...>& d, const K& key)			{ return d[key]; }
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
			template <class T>	using Container			 = OptResult<T>;	// this alias could adjust type (e.g. decay<T>)
			template <class T>	using DeducibleContainer = OptResult<T>;	// this one enables deducing a closed parameter

			template <class T, class Enumerator>
			static Container<T>	FromCurrent(Enumerator& et)
			{
				return OptResult<T>::FromFactory([&]() -> decltype(auto) { return et.Current(); });
			}

			template <class T>
			static Container<T>	NoValue(StopReason rs)	{ return rs; }

			template <class T>
			static bool			HasValue(const DeducibleContainer<T>& o) { return o.HasValue(); }
		};


		// STL doesn't have a small_vector (one with an inline buffer for initial elements, but being able to dynamically expand if needed)
		// - hence only a ListOp. fallback is provided, but the below code presents the way to utilize such type from your favorite library.
		struct FallbackSmallListOperations {
			template <class V, size_t InlineCap, class... Options>
			using Container = ListOperations::Container<V, Options...>;


			template <class V, size_t N, class... Options>
			static Container<V, N, Options...>	Init(size_t capacity = N, const Options&... opts)
			{ 
				return ListOperations::Init<V>(capacity, opts...);
			}

			template <class List>
			static size_t	GetSize(const List& l)		{ return ListOperations::GetSize(l); }

			template <class List, class Vin>
			static void		Add(List& l, Vin&& val)		{ ListOperations::Add(l, std::forward<Vin>(val)); }

			template <class List>
			static void		Clear(List& l)				{ ListOperations::Clear(l); }

			template <class List>
			static auto&	Access(List& l, size_t i)	{ return ListOperations::Access(l, i); }
		};

	}	// namespace DefaultBinding



#ifdef ENUMERABLES_OPTIONAL_BINDING
	using OptionalOperations = ENUMERABLES_OPTIONAL_BINDING;
#else
	using OptionalOperations = DefaultBinding::OptionalOperations;
#endif


#ifdef ENUMERABLES_SMALLLIST_BINDING
	using SmallListOperations = ENUMERABLES_SMALLARRAY_BINDING;

	// conditional: avoid duplicate overload - though deduction would fail anyway
	template <class V, size_t N, class... Opts>   size_t GetSize(const SmallListType<V, N, Opts...>& l)   { return SmallListOperations::GetSize(l); }

#	define ENUMERABLES_WARN_FOR_SMALLLIST 
#else
	using SmallListOperations = DefaultBinding::FallbackSmallListOperations;

	// Fallback is needed internally, but warn for usage on interface.
#	define ENUMERABLES_WARN_FOR_SMALLLIST	[[deprecated("No SmallList type defined, will use standard List type! Use ENUMERABLES_SMALLLIST_BINDING")]]
#endif



	// Some shortcuts
	template <class V, class... Opts>			using ListType		 = typename ListOperations::Container<V, Opts...>;
	template <class V, size_t N, class... Opts>	using SmallListType  = typename SmallListOperations::Container<V, N, Opts...>;
	template <class V, class... Opts>			using SetType		 = typename SetOperations::Container<V, Opts...>;
	template <class K, class V, class... Opts>	using DictionaryType = typename DictOperations::Container<K, V, Opts...>;
	template <class V>							using Optional		 = typename OptionalOperations::Container<V>;

	// "Import" GetSize's + HasValue defined in binding
	template <class V, class... Opts>			size_t GetSize(const ListOperations::DeducibleContainer<V, Opts...>& l)	{ return ListOperations::GetSize(l); }
	template <class V, class... Opts>			size_t GetSize(const SetType<V, Opts...>&  s)							{ return SetOperations::GetSize(s);	 }
	template <class K, class V, class... Opts>	size_t GetSize(const DictionaryType<K, V, Opts...>& d)					{ return DictOperations::GetSize(d); }
	
	template <class T>							bool   HasValue(const OptionalOperations::DeducibleContainer<T>& o)		{ return OptionalOperations::HasValue(o); }


	// NOTE: To improve performance by enabling size hints,
	//		 GetSize for further containers can be introduced by client code to namespace Enumerables.
	//		 Just like HasValue, to enable the .ValuesOnly() shorthand for multiple types. (Requires an operator* too.)
	// TODO 17: std::size


}	// namespace Enumerables


#endif	// ENUMERABLES_CONFIGDEFAULTS_HPP
