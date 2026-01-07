#pragma once


#ifdef TESTSALTMSVC_EXPORTS
#	define TESTSALT_MSVC_DLLEXPORT	__declspec(dllexport)
#else
#	define TESTSALT_MSVC_DLLEXPORT	__declspec(dllimport)
#endif



bool   TESTSALT_MSVC_DLLEXPORT   RunAltTestsMsvc(int argc, const char* argv[]);
