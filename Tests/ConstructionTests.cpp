
#if defined(_MSC_VER) && !defined(__clang__)
#	pragma warning(disable : 4503)
#endif

#include "Tests.hpp"
#include "TestUtils.hpp"
#include "TestAllocator.hpp"
#include "Enumerables.hpp"



namespace EnumerableTests {

	using namespace Enumerables;

	namespace {

		struct Base { int x; };

		struct DerivedA : public Base {
			double d;
			DerivedA(int bx, double d) : Base { bx }, d { d } {}
		};

		struct DerivedB : public Base {
			char c;
			DerivedB(int bx, char c) : Base { bx }, c { c } {}
		};

	}


	// NOTE: Old code, has incomplete testcases - should cover together with Introduction tests.


	// Construction functions using single-object seeds
	static void SeededConstruction()
	{
		using MoveOnly = MoveOnly<int>;

		std::vector<int> intVec;
		intVec.reserve(10);

		NO_MORE_HEAP;

		// Zero-length sequence of any type
		{
			auto empty	   = Empty<MoveOnly>();
			auto emptyRefs = Empty<MoveOnly&>();
			ASSERT_ELEM_TYPE (MoveOnly,  empty);
			ASSERT_ELEM_TYPE (MoveOnly&, emptyRefs);
			ASSERT_EQ		 (0,		 empty.Count());
			ASSERT_EQ		 (0,		 emptyRefs.Count());
			ASSERT			 (!empty.Any());
			ASSERT			 (!emptyRefs.Any());
		}

		// Repeat a single value indefinitely
		{
			// 1. Rval is moved into -> elem type defaults to copy!
			auto infInts = Repeat(5);
			ASSERT_ELEM_TYPE (int,  infInts);
			ASSERT_EQ		 (5,	infInts.First());
			ASSERT_EQ		 (5,	infInts.ElementAt(30));

			// Advanced usage: known bounds (or lack of them) can be queried directly from the Enumerator
			SizeInfo infSize = infInts.GetEnumerator().Measure();
			ASSERT (infSize.IsUnbounded());

			// 2. Lval is stored by reference -> elem type too defaults to &
			MoveOnly mx { 7 };
			auto infRefs = Repeat(mx);
			ASSERT_ELEM_TYPE (MoveOnly&, infRefs);
			ASSERT_EQ		 (&mx,		 &infRefs.First());
			ASSERT_EQ		 (&mx,		 &*infRefs.ElementAt(30));

			// 3. Nothing prevents requesting copies of a reference-captured item:
			auto dataCopies = Repeat<int>(mx.data);
			ASSERT_ELEM_TYPE (int, dataCopies);
			ASSERT_EQ		 (7,   dataCopies.First());
			mx.data++;
			ASSERT_EQ		 (8,   dataCopies.First());

			// 4. Advanced usage
			// 4.1	The explicitly typed object can be the result of conversion, apparently...
			auto infConstructeds = Repeat<MoveOnly>(9);

			// 4.2	Explicitly request for const & is approved for "materialized" enumerables, while capturing rvalues.
			//		(Mutable & is not, since the Enumerable itself stays const along with the stored value inside.)
			//		Note however, this has caveats.
			auto infConsts  = Repeat<const MoveOnly&>(MoveOnly { 3 });

			//		Reference-conversion is ok.
			auto baseConsts = Repeat<const Base&>(DerivedA { 1, 6.6 });

			//		But a temporary can't be referenced!
		 // auto infConsts2 = Repeat<const MoveOnly&>(9);		// CTE!

			ASSERT_ELEM_TYPE (const MoveOnly&,	infConsts);
			ASSERT_ELEM_TYPE (MoveOnly,			infConstructeds);
			ASSERT_ELEM_TYPE (const Base&,		baseConsts);
			ASSERT_EQ		 (3, infConsts.First().data);
			ASSERT_EQ		 (3, infConsts.ElementAt(30)->data);
			ASSERT_EQ		 (9, infConstructeds.First().data);
			ASSERT_EQ		 (9, infConstructeds.ElementAt(30)->data);
			ASSERT_EQ		 (1, baseConsts.First().x);
			ASSERT_EQ		 (1, baseConsts.ElementAt(30)->x);

			// CAUTION: materialized const & enumerations has limitations and can be unsafe!


			// Due to infConsts{MoveOnly} being uncopyable, a fork of it (requested by .Skip) is not possible - nor is type-erasure
		 // auto consts2 = infConsts.Skip(2);								// CTE
		 // Enumerable<const MoveOnly&> interf = infConsts.ToInterfaced();	// CTE - std::function must be copyable

			// A moving chain operation remains possible though:
			auto consts2 = std::move(infConsts).Skip(2);
			ASSERT_ELEM_TYPE (const MoveOnly&,	consts2);
			ASSERT_EQ		 (3, consts2.First().data);
			ASSERT_EQ		 (3, consts2.ElementAt(30)->data);

			// The dangerous part is const & access from rvalues - no current safety mechanism for that!
			const Base& viaRvalue = baseConsts.Skip(1).First();			// dangling after result of .Skip destroys!!!
			int					x = baseConsts.Skip(1).First().x;		// is OK, saving a copy before temporary destroys
			UNUSED (viaRvalue);
			ASSERT_EQ (1, x);

			// CONSIDER: a static_assert is theoretically possible to prevent giving out const& to self-contained values in rvalue enumerables,
			//			 however it requires type-level tracking of which enumerables give self-contained results - which can conflict with
			//			 complete type-erasure of Enumerable<T> --> Enumerable<T, SelfCont = true / false>
			//			 Contra:
			//			 this is typical programmer responsibility in C++, e.g.:
			//			 for (auto& x : GetHashMap(params).Values())	// temporary of GetHashMap() will be dropped, leaving Velues() proxy object dangling
		}


		// Repeat a single value for limited times
		{
			MoveOnly mo { 6 };

			auto fiveTimes = Repeat(mo, 5);
			auto onceOnly  = Once(mo);

			ASSERT_ELEM_TYPE (MoveOnly&, fiveTimes);
			ASSERT_ELEM_TYPE (MoveOnly&, onceOnly);
			ASSERT_EQ		 (5,		 fiveTimes.Count());
			ASSERT_EQ		 (1,		 onceOnly.Count());
			ASSERT_EQ		 (&mo,		 &fiveTimes.First());
			ASSERT_EQ		 (&mo,		 &fiveTimes.Last());
			ASSERT_EQ		 (&mo,		 &onceOnly.Single());
			ASSERT			 (fiveTimes.Addresses().AllEqual());

			auto oneUnsigned = Once(7u);
			ASSERT_ELEM_TYPE (unsigned, oneUnsigned);
			ASSERT_EQ		 (1,		oneUnsigned.Count());
			ASSERT_EQ		 (7,		oneUnsigned.Single());

			auto oneDouble = Once<double>(8);
			ASSERT_ELEM_TYPE (double, oneDouble);
			ASSERT_EQ		 (1,	  oneDouble.Count());
			ASSERT_EQ		 (8.0,	  oneDouble.Single());

			auto oneBase = Once<const Base&>(DerivedA { 2, 4.5 });
			ASSERT_ELEM_TYPE (const Base&,  oneBase);
			ASSERT_EQ		 (1,			oneBase.Count());
			ASSERT_EQ		 (2,			oneBase.Single().x);
		}


		// Arithmetic sequences
		{
			auto toFive = Range(1, 5);
			ASSERT_ELEM_TYPE (int,				 toFive);
			ASSERT (AreEqual ({ 1, 2, 3, 4, 5 }, toFive));

			auto fromFive = RangeDown(5, 5);
			ASSERT_ELEM_TYPE (int,				 fromFive);
			ASSERT (AreEqual ({ 5, 4, 3, 2, 1 }, fromFive));

			auto fiveTen = RangeBetween(5, 10);
			ASSERT_ELEM_TYPE (int,			fiveTen);
			ASSERT (AreEqual (Range(5, 6),  fiveTen));

			auto tenFive = RangeDownBetween(10u, 5u);
			ASSERT_ELEM_TYPE (unsigned,			 tenFive);
			ASSERT (AreEqual (RangeDown(10u, 6), tenFive));

			// Range/RangeBetween use value capture only to avoid surprises
			long n  = 67;
			auto l3 = Range(n, 3);
			ASSERT_ELEM_TYPE (long,			  l3);
			ASSERT (AreEqual ({ 67, 68, 69 }, l3));
			n = 72;
			ASSERT (AreEqual ({ 67, 68, 69 }, l3));

			// IndexRange however is different -
			// uses deferred (i.e. up-to-date) index range of a container
			auto indices	= IndexRange(intVec);
			auto indicesRev = IndexRangeReversed(intVec);
			ASSERT_ELEM_TYPE (size_t, indices);
			ASSERT_ELEM_TYPE (size_t, indicesRev);
			ASSERT (intVec.empty());
			ASSERT (!indices.Any());
			ASSERT (!indicesRev.Any());

			intVec.push_back(1);
			intVec.push_back(7);
			intVec.push_back(5);
			ASSERT_EQ (3, indices.Count());
			ASSERT_EQ (3, indicesRev.Count());
			ASSERT_EQ (0, indices.First());
			ASSERT_EQ (2, indices.Last());
			ASSERT_EQ (2, indicesRev.First());
			ASSERT_EQ (0, indicesRev.Last());

			auto zeroNine = Range(10u);
			auto zeroTen  = Range(11);
			auto zeroElev = Range<short>(12);
			ASSERT_ELEM_TYPE (size_t,	zeroNine);
			ASSERT_ELEM_TYPE (size_t,	zeroTen);
			ASSERT_ELEM_TYPE (short,	zeroElev);
			ASSERT_EQ (10, zeroNine.Count());
			ASSERT_EQ (11, zeroTen.Count());
			ASSERT_EQ (12, zeroElev.Count());
			ASSERT_EQ (0,  zeroNine.First());
			ASSERT_EQ (0,  zeroTen.First());
			ASSERT_EQ (0,  zeroElev.First());
			ASSERT_EQ (9,  zeroNine.Last());
			ASSERT_EQ (10, zeroTen.Last());
			ASSERT_EQ (11, zeroElev.Last());
		}


		// Custom sequences
		{
			auto pow2s = Sequence(1u, FUN(x,  2 * x));
			ASSERT_ELEM_TYPE (unsigned,	pow2s);
			ASSERT_EQ		 (1,		pow2s.First());
			ASSERT_EQ		 (64,		pow2s.ElementAt(6));

			auto sqrts = Sequence(16, &std::sqrtl);
			ASSERT_ELEM_TYPE (long double,	sqrts);
			ASSERT_EQ		 (16.0,			sqrts.First());
			ASSERT_EQ		 (2.0,			sqrts.ElementAt(2));

			// some extremeness
			struct Linked {
				int			data;
				Linked*		next;

				bool	HasNext()	{ return next != nullptr; }
				Linked& Next()		{ return *next; }
			};

			Linked l3 { 3, nullptr };
			Linked l2 { 2, &l3 };
			Linked l1 { 1, &l2 };

			auto linkedElems = Sequence(l1, FUN(x, x.Next()))
								.TakeUntilFinal(FUN(x, !x.HasNext()));

			ASSERT_ELEM_TYPE (Linked&,	linkedElems);
			ASSERT_EQ		 (3,		linkedElems.Count());
			ASSERT_EQ		 (3,		linkedElems.Last().data);
		}
	}


