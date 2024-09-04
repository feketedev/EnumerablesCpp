#ifndef ENUMERABLES_HPP
#define ENUMERABLES_HPP

	/*				Enumerables for C++  -  Custom configuration.			  *
	 *	 																	  *
	 *  This file belongs to the application project.						  *
	 *  Instantiates the library by including Enumerables_Implementation.hpp  *
	 *  after the definition of any custom settings / container bindings.	  *
	 *  For available settings see Enumerables_ConfigDefaults.hpp.			  */


// Customize debug features.
#ifdef _DEBUG
	#include <cassert>
	#define ENUMERABLES_INTERNAL_ASSERT(cond)	assert(cond)

	// These signal events unexpected by user code.
	// Implementation for tests: can bypass bypassing assertions for exception-tests.
	#define ENUMERABLES_CLIENT_BREAK(txt)		EnumerableTests::MaskableClientBreak(txt)

	// Automatically fill 'ResultsView' debug buffer for supported types (2 = at creation;  1 = on lvalue usage)
	// - now disabled for sake of allocation tests
	#define ENUMERABLES_RESULTSVIEW_AUTO_EVAL	0

	#define ENUMERABLES_RESULTSVIEW_MAX_ELEMS	20
#endif


// Customize optimization features
// #define ENUMERABLES_INTERFACED_ETOR_INLINE_SIZE	0


// Custom container bindings can be defined and set here.
// #define ENUMERABLES_SMALLLIST_BINDING			MyBindings::SomeLibrarySmallListBinding



// -- Instantiate the library after all config. --
#include "Enumerables_Implementation.hpp"


// Make the most fundamental identifiers available wherever Enumerables.hpp is included.
using Enumerables::Enumerable;
using Enumerables::Enumerate;

// Defaults to OptResult - why not use it, especially in C++14!
using Enumerables::Optional;


#endif	// ENUMERABLES_HPP
