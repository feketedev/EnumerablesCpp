#include "Tests.hpp"
#include "TestUtils.hpp"



namespace EnumerableTests {


	void RunAll(const char myPath[], int argc, const char* argv[])
	{
		const bool quick				= FindCmdOption('Q', argc, argv);
		const auto summarizeTimes		= FindCmdOption('T', argc, argv);
		const auto summarizeOverheads	= FindCmdOption('O', argc, argv);

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

		if (!quick) {
			NewPerfTests(summarizeTimes, summarizeOverheads);
			// LegacyPerfTests();
		}
	}


}	// namespace EnumerableTests
