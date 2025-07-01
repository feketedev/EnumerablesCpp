
		/* 				 Welcome to Enumerables for C++!			   *
		 *  														   *
		 *  The annotated tests in Introduction*.cpp files present an  *
		 *  overview of the most important features of this library.   *
		 * 															   *
		 *  Part 2:	Transformation methods and built-in "terminal"	   *
		 * 			operations (simple queries or materialization),    *
		 * 			also some internals important for optimization.	   */


#include "Tests.hpp"
#include "TestUtils.hpp"
#include "Enumerables.hpp"



namespace EnumerableTests {


	// 2.1	One-liners to evaluate sequences, handling optionality.
	static void TerminalOperations()
	{
		const int magic = 24;
		auto empty = Enumerables::Empty<int&>();
		auto range = Enumerables::Range(1, 5);
		auto once  = Enumerables::Once(magic);

		// Very basic terminal functions are brought from .Net, with a bit of adjustments
		{
			int		   f  = range.First();		// throws for empty sequence
			const int& m  = once.First();
			const int& m2 = once.Single();		// throws for 0 or multiple elements
			
			int		   l  = range.Last();		// like .First, but iterates all over to end

			ASSERT_EQ (1, f);
			ASSERT_EQ (&magic, &m);
			ASSERT_EQ (&magic, &m2);
			ASSERT_EQ (5, l);
		}

		// Enumerables::Optional is an alias configurable via ENUMERABLES_OPTIONAL_BINDING
		// - see Enumerables_ConfigDefaults.hpp. Default is the included OptResult type -
		// which is:
		//   * immutable 
		//   * offers chainable transformations and
		//   * seamless work with & types
		//   * checked access, throwing Enumerables::LogicException for NoValues
		//   * also queriable reason of error (a very simple built-in enum)
		using Enumerables::Optional;
		using Enumerables::StopReason;

		// Instead of -OrDefault we have -IfAny, and some more!
		{
			Optional<int&>		 n1 = empty.FirstIfAny();		// no throw
			Optional<const int&> m  = once.FirstIfAny();
			
			Optional<const int&> m2 = once.SingleIfAny();		// empty allowed; still throws for multiple!
			Optional<const int&> n2 = empty.SingleIfAny();

			Optional<int&>		 n3 = empty.SingleOrNone();		// no throw - NoValue even for multiple elems
			Optional<int>		 n4 = range.SingleOrNone();

			ASSERT (!n1.HasValue() && !n2.HasValue() && !n3.HasValue() && !n4.HasValue());

			ASSERT_EQ (StopReason::Empty,     n1.ReasonOfMiss());
			ASSERT_EQ (StopReason::Empty,     n2.ReasonOfMiss());
			ASSERT_EQ (StopReason::Empty,     n3.ReasonOfMiss());
			ASSERT_EQ (StopReason::Ambiguous, n4.ReasonOfMiss());

			ASSERT_EQ (magic,  m);
			ASSERT_EQ (&magic, &m.Value());		// variations to obtain address
			ASSERT_EQ (&magic, &*m2);			//
		}

		// Actually OptResult (== Optional) has .OrDefault too!
		// Fallbacks are a separate logical step.
		{
			const int& m1 = empty.FirstIfAny().OrDefault(magic);
			int		   m2 = range.SingleOrNone().OrDefault(magic);

			ASSERT_EQ (&magic, &m1);
			ASSERT_EQ (magic,  m2);

			// can be lazy evaluated
			// [return type must be convertible to original]
			const int& m3 = empty.AsConst().FirstIfAny().OrDefault([&]() -> const int& { return magic; });
			
			int		   p2 = range.SingleOrNone().OrDefault([&]() { return magic + 2; });

			ASSERT_EQ (&magic, &m3);
			ASSERT_EQ (26,		p2);

			// While .OrDefault is for ground values (be they lazy-generated or simple parameters) - to end the chain,
			// .OrFallback can handle an other Optional similarly:

			Optional<int&> a1 = StopReason::Ambiguous;
			Optional<int&> n2 = empty.FirstIfAny().OrFallback(a1);

			ASSERT_EQ (StopReason::Ambiguous, n2.ReasonOfMiss());

			int& p22 = empty.FirstIfAny()
							.OrFallback(a1)
							.OrDefault(p2);
			
			ASSERT_EQ (&p2, &p22);

			int m4 = range.SingleOrNone()
						  .OrFallback([&]() -> Optional<int> { return empty.FirstIfAny(); })
						  .OrFallback([&]() -> Optional<int> { return once.FirstIfAny(); })
						  .OrDefault(p2);
			
			ASSERT_EQ (magic, m4);

			// NOTE: Some common_type mechanism should be considered for lambdas - currently no such implemented.
		}

		// Some other Optional resulting operations
		{
			Optional<int> min = range.Min();	// in .Net depents on elements' nullability - here it's fixed Optional
			int			  max = *range.Max();	// easy to force existence!

			ASSERT_EQ (1, min);
			ASSERT_EQ (5, max);

			Optional<int> third = range.ElementAt(2);	// the discouraged ElementAt is also Optional
			Optional<int> tenth = range.ElementAt(9);

			ASSERT_EQ (3, third);
			ASSERT	  (!tenth.HasValue());
			
			Optional<double> noAvg = empty.Avg<double>();
			ASSERT	  (!noAvg.HasValue());
		}

		// Sum uses default-constructed
		{
			int s1 = empty.Sum();

			Vector2D<double> s2 = Enumerables::Empty<Vector2D<double>>().Sum();

			ASSERT_EQ (0, s1);
			ASSERT_EQ (0.0, s2.x);
			ASSERT_EQ (0.0, s2.y);
		}

		// Some terminal operations form containers out of sequence elements.
		// ToList and ToSet are simplistic and have been covered. ToDictionary needs more input:
		{
			// Suppose a sample record:
			struct Person {
				unsigned		id;
				std::string		name;

				unsigned			GetId()	  const { return id; }
				std::string&		GetName()		{ return name; }
				const std::string&	GetName() const { return name; }

				bool operator ==(const Person& rhs) const   { return id == rhs.id && name == rhs.name; }
			};

			Person personArr[] = {
				{ 1, "Aldo" },
				{ 2, "Ben" },
				{ 3, "Charlie" },
				{ 1, "Dave" }		// duplicate id!
			};

			Enumerable<Person&> persons = personArr;

			// The 1st overload requires just a mapper to designate the keys.
			// Just like .ToList, it decays elements to store them as values:
			std::unordered_map<unsigned, Person>	  dict1 = persons.ToDictionary(FUN(p, p.id));
			
			// The 2nd takes a value-mapper too.
			std::unordered_map<unsigned, std::string> dict2 = persons.ToDictionary(FUN(p, p.id),
																				   FUN(p, p.name));
			ASSERT_EQ (3, dict1.size());
			ASSERT_EQ (3, dict2.size());

			// In both cases, Values are forwarded into the container / value-mapper parameter.
			// This case the input was Person&, so no moves have happened:
			ASSERT_EQ ("Aldo",    personArr[0].name);
			ASSERT_EQ ("Charlie", personArr[2].name);

			// Duplicate keys are handled by the container itself, as configured via DictOperations::Add.
			// Typically the first occurrence should be kept.
			ASSERT_EQ ("Aldo",    dict1[1].name);
			ASSERT_EQ ("Aldo",    dict2[1]);
			ASSERT_EQ ("Charlie", dict1[3].name);
			ASSERT_EQ ("Charlie", dict2[3]);

			// It is possible to use member-pointers:
			std::unordered_map<unsigned, Person>	  dictS1 = persons.ToDictionary(&Person::id);
			std::unordered_map<unsigned, std::string> dictS2 = persons.ToDictionary(&Person::id, &Person::name);

			ASSERT_EQ (dict1, dictS1);
			ASSERT_EQ (dict2, dictS2);

			// To follow .Map/.Select convention, overload-resolution of qualified getters can be enabled by specifying "-Of<K, V>" types
			// explicitly when convenient (although with the caveat that method-pointers cannot be mixed with anything else at the moment).
			std::unordered_map<unsigned, Person>	  dictO1 = persons.ToDictionaryOf<unsigned>(&Person::GetId);
			std::unordered_map<unsigned, std::string> dictO2 = persons.ToDictionaryOf<unsigned, std::string>(&Person::GetId, &Person::GetName);
			std::unordered_map<unsigned, std::string> dictO2C = persons.AsConst()
																	   .ToDictionaryOf<unsigned, std::string>(&Person::GetId, &Person::GetName);
			ASSERT_EQ (dictS1, dictO1);
			ASSERT_EQ (dictS2, dictO2);
			ASSERT_EQ (dictS2, dictO2C);	// called const overload => no "unused function" warning
		}
	}



