
#include "Tests.hpp"
#include "TestUtils.hpp"
#include "Enumerables.hpp"
#include <bitset>
#include <string>
#include <memory>


#ifdef __clang__
#	pragma clang diagnostic ignored "-Wfloat-equal"
#endif



namespace EnumerableTests {

	using Enumerables::Indexed;
	using Enumerables::Optional;
	using Enumerables::LogicException;


	static void Orderings()
	{
		// scalars
		{
			int numsArr[] = { 5, -5, 7, 8, 7, 8, -1 };

			auto nums = Enumerate(numsArr);

			// by operator <
			ASSERT_ELEM_TYPE (int&, nums.Minimums());
			ASSERT_ELEM_TYPE (int&, nums.Maximums());

			ASSERT_EQ (-5, nums.Minimums().Single());
			ASSERT_EQ (8,  nums.Maximums().First());
			ASSERT_EQ (2,  nums.Maximums().Count());

			ASSERT_EQ (numsArr + 1, &nums.Minimums().Single());
			ASSERT_EQ (numsArr + 3, &nums.Maximums().First());
			ASSERT_EQ (numsArr + 5, &nums.Maximums().Last());

			// extreme places of some function
			auto minCoss = nums.MinimumsBy(FUN(x, cos((double)x)));
			auto maxCoss = nums.MaximumsBy(FUN(x, cos((double)x)));

			ASSERT_ELEM_TYPE (int&, minCoss);
			ASSERT_ELEM_TYPE (int&, maxCoss);

			ASSERT_EQ (2,			 minCoss.Count());
			ASSERT_EQ (2,			 maxCoss.Count());
			ASSERT_EQ (numsArr + 3,  &minCoss.First());
			ASSERT_EQ (numsArr + 5,  &minCoss.Last());
			ASSERT_EQ (numsArr + 2,  &maxCoss.First());
			ASSERT_EQ (numsArr + 4,  &maxCoss.Last());

			// take function pointer directly when an exact overload exists:
			auto minFltCos = Enumerables::Range<float>(1.0, 5).As<float>().MinimumsBy(&cosf);

			// template function: needs OverloadResolver => explicit result
			auto minDblCos = Enumerables::Range(1.0, 5).MinimumsBy<double>(&cos);

			ASSERT_ELEM_TYPE (float,  minFltCos);
			ASSERT_ELEM_TYPE (double, minDblCos);

			// using binary comparison function - useful with complex types, here it isn't
			using IntBits = std::bitset<8 * sizeof(int)>;
			auto  minBits = nums.Minimums(FUN(l, r,  IntBits((unsigned)abs(l)).count() < IntBits((unsigned)abs(r)).count()));

			ASSERT_EQ (3, minBits.Count());
			ASSERT_EQ (numsArr + 3, &minBits.First());
			ASSERT_EQ (numsArr + 6, &minBits.Last());
		}

		// records
		{
			struct Record {
				unsigned	index;
				double		data;
				std::string name;

				bool HasName()						   const	{ return !name.empty(); }
				bool HasSmallerData(const Record& rhs) const	{ return data < rhs.data; }
			};

			std::vector<Record> recvec { { 1, 1.8, "" },
										 { 2,-1.0, "apple" },
										 { 3, 1.2, "pine" },
										 { 4, 1.5, "pear" },
										 { 4, 1.2, "berry" },
										 { 5, 1.7, "banana" } };
			auto records = Enumerate(recvec);


			// ---- Extreme places ----

			auto posMinPlaces = records.Where	  (FUN(r, r.data >= 0.0))
									   .MinimumsBy(FUN(r, r.data))
									   .Select	  (FUN(r, r.name));

			ASSERT_ELEM_TYPE (std::string&, posMinPlaces);
			ASSERT_EQ		 (2,			posMinPlaces.Count());
			ASSERT_EQ		 ("pine",		posMinPlaces.First());

			auto validMaxPlaces = records.Where	  (&Record::HasName)
										 .Maximums(&Record::HasSmallerData)   // use method as: a.IsBefore(b)
										 .Select  (&Record::name);

			ASSERT_ELEM_TYPE (std::string&, validMaxPlaces);
			ASSERT_EQ		 ("banana",		validMaxPlaces.Single());


			// ---- Extreme values (terminal operations) ----

			double min = *records.Select(&Record::data).Min();
			double max = *records.Select(&Record::data).Max();
			ASSERT_EQ (-1.0, min);
			ASSERT_EQ (1.8,  max);

			auto empty = Enumerables::Empty<Record&>();

			Optional<double> noMin = empty.Select(&Record::data).Min();
			Optional<double> noMax = empty.Select(&Record::data).Max();
			ASSERT (!noMin.HasValue());
			ASSERT (!noMax.HasValue());


			// ---- Behaviour ----

			// Rerunning query should provide up-to-date results
			recvec.push_back({ 6, 1.2 });
			ASSERT_EQ (3, posMinPlaces.Count());
		}
	}



