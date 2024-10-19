#include "Tests.hpp"
#include "TestUtils.hpp"
#include "Enumerables.hpp"

#include <list>
#include <map>



namespace {

	struct NumIterator;


	template <bool AllowDiff>
	struct EndIterator {
		const int& end;

		template <bool D = AllowDiff>
		std::enable_if_t<D, int>
		operator -(const NumIterator& rhs) const	{ return 1 + end - rhs.x; }
	};


	struct NumIterator {
		static bool creationOccurred;
		static bool incrementOccurred;

		int x;

		NumIterator(int start) : x { start }
		{
			creationOccurred = true;
		}

		void operator ++() 
		{ 
			++x;
			incrementOccurred = true; 
		}

		int	 operator *()								const	{ return x; }
		bool operator !=(const EndIterator<false>& rhs)	const	{ return x <= rhs.end; }
		bool operator !=(const EndIterator<true>&  rhs)	const	{ return x <= rhs.end; }
	};

	bool NumIterator::creationOccurred  = false;
	bool NumIterator::incrementOccurred = false;


	template <bool HasDiff, bool HasSize>
	struct MyStrangeRange {
		const int endNum;

		NumIterator				begin() const { return { 5 }; }
		EndIterator<HasDiff>	end()   const { return { endNum }; }
	};

	// follow std::size
	template <bool D>
	size_t	size(const MyStrangeRange<D, true>& r)	
	{ 
		return static_cast<size_t>(1 + r.endNum - 5);
	}

}



namespace EnumerableTests {

	template <bool HasDiff, bool HasSize>
	static void AssertSizeBehaviour(std::initializer_list<int> expected, const MyStrangeRange<HasDiff, HasSize>& range)
	{
		// -- Standard container capture --

		auto numbers = Enumerate(range);

		ASSERT_EQ (false, NumIterator::creationOccurred);
		ASSERT_EQ (false, NumIterator::incrementOccurred);

		ASSERT_EQ (expected.size(), numbers.Count());

		ASSERT_EQ (!HasSize ||  HasDiff, NumIterator::creationOccurred);		// IteratorEnumerator is preferred for being simpler!
		ASSERT_EQ (!HasSize && !HasDiff, NumIterator::incrementOccurred);

		ASSERT (Enumerables::AreEqual(expected, numbers));

		ASSERT_EQ (true, NumIterator::creationOccurred);
		ASSERT_EQ (true, NumIterator::incrementOccurred);

		NumIterator::incrementOccurred = false;


		// -- Direct iterator capture --

		auto numRange = Enumerate(range.begin(), range.end());
		
		ASSERT_EQ (false, NumIterator::incrementOccurred);
		ASSERT_EQ (expected.size(), numRange.Count());
		ASSERT_EQ (!HasDiff, NumIterator::incrementOccurred);

		ASSERT (Enumerables::AreEqual(expected, numbers));
		ASSERT_EQ (true, NumIterator::incrementOccurred);


		// -- Direct check of Enumerator capability (regardless of actually chosen type) --

		Enumerables::SizeInfo wholeSize = numbers.GetEnumerator().Measure();
		Enumerables::SizeInfo iterSize = numRange.GetEnumerator().Measure();

		ASSERT_EQ (HasSize || HasDiff, wholeSize.IsExact());
		ASSERT_EQ (HasDiff,			   iterSize.IsExact());
		
		if (wholeSize.IsExact())	ASSERT_EQ (expected.size(), wholeSize.value);
		if (iterSize.IsExact())		ASSERT_EQ (expected.size(), iterSize.value);

		NumIterator::creationOccurred  = false;
		NumIterator::incrementOccurred = false;
	}



	static void NonuniformIterators()
	{
		using Enumerables::Def::HasQueryableDistance;
		using Enumerables::Def::HasQueryableSize;

		static_assert ( HasQueryableDistance<NumIterator, EndIterator<true>>::value,  "Internal SFINAE error.");
		static_assert (!HasQueryableDistance<NumIterator, EndIterator<false>>::value, "Internal SFINAE error.");

		// No size info at all
		AssertSizeBehaviour({ 5, 6, 7, 8 }, MyStrangeRange<false, false> { 8 });

		// Iterators have diff
		AssertSizeBehaviour({ 5, 6, 7, 8 }, MyStrangeRange<true,  false> { 8 });
	
		// Collection has queriable size
		AssertSizeBehaviour({ 5, 6, 7, 8 }, MyStrangeRange<false, true> { 8 });

		// Both available (note: IteratorEnumerator is preferred over ContainerEnumerator)
		AssertSizeBehaviour({ 5, 6, 7, 8 }, MyStrangeRange<true,  true> { 8 });
	}



	static void SizeStlContainers()
	{
		// something unused by default binding...
		std::list<int> numList { 3, 5, 8, 10, 12 };

		AllocationCounter allocations;

		auto numbers = Enumerate(numList);

		ASSERT_EQ (numList.size(), numbers.Count());	// O(1)
		
		allocations.AssertFreshCount(0);

		std::vector<int> numVec = numbers.ToList();		// ignore names being messed up a bit vs. STL...
		
		allocations.AssertFreshCount(1);				// 1 reserve

		ASSERT (Enumerables::AreEqual(numList, numVec));


		// -- Also check Enumerator capability directly --

		std::unordered_set<int>	 numSet = numbers.ToHashSet();
		std::map<char, int>		 map { { 'a', 5 }, { 'b', 8 }, { 'c', 10 } };

		Enumerables::SizeInfo s1 = numbers.GetEnumerator().Measure();
		Enumerables::SizeInfo s2 = Enumerate(numVec).GetEnumerator().Measure();
		Enumerables::SizeInfo s3 = Enumerate(numSet).GetEnumerator().Measure();
		Enumerables::SizeInfo sm = Enumerate(map).GetEnumerator().Measure();

		ASSERT (s1.IsExact());
		ASSERT (s2.IsExact());
		ASSERT (s3.IsExact());
		ASSERT (sm.IsExact());

		ASSERT_EQ (numList.size(), s1.value);
		ASSERT_EQ (numList.size(), s2.value);
		ASSERT_EQ (numList.size(), s3.value);
		ASSERT_EQ (map.size(),	   sm.value);
	}



	// should just work automatically
	static void StructuredBindingForIndexed()
	{
		auto numbers = Enumerables::RangeDownBetween(10, 5);

		size_t passed = 0;
		for (auto [index, value] : numbers.Counted()) {

			ASSERT_EQ (passed, index);
			ASSERT_EQ (10 - passed, value);
			
			++passed;
		}
	}



	void TestFeatures17()
	{
		Greet("Features since C++17");
		RESULTSVIEW_DISABLES_ALLOCASSERTS;

		NonuniformIterators();
		SizeStlContainers();
		StructuredBindingForIndexed();
	}


}	// namespace EnumerableTests
