
#include "Tests.hpp"
#include "TestsAltMSVC/TestsAltMSVC.h"
#include <iostream>



int main(int /*argc*/, const char* argv[])
{
	RunAltTestsMsvc();

	std::cout << "Running tests compiled by MSVC." << std::endl;

	EnumerableTests::RunAll(argv[0]);

	std::cout << "Finished." << std::endl;
}
