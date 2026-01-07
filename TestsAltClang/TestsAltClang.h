#pragma once


#ifdef TESTSALTCLANG_EXPORTS
#	define TESTSALT_CLANG_DLLEXPORT	__declspec(dllexport)
#else
#	define TESTSALT_CLANG_DLLEXPORT	__declspec(dllimport)
#endif



bool   TESTSALT_CLANG_DLLEXPORT   RunAltTestsClang(int argc, const char* argv[]);
