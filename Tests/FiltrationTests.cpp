
#include "Tests.hpp"
#include "TestUtils.hpp"
#include "Enumerables.hpp"



namespace EnumerableTests {

	using Enumerables::Def::AutoEnumerable;

	template <class F, class Elem = typename AutoEnumerable<F>::TElem>
	static void AssertEndsLength(const AutoEnumerable<F>& eb, const Elem& first, const Elem& last, size_t length)
	{
		ASSERT_EQ (first,  eb.First());
		ASSERT_EQ (last,   eb.Last());
		ASSERT_EQ (length, eb.Count());
	}



	static void SetOperationsSimple()
	{
		int numsArr[] = { 2, 3, 4, 5, 6, 7, 8, 9, 2, 0 };
		int oddsArr[] = { 1, 3, 5, 7, 9 };

		// basic - lvalue lists captured by reference
		{
			auto nonOdds = Enumerate(numsArr).Except(oddsArr);
			ASSERT_EQ (6, nonOdds.Count());
			ASSERT_EQ (2, nonOdds.First());
			ASSERT_EQ (0, nonOdds.Last());
			ASSERT_EQ (2, nonOdds.Count(2));

			auto odds = Enumerate(numsArr).Intersect(oddsArr);
			ASSERT_EQ (4, odds.Count());
			ASSERT_EQ (3, odds.First());
			ASSERT_EQ (9, odds.Last());
			ASSERT_EQ (0, odds.Count(1));
			ASSERT_EQ (1, odds.Count(5));
		}

		// basic - same with modifications underneeth
		{
			std::vector<int> numsVec		{ std::begin(numsArr), std::end(numsArr) };
			std::vector<int> exclusionsVec	{ std::begin(oddsArr), std::end(oddsArr) };

			auto res = Enumerate(numsVec).Except(exclusionsVec);
			ASSERT_EQ (6, res.Count());
			ASSERT_EQ (2, res.First());
			ASSERT_EQ (0, res.Last());
			ASSERT_EQ (2, res.Count(2));

			numsVec.push_back(2);
			ASSERT_EQ (2, res.Last());
			ASSERT_EQ (3, res.Count(2));

			exclusionsVec.push_back(2);
			ASSERT_EQ (0, res.Last());
			ASSERT_EQ (0, res.Count(2));
			ASSERT_EQ (4, res.Count());
		}

		// basic - referencing an available set
		{

			struct FilteredList {
				std::vector<int>		entries;
				std::unordered_set<int> invalid;

				Enumerable<int>   ValidEntries() const	{ return Enumerate(entries).Except(invalid); }
			};

			FilteredList myObj { { 1, 2, 3, 4, 5 } };

			{
				NO_MORE_HEAP;

				ASSERT_EQ(1, myObj.ValidEntries().First());
				ASSERT_EQ(5, myObj.ValidEntries().Last());
				ASSERT_EQ(5, myObj.ValidEntries().Count());
			}

			myObj.invalid = { 3, 5, 6 };

			{
				NO_MORE_HEAP;

				ASSERT_EQ(1, myObj.ValidEntries().First());
				ASSERT_EQ(4, myObj.ValidEntries().Last());
				ASSERT_EQ(3, myObj.ValidEntries().Count());
			}
		}

		// value capture
		{
			auto getFilteredNums = [&numsArr]() -> Enumerable<int>
			{
				// init list -> unodrered_set overload
				return Enumerate(numsArr).Except({ 3, 5, 7, 9 });
			};

			Enumerable<int> evenNums = getFilteredNums();

			NO_MORE_HEAP;

			ASSERT_EQ (2, evenNums.Count(2));	// .ToReferenced prevents internal copy
			ASSERT_EQ (6, evenNums.Count());
			ASSERT_EQ (2, evenNums.First());
			ASSERT_EQ (0, evenNums.Last());
		}
	}



