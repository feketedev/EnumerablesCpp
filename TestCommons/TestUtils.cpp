#include "TestUtils.hpp"
#include <new>
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

	thread_local size_t		AllocationCounter::globalCount   = 0;
	bool					AllocationCounter::EnableAsserts = true;


	AllocationCounter::AllocationCounter() : myStart { globalCount }
	{
	}


	void AllocationCounter::AssertFreshCount(size_t expected, const char* file, long line)
	{
		if (EnableAsserts)
			AssertEq(expected, Count(), "Allocation count", file, line);
		Reset();
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

	void MaskableClientBreak([[maybe_unused]] const char* err)
	{
		if (MaskClientBreaks)
			return;

#	ifdef _MSC_VER
		_RPT0(_CRT_ASSERT, err);
#	else
		assert(false);
#	endif
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
