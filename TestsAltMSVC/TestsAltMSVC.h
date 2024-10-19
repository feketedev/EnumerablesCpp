#pragma once


#ifdef TESTSALTMSVC_EXPORTS
#	define TESTSALT_MSVC_DLLEXPORT	__declspec(dllexport)
#else
#	define TESTSALT_MSVC_DLLEXPORT	__declspec(dllimport)
#endif



void   TESTSALT_MSVC_DLLEXPORT   RunAltTestsMsvc();