	// 2.2	Transformations mapping 2+ sequences into one.
	static void CombineSequences()
	{
		int arrX[] = { 0, 1, 2, 3 };
		int arrY[] = { 2, 1, 0 };

		// Common in these transformations is having a parameter to capture the second [and third] sequence.
		// Contrary to common lambda (or simple numbers) argruments these are stored BY REFERENCE, whenever
		// an l-value is received [but are able to store an r-value container BY VALUE] - which makes them
		// fully consistent with the behaviour of Enumerate(c).


		// a) Zip two sequences in parallel to one of combined elems (already featured in LambdaUsage)
		{
			auto sums     = Enumerate(arrX).Zip(arrY, FUN(x, y,  x + y));
			auto realSums = Enumerate(arrX).ZipTo<double>(arrY, FUN(x, y,  x + y));		  // explicit result
			auto points   = Enumerate(arrY).Zip(arrX, FUN(y, x,  Vector2D<int>(x, y)));	  // (swapped lengths)

			// Legth should always be determined by shorter input:
			ASSERT_EQ (3, sums.Count());
			ASSERT_EQ (3, realSums.Count());
			ASSERT_EQ (3, points.Count());

			ASSERT_ELEM_TYPE (int,				sums);
			ASSERT_ELEM_TYPE (double,			realSums);
			ASSERT_ELEM_TYPE (Vector2D<int>,	points);

			ASSERT    (sums.AllEqual(2));
			ASSERT    (realSums.AllEqual(2.0));
			ASSERT    (AreEqual({ 0, 1, 2 }, points.Select(&Vector2D<int>::x)));
			ASSERT    (AreEqual({ 2, 1, 0 }, points.Select(&Vector2D<int>::y)));
		}

		// b) Concatenation
		{
			// i) chain syntax - element type is fully determined by original sequence
			{
				auto all	 = Enumerate(arrX).Concat(arrY);
				auto allReal = Enumerate<double>(arrX).Concat(arrY);

				ASSERT_ELEM_TYPE (int&,		all);
				ASSERT_ELEM_TYPE (double,	allReal);

				ASSERT_EQ (7, all.Count());
				ASSERT_EQ (7, allReal.Count());
				ASSERT_EQ (&arrX[0], &all.First());
				ASSERT_EQ (&arrY[2], &all.Last());

				// taking by value - elem types must be matched when no 2nd -> 1st conversion exists
				// direct braced-initializers are not implemented yet - nested Enumerate can be used
				auto valSuffix1 = Enumerate<int>(arrY).Concat(std::vector<int>{ 6, 7 });
				auto valSuffix2 = Enumerate<int>(arrY).Concat(Enumerate({ 6, 7 }));

				ASSERT_EQ (7, valSuffix1.Last());
				ASSERT_EQ (7, valSuffix2.Last());
				ASSERT_EQ (5, valSuffix1.Count());
				ASSERT_EQ (5, valSuffix2.Count());
			}

			// ii) Free, variadic source-creator function
			//		* includes braced-init support
			//		* also selects common type if exact
			//		* forced type can be set (as for source-creators)
			{
				using Enumerables::Concat;

				const int x = 7;

				auto all	   = Concat(arrX, arrY);
				auto repeated  = Concat(arrY, arrX, arrY);
				auto valSuffix = Concat<int>(arrX, { 5 });		// decaying must be explicit
				auto refSuffix = Concat(arrX, { &x });			// "capture-syntax", see Enumerate({...})
																// Braced-init is implemented for first 3 params!
				
				auto doubles = Concat<double>({ 2.0, 3.5 }, { 1 });		// Value-conversion must be exact

				ASSERT_ELEM_TYPE (int&,			all);
				ASSERT_ELEM_TYPE (int&,			repeated);
				ASSERT_ELEM_TYPE (int,			valSuffix);
				ASSERT_ELEM_TYPE (const int&,	refSuffix);

				ASSERT_EQ (7,  all.Count());
				ASSERT_EQ (10, repeated.Count());
				ASSERT_EQ (5,  valSuffix.Count());
				ASSERT_EQ (5,  valSuffix.Count());

				// For references, sequence of ancestors gets selected implicitly
				struct Base {
					int data; 
					Base(int d) : data { d } {}
				};
				struct Derived : public Base {
					using Base::Base;
				};

				Base		  bases[]	   = { 7, 8 };
				const Derived deriveds[] = { 10, 11 };

				auto asBases = Concat(deriveds, bases, { &deriveds[1] });

				ASSERT_ELEM_TYPE (const Base&, asBases);

				ASSERT_EQ (deriveds,	 &asBases.First());
				ASSERT_EQ (deriveds + 1, &asBases.Last());
			}
		}


		// c) Boolean operations - These are special in that eventually they need to form a SetType anyway.
		//		* An Enumerables::SetType<TElem> can be passed in directly (either l- or r-value)
		//		* Other iterables get stored, then converted to set lazily within the Enumerator.
		//		* The result is not a set! These are set-based *filters* for the input sequence.
		{
			auto diff = Enumerate(arrX).Except(arrY);				// by reference storage
			auto isec = Enumerate(arrX).Intersect(arrY);			//

			// Braced-init is effective if SetType supports it - then it'll be stored by value.
			// Notice that &-ness needs not to mach here, we just need rhs operands for "==".
			auto isecBraced = Enumerate(arrX).Intersect({ 3, 4 });

			ASSERT_ELEM_TYPE (int&, diff);
			ASSERT_ELEM_TYPE (int&, isec);
			ASSERT_ELEM_TYPE (int&, isecBraced);

			ASSERT_EQ (3, diff.Single());
			ASSERT_EQ (3, isecBraced.Single());
			ASSERT	  (AreEqual({ 0, 1, 2 }, isec));

			// To discard certain instances, currently, pointers must be used directly.
			auto refDiff = Enumerate(arrX).Addresses()
										  .Except({ &arrX[1], &arrX[3] })
										  .Dereference();

			ASSERT_ELEM_TYPE (int&, refDiff);
			ASSERT_EQ		 (2, refDiff.Count());
			ASSERT_EQ		 (arrX + 0, &refDiff.First());
			ASSERT_EQ		 (arrX + 2, &refDiff.Last());
		}
	}



