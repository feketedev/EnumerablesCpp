#include "AltTests.hpp"
#include "TestUtils.hpp"
#include "EnumerablesAlt.hpp"



namespace EnumerableTests::AltBinding {

	static void BasicOptionalDemo()
	{
		std::vector<int> numVec { 1, 3, 4, 5, 6 };

		NO_MORE_HEAP;

		auto numbers = Enumerate(numVec);
		auto evens   = numbers.Where(FUN(x,  x % 2 == 0));
		auto fifths  = numbers.Where(FUN(x,  x % 5 == 0));

		// std::optional needs decayed type!
		// This is actually handled by the binding - in this setup: Enumerables::StlBinding::OptionalOperations.
		{
			ASSERT_ELEM_TYPE (int&, numbers);

			int&			   i4 = evens.First();
			std::optional<int> v4 = evens.FirstIfAny();
			std::optional<int> v5 = fifths.SingleIfAny();
			std::optional<int> ambig = evens.SingleOrNone();

			ASSERT    (v4.has_value());
			ASSERT    (v5.has_value());
			ASSERT    (!ambig.has_value());

			ASSERT_EQ (&numVec[2], &i4);
			ASSERT_EQ (4, i4);
			ASSERT_EQ (4, *v4);
			ASSERT_EQ (5, *v5);
		}

		// arithmetics
		{
			std::optional<int>    max = evens.Max();
			std::optional<double> avg = evens.Avg<double>();

			ASSERT_EQ (6,	max);
			ASSERT_EQ (5.0,	avg);

			std::optional<int>    max2 = evens.Min([](int& a, int& b) { return b < a; });
			std::optional<int>    noMax = Enumerables::Empty<int>().Max();
			std::optional<double> noAvg = Enumerables::Empty<int>().Avg<double>();

			ASSERT_EQ (6, max2);
			ASSERT    (!noMax.has_value());
			ASSERT    (!noAvg.has_value());
		}
			
		// move
		{
			auto moNums = Enumerables::Range<int>(5).As<MoveOnly<int>>();

			std::optional<MoveOnly<int>> mo3 = moNums.FirstIfAny(FUN(x,  x.data >= 3));

			ASSERT_EQ (3, mo3->data);
			ASSERT_EQ (1, mo3->moveCount);
		}

		// filtration shorthand
		{
			std::optional<int> optIntArr[] = { 1, std::nullopt, 2, 3, std::nullopt };

			auto optInts = Enumerate(optIntArr);

			ASSERT_ELEM_TYPE (std::optional<int>&, optInts);
			ASSERT_EQ		 (5, optInts.Count());

			auto valids = optInts.ValuesOnly();

			ASSERT_ELEM_TYPE (int&, valids);
			ASSERT			 (Enumerables::AreEqual({ 1, 2, 3 }, valids));
			ASSERT_EQ		 (&optIntArr[0].value(), &valids.First());
			ASSERT_EQ		 (&optIntArr[2].value(), &valids.Skip(1).First());

			// type nesting (optional<int>& must be decayed with std::optional)
			auto second = optInts.ElementAt(1);
			ASSERT_TYPE (std::optional<std::optional<int>>, second);
			ASSERT		(second.has_value());
			ASSERT_EQ	(std::nullopt, *second);
			ASSERT		(!second->has_value());
		}
	}



	// Relevant parts of FiltrationTests.hpp/TerminalOperators adjusted for std::optional
	static void TerminalOps()
	{
		NO_MORE_HEAP;

		int  arr[] = { 2, 3, 4, 5, 6 };
		auto nums  = Enumerate(arr);

		// fundamentals
		{
			std::optional<int> gt3 = nums.FirstIfAny  (FUN(x,  3 < x));
			std::optional<int> gt5 = nums.SingleIfAny (FUN(x,  5 < x));
			std::optional<int> gt6 = nums.SingleIfAny (FUN(x,  6 < x));
			std::optional<int> gt7 = nums.FirstIfAny  (FUN(x,  7 < x));
			std::optional<int> gt8 = nums.SingleOrNone(FUN(x,  8 < x));
			std::optional<int> amb = nums.SingleOrNone(FUN(x,  2 < x));
			std::optional<int> ovr = nums.ElementAt(5);
			std::optional<int> id4 = nums.ElementAt(4);
			std::optional<int> lst = nums.LastIfAny();

			ASSERT_EQ (arr[2], gt3);
			ASSERT_EQ (arr[4], gt5);
			ASSERT_EQ (arr[4], id4);
			ASSERT_EQ (arr[4], lst);

			ASSERT (!gt6.has_value());
			ASSERT (!gt7.has_value());
			ASSERT (!gt8.has_value());
			ASSERT (!amb.has_value());
			ASSERT (!ovr.has_value());
			ASSERT (!Enumerables::Empty<int>().LastIfAny().has_value());
		}

		// throwing fundamental cases
		{
			DisableClientBreaks assertGuard;

			try {
				nums.SingleIfAny(FUN(x,  x > 2));
				ASSERT (false);
			}
			catch (const Enumerables::LogicException&) {}

			try {
				Enumerables::Empty<int>().Last();
				ASSERT (false);
			}
			catch (const Enumerables::LogicException&) {}
		}
	}



	void TestStdOptionalResults()
	{
		Greet("Binding std::optional");
		RESULTSVIEW_DISABLES_ALLOCASSERTS;

		BasicOptionalDemo();
		TerminalOps();
	}

}	// namespace EnumerableTests::AltBinding