	// Wrapping collections
	static void CollectionBasics()
	{
		std::vector<int> vec;

		auto nums = Enumerate(vec);
		ASSERT_EQ (0, nums.Count());
		ASSERT	  (!nums.Any());
		ASSERT	  (nums.All(FUN(x,  x > 5)));		// empty list => true for all

		for (int i = 0; i < 3; i++) {
			vec.push_back(i * 4);
			ASSERT_EQ (vec.size(),	nums.Count());
			ASSERT_EQ (vec.front(), nums.First());
			ASSERT_EQ (vec.back(),	nums.Last());
		}
		ASSERT (nums.Any());
		ASSERT (nums.All( FUN(x,  x < 10)));
		ASSERT (!nums.All(FUN(x,  x > 5)));
	}


	// Direct (r-value) braced initializer - using "capture-syntax"
	static void BracedInit()
	{
		// check elements alive after leaving scope -> contents of rvalue initializers are saved to heap
		Enumerable<int> ints = Empty<int>();
		{
			auto localInts = Enumerate({ 1, 2, 3, 4 });
			ints = localInts;
			ASSERT_ELEM_TYPE (int, localInts);
		}
		ASSERT_EQ (4, ints.Count());
		ASSERT_EQ (1, ints.First());
		ASSERT_EQ (4, ints.Last());

		// Forced type disambiguites the initializer
		{
			ints = Enumerate<int>({ 1, 2u, 3, '4' });
		}
		ASSERT_EQ (4,	ints.Count());
		ASSERT_EQ (1,	ints.First());
		ASSERT_EQ ('4', ints.Last());

		// For scalars: Forced type takes precedence over deduction even in exact cases,
		//				allowing only certain narrowing conversions defined by the standard.
		{
			short a = 2;
			ints = Enumerate<int>({ a });			// widening
			ASSERT_EQ (2, ints.Single());

			ints = Enumerate<int>({ 'a', 'b' });	// widening
			ASSERT_EQ ('a', ints.First());
			ASSERT_EQ ('b', ints.Last());
			
			auto shorts = Enumerate<short>   ({ 0, 1, 2 });
			auto uints  = Enumerate<unsigned>({ 0, 1, 2 });
			ASSERT_EQ (0, shorts.First());
			ASSERT_EQ (2, shorts.Last());
			ASSERT_EQ (3, shorts.Count());
			ASSERT (Enumerables::AreEqual(shorts, uints));

		 // -- Importance:
		 // Letting the initializer_list deduce freely, then converting its elements to the Forced type
		 // would lack this static safety (but trigger narrowing warning in benign cases as well):
		 // 
		 //	auto shorts2 = Enumerate<short>({ 1, 200000 });		// CTE: constexpr does not fit
		 //	auto uints   = Enumerate<unsigned>({ 1, 2, -3 });	// 
		}


		// For non-scalars (~class types), however, storing from freely
		// deduced initializer_list is allowed to avoid unnecessary copies:
		{
			auto words = Enumerate<MoveOnly<std::string>>({ "apple", "pie" });

			MoveOnly<std::string> w1 = words.First();
			MoveOnly<std::string> w2 = words.Last();

			ASSERT_EQ ("apple",	*w1);
			ASSERT_EQ ("pie",	*w2);

			// So whenever the deduction of a common element type succeeds:
			// What is seen between the braces is what gets stored (still similarly to a capture block), then
			// only during outputting elements, conversion to the requested type occurs (similarly to Enumerate<R>(container))!

			// Equivalent elaborated version of the same would be
			// [except having 1 small additional chained operation]:
			auto words2 = Enumerate<const char*>({ "apple", "pie" }).As<MoveOnly<std::string>>();
			ASSERT_EQ (std::string("apple"), *words2.First());
			ASSERT_EQ (std::string("pie"),	 *words2.Last());


			// Simulating the undesired scenario:
			auto simExtraCopy = Enumerate(std::initializer_list<CountedCopy<std::string>> { "apple", "pie" });
			ASSERT_EQ (2, simExtraCopy.First().copyCount);

			// Which actually still can happen, whenever the item type is ambiguous:
			std::string s3 = "constructed";
			auto extraCopy = Enumerate<CountedCopy<std::string>>({ "apple", "pie", s3 });
			ASSERT_EQ (2, simExtraCopy.First().copyCount);
			
			// NOTE: On it's own storing exactly the "Forced" type would be clearer, but that
			//		 would render the MoveOnly example impossible / add 1 copy in workable cases,
			//		 while the current mechanism seems to be in line with other overloads' logic!
		}

		// "Capture-syntax" is supported to enumerate references of objects
		// - currently only for inline (rvalue) initializers!
		int a = 5, b = 6, c = 7;
		auto ints2 = Enumerate({ &c, &b, &a });
		ASSERT_ELEM_TYPE (int&, ints2);
		ASSERT_EQ		 (3, ints2.Count());
		ASSERT_EQ		 (7, ints2.First());
		c = 8;
		ASSERT_EQ		 (8, ints2.First());

		// with explicit type
		auto ints3 = Enumerate<int&>({ &c, &b, &a });
		auto ints4 = Enumerate<const int&>({ &c, &b, &a });
		ASSERT_ELEM_TYPE (int&, ints3);
		ASSERT_ELEM_TYPE (const int&, ints4);
		ints3.First() = 1;
		ASSERT_EQ (1, ints4.First());

	 // auto bad1 = Enumerate<int&>({ 1, 2, 3, 4 });	// CTE
	 // auto bad2 = Enumerate<int>({ &c, &b, &a });		// CTE

		// explicit type can override "capture-syntax" to keep simple pointers :)
		auto ptrs = Enumerate<int*>({ &c, &b, &a });
		ASSERT_ELEM_TYPE (int*, ptrs);
		ASSERT_EQ (&a, ptrs.Last());

		// Beware of c-strings! Implicitly stored: they are pointers -> interpreted as char& !
		// (Shortcoming of the generally useful &-syntax here...)
		auto letters = Enumerate({ "ant", "brick" });
		ASSERT_ELEM_TYPE (const char&, letters);
		ASSERT_EQ		 (2,   letters.Count());

		// explicit allows the initializer to accept polymorphic objects too:
		Base		base { 1 };
		DerivedA	derA { 20, 5.5 };
		DerivedB	derB { 33, 'e' };

		auto mixedList = Enumerate<Base&>({ &base, &derA, &derB });
		ASSERT_ELEM_TYPE (Base&,  mixedList);
		ASSERT_EQ		 (3,	  mixedList.Count());
		ASSERT_EQ		 (&base, &mixedList.First());
		ASSERT_EQ		 (&derB, &mixedList.Last());


		// Currently, lvalue initializers are just treated as regular containers!
		NO_MORE_HEAP;
		std::initializer_list<int> lvalList { 10, 11u, 12, 13, 14 };
		auto listReferred = Enumerate(lvalList);
		ASSERT_ELEM_TYPE (const int&, listReferred);
		ASSERT_EQ		 (5,  listReferred.Count());
		ASSERT_EQ		 (10, listReferred.First());
		ASSERT_EQ		 (14, listReferred.Last());

		std::initializer_list<int*> lvalPtrs { &a, &b };
		auto ptrsReferred = Enumerate(lvalPtrs);
		ASSERT_ELEM_TYPE (int* const&, ptrsReferred);		// no fancy tricks here
	}


	
	// Exact copy of r-value BracedInit tests, just with custom allocator parameter.
	static void BracedInitWithAllocator()
	{
		AllocationCounter heapAllocs;

		// only this time, be strict and let's understand the difference for 32 bit...
		constexpr size_t allocBytes	  = 3 * 4 * sizeof(int);					// backing vectors' buffer
		constexpr size_t bookingBytes = 2 * sizeof(int) + 4 * sizeof(int*);		// TestAllocator internal
		constexpr size_t pwords = (allocBytes + bookingBytes) / sizeof(int*);

		std::aligned_storage_t<sizeof(int*), alignof(int*)>  buffer[pwords];
		TestAllocator<int*, 3>	fixedPtrAlloc  { buffer };
		TestAllocator<int, 3>	fixedIntAlloc  { fixedPtrAlloc };
		TestAllocator<Base*, 3>	fixedBaseAlloc { fixedPtrAlloc };
		
		{
			Enumerable<int> ints = Empty<int>();
			{
				auto localInts = Enumerate({ 1, 2, 3, 4 }, fixedIntAlloc);
				ints = localInts;
				ASSERT_ELEM_TYPE (int, localInts);
			}
			ASSERT_EQ (4, ints.Count());
			ASSERT_EQ (1, ints.First());
			ASSERT_EQ (4, ints.Last());

			// Forced type disambiguates the initializer
			{
				ints = Enumerate<int>({ 1, 2u, 3, '4' }, fixedIntAlloc);
			}
			ASSERT_EQ (4,	ints.Count());
			ASSERT_EQ (1,	ints.First());
			ASSERT_EQ ('4', ints.Last());
		}
		heapAllocs.AssertMaxFreshCount(2);	// 2 x type-erasure, not for lists
											// (depends on std::function impl)

		int a = 5, b = 6, c = 7;
		{
			// "Capture-syntax" is supported to enumerate references of objects
			// - currently only for inline (rvalue) initializers!
			auto ints2 = Enumerate({ &c, &b, &a }, fixedPtrAlloc);
			ASSERT_ELEM_TYPE (int&, ints2);
			ASSERT_EQ		 (3, ints2.Count());
			ASSERT_EQ		 (7, ints2.First());
			c = 8;
			ASSERT_EQ		 (8, ints2.First());
		}
		heapAllocs.AssertFreshCount(0);

		{
			// with explicit type
			auto ints3 = Enumerate<int&>({ &c, &b, &a }, fixedPtrAlloc);
			auto ints4 = Enumerate<const int&>({ &c, &b, &a }, fixedPtrAlloc);
			ASSERT_ELEM_TYPE (int&, ints3);
			ASSERT_ELEM_TYPE (const int&, ints4);
			ints3.First() = 1;
			ASSERT_EQ (1, ints4.First());

			// auto bad1 = Enumerate<int&>({ 1, 2, 3, 4 }, fixedIntAlloc);	// CTE
			// auto bad2 = Enumerate<int>({ &c, &b, &a }, fixedPtrAlloc);	// CTE
		}
		heapAllocs.AssertFreshCount(0);

		{
			// explicit type can override "capture-syntax" to keep simple pointers :)
			auto ptrs = Enumerate<int*>({ &c, &b, &a }, fixedPtrAlloc);
			ASSERT_ELEM_TYPE (int*, ptrs);
			ASSERT_EQ (&a, ptrs.Last());

			// explicit allows the initializer to accept polymorphic objects too:
			Base		base { 1 };
			DerivedA	derA { 20, 5.5 };
			DerivedB	derB { 33, 'e' };

			auto mixedList = Enumerate<Base&>({ &base, &derA, &derB }, fixedBaseAlloc);
			ASSERT_ELEM_TYPE (Base&, mixedList);
			ASSERT_EQ		 (3, mixedList.Count());
			ASSERT_EQ		 (&base, &mixedList.First());
			ASSERT_EQ		 (&derB, &mixedList.Last());
		}
		heapAllocs.AssertFreshCount(0);


		// Check the type-only syntax for default-constructible allocators as well (some examples only)
		{
			// Forced type disambiguites the initializer
			auto ints = Enumerate<int, std::allocator<int>>({ 1, 2u, 3, '4' });
			
			ASSERT_EQ (4,	ints.Count());
			ASSERT_EQ (1,	ints.First());
			ASSERT_EQ ('4', ints.Last());


			// with explicit type
			auto ints3 = Enumerate<int&, std::allocator<int*>>({ &c, &b, &a });
			auto ints4 = Enumerate<const int&, std::allocator<int*>>({ &c, &b, &a });
			ASSERT_ELEM_TYPE (int&, ints3);
			ASSERT_ELEM_TYPE (const int&, ints4);
			ints3.First() = 1;
			ASSERT_EQ (1, ints4.First());
		}
	}



