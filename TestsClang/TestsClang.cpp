
#include "Tests.hpp"
#include "TestsAltClang/TestsAltClang.h"
#include <iostream>



int main(int argc, const char* argv[])
{
	RunAltTestsClang();

	std::cout << "Running tests compiled by Clang." << std::endl;
	
	// simplification, implement when needed
	const char* execPath = argv[0];

	EnumerableTests::RunAll(execPath, argc, argv);

	std::cout << "Finished." << std::endl;
}