	// 2.3	Transformations that must produce their results as a whole list - which they enumerate afterwards.
	static void CachingAlgorithms()
	{
		// These algorithms are either multi-pass or simply need to combine all input items to produce a result sequence.
		// Let's have some fictive record for examples:
		struct Measurement {
			unsigned	sensorId;
			short		value;
			long long	time;
			bool		isOfficial;
		};

		std::vector<Measurement> measurements {
			{ 1, 18, 1001, true },
			{ 1, 24, 1005, true },
			{ 1, 21, 1010, true },
			{ 1, 20, 1015, true },
			{ 2, 19, 1001, true },
			{ 2, 22, 1006, true },
			{ 2, 24, 1011, true },
			{ 2, 23, 1016, true },
			{ 3, 15, 1002, false },
			{ 3, 20, 1010, false },
			{ 3, 18, 1021, false },
			{ 4, 17, 1000, true },
			{ 4, 21, 1004, true },
			{ 4, 23, 1010, true },
			{ 4, 24, 1015, true },
		};

		// Find the maximal measured values from "official" sources, ordered by time of record
		{
			auto maxesAsc = Enumerate(measurements).Where     (&Measurement::isOfficial)
												   .MaximumsBy(&Measurement::value)
												   .OrderBy   (&Measurement::time);

			// Having only filtering/ordering steps, & access to originals is retained:
			Measurement& max0 = maxesAsc.First();
			ASSERT_EQ (1, max0.sensorId);

			// Keep in mind though, that even just calling .First() requires the full evaluation of results:
			//	-> .MaximumsBy is the first caching step
			//		-> steps upstream (.Where) are simple on-line filters, yielding consecutive elements one-by-one
			//		-> upon first Fetch the total input gets evaluated and an array (configured Enumerables::ListType)
			//		   is formed from the maximum places
			//	-> .OrderBy needs the same cache: a complete array to call std::sort on
			//		-> by default it would iterate over input to build it, just like .MaximumsBy, but
			//		-> due to an optimization it detects the exact necessary array is obtainable from the previous step
			//		   [=> no extra copy here], so sorts them directly upon first Fetch
			//		-> being the last step, its cached results stay stored in its Enumerator, waiting for the iteration
			//		   to move forward or be over.
			//	-> .First issues only 1 .FetchNext call - after which the Enumerator gets destroyed, releasing the cache.
			
			// Examining if there's a second item basically executes the whole algorithm again!
			Optional<Measurement&> secondMaxed = maxesAsc.Skip(1).FirstIfAny();
			ASSERT_EQ (2, secondMaxed->sensorId);

			// To avoid this, .ToSnapshot can be put to the end of the Query
			// -> this results in the cache getting stored in the Enumerable object itself, rather than the Enumerator.
			auto maxesSnapshot = maxesAsc.ToSnapshot();

			// Now it's cheap to create and destroy Enumerators multiple times
			// - in return we have a snapshot only, disregarding follow-on changes to the source collection.
			size_t		 count = maxesSnapshot.Count();
			Measurement& max1  = *maxesAsc.ElementAt(1);
			Measurement& max2  = *maxesAsc.ElementAt(2);

			ASSERT_EQ (3,					 count);
			ASSERT_EQ (&secondMaxed.Value(), &max1);
			ASSERT_EQ (4,					 max2.sensorId);

			// Another obvious way is to create a list.
			// Note however that .ToList always decays items - to make them storable in standard containers.
			// This means they get copied from the result cache to a new array (also losing their identity).
			std::vector<Measurement> maxList = maxesAsc.ToList();

			ASSERT_EQ (3, maxList.size());
			ASSERT_EQ (max0.time,	maxList[0].time);
			ASSERT_EQ (max1.time,	maxList[1].time);
			ASSERT_EQ (max2.time,	maxList[2].time);
		}

		// Knowing a bit about the internals, a more efficient version can be written
		// by working with pointers directly (which is, of course, the same as what
		// .ToSnapshot did internally - but also provides access to original objects).
		{
			auto maxesAsc = Enumerate(measurements).Where     (&Measurement::isOfficial)
												   .Addresses ()
												   .MaximumsBy(&Measurement::value)
												   .OrderBy   (&Measurement::time);

			// This way the exact Enumerables::ListType is wanted that is used by .MaximumsBy and .OrderBy,
			// so it can be obtained as a whole!
			std::vector<Measurement*> maxList = maxesAsc.ToList();

			ASSERT_EQ (3,	maxList.size());
			ASSERT_EQ (1,	maxList.front()->sensorId);
			ASSERT_EQ (4,	maxList.back()->sensorId);

			// (This mechanism can work even with interfaced Enuemrable<T>'s involved between the steps
			//  - by using dynamic_cast between Enumerators internally.)
		}

		// Working with interfaced Enumerable<T>, the check for obtainable cache is possible via dynamic_cast.
		// This has great benefit when succeeds (resorting to a single virtual call per full enumeration),
		// but in return adds the unnecessary overhead of dynamic_cast in the (more likely) unsuccessful cases.
		// To opt-out, define ENUMERABLES_EMPLOY_DYNAMICCAST = false in config.
		{
			auto maxes = Enumerate(measurements).Addresses().MaximumsBy(&Measurement::value);
			
			AllocationCounter allocations;

			std::vector<Measurement*> maxList = maxes.ToList();

			const size_t maxListAllocs = allocations.Count();
			
			// have multiple max-places for this test
			ASSERT (maxListAllocs > 1);
			allocations.Reset();

			Enumerable<Measurement*>  maxesIfaced   = maxes;
			std::vector<Measurement*> maxListIfaced = maxesIfaced.ToList();

			// Depending on configured inline buffer size, creation of complex Enumerators could require allocation when type-erased.
			// Not this time:
			static_assert (sizeof(decltype(maxes)::TEnumerator) <= ENUMERABLES_INTERFACED_ETOR_INLINE_SIZE, "Wrong test setup.");

			// The sorted list can be obtained even through type-erasure (no copy occurs)!
			ASSERT_EQ (maxListAllocs, allocations.Count());
		}
		

		// There's no .ThenBy to introduce secondary ordering critera in the Linq way.
		// The simplest way probably is using std::tie!
		{
			auto timeThanIdAsc = Enumerate(measurements).OrderBy(FUN(m, std::tie(m.time, m.sensorId)));

			// check:
			auto areOrdered = [](const Measurement& p, const Measurement& n)
			{
				return p.time < n.time
					|| p.time == n.time && p.sensorId < n.sensorId;
			};
			ASSERT (timeThanIdAsc.AllNeighbors(areOrdered));
		}

		// Notice that -By variants of these arithmetic functions (OrderBy/MinimumsBy/MaximumsBy) take
		// a selector for a property to compare.
		// Custom comparison functions can be used with Order/Minimums/Maximums -
		// either less(a, b) or a.less(b) form is supported - consult ArithmeticTests.cpp for examples.
	}



