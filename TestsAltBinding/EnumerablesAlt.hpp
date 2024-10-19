#ifndef ENUMERABLES_ALT_HPP
#define ENUMERABLES_ALT_HPP

	/*  	  Enumerables for C++  -  Custom configuration.			*
	 *   															*
	 *  Alternate version to demonstrate configurability for usage	*
	 *  with non-default result types - for example std::optional.	*
	 *  For available settings see Enumerables_ConfigDefaults.hpp.	*/


#include <optional>

#ifdef _DEBUG
	#include <cassert>
	#define ENUMERABLES_INTERNAL_ASSERT(cond)	assert(cond)
	#define ENUMERABLES_CLIENT_BREAK(txt)		EnumerableTests::MaskableClientBreak(txt)

	// 2 = at creation;  1 = on lvalue usage; Now disabled for sake of allocation tests
	#define ENUMERABLES_RESULTSVIEW_AUTO_EVAL	0
	#define ENUMERABLES_RESULTSVIEW_MAX_ELEMS	10
#endif


// Custom container bindings can be defined and set here.
// 
// StlBinding::OptionalOperations is readily made available whenever <optional> is included,
// but an own binding struct could be defined here as well.
#define ENUMERABLES_OPTIONAL_BINDING	Enumerables::StlBinding::OptionalOperations;


// -- Instantiate the library after all config. --
#include "Enumerables_Implementation.hpp"


// Make the most fundamental identifiers available wherever EnumerablesAlt.hpp is included.
using Enumerables::Enumerable;
using Enumerables::Enumerate;


#endif	// ENUMERABLES_ALT_HPP