	static void SetOperationsByReference()
	{
		using Obj = std::pair<char, int>;

		std::vector<Obj> objList { { 'a',  1 },
								   { 'b',  1 },
								   { 'c',  2 },
								   { 'a',  5 },
								   { 'a',  1 } };

		// T& sequence -> filter by Set<T>
		{
			const std::unordered_set<Obj> subset { { 'c',  2 }, { 'a',  1 } };

			NO_MORE_HEAP;

			auto common    = Enumerate(objList).Intersect(subset);
			auto remaining = Enumerate(objList).Except(subset);

			ASSERT_EQ (3,					common.Count());
			ASSERT_EQ (objList.size() - 3,	remaining.Count());
			ASSERT_EQ (Obj('a', 1),			common.Last());
			ASSERT_EQ (Obj('a', 5),			remaining.Last());
			ASSERT_EQ (Obj('a', 1),			common.First());
			ASSERT_EQ (Obj('b', 1),			remaining.First());
		}
		// Same with non-set operands
		{
			Obj				 subsetArr[] = { { 'c',  2 }, { 'a',  1 }, { 'c',  2 } };
			std::vector<Obj> subsetVec	 = { { 'c',  2 }, { 'a',  1 }, { 'c',  2 } };

			auto common1    = Enumerate(objList).Intersect(subsetArr);
			auto common2    = Enumerate(objList).Intersect(subsetVec);
			auto remaining1 = Enumerate(objList).Except(subsetArr);
			auto remaining2 = Enumerate(objList).Except(subsetVec);

			ASSERT_EQ (3,					common2.Count());
			ASSERT_EQ (3,					common1.Count());
			ASSERT_EQ (objList.size() - 3,	remaining1.Count());
			ASSERT_EQ (objList.size() - 3,	remaining2.Count());
			ASSERT_EQ (Obj('a', 1),			common1.Last());
			ASSERT_EQ (Obj('a', 1),			common2.Last());
			ASSERT_EQ (Obj('a', 5),			remaining1.Last());
			ASSERT_EQ (Obj('a', 5),			remaining2.Last());
			ASSERT_EQ (Obj('a', 1),			common1.First());
			ASSERT_EQ (Obj('a', 1),			common2.First());
			ASSERT_EQ (Obj('b', 1),			remaining1.First());
			ASSERT_EQ (Obj('b', 1),			remaining2.First());
		}

		// T& -> T* sequence -> filter by Set<T*>
		// (Probably best to stay explicit working with .Addresses())
		{
			const std::unordered_set<Obj*> subset { &objList[2], &objList[0] };

			NO_MORE_HEAP;

			auto common    = Enumerate(objList).Addresses().Intersect(subset);
			auto remaining = Enumerate(objList).Addresses().Except(subset);

			ASSERT_EQ (2,					common.Count());
			ASSERT_EQ (objList.size() - 2,	remaining.Count());
			ASSERT_EQ (&objList[2],			common.Last());
			ASSERT_EQ (&objList.back(),		remaining.Last());
			ASSERT_EQ (&objList.front(),	common.First());
			ASSERT_EQ (&objList[1],			remaining.First());
		}
		// Same with non-set operands
		{
			Obj*			  subsetArr[] = { &objList[2], &objList[0] };
			std::vector<Obj*> subsetVec   = { &objList[2], &objList[0] };

			auto common1    = Enumerate(objList).Addresses().Intersect(subsetArr);
			auto common2    = Enumerate(objList).Addresses().Intersect(subsetVec);
			auto remaining1 = Enumerate(objList).Addresses().Except(subsetArr);
			auto remaining2 = Enumerate(objList).Addresses().Except(subsetVec);

			ASSERT_EQ (2,					common2.Count());
			ASSERT_EQ (2,					common1.Count());
			ASSERT_EQ (objList.size() - 2,	remaining1.Count());
			ASSERT_EQ (objList.size() - 2,	remaining2.Count());
			ASSERT_EQ (&objList[2],			common1.Last());
			ASSERT_EQ (&objList[2],			common2.Last());
			ASSERT_EQ (&objList.back(),		remaining1.Last());
			ASSERT_EQ (&objList.back(),		remaining2.Last());
			ASSERT_EQ (&objList.front(),	common1.First());
			ASSERT_EQ (&objList.front(),	common2.First());
			ASSERT_EQ (&objList[1],			remaining1.First());
			ASSERT_EQ (&objList[1],			remaining2.First());
		}
	}



