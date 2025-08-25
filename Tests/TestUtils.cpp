#include "TestUtils.hpp"
#include "Enumerables.hpp"		// ResultsView macros only
#include <new>
#include <cctype>
#include <cassert>


// imprecise check just for tests: in MSVC debug
#if defined(_CRT_ASSERT) && defined(_DEBUG)
#	define	TEST_FAIL_BREAK(txt, file, line)	_CrtDbgReport(_CRT_ASSERT, file, line, nullptr, txt)
#else
#	define	TEST_FAIL_BREAK(txt, file, line)	((void)txt,(void)file,(void)line, 1)
#	ifndef _CrtDbgBreak
#		define	_CrtDbgBreak() 
#	endif
#endif



namespace EnumerableTests {

	// Skip allocation checks if ResultsView is enabled for manual testing.
	static constexpr bool noResultsView = !ENUMERABLES_USE_RESULTSVIEW
									   || !ENUMERABLES_RESULTSVIEW_AUTO_EVAL;

	thread_local size_t   AllocationCounter::globalCount   = 0;
	const bool			  AllocationCounter::EnableAsserts = noResultsView;


	AllocationCounter::AllocationCounter() : myStart { globalCount }
	{
	}


	void AllocationCounter::AssertFreshCount(size_t expected, const char* file, long line)
	{
		if (EnableAsserts)
			AssertEq(expected, Count(), "Allocation count", file, line);
		Reset();
	}


	void AllocationCounter::AssertMaxFreshCount(size_t maxExpected, const char* file, long line)
	{
		if (EnableAsserts)
			Assert(maxExpected >= Count(), "Allocation count exceeded", file, line);
		Reset();
	}


	std::pair<bool, std::string> FindCmdOption(char letter, int argc, const char* argv[])
	{
		std::string trg1 = "-";
		std::string trg2 = trg1;
		trg1.push_back(static_cast<char>(std::toupper(letter)));
		trg2.push_back(static_cast<char>(std::tolower(letter)));

		for (int i = 1; i < argc; i++) {
			if (trg1 == argv[i] || trg2 == argv[i]) {
				if (argc > i + 1 && argv[i + 1][0] != '-')
					return { true, argv[i + 1] };
				else
					return { true, "" };
			}
		}
		return { false, "" };
	}


	void Greet(const char* testName)
	{
		std::cout << "  " << testName << std::endl;
	}


	void PrintFail(const char* txt, const char* file, long line)
	{
		std::cout << "    Test assertion failed:  " << txt << "\n    at: "
				  << file << ':' << line << std::endl;
	}


	void AskForBreak(const char* txt, const char* file, long line)
	{
		int err = TEST_FAIL_BREAK(txt, file, line);
		if (err < 0)
			std::exit(7);

		if (err > 0)
			_CrtDbgBreak();
	}



	static bool MaskClientBreaks = false;

	void MaskableClientBreak(const char* err)
	{
		if (MaskClientBreaks)
			return;

#	ifdef _MSC_VER
		_RPT0(_CRT_ASSERT, err);
#	else
		assert(false);
#	endif
		UNUSED (err);	// Release or non-MS
	}


	DisableClientBreaks::DisableClientBreaks() noexcept
	{
		MaskClientBreaks = true;
	}

	DisableClientBreaks::~DisableClientBreaks() noexcept
	{
		MaskClientBreaks = false;
	}



	bool Assert(bool cond, const char* txt, const char* file, long line)
	{
		if (cond)
			return true;

		PrintFail(txt, file, line);
		AskForBreak(txt, file, line);
		return false;
	}

}	// namespace EnumerableTests




void *operator new(size_t bytes)
{
	if(void* mem = std::malloc(bytes > 0 ? bytes : 1)) {
		++EnumerableTests::AllocationCounter::globalCount;
		return mem;
	}
	throw std::bad_alloc();
}


void operator delete(void* memory) noexcept
{
	std::free(memory);
}
