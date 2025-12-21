#pragma once

// For allocation tests: Disable allocation on empty std::vector construction
#ifdef _DEBUG
#	ifdef __clang__
#		pragma clang diagnostic ignored "-Wreserved-id-macro"
#	endif
#	define _ITERATOR_DEBUG_LEVEL 0
#endif




namespace EnumerableTests {

	struct CmdOption;


	bool RunAll(const char myPath[], int argc, const char* argv[]);


	void LegacyTests(const char myPath[]);
	void LegacyPerfTests();

	void NewPerfTests(const CmdOption& summarizeTimes, const CmdOption& summarizeOverheads);


	void RunIntroduction1();
	void RunIntroduction2();
	void RunIntroduction3();

	void TestTypeHelpers();
	void TestOptResult();
	void TestConstruction();
	void TestLambdaUsage();
	void TestFiltration();
	void TestArithmetics();
	void TestMisc();
	void TestCollectionCustomizability();

}
