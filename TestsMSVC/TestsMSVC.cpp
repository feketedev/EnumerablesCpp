
#include "Tests.hpp"
#include "TestsAltMSVC/TestsAltMSVC.h"
#include <iostream>



int main(int argc, const char* argv[])
{
	RunAltTestsMsvc();

	std::cout << "Running tests compiled by MSVC." << std::endl;

	// simplification, implement when needed
	const char* execPath = argv[0];

	EnumerableTests::RunAll(execPath, argc, argv);

	std::cout << "Finished." << std::endl;
}
