#pragma once


#ifdef TESTSALTCLANG_EXPORTS
#	define TESTSALT_CLANG_DLLEXPORT	__declspec(dllexport)
#else
#	define TESTSALT_CLANG_DLLEXPORT	__declspec(dllimport)
#endif



void   TESTSALT_CLANG_DLLEXPORT   RunAltTestsClang();
