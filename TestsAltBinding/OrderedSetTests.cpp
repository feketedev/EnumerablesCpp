#include "AltTests.hpp"
#include "TestUtils.hpp"
#include "TestAllocator.hpp"
#include "EnumerablesAlt.hpp"



namespace EnumerableTests::AltBinding {

	namespace {

		// A simple pair-like structure with no hash
		struct Record {
			unsigned	id;
			char		data;

			// default order: id assumed unique
			bool operator <(const Record& rhs) const { return id < rhs.id; }

			// helper: a complete comparison of contents
			struct TotalOrder;
		};


		struct Record::TotalOrder {
			bool operator ()(const Record& r1, const Record& r2) const
			{
				return std::tie(r1.id, r1.data) < std::tie(r2.id, r2.data);
			}
		};

	}



	// Check .ToSet over std::set, briefly trying custom comparers and allocators
	static void SetCreation()
	{
		Record recordStore[] = {{ 1, 'a' }, { 2, 'd' }, { 1, 'b' }, { 5, 'e' }, { 2, 'c' }};

		auto records = Enumerate(recordStore);

		// Using defaults
		std::set<Record> distinctById = records.ToSet();
		ASSERT_EQ (3, distinctById.size());
		ASSERT    (distinctById.find({ 1, 'a' }) != distinctById.end());
		ASSERT    (distinctById.find({ 2, 'c' }) != distinctById.end());
		ASSERT    (distinctById.find({ 5, 'e' }) != distinctById.end());


		// Using custom comparer
		std::set<Record, Record::TotalOrder> distincts = records.ToSet<Record::TotalOrder>();
		ASSERT_EQ (5, distincts.size());
		ASSERT    (records.All(FUN(r, distincts.find(r) != distincts.end())));


		// Passing a stateful allocator
		NO_MORE_HEAP;
		std::aligned_storage_t<sizeof(Record), alignof(void*)>	buffer[30 + IFNO_NRVO(8)];
		TestAllocator<Record, 4 + IFNO_NRVO(1)>					fixedAlloc { buffer };

		std::set<Record, std::less<>, decltype(fixedAlloc)> distinctById2 = records.ToSet(3u, std::less<>{}, fixedAlloc);
		ASSERT (EqualSets(distinctById, distinctById2));
	}


	// Check .ToDictionary over std::map, briefly trying custom comparers and allocators
	static void MapCreation()
	{
		Record recordStore[] = {{ 1, 'a' }, { 2, 'd' }, { 1, 'b' }, { 5, 'e' }, { 2, 'c' }};
		char   extData[]     = { 'i', 'j', 'k', 'l', 'm' };

		auto getExtData = [&extData](const Record& r) { return extData[r.id - 1]; };

		auto records = Enumerate(recordStore);

		// Using defaults
		std::map<Record, char> distinctById = records.ToDictionary(FUN(r, r), getExtData);
		ASSERT_EQ (3,   distinctById.size());
		ASSERT_EQ ('i', distinctById.at({ 1, 'a' }));	// Record.data is actually ignored,
		ASSERT_EQ ('j', distinctById.at({ 2, 'c' }));	// just a weird testcase.
		ASSERT_EQ ('m', distinctById.at({ 5, 'e' }));

		// Using custom comparer
		std::map<Record, char, Record::TotalOrder> distincts = records.ToDictionary<Record::TotalOrder>(FUN(r, r), getExtData);
		ASSERT_EQ (5, distincts.size());
		ASSERT    (records.All(FUN(r, distincts.find(r) != distincts.end())));
		ASSERT_EQ ('i', distincts.at({ 1, 'a' }));
		ASSERT_EQ ('i', distincts.at({ 1, 'b' }));		// using TotalOrder, Record.data counts.
		ASSERT_EQ ('j', distincts.at({ 2, 'c' }));
		ASSERT_EQ ('j', distincts.at({ 2, 'd' }));
		ASSERT_EQ ('m', distincts.at({ 5, 'e' }));
		ASSERT_EQ (distincts.end(), distincts.find({ 1, 'w' }));

		// Passing a stateful allocator
		NO_MORE_HEAP;
		using Pair = std::pair<const Record, char>;
		std::aligned_storage_t<sizeof(Pair), alignof(void*)>	buffer[30 + IFNO_NRVO(8)];
		TestAllocator<Pair, 4 + IFNO_NRVO(1)>					fixedAlloc { buffer };

		std::map<Record, char, std::less<>, decltype(fixedAlloc)> distinctById2 =
			records.ToDictionary(FUN(r, r), getExtData, 3u, std::less<>{}, fixedAlloc);
		ASSERT_EQ (3, distinctById.size());
		for (const Pair& p : distinctById2) {
			ASSERT_EQ (distinctById.at(p.first), p.second);
		}
	}


