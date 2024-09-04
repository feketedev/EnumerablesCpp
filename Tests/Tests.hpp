#pragma once

#ifdef __clang__
#	pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#	pragma clang diagnostic ignored "-Wreturn-std-move-in-c++11"
#	pragma clang diagnostic ignored "-Wreserved-id-macro"
#endif

// For allocation tests: Disable allocation on empty std::vector construction
#ifdef _DEBUG
#	define _ITERATOR_DEBUG_LEVEL 0
#endif


namespace EnumerableTests {

	void LegacyTests(const char myPath[]);
	void LegacyPerfTests();

	void NewPerfTests();

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



	inline void RunAll(const char myPath[])
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

		NewPerfTests();
	 // LegacyPerfTests();
	}

}
