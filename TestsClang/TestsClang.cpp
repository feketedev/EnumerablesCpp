
#include "Tests.hpp"
#include <iostream>



int main(int /*argc*/, const char* argv[])
{
	std::cout << "Running tests compiled by Clang." << std::endl;

	EnumerableTests::RunAll(argv[0]);

	std::cout << "Finished." << std::endl;
}
