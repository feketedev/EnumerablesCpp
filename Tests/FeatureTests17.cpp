#include "Tests.hpp"
#include "TestUtils.hpp"
#include "Enumerables.hpp"

#include <list>



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
		static bool creationCheck;
		static bool incrementCheck;

		int x;

		NumIterator(int start) : x { start }
		{
			creationCheck = true;
		}

		void operator ++() 
		{ 
			++x;
			incrementCheck = true; 
		}

		int	 operator *()								const	{ return x; }
		bool operator !=(const EndIterator<false>& rhs)	const	{ return x <= rhs.end; }
		bool operator !=(const EndIterator<true>&  rhs)	const	{ return x <= rhs.end; }
	};

	bool NumIterator::creationCheck  = false;
	bool NumIterator::incrementCheck = false;


	template <bool HasDiff, bool HasSize>
	struct MyStrangeRange {
		const int endNum;

		NumIterator				begin() const { return { 5 }; }
		EndIterator<HasDiff>	end()   const { return { endNum }; }
	};

	// follow std::size
	template <bool D>
	size_t	size(const MyStrangeRange<D, true>& r)	{ return r.endNum - 5; }

}



namespace EnumerableTests {

	template <bool HasDiff, bool HasSize>
	static void AssertSizeBehaviour(std::initializer_list<int> expected, const MyStrangeRange<HasDiff, HasSize>& range)
	{
		// -- Standard container capture --

		auto numbers = Enumerate(range);

		ASSERT_EQ (false, NumIterator::creationCheck);
		ASSERT_EQ (false, NumIterator::incrementCheck);

		ASSERT_EQ (expected.size(), numbers.Count());

		ASSERT_EQ (!HasSize,			 NumIterator::creationCheck);
		ASSERT_EQ (!HasSize && !HasDiff, NumIterator::incrementCheck);

		ASSERT (Enumerables::AreEqual(expected, numbers));

		ASSERT_EQ (true, NumIterator::creationCheck);
		ASSERT_EQ (true, NumIterator::incrementCheck);

		NumIterator::incrementCheck = false;


		// -- Direct iterator capture --

		auto numRange = Enumerate(range.begin(), range.end());
		
		ASSERT_EQ (false, NumIterator::incrementCheck);
		ASSERT_EQ (expected.size(), numRange.Count());
		ASSERT_EQ (!HasDiff, NumIterator::incrementCheck);

		ASSERT (Enumerables::AreEqual(expected, numbers));
		ASSERT_EQ (true, NumIterator::incrementCheck);

		NumIterator::creationCheck  = false;
		NumIterator::incrementCheck = false;
	}



	static void NonuniformIterators()
	{
		using Enumerables::Def::HasQueryableDistance;
		using Enumerables::Def::HasQueryableSize;

		static_assert ( HasQueryableDistance<NumIterator, EndIterator<true>>::value,  "Internal SFINAE error.");
		static_assert (!HasQueryableDistance<NumIterator, EndIterator<false>>::value, "Internal SFINAE error.");


		AssertSizeBehaviour({ 5, 6, 7, 8 }, MyStrangeRange<false, false> { 8 });
		AssertSizeBehaviour({ 5, 6, 7, 8 }, MyStrangeRange<true,  false> { 8 });

	// TODO: apply std::size
	//	AssertSizeBehaviour({ 5, 6, 7, 8 }, MyStrangeRange<false, true> { 8 });
	//	AssertSizeBehaviour({ 5, 6, 7, 8 }, MyStrangeRange<true,  true> { 8 });
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

		NonuniformIterators();
		StructuredBindingForIndexed();
	}


}	// namespace EnumerableTests
