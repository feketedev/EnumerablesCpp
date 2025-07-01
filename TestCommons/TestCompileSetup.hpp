#pragma once

// For allocation tests: Disable allocation on empty std::vector construction
#ifdef _DEBUG
#	ifdef __clang__
#		pragma clang diagnostic ignored "-Wreserved-id-macro"
#	endif
#	define _ITERATOR_DEBUG_LEVEL 0
#endif


// TODO: find a convenient way - on master too
#define _ENFORCE_MATCHING_ALLOCATORS 0