	// copy from FiltrationTests.cpp, but with std::set
	static void SetOperationsSimple()
	{
		int numsArr[] = { 2, 3, 4, 5, 6, 7, 8, 9, 2, 0 };
		int oddsArr[] = { 1, 3, 5, 7, 9 };
		const int oddsConst[] = { 1, 3, 5, 7, 9 };

		// basic - lvalue lists captured by reference
		{
			auto nonOdds = Enumerate(numsArr).Except(oddsArr);
			ASSERT_EQ (6, nonOdds.Count());
			ASSERT_EQ (2, nonOdds.First());
			ASSERT_EQ (0, nonOdds.Last());
			ASSERT_EQ (2, nonOdds.Count(2));
			ASSERT_ELEM_TYPE (int&, nonOdds);

			auto odds = Enumerate(numsArr).Intersect(oddsArr);
			ASSERT_EQ (4, odds.Count());
			ASSERT_EQ (3, odds.First());
			ASSERT_EQ (9, odds.Last());
			ASSERT_EQ (0, odds.Count(1));
			ASSERT_EQ (1, odds.Count(5));
			ASSERT_ELEM_TYPE (int&, odds);

			auto copNonOdds = Enumerate(numsArr).Except(oddsConst);
			auto copOdds = Enumerate(numsArr).Intersect(oddsConst);

			ASSERT (AreEqual(nonOdds, copNonOdds));
			ASSERT (AreEqual(odds,    copOdds));
			ASSERT_ELEM_TYPE (int&, copNonOdds);
			ASSERT_ELEM_TYPE (int&, copOdds);
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

			ASSERT_ELEM_TYPE (int&, res);
		}

		// basic - referencing an available set
		{
			struct FilteredList {
				std::vector<int>	entries;
				std::set<int>		invalid;

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

			// check mixing mutable + const
			{
				NO_MORE_HEAP;					// <-- the actual point here!

				std::vector<int>&		entries = myObj.entries;
				const std::set<int>&	invalid = myObj.invalid;

				auto copRes = Enumerate(entries).Except(invalid);
				auto cRes   = Enumerate(entries).AsConst().Except(myObj.invalid);
				auto ccRes  = Enumerate(entries).AsConst().Except(invalid);

				ASSERT_ELEM_TYPE (int&, copRes);
				ASSERT_ELEM_TYPE (const int&, cRes);
				ASSERT_ELEM_TYPE (const int&, ccRes);

				ASSERT (AreEqual(myObj.ValidEntries(), copRes));
				ASSERT (AreEqual(copRes, cRes));
				ASSERT (AreEqual(copRes, ccRes));
			}
		}

		// value capture
		{
			auto getFilteredNums = [&numsArr]() -> Enumerable<int>
			{
				// init list -> SetType overload [relies on the container to implement it]
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


	// copy from FiltrationTests.cpp, tailored for std::set
	static void SetOperationsByReference()
	{
		// a pair having no hash
		struct Obj {
			char first;
			int  second;

			Obj(char f, int s) : first { f }, second { s } {}

			bool operator <(const Obj& rh) const
			{
				return first < rh.first
					|| first == rh.first && second < rh.second;
			}
		};

		std::vector<Obj> objList { { 'a', 1 },
								   { 'b', 1 },
								   { 'c', 2 },
								   { 'a', 5 },
								   { 'a', 1 } };

		// T& sequence -> filter by Set<T>
		{
			const std::set<Obj> subset { { 'c', 2 }, { 'a', 1 } };

			NO_MORE_HEAP;

			auto common    = Enumerate(objList).Intersect(subset);
			auto remaining = Enumerate(objList).Except(subset);

			ASSERT_EQ (3,					common.Count());
			ASSERT_EQ (objList.size() - 3,	remaining.Count());
			ASSERT_EQ ('a',		common.Last().first);
			ASSERT_EQ ( 1,		common.Last().second);
			ASSERT_EQ ('a',		remaining.Last().first);	// avoid requiring op ==
			ASSERT_EQ ( 5,		remaining.Last().second);	// just for tests
			ASSERT_EQ ('a',		common.First().first);
			ASSERT_EQ ( 1,		common.First().second);
			ASSERT_EQ ('b',		remaining.First().first);
			ASSERT_EQ ( 1,		remaining.First().second);
		}
		// Same with non-set operands
		{
			Obj				 subsetArr[] = { { 'c', 2 }, { 'a', 1 }, { 'c', 2 } };
			std::vector<Obj> subsetVec	 = { { 'c', 2 }, { 'a', 1 }, { 'c', 2 } };

			auto common1    = Enumerate(objList).Intersect(subsetArr);
			auto common2    = Enumerate(objList).Intersect(subsetVec);
			auto remaining1 = Enumerate(objList).Except(subsetArr);
			auto remaining2 = Enumerate(objList).Except(subsetVec);

			ASSERT_EQ (3,					common2.Count());
			ASSERT_EQ (3,					common1.Count());
			ASSERT_EQ (objList.size() - 3,	remaining1.Count());
			ASSERT_EQ (objList.size() - 3,	remaining2.Count());
			ASSERT_EQ ('a',		common1.Last().first);
			ASSERT_EQ ( 1,		common1.Last().second);
			ASSERT_EQ ('a',		common2.Last().first);		// avoid requiring op ==
			ASSERT_EQ ( 1,		common2.Last().second);		// just for tests
			ASSERT_EQ ('a',		remaining1.Last().first);
			ASSERT_EQ ( 5,		remaining1.Last().second);
			ASSERT_EQ ('a',		remaining2.Last().first);
			ASSERT_EQ ( 5,		remaining2.Last().second);
			ASSERT_EQ ('a',		common1.First().first);
			ASSERT_EQ ( 1,		common1.First().second);
			ASSERT_EQ ('a',		common2.First().first);
			ASSERT_EQ ( 1,		common2.First().second);
			ASSERT_EQ ('b',		remaining1.First().first);
			ASSERT_EQ ( 1,		remaining1.First().second);
			ASSERT_EQ ('b',		remaining2.First().first);
			ASSERT_EQ ( 1,		remaining2.First().second);
		}

		// T& -> T* sequence -> filter by Set<T*>
		// (Probably best to stay explicit working with .Addresses())
		{
			const std::set<Obj*> subset { &objList[2], &objList[0] };

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


	// copy from FiltrationTests.cpp, tailored for std::set
	static void SetOperationsDefaultConversions()
	{
		using Base = std::pair<int, int>;	// using pair, just for convenience

		struct Derived : Base {
			char c;
			Derived(int x, int y, char c) : Base(x, y),	c(c)  {}
			Derived(const Base& b)		  : Base(b),	c(0)  {}	// for testing "CTE" lines
		};

		ASSERT_EQ (1, Derived(Base(1, 2)).first);					// suppress unused warnings


		Base	 bases[]	= { { 3, 3 }, { 1, 1 }, { 2, 2 }, { 3, 3 } };
		Derived	 deriveds[] = { { 1, 1, 'a' }, { 3, 3, 'e' }, { 1, 1, 'b' }, { 4, 4, 'a' } };

		// Descendants are accepted [only] by reference
		//				-> compared as type Base
		{
			auto remaining = Enumerate(bases).Except(deriveds);
			auto common    = Enumerate(bases).Intersect(deriveds);
			ASSERT_ELEM_TYPE (Base&, remaining);
			ASSERT_ELEM_TYPE (Base&, common);

			ASSERT_EQ (bases + 2, &remaining.Single());
			ASSERT_EQ (3,		   common.Count());
			ASSERT_EQ (bases + 0, &common.First());
			ASSERT_EQ (bases + 3, &common.Last());

			auto r1 = Enumerate<const Base&>(bases).Except(deriveds);
			auto c1 = Enumerate<const Base&>(bases).Intersect(deriveds);

			auto r2 = Enumerate<Base>(bases).Except(deriveds);
			auto c2 = Enumerate<Base>(bases).Intersect(deriveds);

			ASSERT_ELEM_TYPE (const Base&, r1);
			ASSERT_ELEM_TYPE (const Base&, c1);
			ASSERT_ELEM_TYPE (Base, r2);
			ASSERT_ELEM_TYPE (Base, c2);
			ASSERT (AreEqual(remaining, r1));
			ASSERT (AreEqual(remaining, r2));
			ASSERT (AreEqual(common,    c1));
			ASSERT (AreEqual(common,    c2));

			const auto& constDeriveds = deriveds;

			auto r3 = Enumerate(bases).Except(constDeriveds);
			auto c3 = Enumerate(bases).Intersect(constDeriveds);

			ASSERT_ELEM_TYPE (Base&, r3);
			ASSERT_ELEM_TYPE (Base&, c3);
			ASSERT (AreEqual(remaining, r3));
			ASSERT (AreEqual(common,    c3));

			// However, slicing filter values is not allowed:
			//  auto r4 = Enumerate(bases).Except(Enumerate<Derived>(deriveds));	// CTE

			// Nor is a filter of partial data (even if a conversion exists):
			//  auto r5 = Enumerate(deriveds).Except(Enumerate<Base>(bases));		// CTE
			//  auto r6 = Enumerate(deriveds).Except(bases);						// CTE
			//  static_assert (std::is_convertible<Base&, Derived>(), "missing conversion");

			// If needed, the strange conversion could be used explicitly
			// (only there's no std::hash<Derived> implemented in this test):
			//  auto r7 = Enumerate(deriveds).Except(Enumerate(bases).As<Derived>());
		}

		// Pointers can be mixed freely, as long as they are similar and compatible
		{
			auto derivedPtrs = Enumerate(deriveds).Addresses();
			auto basePtrs    = Enumerate<Base*>({ &deriveds[1], &deriveds[3] });	// a subset

			auto remaining = derivedPtrs.Except(basePtrs);
			auto common = derivedPtrs.Intersect(basePtrs);
			ASSERT_ELEM_TYPE (Derived*, remaining);
			ASSERT_ELEM_TYPE (Derived*, common);
			ASSERT_EQ (2, remaining.Count());
			ASSERT_EQ (deriveds + 0, remaining.First());
			ASSERT_EQ (deriveds + 2, remaining.Last());
			ASSERT    (AreEqual(common, basePtrs));

			auto r1 = derivedPtrs.AsConst().Except(basePtrs);
			auto c1 = derivedPtrs.AsConst().Intersect(basePtrs);
			auto r2 = derivedPtrs.Except(basePtrs.AsConst());
			auto c2 = derivedPtrs.Intersect(basePtrs.AsConst());

			ASSERT_ELEM_TYPE (const Derived*, r1);
			ASSERT_ELEM_TYPE (const Derived*, c1);
			ASSERT_ELEM_TYPE (Derived*, r2);
			ASSERT_ELEM_TYPE (Derived*, c2);
			ASSERT (AreEqual(remaining, r1));
			ASSERT (AreEqual(remaining, r2));
			ASSERT (AreEqual(common,	c2));
			ASSERT (AreEqual(common,	c2));

			auto rr = basePtrs.Except(derivedPtrs.AsConst());
			auto cr = basePtrs.Intersect(derivedPtrs.AsConst());
			ASSERT (!rr.Any());
			ASSERT (AreEqual(common, cr));
		}

		// Value conversions to TElem is supported only from unrelated types
		{
			std::pair<short, short> convables[] = { { 1, 1 }, { 3, 3 } };

			auto remaining = Enumerate(bases).Except(convables);
			auto common    = Enumerate(bases).Intersect(convables);

			ASSERT_ELEM_TYPE (Base&, remaining);
			ASSERT_ELEM_TYPE (Base&, common);

			ASSERT_EQ (2, remaining.Single().first);
			ASSERT_EQ (3, common.Count());
			ASSERT_EQ (3, common.First().first);
			ASSERT_EQ (3, common.Last().first);

			// Friendly errors for no conversion:
			//	auto ops = Enumerate(convables);
			//	auto rn = Enumerate(deriveds).Except(ops);			// CTE
			//	auto rd = Enumerate(deriveds).Except(ops.Copy());	// CTE

			// More typical example (though providing a transparent hash/equals could avoid conversions from C++17 - see TransparentComparisons lower):
			std::string fruits[] = { "apple", "banana" };
			auto remFruits1 = Enumerate(fruits).Except({ "coconut", "banana" });	// direct (eager) operand set

			const char* toHide[] = { "coconut", "banana" };
			auto remFruits2 = Enumerate(fruits).Except(Enumerate(toHide));			// deferred operand set
			ASSERT_ELEM_TYPE (std::string&, remFruits1);
			ASSERT_ELEM_TYPE (std::string&, remFruits2);
			ASSERT_EQ (fruits + 0, &remFruits1.Single());
			ASSERT_EQ (fruits + 0, &remFruits2.Single());
		}

		// Convenience overloads for reference-capturing elements via init-lists
		{
			const Base	b1 { 1, 1 };
			Derived		d3 { 3, 3, 'z' };

			auto remaining = Enumerate(bases).Except({ &b1, &d3 });
			auto common = Enumerate(bases).Intersect({ &b1, &d3 });
			ASSERT_ELEM_TYPE (Base&, remaining);
			ASSERT_ELEM_TYPE (Base&, common);
			ASSERT_EQ (2, remaining.Single().first);	// still value comparison!
			ASSERT_EQ (3, common.Count());
			ASSERT_EQ (3, common.First().first);
			ASSERT_EQ (3, common.Last().first);

			auto r1 = Enumerate<const Base&>(bases).Except({ &b1, &d3 });
			auto c1 = Enumerate<const Base&>(bases).Intersect({ &b1, &d3 });
			ASSERT_ELEM_TYPE (const Base&, r1);
			ASSERT_ELEM_TYPE (const Base&, c1);
			ASSERT (AreEqual(remaining, r1));
			ASSERT (AreEqual(common, c1));

			// No ambiguity emerge with pointer elements, "ref-capture" always mean 1 extra indirection.
			// However, "ref-capture" syntax is forbidden for scalars (by static_assert), because copying them
			// is more efficient + with these eager sets no referred item can change until enumeration anyway!
			int  n		= 2;
			int* ptrs[] = { &n, &d3.first };
			int* p		= &n;

		 //	auto remp = Enumerate(ptrs).Except({ &p });		// deliberate CTE
		 //	auto comp = Enumerate(ptrs).Intersect({ &p });	// deliberate CTE

			auto remp = Enumerate(ptrs).Except({ p });		// same semantics
			auto comp = Enumerate(ptrs).Intersect({ p });	//
			ASSERT_ELEM_TYPE (int*&, remp);
			ASSERT_ELEM_TYPE (int*&, comp);
			ASSERT_EQ (&d3.first, remp.Single());
			ASSERT_EQ (p,		  comp.Single());
		}
	}


	// Demonstrate transparent comparisons to avoid unnecessary conversions
	static void TransparentComparisons()
	{
		// 1. Technically trivial direction: a set<string> is formed as filter
		{
			// Testcase should avoid short-string optimization!
			const char* fruits[] = { "apple longstring", "banana longstring" };
			std::string toHide[] = { "coconut longstring", "banana longstring" };	// deferred operand set

			AllocationCounter allocs;

			auto remFruits = Enumerate(fruits).Except<std::less<>>(toHide);
			auto hidFruits = Enumerate(fruits).Intersect<std::less<>>(toHide);
			ASSERT_ELEM_TYPE (const char*&, remFruits);
			ASSERT_ELEM_TYPE (const char*&, hidFruits);
			ASSERT_EQ (fruits[0], remFruits.Single());
			ASSERT_EQ (fruits[1], hidFruits.Single());

			// 2 items + 1 head for each operation - but no separate string objects!
			static constexpr unsigned expectedNodes	= 3;
			static constexpr unsigned setAllocs		= 2 * (expectedNodes + IFNO_NRVO(1));
			allocs.AssertMaxFreshCount(setAllocs);


			// ----- Using custom comparator type -----
			struct AsString {

				// NOTE: Without this statement std::set.find only accepts the exact item type
				//		 - which is problematic in case of T& elements -> wrapped as Ref<T>
				//		 I consider this an acceptable limitation.
				using is_transparent = int;

				// Fix overloads are fine
				bool operator ()(const std::string& lhs, const std::string& rhs) const { return lhs < rhs; }
				bool operator ()(const std::string& lhs, const char* rhs)		 const { return lhs < rhs; }
				bool operator ()(const char* lhs, const std::string& rhs)		 const { return lhs < rhs; }
			};
			auto rem2 = Enumerate(fruits).Except<AsString>(toHide);
			auto hid2 = Enumerate(fruits).Intersect<AsString>(toHide);
			ASSERT_ELEM_TYPE (const char*&, remFruits);
			ASSERT_ELEM_TYPE (const char*&, hidFruits);
			ASSERT_EQ (fruits[0], rem2.Single());
			ASSERT_EQ (fruits[1], hid2.Single());
			allocs.AssertMaxFreshCount(setAllocs);


			// ----- The same utilizing string_view -----
			struct AsView : std::less<std::string_view> {
				using is_transparent = int;
				// Both operand types are convertible -
				// declaring transparency is needed against fixed find(RefHolder<string>) in STL
			};
			auto rem3 = Enumerate(fruits).Except<AsView>(toHide);
			auto hid3 = Enumerate(fruits).Intersect<AsView>(toHide);
			ASSERT_ELEM_TYPE (const char*&, remFruits);
			ASSERT_ELEM_TYPE (const char*&, hidFruits);
			ASSERT_EQ (fruits[0], rem3.Single());
			ASSERT_EQ (fruits[1], hid3.Single());
			allocs.AssertMaxFreshCount(setAllocs);


			// ----- Using fixed parameter is possible when no RefHolder is involved -----
			auto copiesToHide = Enumerate(toHide).Copy();
			auto rem4 = Enumerate(fruits).Except<std::less<std::string_view>>(copiesToHide);
			auto hid4 = Enumerate(fruits).Intersect<std::less<std::string_view>>(copiesToHide);
			ASSERT_ELEM_TYPE (const char*&, remFruits);
			ASSERT_ELEM_TYPE (const char*&, hidFruits);
			ASSERT_EQ (fruits[0], rem4.Single());
			ASSERT_EQ (fruits[1], hid4.Single());
			allocs.AssertFreshCount(setAllocs + 4 + 4);	// HOWEVER, non-transparent set.find(string)
			//									^	^	   incurs additional unnecessary copies!
			//	legit copies for the testcase  -'	|
			//	  penalty for non-transparent find -'
		}

		// 2. Filter strings by char* operands
		{
			// Testcase should avoid short-string optimization!
			std::string fruits[] = { "apple longstring", "banana longstring" };
			const char* toHide[] = { "coconut longstring", "banana longstring" };

			AllocationCounter allocs;

			// ----- Cleanest way: as string_view -----
			auto hideViews = Enumerate<std::string_view>(toHide);

			auto remFruits = Enumerate(fruits).Except<std::less<>>(hideViews);
			auto hidFruits = Enumerate(fruits).Intersect<std::less<>>(hideViews);
			ASSERT_ELEM_TYPE (std::string&, remFruits);
			ASSERT_ELEM_TYPE (std::string&, hidFruits);
			ASSERT_EQ (fruits[0], remFruits.Single());
			ASSERT_EQ (fruits[1], hidFruits.Single());

			static constexpr unsigned setAllocs = 2 * (3 + IFNO_NRVO(1));
			allocs.AssertMaxFreshCount(setAllocs);

			// In fact this needs no transparency, just a custom comparator
			// to override comparing as TElem = string
			auto rem2 = Enumerate(fruits).Except<std::less<std::string_view>>(hideViews);
			auto hid2 = Enumerate(fruits).Intersect<std::less<std::string_view>>(hideViews);
			ASSERT_ELEM_TYPE (std::string&, rem2);
			ASSERT_ELEM_TYPE (std::string&, hid2);
			ASSERT_EQ (fruits[0], rem2.Single());
			ASSERT_EQ (fruits[1], hid2.Single());
			allocs.AssertMaxFreshCount(setAllocs);


			// ----- Using custom comparator type on char* directly -----
			struct AsStringView : std::less<std::string_view> {
				using is_transparent = int;
			};
			auto rem3 = Enumerate(fruits).Except<AsStringView>(toHide);
			auto hid3 = Enumerate(fruits).Intersect<AsStringView>(toHide);
			ASSERT_ELEM_TYPE (std::string&, rem3);
			ASSERT_ELEM_TYPE (std::string&, hid3);
			ASSERT_EQ (fruits[0], rem3.Single());
			ASSERT_EQ (fruits[1], hid3.Single());
			allocs.AssertMaxFreshCount(setAllocs);
		}
	}



	void TestOrderedSetBindings()
	{
		Greet("Binding std::set, std::map");
		RESULTSVIEW_DISABLES_ALLOCASSERTS;

		SetCreation();
		MapCreation();

		SetOperationsSimple();
		SetOperationsByReference();
		SetOperationsDefaultConversions();

		TransparentComparisons();
	}

}	// namespace EnumerableTests::AltBinding
