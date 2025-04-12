#include "Tests.hpp"
#include "TestUtils.hpp"
#include "Enumerables.hpp"



namespace EnumerableTests {

	
	//  --- Most importantly with C++20, co_yield has arrived! ---


	#pragma region Basic yielding

	// To have a usable coroutine, it must return CoEnumerator<T>.
	// This foundation is analogous to returning IEnumerator<T> in C#.

	static CoEnumerator<int>   YieldIntegers()
	{
		co_yield 1;
		co_yield 3;
		co_yield 4;
	}


	static CoEnumerator<CountedCopy<int>>   YieldHeavyInts()
	{
		CountedCopy<int> local = 1;
			
		co_yield 1;
			
		co_yield local;
			
		local.data++;
		co_yield std::move(local);
			
		co_yield CountedCopy<int>(5);
	}


	// NOTE: This is CTE currently, for .Current() calls must be repeatable.
	//		 ConsumeCurrent() could make it possible!
	// 
	// static CoEnumerator<MoveOnly<int>>	YieldUncopiableInts()
	// {
	//		co_yield 1;
	// }

	// in some cases changing the storage type might be a useful trick
	static CoEnumerator<MoveOnly<int>, int>   YieldUncopiableInts()
	{
		co_yield 1;
	}


	template <class S>
	static CoEnumerator<int>   YieldTriviallyFiltered(S source, int filterMultiples, int stopAt)
	{
		for (int i : source) {
			if (i == stopAt)
				co_return;
			if (i % filterMultiples > 0)
				co_yield i;
		}
	}


	static void TestYielding()
	{
		// Simple enumerator
		{
			CoEnumerator<int> et = YieldIntegers();

			ASSERT (et.FetchNext());
			ASSERT_EQ (1, et.Current());
			ASSERT (et.FetchNext());
			ASSERT_EQ (3, et.Current());
			ASSERT (et.FetchNext());
			ASSERT_EQ (4, et.Current());

			ASSERT (!et.FetchNext());
			ASSERT (!et.FetchNext());		// should be tolerant
		}

		// Custom intermediate filter
		{
			CoEnumerator<int> et = YieldTriviallyFiltered(Enumerables::Range<int>(10), 3, 7);

			ASSERT (et.FetchNext());
			ASSERT_EQ (1, et.Current());
			ASSERT (et.FetchNext());
			ASSERT_EQ (2, et.Current());
										   // 3 skipped
			ASSERT (et.FetchNext());
			ASSERT_EQ (4, et.Current());
			ASSERT (et.FetchNext());
			ASSERT_EQ (5, et.Current());
										  // 6 skipped
										  // 7 marks stop
			ASSERT (!et.FetchNext());
		}

		// move/copy test
		{
			CoEnumerator<CountedCopy<int>> et = YieldHeavyInts();

			ASSERT (et.FetchNext());

			CountedCopy<int>&& first = et.Current();
			ASSERT_EQ (1, first.data);
			ASSERT_EQ (1, first.copyCount);			// constructed in-place, copied result
			ASSERT_EQ (0, first.moveCount);

			ASSERT (et.FetchNext());
			CountedCopy<int>&& second = et.Current();
			ASSERT_EQ (1, second.data);
			ASSERT_EQ (2, second.copyCount);		// copied local to promise
			ASSERT_EQ (0, second.moveCount);

			ASSERT (et.FetchNext());
			CountedCopy<int>&& third = et.Current();
			ASSERT_EQ (2, third.data);
			ASSERT_EQ (1, third.copyCount);			// moved local
			ASSERT_EQ (1, third.moveCount);			//

			ASSERT (et.FetchNext());
			CountedCopy<int>&& fourth = et.Current();
			ASSERT_EQ (5, fourth.data);
			ASSERT_EQ (1, fourth.copyCount);		// moved temporary
			ASSERT_EQ (1, fourth.moveCount);		//

			ASSERT (!et.FetchNext());
		}

		// conversion test
		{
			// Only Storage [int] needs to be copied!
			CoEnumerator<MoveOnly<int>, int> et = YieldUncopiableInts();

			ASSERT	  (et.FetchNext());
			ASSERT_EQ (1, et.Current().data);
			ASSERT_EQ (0, et.Current().moveCount);	// simply converts on the fly
		
			ASSERT	  (!et.FetchNext());
		}
	}

