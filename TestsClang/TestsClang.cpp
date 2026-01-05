
#include "Tests.hpp"
#include "TestEnvironment.h"
#include "TestsAltClang/TestsAltClang.h"
#include <iostream>



int main(int argc, const char* argv[])
{
	// simplification, implement when needed
	const char* execPath = argv[0];

	const bool altOk = RunAltTestsClang(argc, argv);

	std::cout << "Running tests compiled by Clang." << std::endl;
	EnumerableTests::SetupCommonEnv(argc, argv);
	EnumerableTests::RunAll(execPath, argc, argv);
	
	const bool mainOk = !EnumerableTests::FailureDetected;
	if (altOk && mainOk) {
		std::cout << "Finished." << std::endl;
		return 0;
	}
	std::cout << (mainOk ? "Finished with errors in alternate binding tests." 
						 : "Finished with errors."							 )
			  << std::endl;
	return 1;
}
