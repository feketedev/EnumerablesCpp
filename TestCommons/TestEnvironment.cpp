#include "TestEnvironment.h"
#include "TestUtils.hpp"



namespace EnumerableTests {

	bool NoAssertMessages = false;
	bool FailureDetected  = false;


	void SetupCommonEnv(int argc, const char* argv[])
	{
		FailureDetected  = false;
		NoAssertMessages = FindCmdOption('N', argc, argv);
	}

}
