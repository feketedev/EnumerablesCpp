#pragma once

#include "TestCompileSetup.hpp"



namespace EnumerableTests {

	struct CmdOption;


	void RunAll(const char myPath[], int argc, const char* argv[]);


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

	void TestFeatures17();
	void TestFeatures20();

}
