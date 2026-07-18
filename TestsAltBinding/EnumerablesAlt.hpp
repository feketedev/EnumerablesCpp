#ifndef ENUMERABLES_ALT_HPP
#define ENUMERABLES_ALT_HPP

	/*  	  Enumerables for C++  -  Custom configuration.			*
	 *   															*
	 *  Alternate version to demonstrate configurability for usage	*
	 *  with non-default result types - for example std::optional.	*
	 *  For available settings see Enumerables_ConfigDefaults.hpp.	*/


#include <optional>
#include <set>
#include <map>

#ifdef _DEBUG
	#include <cassert>
	#define ENUMERABLES_INTERNAL_ASSERT(cond)	assert(cond)
	#define ENUMERABLES_CLIENT_BREAK(txt)		EnumerableTests::MaskableClientBreak(txt)

	// 2 = at non-trivial creation;  1 = on lvalue usage; Now disabled for sake of allocation tests
	#define ENUMERABLES_RESULTSVIEW_AUTO_EVAL	0
	#define ENUMERABLES_RESULTSVIEW_MAX_ELEMS	10
#endif


// Custom container bindings can be defined and set here.
//
// StlBinding::OptionalOperations is readily made available whenever <optional> is included.
// Similarly, alternative bindings in StlBinding::Ordered are available for <set> and <map>.
// [Custom binding structs could be defined here just as well.]
#define ENUMERABLES_OPTIONAL_BINDING	Enumerables::StlBinding::OptionalOperations;
#define ENUMERABLES_SET_BINDING			Enumerables::StlBinding::Ordered::SetOperations;
#define ENUMERABLES_DICTIONARY_BINDING	Enumerables::StlBinding::Ordered::DictionaryOperations;



// For custom types, you might want to define appropriate GetSize / HasValue overloads.
// These are allowed extensions of the Enumerables namespace. (Consider supporting std::size instead!)
namespace Enumerables
{
	//template <class T>
	//auto GetSize(const MyContainer<T>& c)	{ return c.GetMySize(); }

	//template <class T>
	//bool HasValue(const MyOptional<T>& o)	{ return o.HasValue();	}
}



// For auto-testing this project: allow force-enabling ResultsView from command-line
#ifdef TEST_RESULTSVIEW_LVL
	#pragma warning (disable : 4005)			// redefined macros
	#define ENUMERABLES_USE_RESULTSVIEW			true
	#define ENUMERABLES_RESULTSVIEW_AUTO_EVAL	TEST_RESULTSVIEW_LVL
#endif



// -- Instantiate the library after all config. --
#include "Enumerables_Implementation.hpp"


// Make the most fundamental identifiers available wherever EnumerablesAlt.hpp is included.
using Enumerables::Enumerable;
using Enumerables::Enumerate;


#endif	// ENUMERABLES_ALT_HPP