	#pragma endregion



	#pragma region Exceptions

	static CoEnumerator<int>	ThrowOnSecond()
	{
		co_yield 1;

		throw std::logic_error("Test exception.");
		co_yield 2;
	}


	static void TestExceptions()
	{
		CoEnumerator<int> et = ThrowOnSecond();

		ASSERT	  (et.FetchNext());
		ASSERT_EQ (1, et.Current());

		ASSERT_THROW (std::logic_error, et.FetchNext());
	}

	#pragma endregion



	#pragma region Wrapping to Enumerable

	// Unfortunately, the creation of udnerlying state objects is tied to coroutine calls,
	// and the result (even std::generator of C++23) becomes exhausted after a single iteration
	// => to maintain reusability (aka. virtual collection-like behaviour) 
	//    Enumerables must wrap a pointer to the coroutine, and capture its desired arguments!
	//
	// Thus, in contrast to C# automatism, using a coroutine as an Enumerable requires a manual nesting step.


	static CoEnumerator<int>   YieldFrom(int s)
	{
		co_yield s;
		co_yield s + 1;
		co_yield s + 2;
	}

	static Enumerable<int>	WrapExplicitly()
	{
		// Enumerate can detect if its argument is a CoEnumerator coroutine.
		return Enumerate(&YieldIntegers);
	}

	static Enumerable<int>	WrapAutomatically()
	{
		// Auto conversion is also supported for parameterless calls.
		return &YieldIntegers;
	}


	static Enumerable<int>	WrapExplicitlyL(int start)
	{
		return Enumerate([=]() -> CoEnumerator<int>
		{
			co_yield start;
			co_yield start + 1;
			co_yield start + 2;
		});
	}

	// I consider this the most concise form:
	// Returning a lambda directly allows proper (user-controlled) capture 
	// of the required arguments, making the implicit conversion possible.
	static Enumerable<int>	WrapAutomaticallyL(int start)
	{
		return [=]() -> CoEnumerator<int>
		{
			co_yield start;
			co_yield start + 1;
			co_yield start + 2;
		};
	}


	static Enumerable<int>	WrapExplicitArgs(int start)
	{
		return Enumerate(&YieldFrom, start);
	}

	static Enumerable<int>	WrapExplicitArgsL(int start)
	{
		return Enumerate([](int x) -> CoEnumerator<int>
		{
			co_yield x;
			co_yield x + 1;
			co_yield x + 2;
		}, 
		start);
	}


	static void TestCoroutineWrapping()
	{
		using Enumerables::AreEqual;

		ASSERT (AreEqual({ 1, 3, 4 }, WrapAutomatically()));
		ASSERT (AreEqual({ 3, 4, 5 }, WrapAutomaticallyL(3)));
		ASSERT (AreEqual({ 1, 3, 4 }, WrapExplicitly()));
		ASSERT (AreEqual({ 3, 4, 5 }, WrapExplicitlyL(3)));
		ASSERT (AreEqual({ 3, 4, 5 }, WrapExplicitArgs(3)));
		ASSERT (AreEqual({ 3, 4, 5 }, WrapExplicitArgsL(3)));
	}

	#pragma endregion



	void TestFeatures20()
	{
		Greet("Features since C++20");
		RESULTSVIEW_DISABLES_ALLOCASSERTS;

		TestYielding();
		TestExceptions();
		TestCoroutineWrapping();
	}


}	// namespace EnumerableTests