	static void Summation()
	{
		NO_MORE_HEAP;

		Indexed<double> records[] = { { 1, 1.2 },
									  { 2,-1.0 },
									  { 3, 1.2 },
									  { 4, 1.5 },
									  { 5, 1.7 } };

		auto values  = Enumerate(records).Select(&Indexed<double>::value);
		auto indices = Enumerate(records).Select(&Indexed<double>::index);
		ASSERT_ELEM_TYPE (double&, values);
		ASSERT_ELEM_TYPE (size_t&, indices);

		ASSERT_EQ (std::nextafter(0.92, 0.0),	values.Avg());			// undershots by 1 notch - seems acceptable
		ASSERT_EQ (3.0,							indices.Avg<double>());
		ASSERT_EQ (3.0f,						indices.Avg<float>());

		ASSERT_EQ (15,	 indices.Sum());
		ASSERT_EQ (4.6,	 values.Sum());

		auto empty = Enumerables::Empty<double&>();
		ASSERT (!empty.Avg().HasValue());
		ASSERT (empty.Sum() == 0.0);

		// compensated summation example from wiki
		double edgeCase[] = { 1.0, 1e100, 1.0, -1e100 };
		ASSERT_EQ (2.0, Enumerate(edgeCase).Sum());
	}



	static void CopyAvoidance()
	{
		int numsArr[] = { 5, -5, 7, 8, 7, 8, -1 };

		AllocationCounter allocations;

		// --- Obtain Ordered results ---

		// alloc count for filtered (unknown length) results will
		// depend on std::vector's own strategy, so let's test it
		size_t vecResizes2;
		size_t vecResizes5;
		{
			std::vector<int> testVec;
			testVec.push_back(1);
			testVec.push_back(1);
			vecResizes2 = allocations.Count();

			for (int i = 2; i < 5; i++)
				testVec.push_back(1);
			vecResizes5 = allocations.Count();

			ASSERT (vecResizes2 <= 2);
		}
		allocations.Reset();

		// Note that with implicit type <int&> +1 allocation (and only +1) would be required
		// by ToList to convert to decayed <int>.
		std::vector<int> result = Enumerate<int>(numsArr).Where(FUN(x,  x % 2 != 0))
														 .Order()
														 .ToList();
		ASSERT_EQ (5,  result.size());
		ASSERT_EQ (-5, result.front());
		ASSERT_EQ (7,  result.back());
		allocations.AssertFreshCount(vecResizes5);	// point is that only 1 vector gets allocated


		// --- Obtain Ordered results (reserve) ---

		std::vector<double> exactLength = Enumerate(numsArr).MapTo<double>(&std::sqrt<int>)
															.Order()
															.ToList();
		ASSERT_EQ (7,  exactLength.size());
		allocations.AssertFreshCount(1);					// here we can be optimal


		// --- Obtain Minimums ---

		std::vector<int> minimums = Enumerate<int>(numsArr).Minimums()
														   .ToList();
		ASSERT_EQ (1,  minimums.size());					// rolling minimum is exact all along numsArr
		allocations.AssertFreshCount(1);					// - and its list can be obtained



		// --- Obtain and reuse Minimums to Order them ---

		Vector2D<int>				vec2Ds[] = { { 1, 3 }, { -2, 4 }, { 0, 3 }, { -2, 1 } };

		std::vector<Vector2D<int>>	leftmosts = Enumerate<Vector2D<int>>(vec2Ds)
													.MinimumsBy(&Vector2D<int>::x)
													.OrderBy   (&Vector2D<int>::y)
													.ToList();
		ASSERT_EQ (2,			leftmosts.size());					// rolling minimum peak length is 2 here
		allocations.AssertFreshCount(vecResizes2);


		// --- Obtain and reuse Pointed Minimums to Order them ---

		std::vector<Vector2D<int>*>	leftmostPtrs = Enumerate(vec2Ds).Addresses()
																	.MinimumsBy(&Vector2D<int>::x)
																	.OrderBy   (&Vector2D<int>::y)
																	.ToList();
		ASSERT_EQ (2,			leftmostPtrs.size());
		ASSERT_EQ (vec2Ds + 3,	leftmostPtrs[0]);
		ASSERT_EQ (vec2Ds + 1,	leftmostPtrs[1]);					// ordered by y!
		allocations.AssertFreshCount(vecResizes2);


		// --- Obtain and reuse Minimums to Order them (Snapshot) ---

		// A more typical scenario is to just iterate through results instead of .ToList()
		// - without the need to pay attention to ToList's decaying behaviour.
		// Saving the results for repeated enumeration can be achieved by .ToSnapshot().
		// NOTE: Probably could make a shift to std::ref to work with containers, but:
		//			- what about BINDINGs to other libraries
		//			- more often then not a decayed result List is more natural!

		auto leftmostSnap = Enumerate(vec2Ds).MinimumsBy(&Vector2D<int>::x)
											 .OrderBy   (&Vector2D<int>::y)
											 .ToSnapshot();

		ASSERT_EQ (2,			leftmostSnap.Count());
		ASSERT_EQ (vec2Ds + 3,	&leftmostSnap.First());
		ASSERT_EQ (vec2Ds + 1,	&leftmostSnap.Last());
		ASSERT    (vecResizes2 <= 2);
		allocations.AssertFreshCount(vecResizes2);
	}