	// Concatenation (with const- or value conversion)
	static void ConcatenationsSimple()
	{
		std::vector<int> nums1 { 1, 2, 3 };
		std::vector<int> nums2 { 4, 5 };
		std::vector<int> numsE {};
		std::vector<int> nums4 { 6, 7 };

		// basic
		{
			NO_MORE_HEAP;

			auto s2  = Concat(nums1, nums2);
			auto s4  = Concat(nums1, nums2, numsE, nums4);
			auto s4v = Concat<int>(nums1, nums2, numsE, nums4);
			auto s4c = Concat<const int&>(nums1, nums2, numsE, nums4);

			ASSERT_ELEM_TYPE (int&,		  s2);
			ASSERT_ELEM_TYPE (int&,		  s4);
			ASSERT_ELEM_TYPE (int,		  s4v);
			ASSERT_ELEM_TYPE (const int&, s4c);

			ASSERT (s2.Count() == 5);
			ASSERT (s4.Count() == 7);
			ASSERT (s4v.Count() == 7);
			ASSERT (s4c.Count() == 7);

			auto diffs2  = s2.MapNeighbors<int> (FUN(p, n,  n - p));
			auto diffs4  = s4.MapNeighbors<int> (FUN(p, n,  n - p));
			auto diffs4v = s4v.MapNeighbors<int>(FUN(p, n,  n - p));
			auto diffs4c = s4c.MapNeighbors<int>(FUN(p, n,  n - p));

			auto allDiffs = diffs2.Concat(diffs4).Concat(diffs4v).Concat(diffs4c);
			ASSERT (allDiffs.All(FUN(d,  d == 1)));
			ASSERT (allDiffs.Count() == nums1.size() + nums2.size() - 1
										 + 3 * (nums1.size() + nums2.size() + numsE.size() + nums4.size() - 1));

			// basic mutation test
			int& n1 = s2.First();
			int& n7 = s4.Last();
			n1 = 10;
			n7 = 70;
			ASSERT_EQ (10, nums1[0]);
			ASSERT_EQ (70, nums4[1]);

			ASSERT_EQ (nums1[0], s2.First());
			ASSERT_EQ (nums1[0], s4.First());
			ASSERT_EQ (nums1[0], s4v.First());
			ASSERT_EQ (nums1[0], s4c.First());

			ASSERT_EQ (nums2[1], s2.Last());
			ASSERT_EQ (nums4[1], s4.Last());
			ASSERT_EQ (nums4[1], s4v.Last());
			ASSERT_EQ (nums4[1], s4c.Last());

			n1 = 1;
			n7 = 7;
		}

		// value conversions - allowed only explicitly
		{
			NO_MORE_HEAP;

			double dubs[] = { 7.5, 8.5 };

		 // auto sd = Concat(nums1, nums2, dubs);								// CTE
		 // auto sd = Concat(Enumerate<int>(nums1), Enumerate<double>(dubs));	// CTE
			auto sd = Concat<double>(nums1, nums2, dubs);						// explicit OK

			ASSERT_ELEM_TYPE (double, sd);

			ASSERT_EQ (1.0, sd.First());
			ASSERT_EQ (8.5, sd.Last());
		}

		// with init lists - note that those get stored on the heap for the Enumerable if inline (since rvalues)
		//				   - use lvalues to reference them
		{
			auto s = Concat<int>(nums1, { 4, 5 });
			ASSERT_EQ (5, s.Count());
			ASSERT_ELEM_TYPE (int, s);

			auto s2 = Concat<int>(nums1, { 4 }, nums2);
			ASSERT_EQ (6, s2.Count());

			auto s3 = Concat<int>({ 2 }, nums1, { 4, 5 });
			ASSERT_EQ (6, s3.Count());

			// can use "capture-syntax" inline (aka. pointers) to concat references
			int  x1 = 8;
			int  x2 = 9;
			auto s4 = Concat({ &x1 }, nums1, { &x1, &x2 });
			ASSERT_ELEM_TYPE (int&, s4);
			ASSERT_EQ		 (6,	s4.Count());
			ASSERT_EQ		 (&x1, &s4.First());
			ASSERT_EQ		 (&x2, &s4.Last());

			// with some conversions
			double d1 = 5.9;
			double d2 = x2;
			auto dubs = Concat<double>({ d1 }, nums1, { 6, 8, d2 });
			ASSERT_ELEM_TYPE (double, dubs);
			ASSERT_EQ		 (9.0,    dubs.Last());
			ASSERT_EQ		 (5.9,    dubs.First());

			NO_MORE_HEAP;

			// with lvalues -> stored as reference -> same as regular containers
			auto i1 = { 2 };
			auto i2 = { 4, 5 };
			auto s3b = Concat(i1, nums1, i2);
			ASSERT_EQ (6, s3b.Count());

			// Note: these are indeed treated as regular containers, so "capture-syntax" is not supported.
			// auto r1 = { &i1 };
			// auto s3c = Concat<int&>(r1, nums1);		// CTE
		}

		// Init-list conversion nuances - should follow singular Enumerate({ ... }) rules
		{
			// scalars: explicit type enforced to init-lists
			auto ds1 = Concat<double>({ 2.0, 3.5 }, { 1 });
			auto ds2 = Concat<double>({ 2.0, 3u },  { 1 });
			auto ds3 = Concat<double>(nums1, { 2.0, 3u });
			auto ds4 = Concat<double>(nums1, { 4u });

			ASSERT (AreEqual({ 2.0, 3.5, 1.0 },			  ds1));
			ASSERT (AreEqual({ 2.0, 3.0, 1.0 },			  ds2));
			ASSERT (AreEqual({ 1.0, 2.0, 3.0, 2.0, 3.0 }, ds3));
			ASSERT (AreEqual({ 1.0, 2.0, 3.0, 4.0 },	  ds4));

			unsigned short ushortArr[] = { 1, 2 };

			auto unsigneds  = Concat<unsigned>({ 3, 4 },  ushortArr);
		 //	auto unsigneds2 = Concat<unsigned>({ 3, -4 }, ushortArr);	// CTE: narrowing literal

			ASSERT (AreEqual({ 3, 4, 1, 2 }, unsigneds));


			// classes: conversion still only takes place during enumeration
			std::string wordsArr[] = { "eaten" };
			auto words1 = Concat<MoveOnly<std::string>>({ "apple", "pie" }, { std::string("baked") });
			auto words2 = Concat<MoveOnly<std::string>>({ "apple", "pie" }, wordsArr);

			ASSERT (AreEqual({ "apple", "pie", "baked" }, words1.Dereference()));
			ASSERT (AreEqual({ "apple", "pie", "eaten" }, words2.Dereference()));
		}

		// Concat with fluent syntax
		{
			NO_MORE_HEAP;

			// continuation is already an Enumerable
			auto catFiltered = Enumerate(nums1).Concat(Filter(nums2, FUN(x, x < 5)));
			ASSERT_EQ (1, catFiltered.First());
			ASSERT_EQ (4, catFiltered.Last());

			// collection directly as continuation
			auto catVec = Enumerate(nums1).Concat(nums2);
			ASSERT_EQ (1, catVec.First());
			ASSERT_EQ (5, catVec.Last());

			// elem type propagation - No intelligence here, just forcing the first type to the continuation.
			auto prInts = Range<int>(5, 2).Concat(nums1);

			ASSERT_ELEM_TYPE (int,  prInts);
			ASSERT_ELEM_TYPE (int&, catVec);

			ASSERT_EQ (5, prInts.First());
			ASSERT_EQ (3, prInts.Last());

			// concat multiple
			auto all = Enumerate(numsE).Concat(nums1).Concat(nums2).Concat(numsE).Concat(nums4);
			ASSERT_EQ (1, all.First());
			ASSERT_EQ (7, all.Last());
			ASSERT_EQ (7, all.Count());
		}
	}


