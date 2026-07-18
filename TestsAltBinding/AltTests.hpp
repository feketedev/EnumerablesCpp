#pragma once

#include "TestCompileSetup.hpp"



namespace EnumerableTests::AltBinding {

	void TestStdOptionalResults();
	void TestOrderedSetBindings();



	inline void RunAll()
	{
		TestStdOptionalResults();
		TestOrderedSetBindings();
	}

}