	// 2.4	Internals to help container materialization.
	static void PreallocationHelp()
	{
		// Amongst the first places to seek for heap-allocation count reduction is filling up containers
		// - sufficient space should be reserved at once, whenever the element count is known ex ante,
		// to avoid reallocations during insertions - and that's what the library intends to achieve:
		{
			AllocationCounter allocations;

			std::vector<int> vec = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };

			allocations.AssertFreshCount(1);

			auto roots = Enumerate(vec)					// known length
							.MapTo<double>(&sqrt);		// 1 -> 1 transform

			std::vector<double> rootVec = roots.ToList();
			
			// In simple cases like this, .ToList should be able to see and preallocate as necessary!
			allocations.AssertFreshCount(1);
			ASSERT_EQ (rootVec.size(), rootVec.capacity());

			// The internals of this are based on the IEnumerator::Measure() (publicly usable) method:
			{
				auto etor = roots.GetEnumerator();

				// does not Fetch any items - so won't trigger any caching -,
				// just a lightweight query which might or might not succeed
				Enumerables::SizeInfo size = etor.Measure();

				// ...this time it will
				ASSERT (size.IsExact() && size == vec.size());
			}

			// If any unpredictable filter is applied though:
			{
				auto smallRoots = roots.Where(FUN(r,  r <= 2));

				Enumerables::SizeInfo size = smallRoots.GetEnumerator().Measure();

				ASSERT (!size.IsExact());
				ASSERT (size.IsBounded() && size.value == vec.size());

				// This SizeInfo won't trigger any reservations, as the result could even be empty.
				// But if you happen to have a good prediction, it can be passed to materializer methods:
				std::vector<double> srVec = smallRoots.ToList(4);

				ASSERT_EQ (4, srVec.capacity());
				allocations.AssertFreshCount(1);
			}
		}

		// same check with HashSets
		{
			AllocationCounter allocations;

			std::unordered_set<int> set0	  { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
			const size_t			setAllocs = allocations.Count();

			allocations.Reset();
			std::unordered_set<int>			set1 = std::move(set0);
			[[maybe_unused]] const size_t	setMoveAllocs = allocations.Count();

			allocations.Reset();
			auto roots = Enumerate(set1).MapTo<double>(&sqrt);
			allocations.AssertFreshCount(0);

			std::unordered_set<double> rootSet = roots.ToSet();

#		if defined(_DEBUG) && !defined(__clang__)
			// MSVC doesn't apply NRVO in debug + its move ctor does allocate!
			allocations.AssertFreshCount(2 * setMoveAllocs + setAllocs);
#		else
			allocations.AssertFreshCount(setAllocs);
#		endif
		}
	}




	void RunIntroduction2()
	{
		Greet("Introduction 2");
		RESULTSVIEW_DISABLES_ALLOCASSERTS;

		TerminalOperations();
		CombineSequences();
		CachingAlgorithms();
		PreallocationHelp();
	}

}	// namespace EnumerableTests