	// Concatenation of derived types
	static void ConcatenationsInheritance()
	{
		std::vector<Base>		bases1 { {1}, {2} };
		std::vector<Base>		bases2 { {3}, {4} };

		std::vector<DerivedA>	deriveds1 { { 11, 1.5 }, { 12, 2.5 } };
		std::vector<DerivedA>	deriveds2 { { 13, 3.5 }, { 14, 4.5 } };

		std::vector<DerivedB>	derivedsB { { 26, 'a' }, { 27, 'c' } };

		// mixing related types
		{
			NO_MORE_HEAP;

			const std::vector<Base>&	 bases2C	= bases2;
			const std::vector<DerivedA>& deriveds1C	= deriveds1;

			auto bases			= Concat(bases1, bases2);
			auto constBases		= Concat(bases1, bases2C);
			auto deriveds		= Concat(deriveds1, deriveds2);
			auto constDeriveds	= Concat(deriveds1C, deriveds2);
			auto mixed			= Concat(bases1, deriveds1, bases2);
			auto constMixed1	= Concat(bases1, deriveds1, bases2C);
			auto constMixed2	= Concat(deriveds1C, bases2);
			auto constMixed3	= Concat(deriveds1, bases1, bases2C);

			ASSERT_ELEM_TYPE (Base&,			bases);
			ASSERT_ELEM_TYPE (DerivedA&,		deriveds);
			ASSERT_ELEM_TYPE (Base&,			mixed);
			ASSERT_ELEM_TYPE (const DerivedA&,	constDeriveds);
			ASSERT_ELEM_TYPE (const Base&,		constBases);
			ASSERT_ELEM_TYPE (const Base&,		constMixed1);
			ASSERT_ELEM_TYPE (const Base&,		constMixed2);
			ASSERT_ELEM_TYPE (const Base&,		constMixed3);

			ASSERT_EQ (4, bases.Count());
			ASSERT_EQ (4, bases.Count());
			ASSERT_EQ (6, mixed.Count());
			ASSERT_EQ (6, constMixed1.Count());
			ASSERT_EQ (4, constMixed2.Count());
			ASSERT    (constMixed1.First().x == 1  && constMixed1.Last().x == 4);
			ASSERT    (constMixed3.First().x == 11 && constMixed3.Last().x == 4);
			ASSERT_EQ (12, constMixed1.ElementAt(3)->x);

			mixed.ElementAt(3)->x = 20;
			deriveds.Last().d	  = 50.5;
			ASSERT_EQ (20,   constMixed1.ElementAt(3)->x);
			ASSERT_EQ (50.5, deriveds.Last().d);
			ASSERT_EQ (20,   constDeriveds.ElementAt(1)->x);

			// explicitly:
			auto forcedBaseDeriveds = Concat<Base&> (deriveds1, deriveds2);
		 // auto forcedBaseDeriveds = Concat<Base&> (deriveds1C, deriveds2);	// CTE (losing const)
		 // auto forcedDeriveds = Concat<DerivedA&> (deriveds1, bases1);		// CTE (incompatible type)

			ASSERT_ELEM_TYPE (Base&, forcedBaseDeriveds);

			// Siblings - must be explicit:
			auto siblings = Concat<Base&>(deriveds1, derivedsB);

			ASSERT_ELEM_TYPE (Base&, siblings);

			// Although a common ancestor present can keep it work implicitly in certain cases.
			// Common-base deduction proceeds left to right:
			auto siblingsAndBases1 = Concat(bases1, deriveds1, derivedsB);
			auto siblingsAndBases2 = Concat(deriveds1, bases1, derivedsB);

			// This case the first deduction step fails:
		 // auto siblingsAndBases3 = Concat(deriveds1, derivedsB, bases1);		// CTE - not supported

			ASSERT_ELEM_TYPE (Base&, siblingsAndBases1);
			ASSERT_ELEM_TYPE (Base&, siblingsAndBases2);

			// constness must work too
			auto siblingsAndBases1C = Concat(bases2C,   deriveds1, derivedsB);
			auto siblingsAndBases2C = Concat(deriveds1, bases2C,   derivedsB);

			ASSERT_ELEM_TYPE (const Base&, siblingsAndBases1C);
			ASSERT_ELEM_TYPE (const Base&, siblingsAndBases2C);

			auto siblingsAndBasesDC1 = Concat(deriveds1C, bases1,     derivedsB);
			auto siblingsAndBasesDC2 = Concat(bases1,     deriveds1C, derivedsB);

			ASSERT_ELEM_TYPE (const Base&, siblingsAndBasesDC1);
			ASSERT_ELEM_TYPE (const Base&, siblingsAndBasesDC2);
		}

		// with init lists - note that those get stored into the Enumerable as a ListType
		//					 [or the container configured by ENUMERABLES_BRACEDINIT_BINDING]
		{
			DerivedA d1 { 50, 2.5 };
			DerivedB b1 { 65, 'b' };

			auto s = Concat(bases1, { &d1 });
			ASSERT_EQ		 (3,	 s.Count());
			ASSERT_ELEM_TYPE (Base&, s);

			// Explicitly determined initializer type => can mix convertible elements
			auto s2 = Concat<Base&>(bases1, { &d1, &b1 }, bases2);
			ASSERT_EQ		 (6, s2.Count());
			ASSERT_ELEM_TYPE (Base&, s2);

			auto simplePtrs = Enumerate<Base*>({ &bases1[0], &bases1[1], &d1, &b1, &bases2[0], &bases2[1] });
			ASSERT (AreEqual(simplePtrs, s2.Addresses()));
		}

		// with fluent syntax
		{
			NO_MORE_HEAP;

			// continuation is already an Enumerable
			auto catFiltered = Enumerate(bases1).Concat(Filter(derivedsB, FUN(d, d.c != 'c')));
			ASSERT_ELEM_TYPE (Base&, catFiltered);
			ASSERT_EQ		 (1,	 catFiltered.First().x);
			ASSERT_EQ		 (26,	 catFiltered.Last().x);

			// collection directly as continuation
			auto catVec = Enumerate(bases1).Concat(derivedsB);
			ASSERT_ELEM_TYPE (Base&, catVec);
			ASSERT_EQ		 (1,	 catVec.First().x);
			ASSERT_EQ		 (27,	 catVec.Last().x);
		}
	}


	// Wrapping rvalue collection by value, move internal data on chained operations
	static void ConstructionByMove()
	{
		AllocationCounter allocations;

		auto createList = []() -> Enumerable<Vector2D<int>>
		{
			std::vector<Vector2D<int>> res;
			res.reserve(4);
			res.emplace_back(0, 1);
			res.emplace_back(0, 2);
			res.emplace_back(1, 1);

			return res;
		};

		Vector2D<int> v1 = createList().Where(FUN(v, v.x > 0)).First();

		ASSERT_EQ (1, v1.x);
		ASSERT_EQ (1, v1.y);
		allocations.AssertFreshCount(1);
	}



	void TestConstruction()
	{
		Greet("Construction");

		SeededConstruction();
		CollectionBasics();
		BracedInit();
		BracedInitWithAllocator();
		ConcatenationsSimple();
		ConcatenationsInheritance();
		ConstructionByMove();
	}

}	// namespace EnumerableTests
