#include "TestsAltClang.h"
#include "AltTests.hpp"
#include <iostream>



void RunAltTestsClang()
{
	std::cout << "Running tests of ALTERNATE Binding compiled by Clang." << std::endl;

	EnumerableTests::AltBinding::RunAll();
}
