#pragma once

#ifdef __clang__
#	pragma clang diagnostic push
#	pragma clang diagnostic ignored "-Wreserved-id-macro"
#endif


// suppress when compiling in c++23 mode to have more range algorithms
#define _SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING

// For allocation tests: Disable allocation on empty std::vector construction
#ifdef _DEBUG
#	define _ITERATOR_DEBUG_LEVEL 0
#endif


#ifdef __clang__
#	pragma clang diagnostic pop
#endif
