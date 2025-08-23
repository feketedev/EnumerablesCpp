
#include "Tests.hpp"
#include <iostream>



int main(int argc, const char* argv[])
{
	std::cout << "Running tests compiled by MSVC." << std::endl;

	// simplification, implement when needed
	const char* execPath = argv[0];

	EnumerableTests::RunAll(execPath, argc, argv);

	std::cout << "Finished." << std::endl;
}