	static void SimpleSubrange()
	{
		NO_MORE_HEAP;

		int numsArr[] = { 1, 2, 3, 4, 5, 6, 7 };

		auto nums = Enumerate(numsArr);

		auto nums24    = nums.Skip(1).Take(2);
		auto numsFirst = nums.Skip(0).Take(1);
		auto numsLast  = nums.Skip(6).Take(1);
		auto numsEmpt1 = nums.Skip(7).Take(1);
		auto numsEmpt2 = nums.Skip(8).Take(1);
		auto numsEmpt3 = nums.Skip(1).Take(0);

		AssertEndsLength(nums24,	2, 3, 2);
		AssertEndsLength(numsFirst,	1, 1, 1);
		AssertEndsLength(numsLast,	7, 7, 1);
		ASSERT (!numsEmpt1.Any());
		ASSERT (!numsEmpt2.Any());
		ASSERT (!numsEmpt3.Any());
	}



	static void ConditionalSubrange()
	{
		NO_MORE_HEAP;

		auto nums = Enumerables::RangeBetween(1, 7);

		auto post4 = nums.SkipUntil(FUN(x, x == 4));
		auto pre4  = nums.TakeWhile(FUN(x, x != 4));
		auto till4 = nums.TakeUntilFinal(FUN(x, x == 4));

		AssertEndsLength(post4,	4, 7, 4);
		AssertEndsLength(pre4,	1, 3, 3);
		AssertEndsLength(till4,	1, 4, 4);

		auto unaffacted1 = nums.SkipUntil(FUN(x, x == 1));
		auto unaffacted2 = nums.TakeWhile(FUN(x, x != 99));
		auto unaffacted3 = nums.TakeUntilFinal(FUN(x, x == 99));

		AssertEndsLength(unaffacted1, 1, 7, 7);
		AssertEndsLength(unaffacted2, 1, 7, 7);
		AssertEndsLength(unaffacted3, 1, 7, 7);

		auto empty1 = nums.SkipUntil(FUN(x, x == 99));
		auto empty2 = nums.TakeWhile(FUN(x, x != 1));
		auto first1 = nums.TakeUntilFinal(FUN(x, x == 1));

		ASSERT	  (!empty1.Any());
		ASSERT	  (!empty2.Any());
		ASSERT_EQ (1, first1.First());

		// exercise .Measure too
		ASSERT_EQ (0, empty1.Count());
		ASSERT_EQ (0, empty2.Count());
		ASSERT_EQ (1, first1.Count());
	}



