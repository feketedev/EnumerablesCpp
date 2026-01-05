#pragma once

#include "TestCompileSetup.hpp"



namespace EnumerableTests {

	extern bool NoAssertMessages;	// Hide assert dialogs for automatic test runs.
	extern bool FailureDetected;	// Any assertion has failed. [No reset by individual tests.]

	void SetupCommonEnv(int argc, const char* argv[]);			// Resets failure

}