	// Scan/Aggregate - This is kind of experimental territory.
	static void Aggregation()
	{
		int	 numsArr[] = { 0, 1, 2, 3 };
		auto nums      = Enumerate<int&>(numsArr);

		// No overload-resolution here currently. Let's have exact pointers.
		const size_t& (*maxSize)(const size_t&, const size_t&)	= &std::max<size_t>;
		const int&	  (*maxInt) (const int&, const int&)		= &std::max<int>;

		// Simple aggregation (aka. foldl1)
		//   Accumulator type is deduced based on f(TElem, TElem), if that's well-formed.
		//   &-ness is enabled by lvalueness of TElem itself (.Select(subobject) semantics)
		//   or by explicit override.
		{
			// from prvalues
			{
				size_t	sum03 = Enumerables::Range(4).Aggregate(FUN(a, x,		a + x));
				size_t	max03 = Enumerables::Range(4).Aggregate(maxSize);
				int		sub03 = Enumerables::Range<int>(4).Aggregate(FUN(a, x,	a - x));

				ASSERT_EQ ( 6, sum03);
				ASSERT_EQ ( 3, max03);
				ASSERT_EQ (-6, sub03);

				// Short inputs:
				ASSERT_EQ	 (2,				Enumerables::Once(2).Aggregate(maxInt));
				ASSERT_THROW (LogicException,	Enumerables::Empty<int>().Aggregate(maxInt));
			}

			// from references
			{
				int	 sum03 = nums.Aggregate(FUN(a, x,  a + x));			// function returns prvalue
				int	 sub03 = nums.Aggregate(FUN(a, x,  a - x));
				const int& max03  = nums.Aggregate<const int&>(maxInt);	// const due to std::max
				const int& max03i = nums.Aggregate(maxInt);				// implicit &-ness allowed by lvalue (int&) input

				ASSERT_EQ (6, sum03);
				ASSERT_EQ (-6, sub03);
				ASSERT_EQ (numsArr + 3, &max03);
				ASSERT_EQ (&max03,		&max03i);

				// have some non-const &s too
				auto evenMax = [](int& acc, int& x) -> int&
				{
					return (x % 2 > 0 || acc >= x) ? acc : x;
				};

				int& maxEven  = nums.Aggregate<int&>(evenMax);
				int& maxEveni = nums.Aggregate(evenMax);

				ASSERT_EQ (numsArr + 2, &maxEven);
				ASSERT_EQ (&maxEven,	&maxEveni);

				// Short inputs:
				ASSERT_EQ	 (numsArr + 2,		&Enumerables::Once(numsArr[2]).Aggregate(maxInt));
				ASSERT_THROW (LogicException,	Enumerables::Empty<int&>().Aggregate(maxInt));
			}
		}

		// Initialized accumulator (aka. foldl)
		{
			size_t init = 6;

			auto max503 = Enumerables::Range(4).Aggregate((size_t)5, maxSize);		// In any mixed/rvalue cases,
			auto max603 = Enumerables::Range(4).Aggregate(init,		 maxSize);		// the implicit type is init argument decayed!
			ASSERT_TYPE (size_t, max503);
			ASSERT_TYPE (size_t, max603);
			ASSERT_EQ	(5,		 max503);
			ASSERT_EQ	(6,		 max603);

			int large = 20;
			const int& max103 = nums.Aggregate(numsArr[1], maxInt);					// elemtype+init both lvalues => & result allowed
			const int& maxLrg = nums.Aggregate(large,      maxInt);					// -> const here due to std::max
			ASSERT_EQ (numsArr + 3, &max103);
			ASSERT_EQ (&large,		&maxLrg);

			ASSERT_TYPE (int, nums.Aggregate<int>(numsArr[1], maxInt));				// decaying can be forced

			// Some help is included to decay for incompatible references (but beware and use forced type when necessary)
			ASSERT_TYPE (int, Enumerables::Empty<short&>().Aggregate(large, maxInt));

			std::string abcd = Enumerables::Range('a', 4).Aggregate<std::string>("",	 FUN(s, c,  s + c));
			std::string iii  = Enumerables::Range('a', 4).Aggregate(std::string(3, 'i'), FUN(s, c,  s + c));
			ASSERT_EQ ("abcd",	  abcd);
			ASSERT_EQ ("iiiabcd", iii);

			// Short inputs:
			ASSERT_EQ (5,		Enumerables::Empty<int>().Aggregate(5, maxInt));	// only for value init: won't throw for Empty
			ASSERT_EQ (large,	Enumerables::Once(large).Aggregate(5, maxInt));
			ASSERT_EQ (&large,	&Enumerables::Once(large).Aggregate(numsArr[0], maxInt));
		}

		// Mapped initial accumulator
		{
			std::string bcd = nums.Aggregate(FUN(x,		 std::to_string(x)),
											 FUN(s, x,	 s + static_cast<char>('a' + x)));

			char projChars[] = "ekfghijl";

			std::string ekfg = nums.Aggregate(FUN(x,	 std::string(1, projChars[x])),
											  FUN(s, x,	 s + projChars[x]));

			// NOTE: probably strange (reason for) behaviour with free projections - due to safety measures for subobject selection:
			//		 nums are int & ==> char & result is allowed implicitly...
			char& maxProj1 = nums.Aggregate(FUN(x,		projChars[x]),
											FUN(a, x,	a >= projChars[x] ? a : projChars[x]));

			//		 ...but when input is a prvalue, & needs to be explicitly specified.
			//		 (This really saves the day when the projected value is a subobject of the input.)
			decltype(auto) maxProj2 = Enumerables::Range(4).Aggregate(FUN(x,	projChars[x]),
																	  FUN(a, x,	a >= projChars[x] ? a : projChars[x]));
			ASSERT_TYPE (char, maxProj2);

			char& maxProj3 = Enumerables::Range(4).Aggregate<char&>(FUN(x,		projChars[x]),
																	FUN(a, x,	a >= projChars[x] ? a : projChars[x]));

			ASSERT_EQ ("0bcd",	bcd);
			ASSERT_EQ ("ekfg",	ekfg);
			ASSERT_EQ ('k',				maxProj2);
			ASSERT_EQ (projChars + 1,	&maxProj1);
			ASSERT_EQ (projChars + 1,	&maxProj3);

			// Short inputs:
			ASSERT_THROW (LogicException,	Enumerables::Empty<int>().Aggregate(FUN(x,		std::to_string(x)),
																				FUN(s, x,	s + static_cast<char>('a' + x))));

			ASSERT_EQ	 ("8",				Enumerables::Once(8).Aggregate(FUN(x,		std::to_string(x)),
																		   FUN(s, x,	s + static_cast<char>('a' + x))));
		}

		// Ambiguous cases
		{
			// An object having operator() that accepts the elements.
			// (At the same time a Non-Movable, Non-Assignable accumulator type)
			struct Incrementable {
				const int val;

				Incrementable(int v) : val { v } {}

				Incrementable  operator()(int x) const	 { return Incrementable { x + val }; }

				static Incrementable Decrease(Incrementable obj, int by) { return { obj.val - by }; }
			};

			// - Implicitly:	1st param is treated as a mapper, whenever it's callable with TElem
			//					[No assessment of the result against the given combiner, for that would trigger Error inside lambdas (template callables).]
			// - Explicit type:	1st param is used for initialization, provided it's convertible
			//					To force using its operator() for initial mapping, a wrapper lambda should be utilized.
			//
			// The reason behing these are language mechanics. Probably a rarely encountered case.

			auto wrappedInc = FUN(x,  Incrementable { 5 }(x));

			Incrementable usedMapping = Enumerables::RangeDown(4, 3).Aggregate				 (Incrementable { 5 },	&Incrementable::Decrease);
			Incrementable initDirect  = Enumerables::RangeDown(4, 3).Aggregate<Incrementable>(Incrementable { 5 },	&Incrementable::Decrease);
			Incrementable initConvert = Enumerables::RangeDown(4, 3).Aggregate<Incrementable>(5,					&Incrementable::Decrease);
			Incrementable mapExplicit = Enumerables::RangeDown(4, 3).Aggregate<Incrementable>(wrappedInc,			&Incrementable::Decrease);

			ASSERT_EQ (+4, usedMapping.val);
			ASSERT_EQ (-4, initDirect.val);
			ASSERT_EQ (-4, initConvert.val);
			ASSERT_EQ (+4, mapExplicit.val);
		}


		// Should-work friendly errors:
		//	Enumerables::Range(5).Aggregate<int>(std::make_unique<int>(2), FUN(a, x,  a + x));		// Not callable, nor convertible
		//	Enumerables::Range(5).Aggregate<int>([](std::string s, int x) { return 5; });			// Not callable as combiner
		//	Enumerables::Range<int>(5).Aggregate(maxSize);											// acc assignment is narrowing
		//	Enumerables::Range(5).Aggregate<int>(FUN(a, x,  std::to_string(a + x)));				// Unconvertible combiner result
		//	Enumerables::Range(5).Aggregate([](std::string s, int x) { return 5; });				// Not callable with TElem,TElem
		//	Enumerables::Range(5).Aggregate<int>("asd", FUN(a, x, a + x));							// Can't initialize with value
		//	Enumerables::Range(5).Aggregate<int>(FUN(x, "asd"), FUN(a, x, a + x));					// Can't initialize with result


		// ---- And the same using Scan, with intermediate results accessible ----

		// Simple scan
		{
			using Enumerables::AreEqual;

			// from prvalues
			{
				auto sums  = Enumerables::Range(4).Scan(FUN(a, x,		a + x));
				auto maxes = Enumerables::Range(4).Scan(maxSize);
				auto diffs = Enumerables::Range<int>(4).Scan(FUN(a, x,	a - x));

				ASSERT (AreEqual({ 0u, 1u, 3u, 6u }, sums));
				ASSERT (AreEqual({ 0u, 1u, 2u, 3u }, maxes));
				ASSERT (AreEqual({ 0,  -1, -3, -6 }, diffs));

				// Short inputs:
				ASSERT_EQ (false, Enumerables::Empty<int>().Scan(maxInt).Any());
				ASSERT_EQ (2,	  Enumerables::Once(2).Scan(maxInt).Single());
			}

			// from references
			{
				auto sums   = nums.Scan(FUN(a, x,  a + x));
				auto diffs  = nums.Scan(FUN(a, x,  a - x));
				auto maxes  = nums.Scan<const int&>(maxInt);
				auto maxesi = nums.Scan(maxInt);

				ASSERT_ELEM_TYPE (int,		 sums);		// function returns prvalue
				ASSERT_ELEM_TYPE (int,		 diffs);
				ASSERT_ELEM_TYPE (const int&, maxes);	// const due to std::max
				ASSERT_ELEM_TYPE (const int&, maxesi);	// implicit &-ness allowed by lvalue (int&) input

				ASSERT (AreEqual({ 0,  1,  3,  6 }, sums));
				ASSERT (AreEqual({ 0, -1, -3, -6 }, diffs));
				ASSERT (AreEqual({ 0,  1,  2,  3 }, maxes));
				ASSERT (AreEqual(maxes.Addresses(), maxesi.Addresses()));

				// have some non-const &s too
				auto evenMax = [](int& acc, int& x) -> int&
				{
					return (x % 2 > 0 || acc >= x) ? acc : x;
				};

				auto maxEvens  = nums.Scan<int&>(evenMax);
				auto maxEvensi = nums.Scan(evenMax);

				ASSERT_ELEM_TYPE (int&, maxEvens);
				ASSERT_ELEM_TYPE (int&, maxEvensi);

				const int& first = numsArr[0];
				const int& third = numsArr[2];

				ASSERT (AreEqual({ &first, &first, &third, &third }, maxEvens.Addresses()));
				ASSERT (AreEqual({ &first, &first, &third, &third }, maxEvensi.Addresses()));

				// Short inputs:
				ASSERT_EQ (false,	Enumerables::Empty<int&>().Scan(maxInt).Any());
				ASSERT_EQ (&third,	&Enumerables::Once(third).Scan(maxInt).Single());
			}

			// Initialized accumulator (aka. foldl)
			{
				size_t init = 6;

				auto maxes503 = Enumerables::Range(4).Scan((size_t)5, maxSize);
				auto maxes603 = Enumerables::Range(4).Scan(init,	  maxSize);

				ASSERT_ELEM_TYPE (size_t, maxes503);							// In any mixed/rvalue cases,
				ASSERT_ELEM_TYPE (size_t, maxes603);							// the implicit type is init argument decayed!
				ASSERT (AreEqual({ 5, 5, 5, 5 },  maxes503));
				ASSERT (AreEqual({ 6, 6, 6, 6 },  maxes603));

				int large = 20;
				auto maxes103 = nums.Scan(numsArr[1], maxInt);
				auto maxesLrg = nums.Scan(large,      maxInt);
				ASSERT_ELEM_TYPE (const int&,		maxes103);					// elemtype+init both lvalues => & result allowed
				ASSERT_ELEM_TYPE (const int&,		maxesLrg);					// -> const here due to std::max

				ASSERT (AreEqual({ numsArr + 1, numsArr + 1, numsArr + 2, numsArr + 3 },	maxes103.Addresses()));
				ASSERT (AreEqual(Enumerables::Repeat(&large, 4),							maxesLrg.Addresses()));

				ASSERT_ELEM_TYPE (int, nums.Scan<int>(numsArr[1], maxInt));		// decaying can be forced

				// Some help is included to decay for incompatible references (but beware and use forced type when necessary)
				using ExpectedAcc = std::conditional_t< std::is_same_v<size_t, unsigned>,
														const unsigned&,							// on 32 bit: types match (ref-compatible)
														size_t >;									// on 64 bit: must decay (and convert)
				ASSERT_ELEM_TYPE (ExpectedAcc, Enumerables::Empty<unsigned&>().Scan(init, maxSize));

				auto abcd = Enumerables::Range('a', 4).Scan<std::string>("",	 FUN(s, c,  s + c));
				auto iii  = Enumerables::Range('a', 4).Scan(std::string(3, 'i'), FUN(s, c,  s + c));
				ASSERT (AreEqual({ "a", "ab", "abc", "abcd" },				abcd));
				ASSERT (AreEqual({ "iiia", "iiiab", "iiiabc", "iiiabcd" },	iii));

				// Short inputs:
				ASSERT_EQ (false,	Enumerables::Empty<int>().Scan(5, maxInt).Any());
				ASSERT_EQ (large,	Enumerables::Once(large).Scan(5, maxInt).Single());
				ASSERT_EQ (&large,	&Enumerables::Once(large).Scan(numsArr[0], maxInt).Single());
			}

			// Mapped initial accumulator
			{
				auto bcd = nums.Scan(FUN(x,		std::to_string(x)),
									 FUN(s, x,	s + static_cast<char>('a' + x)));

				char projChars[] = "ekfghijl";

				auto ekfg = nums.Scan(FUN(x,	 std::string(1, projChars[x])),
									  FUN(s, x,	 s + projChars[x]));

				// NOTE: probably strange (reason for) behaviour with free projections - due to safety measures for subobject selection:
				//		 nums are int & ==> char & result is allowed implicitly...
				auto maxProj1 = nums.Scan(FUN(x,	projChars[x]),
										  FUN(a, x,	a >= projChars[x] ? a : projChars[x]));

				//		 ...but when input is a prvalue, & needs to be explicitly specified.
				//		 (This really saves the day when the projected value is a subobject of the input.)
				auto maxProj2 = Enumerables::Range(4).Scan(FUN(x,	 projChars[x]),
														   FUN(a, x, a >= projChars[x] ? a : projChars[x]));

				auto maxProj3 = Enumerables::Range(4).Scan<char&>(FUN(x,	projChars[x]),
																  FUN(a, x,	a >= projChars[x] ? a : projChars[x]));

				ASSERT_ELEM_TYPE (std::string,  bcd);
				ASSERT_ELEM_TYPE (std::string,  ekfg);
				ASSERT_ELEM_TYPE (char,			maxProj2);
				ASSERT_ELEM_TYPE (char&,		maxProj1);
				ASSERT_ELEM_TYPE (char&,		maxProj3);

				ASSERT (AreEqual({ "0", "0b", "0bc", "0bcd" },	bcd));
				ASSERT (AreEqual({ "e", "ek", "ekf", "ekfg" },	ekfg));
				ASSERT (AreEqual({ 'e', 'k', 'k', 'k' },		maxProj2));
				ASSERT (AreEqual({  0,   1,   1,   1  },		maxProj1.MapTo<size_t>(FUN(x,  &x - projChars))));
				ASSERT (AreEqual({  0,   1,   1,   1  },		maxProj3.MapTo<size_t>(FUN(x,  &x - projChars))));

				// Short inputs:
				ASSERT_EQ (false,	Enumerables::Empty<int>().Scan(FUN(x,		std::to_string(x)),
																   FUN(s, x,	s + static_cast<char>('a' + x)))
															 .Any());

				ASSERT_EQ ("8",		Enumerables::Once(8).Scan(FUN(x, std::to_string(x)),
															  FUN(s, x,	s + static_cast<char>('a' + x)))
														.Single());
			}
		}

	}



	void TestArithmetics()
	{
		Greet("Arithmetics");

		Orderings();
		Summation();
		CopyAvoidance();
		Aggregation();
	}

}	// namespace EnumerableTests