	static void TerminalOperators()
	{
		NO_MORE_HEAP;

		int  arr[] = { 2, 3, 4, 5, 6 };
		auto nums  = Enumerate(arr);

		// fundamentals
		{
			ASSERT (nums.Any());
			ASSERT (nums.Any(FUN(x,  4 < x)));
			ASSERT (nums.All(FUN(x,  1 < x)));
			ASSERT (nums.All(FUN(x,  1 < x)));

			int& gt2 = nums.First (FUN(x,  2 < x));
			int& eq2 = nums.Single(FUN(x,  2 == x));

			ASSERT_EQ (&arr[1], &gt2);
			ASSERT_EQ (&arr[0], &eq2);

			Optional<int&> gt3 = nums.FirstIfAny  (FUN(x,  3 < x));
			Optional<int&> gt5 = nums.SingleIfAny (FUN(x,  5 < x));
			Optional<int&> gt6 = nums.SingleIfAny (FUN(x,  6 < x));
			Optional<int&> gt7 = nums.FirstIfAny  (FUN(x,  7 < x));
			Optional<int&> gt8 = nums.SingleOrNone(FUN(x,  8 < x));
			Optional<int&> amb = nums.SingleOrNone(FUN(x,  2 < x));
			Optional<int&> ovr = nums.ElementAt(5);
			Optional<int&> id4 = nums.ElementAt(4);
			Optional<int&> lst = nums.LastIfAny();

			ASSERT_EQ (&arr[2], &gt3.Value());
			ASSERT_EQ (&arr[4], &gt5.Value());
			ASSERT_EQ (&arr[4], &id4.Value());
			ASSERT_EQ (&arr[4], &lst.Value());

			ASSERT (!gt6.HasValue());
			ASSERT (!gt7.HasValue());
			ASSERT (!gt8.HasValue());
			ASSERT (!amb.HasValue());
			ASSERT (!ovr.HasValue());
			ASSERT (!Enumerables::Empty<int>().LastIfAny().HasValue());
		}

		// throwing fundamental cases
		{
			DisableClientBreaks assertGuard;

			auto selNone  = FUN(x,  x == 99);
			auto selMulti = FUN(x,  x > 2);

			try {
				nums.First(selNone);
				ASSERT (false);
			}
			catch (const Enumerables::LogicException&) {}

			try {
				nums.Single(selNone);
				ASSERT (false);
			}
			catch (const Enumerables::LogicException&) {}

			try {
				nums.Single(selMulti);
				ASSERT (false);
			}
			catch (const Enumerables::LogicException&) {}

			try {
				nums.SingleIfAny(selMulti);
				ASSERT (false);
			}
			catch (const Enumerables::LogicException&) {}

			try {
				Enumerables::Empty<int>().Last();
				ASSERT (false);
			}
			catch (const Enumerables::LogicException&) {}
		}

		// counting + equality
		{
			ASSERT_EQ (5, nums.Count());
			ASSERT_EQ (5, nums.Count(FUN(x,  x > 0)));
			ASSERT_EQ (3, nums.Count(FUN(x,  x > 3)));
			ASSERT_EQ (0, nums.Count(FUN(x,  x > 99)));

			ASSERT_EQ (1, nums.Count(5));
			ASSERT_EQ (0, nums.Count(7));

			ASSERT_EQ (&arr[0], &*nums.ElementAt(0));
			ASSERT_EQ (&arr[4], &*nums.ElementAt(4));
			ASSERT_EQ (&arr[4], &nums.Last());

			auto fives = Enumerables::Repeat(5, 3u);

			ASSERT_EQ (3, fives.Count());
			ASSERT_EQ (3, fives.Count(5));
			ASSERT_EQ (0, fives.Count(4));

			ASSERT (fives.AllEqual());
			ASSERT (fives.AllEqual(5));
			ASSERT (!fives.AllEqual(4));
			ASSERT (!nums.AllEqual());
			ASSERT (!nums.AllEqual(2));

			ASSERT (nums.Contains(4));
			ASSERT (!nums.Contains(99));
			ASSERT (!fives.Contains(4));
			ASSERT (fives.Contains(5));

			ASSERT (fives.AllNeighbors(FUN(p,n,  p == n)));
			ASSERT (nums.AllNeighbors(FUN(p,n,	 p + 1 == n)));
			ASSERT (nums.AnyNeighbors(FUN(p,n,	 p + n < 6)));
			ASSERT (!nums.AnyNeighbors(FUN(p,n,	 p + n < 5)));
		}

	}



	static void Shorthands()
	{
		NO_MORE_HEAP;

		using Enumerables::OptResult;
		using Enumerables::StopReason;

		{
			OptResult<int> optInts[] = { 1, 2, StopReason::Empty, 5, StopReason::Empty };

			auto optionals = Enumerate(optInts);
			auto ints	   = optionals.ValuesOnly();

			ASSERT_ELEM_TYPE (OptResult<int>&, optionals);
			ASSERT_ELEM_TYPE (int&,			   ints);

			AssertEndsLength(ints, 1, 5, 3);


			auto toSqr = [](const OptResult<int>& opt)	{ return opt.MapValue(FUN(x,  x * x)); };

			auto optSqrs = optionals.Map(toSqr);
			auto sqrVals = optSqrs.ValuesOnly();

			ASSERT_ELEM_TYPE (OptResult<int>, optSqrs);
			ASSERT_ELEM_TYPE (int,			  sqrVals);

			AssertEndsLength(sqrVals, 1, 25, 3);
		}

		{
			int targets[] = { 1, 2, 3 };

			int* ptrs[] = { nullptr, targets, targets + 2, nullptr };

			auto pointers = Enumerate(ptrs);
			auto intPtrs  = pointers.NonNulls();
			auto values	  = intPtrs.Dereference();

			ASSERT_ELEM_TYPE (int*&, pointers);
			ASSERT_ELEM_TYPE (int*&, intPtrs);
			ASSERT_ELEM_TYPE (int&,  values);

			ASSERT (pointers.Contains(nullptr));
			ASSERT (!intPtrs.Contains(nullptr));
			AssertEndsLength(values, 1, 3, 2);
		}
	}



	void TestFiltration()
	{
		Greet("Filtrations");

		SetOperationsSimple();
		SetOperationsByReference();
		SimpleSubrange();
		ConditionalSubrange();
		TerminalOperators();
		Shorthands();
	}

}	// namespace EnumerableTests
