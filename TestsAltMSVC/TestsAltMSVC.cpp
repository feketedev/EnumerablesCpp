#include "TestsAltMSVC.h"
#include "AltTests.hpp"
#include "TestEnvironment.h"
#include <iostream>



bool RunAltTestsMsvc(int argc, const char* argv[])
{
	std::cout << "Running tests of ALTERNATE Binding compiled by MSVC." << std::endl;
	EnumerableTests::SetupCommonEnv(argc, argv);

	EnumerableTests::AltBinding::RunAll();

	if (EnumerableTests::FailureDetected) {
		std::cout << "Errors detected!" << std::endl;
		return false;
	}
	return true;
}
