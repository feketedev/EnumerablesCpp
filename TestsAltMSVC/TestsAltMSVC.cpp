#include "TestsAltMSVC.h"
#include "AltTests.hpp"
#include <iostream>



void RunAltTestsMsvc()
{
	std::cout << "Running tests of ALTERNATE Binding compiled by MSVC." << std::endl;

	EnumerableTests::AltBinding::RunAll();
}
