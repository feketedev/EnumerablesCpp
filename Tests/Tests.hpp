#pragma once

// For allocation tests: Disable allocation on empty std::vector construction
#ifdef _DEBUG
#	ifdef __clang__
#		pragma clang diagnostic ignored "-Wreserved-id-macro"
#	endif
#	define _ITERATOR_DEBUG_LEVEL 0
#endif



namespace EnumerableTests {

	void LegacyTests(const char myPath[]);
	void LegacyPerfTests();

	void NewPerfTests(int argc, const char* argv[]);

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



	inline void RunAll(const char myPath[], int argc, const char* argv[])
	{
		LegacyTests(myPath);
		RunIntroduction1();
		RunIntroduction2();
		RunIntroduction3();
		TestConstruction();
		TestTypeHelpers();
		TestOptResult();
		TestFiltration();
		TestLambdaUsage();
		TestArithmetics();
		TestMisc();
		TestCollectionCustomizability();

		NewPerfTests(argc, argv);
	 // LegacyPerfTests();
	}

}
