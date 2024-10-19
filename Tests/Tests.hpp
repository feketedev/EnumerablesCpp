#pragma once

#include "TestCompileSetup.hpp"



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

	void TestFeatures17();



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
		TestFeatures17();

		NewPerfTests();
	 // LegacyPerfTests();
	}

}
