#pragma once

#include "TestCompileSetup.hpp"



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

	void TestFeatures17();
	void TestFeatures20();



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
		TestFeatures17();
		TestFeatures20();

		NewPerfTests(argc, argv);
	 // LegacyPerfTests();
	}

}
