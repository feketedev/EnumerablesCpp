
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

		struct DerivedA : Base {
			double d;
			DerivedA(int bx, double d) : Base { bx }, d { d } {}
		};

		struct DerivedB : Base {
			char c;
			DerivedB(int bx, char c) : Base { bx }, c { c } {}
		};



		static void Add2Float(float& x)		{ x += 2.0f; }

		static void CutToSqrt(float& x)		{ x = std::sqrt(x); }
		static void CutToSqrt(double& x)	{ x = std::sqrt(x); }
	}



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

			// 3. Explicit output (element) type
			// 3.1  Nothing prevents requesting copies of a reference-captured item:
			auto dataCopies = Repeat<int>(mx.data);
			ASSERT_ELEM_TYPE (int, dataCopies);
			ASSERT_EQ		 (7,   dataCopies.First());
			mx.data++;
			ASSERT_EQ		 (8,   dataCopies.First());

			// 3.2  Narrowing rvalue scalars happens at construction
			//		-> constexpr values can be used without warnings
			auto narrowedSix = Repeat<unsigned short>(6);
			ASSERT_ELEM_TYPE (unsigned short, narrowedSix);
			ASSERT_EQ		 (6, narrowedSix.First());

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

			auto oneFloat = Once<float>(9.0);
			ASSERT_ELEM_TYPE (float, oneFloat);
			ASSERT_EQ		 (1,	 oneFloat.Count());
			ASSERT_EQ		 (9.0f,	 oneFloat.Single());

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
	}



	static void SeededConstructionAdvanced()
	{
#	if defined(_DEBUG) && !defined(__clang__) && (_MSC_VER < 1934)
		// Older MSVC doesn't apply NRVO in debug + its move ctor does allocate!
		constexpr unsigned ResultMoves = 1;
#	else
		constexpr unsigned ResultMoves = 0;
#	endif
		
		auto nextChar = [](char c) -> char { return static_cast<char>(c + 1); };


		// ------ I. Custom sequences - using pure step function ------

		// 1. Deduced elem type = Acc = decltype(step(seed)), or the declared result type of step if available
		{
			auto pow2s = Sequence(1u, FUN(x,  2 * x));							// template lambda
			ASSERT_ELEM_TYPE (unsigned,	pow2s);
			ASSERT_EQ		 (1,		pow2s.First());
			ASSERT_EQ		 (64,		pow2s.ElementAt(6));

			auto odds1 = Sequence(1, [](long x) -> long { return x + 2; });		// declared return type
			auto odds2 = Sequence(1, [](long x)         { return x + 2; });		// implicit, but same
			ASSERT_ELEM_TYPE (long,	odds1);
			ASSERT_ELEM_TYPE (long,	odds2);
			ASSERT_EQ		 (1,	odds1.First());
			ASSERT_EQ		 (1,	odds2.First());
			ASSERT_EQ		 (11,	odds1.ElementAt(5));
			ASSERT_EQ		 (11,	odds2.ElementAt(5));

			auto fsqrts1 = Sequence(16.0f, &std::sqrtf);	// matching types, no overloading
		 	auto fsqrts2 = Sequence(16,    &std::sqrtf);	// compiles with warning: Seed -> Accumulator is narrowing init!
			ASSERT_ELEM_TYPE (float,	fsqrts1);
			ASSERT_ELEM_TYPE (float,	fsqrts2);			// element/accumulator type is inferred from step result
			ASSERT_EQ		 (16.0f,	fsqrts1.First());
			ASSERT_EQ		 (2.0f,		fsqrts1.ElementAt(2));
			ASSERT_EQ		 (2.0f,		fsqrts2.ElementAt(2));

			// Seed != Accumulator, illustrating deduction through "probing call"
			auto strings1 = Sequence("a", FUN(s,  std::string(1, nextChar(s[0])) + '-' + s));
			ASSERT_ELEM_TYPE (std::string,	strings1);
			ASSERT_EQ		 ("a",		strings1.First());
			ASSERT_EQ		 ("b-a",	strings1.ElementAt(1));
			ASSERT_EQ		 ("c-b-a",	strings1.ElementAt(2));

			// moving to next elem is allowed (although a bit weird example)
			auto strings2 = Sequence(std::string("x"), [](std::string& s) {
				if (s.back() == '-') {
					s.back() = s[s.size() - 2];
					return std::move(s);
				}
				return std::move(s) + '-';
			});
			ASSERT_ELEM_TYPE (std::string,	strings2);
			ASSERT_EQ		 ("x",		strings2.First());
			ASSERT_EQ		 ("x-",		strings2.ElementAt(1));
			ASSERT_EQ		 ("xx",		strings2.ElementAt(2));
			ASSERT_EQ		 ("xx-",	strings2.ElementAt(3));
		}

		// 2. explicit type: can constexpr-narrow scalar seed + enables overload resolution!
		{
			auto dsqrts = Sequence<double>(16, &std::sqrt);		// overload selected +
			ASSERT_ELEM_TYPE (double,	dsqrts);				// "16" is stored as double!
			ASSERT_EQ		 (16.0,		dsqrts.First());
			ASSERT_EQ		 (2.0,		dsqrts.ElementAt(2));

			auto sqrts = Sequence<float>(16, &std::sqrtf);		// not overloaded, but
			ASSERT_ELEM_TYPE (float,	sqrts);					// pre-converts seed
			ASSERT_EQ		 (16.0,		sqrts.First());
			ASSERT_EQ		 (2.0,		sqrts.ElementAt(2));

			auto pow2sf = Sequence<float>(1, FUN(x,  2 * x));
			ASSERT_ELEM_TYPE (float,	pow2sf);
			ASSERT_EQ		 (1.0f,		pow2sf.First());
			ASSERT_EQ		 (8.0f,		pow2sf.ElementAt(3));

			// Seed -> Accumulator conversion
			auto strings = Sequence<std::string>("a", FUN(s,  s + '-' + nextChar(s.back())));
			ASSERT_ELEM_TYPE (std::string,	strings);
			ASSERT_EQ		 ("a",		strings.First());
			ASSERT_EQ		 ("a-b-c",	strings.ElementAt(2));

			// Explicit type => type in lambda is exact (as opposed to when deduction from seed is needed)
			// Only the seed [this time: int] is stored in the factory
			auto wrappedOdds = Sequence<CountedCopy<int>>(5, FUN(x, x.data + 2));
			ASSERT_ELEM_TYPE (CountedCopy<int>,	wrappedOdds);
			ASSERT_EQ		 (5,				*wrappedOdds.First());
			ASSERT_EQ		 (9,				**wrappedOdds.ElementAt(2));
			ASSERT_EQ		 (1,				wrappedOdds.ElementAt(2)->copyCount);
			ASSERT_EQ		 (ResultMoves,		wrappedOdds.ElementAt(2)->moveCount);

			// Separate Element and Acc. type - convert to complex type only on output
			auto nocopyOdds = Sequence<MoveOnly<int>, int>(5, FUN(x, x + 2));
			ASSERT_ELEM_TYPE (MoveOnly<int>,	nocopyOdds);
			ASSERT_EQ		 (5,				*nocopyOdds.First());
			ASSERT_EQ		 (9,				**nocopyOdds.ElementAt(2));
			ASSERT_EQ		 (ResultMoves,		nocopyOdds.ElementAt(2)->moveCount);

		}

		// +1. Some extremeness + test ref elements at once
		{
			struct Linked {
				int			data;
				Linked*		next;

				bool	HasNext()	const { return next != nullptr; }
				Linked& Next()			  { return *next; }
				// intentionally no const overload!
			};

			Linked l3 { 3, nullptr };
			Linked l2 { 2, &l3 };
			Linked l1 { 1, &l2 };

			auto linkedElems1 = Sequence(l1, FUN(x, x.Next()))
								.TakeUntilFinal(FUN(x, !x.HasNext()));
			
			auto linkedElems2 = Sequence(l1, &Linked::Next)			// test member-pointers -
								.TakeWhile(&Linked::HasNext);		// this logic will skip l3!

			auto linkedPtrs = Sequence(&l1, &Linked::next)
								.TakeWhile(FUN(x,  x != nullptr));	// this works well again

			ASSERT_ELEM_TYPE (Linked&,	linkedElems1);
			ASSERT_ELEM_TYPE (Linked&,	linkedElems2);
			ASSERT_ELEM_TYPE (Linked*,	linkedPtrs);
			ASSERT_EQ		 (3,		linkedElems1.Count());
			ASSERT_EQ		 (3,		linkedElems1.Last().data);
			ASSERT_EQ		 (2,		linkedElems2.Count());
			ASSERT_EQ		 (2,		linkedElems2.Last().data);
			ASSERT_EQ		 (3,		linkedPtrs.Count());
			ASSERT_EQ		 (&l3,		linkedPtrs.Last());

			// Elements as const - using different output and accumulator types
			auto constElems = Sequence<const Linked&, Linked&>(l1, &Linked::Next)
								.TakeUntilFinal(FUN(x, !x.HasNext()));
			
			ASSERT_ELEM_TYPE (const Linked&, constElems);
			ASSERT_EQ		 (3,			 constElems.Count());
			ASSERT_EQ		 (3,			 constElems.Last().data);
		}

		// +2: Member overload resolution
		{
			struct WrappedInt {
				int value;

				WrappedInt(int v) : value { v } {}

				// for testing, no actual sense of overloading
				WrappedInt Next()		{ return { value + 1 }; }
				WrappedInt Next() const { return { value + 1 }; }

				// for testing, utterly ridiculous :)
				void Step()			{ ++value; }
				int  Step() const	{ return value + 1; }
			};

			auto numsPureStep = Sequence<WrappedInt>(1, &WrappedInt::Next);
			auto numsMutating = Sequence<WrappedInt>(1, &WrappedInt::Step);
			ASSERT_ELEM_TYPE (WrappedInt,	numsPureStep);
			ASSERT_ELEM_TYPE (WrappedInt,	numsMutating);

			ASSERT_EQ (1, numsPureStep.First().value);
			ASSERT_EQ (1, numsMutating.First().value);
			ASSERT_EQ (3, numsPureStep.ElementAt(2)->value);
			ASSERT_EQ (3, numsMutating.ElementAt(2)->value);

			WrappedInt (WrappedInt::* constNext)() const = &WrappedInt::Next;
			int	       (WrappedInt::* constStep)() const = &WrappedInt::Step;
			UNUSED (constNext);
			UNUSED (constStep);
		}


		// ------ II. Custom sequences - using mutator step action ------

		// 1. Deduced elem type = Acc = seed
		{
			auto pow2s = Sequence(1u, [](auto& x) { x *= 2; });		// template lambda
			ASSERT_ELEM_TYPE (unsigned,	pow2s);
			ASSERT_EQ		 (1,		pow2s.First());
			ASSERT_EQ		 (64,		pow2s.ElementAt(6));

			auto odds = Sequence(1u, [](unsigned& x) { x += 2; });	// declared parameter type -
			ASSERT_ELEM_TYPE (unsigned,	odds);						// must match Acc& (or compatible &)!
			ASSERT_EQ		 (1,	odds.First());
			ASSERT_EQ		 (11,	odds.ElementAt(5));
		 //	auto odds2 = Sequence(1, [](unsigned& x) { x += 2; });	// CTE, Acc = int

			auto evens = Sequence(16.0f, &Add2Float);				// matching types, no overloading
			ASSERT_ELEM_TYPE (float,	evens);
			ASSERT_EQ		 (16.0f,	evens.First());
			ASSERT_EQ		 (20.0f,	evens.ElementAt(2));

			auto strings = Sequence(std::string("x"), [](std::string& s) {
				if (s.back() == '-')
					s.back() = s[s.size() - 2];
				else
					s += '-';
			});
			ASSERT_ELEM_TYPE (std::string,	strings);
			ASSERT_EQ		 ("x",		strings.First());
			ASSERT_EQ		 ("x-",		strings.ElementAt(1));
			ASSERT_EQ		 ("xx",		strings.ElementAt(2));
			ASSERT_EQ		 ("xx-",	strings.ElementAt(3));

			// Using a mutator function, the accumulator must be decayed
			// - though the seed can be ref-captured still:
			float start = 4.0;
			auto refInit = Sequence(start, &Add2Float);
			ASSERT_ELEM_TYPE (float,	refInit);
			start = 3.0;
			ASSERT_EQ		 (7.0f,		refInit.ElementAt(2));
			ASSERT_EQ		 (3.0f,		refInit.First());
			start = -1.0;
			ASSERT_EQ		 (-1.0f,	refInit.First());
			ASSERT_EQ		 (+3.0f,	refInit.ElementAt(2));
		}

		// 2. explicit type
		{
			auto dsqrts = Sequence<double>(16, &CutToSqrt);			// overload selected +
			ASSERT_ELEM_TYPE (double,	dsqrts);					// "16" is stored as double!
			ASSERT_EQ		 (16.0,		dsqrts.First());
			ASSERT_EQ		 (2.0,		dsqrts.ElementAt(2));
			{
				void (& flOverload)(float&) = CutToSqrt;
				UNUSED (flOverload);
			}

			auto evens = Sequence<float>(16, &Add2Float);			// not overloaded, but
			ASSERT_ELEM_TYPE (float,	evens);						// pre-converts seed
			ASSERT_EQ		 (16.0,		evens.First());
			ASSERT_EQ		 (20.0,		evens.ElementAt(2));
	
			auto pow2sf = Sequence<float>(1, [](float& x) { x *= 2; });
			ASSERT_ELEM_TYPE (float,	pow2sf);
			ASSERT_EQ		 (1.0f,		pow2sf.First());
			ASSERT_EQ		 (8.0f,		pow2sf.ElementAt(3));
	
			// Seed -> Accumulator conversion
			auto strings = Sequence<std::string>("a", [&](auto& s) { s += nextChar(s.back()); });
			ASSERT_ELEM_TYPE (std::string,	strings);
			ASSERT_EQ		 ("a",			strings.First());
			ASSERT_EQ		 ("abc",		strings.ElementAt(2));

			// Only the seed [this time: int] is stored in the factory
			auto wrappedOdds = Sequence<CountedCopy<int>>(5, [](auto& x) { x.data += 2; });
			ASSERT_ELEM_TYPE (CountedCopy<int>,	wrappedOdds);
			ASSERT_EQ		 (5,				*wrappedOdds.First());
			ASSERT_EQ		 (9,				**wrappedOdds.ElementAt(2));
			ASSERT_EQ		 (1,				wrappedOdds.ElementAt(2)->copyCount);	// each elem is copied from Acc
			ASSERT_EQ		 (ResultMoves,		wrappedOdds.ElementAt(2)->moveCount);	// Acc constructed in-place from int Seed

			// Separate Element and Acc. type - convert to complex type only on output
			auto nocopyOdds = Sequence<MoveOnly<int>, int>(5, [](int& x) { x += 2; });
			ASSERT_ELEM_TYPE (MoveOnly<int>,	nocopyOdds);
			ASSERT_EQ		 (5,				*nocopyOdds.First());
			ASSERT_EQ		 (9,				**nocopyOdds.ElementAt(2));
			ASSERT_EQ		 (ResultMoves,		nocopyOdds.ElementAt(2)->moveCount);

			// Ref-captured seed should work with explicit type as well:
			float start = 4.0;
			auto refInit = Sequence<float>(start, &Add2Float);
			ASSERT_ELEM_TYPE (float,	refInit);
			start = 3.0;
			ASSERT_EQ		 (7.0f,		refInit.ElementAt(2));
			ASSERT_EQ		 (3.0f,		refInit.First());
			start = -1.0;
			ASSERT_EQ		 (-1.0f,	refInit.First());
			ASSERT_EQ		 (+3.0f,	refInit.ElementAt(2));
		}

		// +1: Using a void mutator function entails a decayed<Seed> accumulator and element
		{
			int s = 7;
			auto odds = Sequence(s, [](int& x) { x += 2; });
			ASSERT_ELEM_TYPE (int,	odds);
			ASSERT_EQ (7,	odds.First());
			ASSERT_EQ (11,	odds.ElementAt(2));
			s = 1;
			ASSERT_EQ (1,	odds.First());
			ASSERT_EQ (5,	odds.ElementAt(2));
		}
	}

	

	// Wrapping collections
	// NOTE: Old code, incomplete - should cover together with Introduction tests.
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

			// Pointers are scalars as well...
			DerivedA derived1 { 1, 5.0 };
			DerivedA derived2 { 2, 5.1 };

			auto basePtrs = Enumerate<Base*>({ &derived1, &derived2 });
			ASSERT_ELEM_TYPE (Base*, basePtrs);
			ASSERT_EQ (&derived1, basePtrs.First());
			ASSERT_EQ (&derived2, basePtrs.Last());

			auto constPtrs = Enumerate<const Base*>({ &derived1, &derived2 });
			ASSERT_ELEM_TYPE (const Base*, constPtrs);
			ASSERT_EQ (&derived1, constPtrs.First());
			ASSERT_EQ (&derived2, constPtrs.Last());

			// -- Unimplemented combination:
			// No conversion from a previously deduced init-list of pointers.
			// Their runtime conversion would be safe (when possible), but as of now nothing necessitates it internally,
			// using init-lists to yield pointers (and not references) requires explicit type in each top-level function.
			// [In contrast the reference-producing overloads do need such ability to facilitate implicit Concat(bases, deriveds).]
			//
			//	std::initializer_list<DerivedA*> derivedPtrs { &derived1, &derived2 };
			//
			//	auto basePtrs2 = Enumerate<Base*>(std::move(derivedPtrs));		// simulate forwarded param
			//	ASSERT_ELEM_TYPE (Base*, basePtrs2);
			//	ASSERT_EQ (&derived1, basePtrs2.First());
			//	ASSERT_EQ (&derived2, basePtrs2.Last());
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
		{
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

			// Beware of c-strings can decay to pointers! Implicitly, pointers would be interpreted as char& elements.
			// However, if ENUMERABLES_ADD_STRINGLIST_OVERLOADS is true, special overloads guard this situation.
			// (Otherwise, explicit type is required to get pointer semantics.)
			auto words = Enumerate/*<const char*>*/({ "ant", "brick" });
			ASSERT_ELEM_TYPE (const char*, words);
			ASSERT_EQ		 (2, words.Count());
			ASSERT_EQ		 (0, strcmp("ant", words.First()));

			const char c1 = 'a', c2 = 'b';
			auto letters = Enumerate({ &c1, &c2 });
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
		}

		// Currently, lvalue initializers are just treated as regular containers!
		{
			NO_MORE_HEAP;

			int a = 5;

			std::initializer_list<int> lvalList { 10, 11u, 12, 13, 14 };
			auto listReferred = Enumerate(lvalList);
			ASSERT_ELEM_TYPE (const int&, listReferred);
			ASSERT_EQ		 (5,  listReferred.Count());
			ASSERT_EQ		 (10, listReferred.First());
			ASSERT_EQ		 (14, listReferred.Last());

			std::initializer_list<int*> lvalPtrs { &a };
			auto ptrsReferred = Enumerate(lvalPtrs);
			ASSERT_ELEM_TYPE (int* const&, ptrsReferred);		// no fancy tricks here
			ASSERT_EQ		 (&a, ptrsReferred.Single());
		}
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

		// with init lists - note that those might get stored on the heap for the Enumerable if inline (since rvalues)
		//					 -> according to ENUMERABLES_LIST_BINDING by default
		//					 -> define ENUMERABLES_BRACEDINIT_BINDING to use separate container type
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

		// Init-list implicit type deductions
		{
			// No further context: follow Enumerate({ ... }) rules
			{
				auto values = Concat({ 1, 2, 3 }, { 4, 5 }, { 6 });

				// pointers => "ref-capture"
				const int a = 1, b = 2, c = 3;
				auto refs = Concat({ &a }, { &b, &c }, { &a });

				ASSERT_ELEM_TYPE (int,		values);
				ASSERT_ELEM_TYPE (const int&, refs);
				ASSERT    (AreEqual(RangeBetween(1, 6), values));
				ASSERT_EQ (a, refs.First());
				ASSERT_EQ (4, refs.Count());
				ASSERT_EQ (a, refs.Last());
			}

			// Pointer disambiguation by context (neighbor sequences)
			{
				int  nums[] = { 3, 2, 1 };
				int  x = 5, y = 4;
				auto byRef	= Concat(nums, { &x, &y });
				auto copies = Concat(Enumerate(nums).Decay(), { x, y });	// no auto-decay (yet?)
			 //	auto copiesB = Concat(nums, { x, y });						// CTE
				ASSERT_ELEM_TYPE (int&,	 byRef);
				ASSERT_ELEM_TYPE (int,	 copies);
				ASSERT (AreEqual({ 3, 2, 1, 5, 4 }, byRef));
				ASSERT (AreEqual({ 3, 2, 1, 5, 4 }, copies));

				int* ptrs[]	= { nums + 0, nums + 1, nums + 2 };
				int* p = &x;
				auto byRefPtrs = Concat(ptrs, { &p });
				auto copyPtrs  = Concat(Enumerate(ptrs).Decay(), { &x, &y });
			 // auto copyPtrsB = Concat(ptrs, { &x, &y });					// CTE
				ASSERT_ELEM_TYPE (int*&, byRefPtrs);
				ASSERT_ELEM_TYPE (int*,  copyPtrs);
				ASSERT    (AreEqual(byRef.Take(4).Addresses(), byRefPtrs));
				ASSERT    (AreEqual(byRef.Addresses(),			copyPtrs));
				ASSERT_EQ (&p, &byRefPtrs.Last());

				// test some other overloads too:
				auto byRef2	    = Concat({ &x }, nums, { &y });
				auto byRefPtrs2 = Concat({ &p }, ptrs, { &p });
				auto copyPtrs2  = Concat({ &x }, Enumerate(ptrs).Decay(), { p });
				auto copies2    = Concat({ x },  Enumerate(nums).Decay(), { y });
				ASSERT_ELEM_TYPE (int&,	 byRef2);
				ASSERT_ELEM_TYPE (int*&, byRefPtrs2);
				ASSERT_ELEM_TYPE (int*,  copyPtrs2);
				ASSERT_ELEM_TYPE (int,	 copies2);
				ASSERT    (AreEqual({ 5, 3, 2, 1, 4 }, byRef2));
				ASSERT    (AreEqual({ 5, 3, 2, 1, 4 }, copies2));
				ASSERT    (AreEqual(byRef2.Take(4).Addresses(), byRefPtrs2.Take(4)));
				ASSERT_EQ (&p, &byRefPtrs2.Last());
				ASSERT    (AreEqual(byRefPtrs2, copyPtrs2));

				auto byRef3	    = Concat({ &x }, nums);
				auto byRefPtrs3 = Concat({ &p }, ptrs);
				auto copyPtrs3  = Concat({ &x }, Enumerate(ptrs).Decay());
				auto copies3    = Concat({ x },  Enumerate(nums).Decay());
				ASSERT_ELEM_TYPE (int&,	 byRef3);
				ASSERT_ELEM_TYPE (int*&, byRefPtrs3);
				ASSERT_ELEM_TYPE (int*,  copyPtrs3);
				ASSERT_ELEM_TYPE (int,	 copies3);
				ASSERT    (AreEqual({ 5, 3, 2, 1 }, byRef3));
				ASSERT    (AreEqual({ 5, 3, 2, 1 }, copies3));
				ASSERT    (AreEqual(byRef3.Addresses(), byRefPtrs3));
				ASSERT_EQ (ptrs + 2, &byRefPtrs3.Last());
				ASSERT    (AreEqual(byRefPtrs3, copyPtrs3));
			}

			// ref-capturing const character types is disallowed without explicit type:
			// Concat doesn't have extra overload-pairs to disambiguate string literals!
			{
				auto chars = Concat({ 'a', 'b' }, { 'c' }, { 'd' });
				char a, b;
				auto charRefs = Concat({ &a, &b }, { &a }, { &b });		// non-const, OK
				ASSERT_ELEM_TYPE (char,  chars);
				ASSERT_ELEM_TYPE (char&, charRefs);
				ASSERT_EQ ('d', chars.Last());
				ASSERT_EQ (4,   chars.Count());
				ASSERT_EQ (&b,  &charRefs.Last());
				ASSERT_EQ (4,   charRefs.Count());

			 	const char k = 'a', l = 'b';
			 //	auto constRefs = Concat({ &k, &l }, { &k });			// const char*, CTE
			 //	auto strings   = Concat({ "apple" }, { "pie" });		//

				// still should work explicitly
				auto constRefs = Concat<const char&>({ &k, &l }, { &k });
			 	auto strings   = Concat<const char*>({ "apple" }, { "pie" });
				ASSERT_ELEM_TYPE (const char&, constRefs);
				ASSERT_ELEM_TYPE (const char*, strings);
				ASSERT_EQ (k, constRefs.Last());
				ASSERT_EQ (3, constRefs.Count());
				ASSERT_EQ (0, strcmp("pie", strings.Last()));
			}

			// String literals
			{
				const char* cstrings[] = { "apple", "pie" };

				// Prefix/Suffix of const char* / const char& elems resolves the ambiguity of init-lists
				auto s1 = Concat(Enumerate(cstrings).Decay(), { "baked" });
				auto s2 = Concat({ "more" }, Enumerate(cstrings).Decay());
				auto s3 = Concat({ "more" }, Enumerate(cstrings).Decay(), { "baked" });
				auto s4 = Concat({ "send" }, { "more" }, Enumerate(cstrings).Decay());
				ASSERT_ELEM_TYPE (const char*, s1);
				ASSERT_ELEM_TYPE (const char*, s2);
				ASSERT_ELEM_TYPE (const char*, s3);
				ASSERT_ELEM_TYPE (const char*, s4);
				auto strJoin = [](std::string acc, const char* s) { return acc + " " + s; };
				ASSERT_EQ ("apple pie baked",		s1.Aggregate(strJoin));
				ASSERT_EQ ("more apple pie",		s2.Aggregate(strJoin));
				ASSERT_EQ ("more apple pie baked",	s3.Aggregate(strJoin));
				ASSERT_EQ ("send more apple pie",	s4.Aggregate(strJoin));
			 // ASSERT_EQ ("apple pie baked",		s1.Aggregate<std::string>(FUN(a,s, a + " " + s)));
			 // TODO Aggregate: probably should compile  --------^

				const char letters[] = { 'a', 'b', 'c', 'd' };
				const char*   ptrs[] = { letters + 0, letters + 1 };
				const char letter = 'X';

				auto c1 = Concat(letters, { &letter });
				auto p1 = Concat(Enumerate(ptrs).Decay(), { &letter });
				ASSERT_ELEM_TYPE (const char&, c1);
				ASSERT_ELEM_TYPE (const char*, p1);
				ASSERT_EQ ("abcdX",	c1				.Aggregate(FUN(c0, std::string(1, c0)), FUN(a,c, a += c)));
				ASSERT_EQ ("abX",	p1.Dereference().Aggregate(FUN(c0, std::string(1, c0)), FUN(a,c, a += c)));

				// non-const char pointers are unambiguous (can't be string literal)
				char  mutLetters[] = { 'a', 'b' };
				char* mutPtrs[]	   = { mutLetters + 0, mutLetters + 1 };
				char  mutLetter = 'Y';
				auto c2 = Concat(mutLetters, { &mutLetter });
				auto p2 = Concat(Enumerate(mutPtrs).Decay(), { &mutLetter });
				ASSERT_ELEM_TYPE (char&, c2);
				ASSERT_ELEM_TYPE (char*, p2);
				ASSERT (AreEqual({ 'a', 'b', 'Y' }, c2));
				ASSERT (AreEqual(c2, p2.Dereference()));

				// Still no auto-decay
				// auto b1 = Concat(cstrings, { "baked" });		// CTE (friendly error)

				// However, constness should be propagated without any issue:
				auto c3 = Concat(mutLetters, { &letter });
				auto c4 = Concat(letters, { &mutLetter });
				auto p3 = Concat(Enumerate(mutPtrs).Decay(), { &letter });
				auto p4 = Concat(Enumerate(ptrs).Decay(), { &mutLetter });
				ASSERT_ELEM_TYPE (const char&, c3);
				ASSERT_ELEM_TYPE (const char&, c4);
				ASSERT_ELEM_TYPE (const char*, p3);
				ASSERT_ELEM_TYPE (const char*, p4);
				ASSERT (AreEqual({ 'a', 'b', 'X' }, c3));
				ASSERT (AreEqual({ 'a', 'b', 'c', 'd', 'Y' }, c4));
				ASSERT_EQ (3, p3.Count());
				ASSERT_EQ (3, p4.Count());
				ASSERT_EQ (mutLetters,	p3.First());
				ASSERT_EQ (letters,		p4.First());
				ASSERT_EQ (&letter,		p3.Last());
				ASSERT_EQ (&mutLetter,	p4.Last());
			}
		}

		// Concat with fluent syntax - continuation must yield convertible elems!
		{
			// init_lists - with possible literal-conversion
			auto catBraced = Enumerate<int>(nums2).Concat({ 8l, 9u });
			ASSERT_EQ (4, catBraced.First());
			ASSERT_EQ (9, catBraced.Last());
			ASSERT_ELEM_TYPE (int, catBraced);

			// init_lists - runtime conversion allowed to non-scalars
			auto catBracedConv = Enumerate<MoveOnly<int>>(nums2).Concat({ 7, 8 });
			ASSERT_EQ (4, *catBracedConv.First());
			ASSERT_EQ (8, *catBracedConv.Last());
			ASSERT_ELEM_TYPE (MoveOnly<int>, catBracedConv);

			// string literals - work simply due to known TElem
			auto catStrings = Enumerate<std::string>({ "apple", "and" }).Concat({ "orange", "pie" });
			ASSERT_ELEM_TYPE (std::string, catStrings);
			ASSERT_EQ (4, catStrings.Count());
			ASSERT_EQ ("apple and orange pie", catStrings.Aggregate(FUN(a,s,  a + " " + s)));

			auto catCstrings = Enumerate<const char*>({ "apple", "and" }).Concat({ "orange", "pie" });
			ASSERT_ELEM_TYPE (const char*, catCstrings);
			ASSERT_EQ (4, catCstrings.Count());
			ASSERT_EQ (3, strlen(catCstrings.Last()));

			// check const propagation (single-direction only due to fluent Concat)
			const char word[] = "apple";
			char letter = 'X';
			auto catChars = Enumerate(word).Concat({ &letter });
			ASSERT_ELEM_TYPE (const char&, catChars);
			ASSERT_EQ (word,	&catChars.First());
			ASSERT_EQ (&letter, &catChars.Last());

			NO_MORE_HEAP;

			// continuation is already an Enumerable
			auto catFiltered = Enumerate(nums1).Concat(Filter(nums2, FUN(x, x < 5)));
			ASSERT_EQ (1, catFiltered.First());
			ASSERT_EQ (4, catFiltered.Last());
			ASSERT_ELEM_TYPE (int&, catFiltered);

			// collection directly as continuation
			auto catVec = Enumerate(nums1).Concat(nums2);
			ASSERT_EQ (1, catVec.First());
			ASSERT_EQ (5, catVec.Last());
			ASSERT_ELEM_TYPE (int&, catVec);

			// elem type propagation - No intelligence here, just forcing the first type to the continuation.
			auto prCatRef = Range<int>(5, 2).Concat(nums1);
			ASSERT_EQ (5, prCatRef.First());
			ASSERT_EQ (3, prCatRef.Last());
			ASSERT_ELEM_TYPE (int, prCatRef);

			double dubs[] = { 7.0, 8.0 };

			auto catConv1 = Enumerate<double>(dubs).Concat(Enumerate(nums2));
			ASSERT_EQ (7, catConv1.First());
			ASSERT_EQ (5, catConv1.Last());
			ASSERT_ELEM_TYPE (double, catConv1);

			auto catConv2 = Enumerate<double>(dubs).Concat(nums2);
			ASSERT_EQ (7, catConv2.First());
			ASSERT_EQ (5, catConv2.Last());
			ASSERT_ELEM_TYPE (double, catConv2);

		 // of course, incompatible references shouldn't be concatenable!
		 //	auto catConv = Enumerate(dubs).Concat(nums2);		  // CTE!

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
		std::vector<Base>		bases1 { { 1 }, { 2 } };
		std::vector<Base>		bases2 { { 3 }, { 4 } };

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

		// with init lists - note that those get stored into the Enumerable as a ListType
		//					 [or the container configured by ENUMERABLES_BRACEDINIT_BINDING]
		{
			DerivedA d1 { 50, 2.5 };
			DerivedA d2 { 52, 4.0 };
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

			// Without further context, init-lists of pointers default to ref items - as with Enumerate({...})
			// (of course init-lists need to be homogeneous to be deducible this implicitly)
			auto implicitRefs = Concat({ &d1, &d2 }, { &bases1[0] });
			ASSERT_EQ		 (3, implicitRefs.Count());
			ASSERT_ELEM_TYPE (Base&, implicitRefs);

			std::vector<Base*> basePtrs1 = Enumerate(bases1).Addresses().ToList();
			std::vector<Base*> basePtrs2 = Enumerate(bases2).Addresses().ToList();

			// Working with pointers requires explicit type if some elements need decay
			// or no decisive container is present (init-lists only)
			auto s3 = Concat<const Base*>(basePtrs1, { &d1, &b1 }, basePtrs2);
			auto s31 = Concat<Base*>({ &d1 }, { &d2 });
			ASSERT_EQ		 (6, s3.Count());
			ASSERT_EQ		 (2, s31.Count());
			ASSERT_ELEM_TYPE (const Base*, s3);
			ASSERT (AreEqual(simplePtrs, s3));

			// however, a readily instantiated neighbor sequence can decide between pointers or refs implicitly:
			auto s32 = Concat(Enumerate(basePtrs1).Decay(), { &d1, &d2 });						// no auto-decay
			auto s33 = Concat(bases1, { &d1, &d2 });
			auto s34 = Concat({ &d1 }, { &d2 }, Enumerate(basePtrs1).Decay());
			auto s35 = Concat({ &d1 }, { &d2 }, bases1);
			ASSERT_ELEM_TYPE (Base*, s32);
			ASSERT_ELEM_TYPE (Base*, s34);
			ASSERT_ELEM_TYPE (Base&, s33);
			ASSERT_ELEM_TYPE (Base&, s35);
			ASSERT (AreEqual({ 1, 2, 50, 52 }, s32.Select(&Base::x)));
			ASSERT (AreEqual({ 1, 2, 50, 52 }, s33.Select(&Base::x)));
			ASSERT (AreEqual({ 50, 52, 1, 2 }, s34.Select(&Base::x)));
			ASSERT (AreEqual({ 50, 52, 1, 2 }, s35.Select(&Base::x)));

			auto s4 = Concat<Base*>(basePtrs1, { &d1, &d2 }, basePtrs2);
			ASSERT_EQ		 (6, s4.Count());
			ASSERT_EQ		 (simplePtrs.First(), s4.First());
			ASSERT_EQ		 (simplePtrs.Last(), s4.Last());
			ASSERT_EQ		 (&d1, s4.ElementAt(basePtrs1.size()));
			ASSERT_EQ		 (&d2, s4.ElementAt(basePtrs1.size() + 1));
			ASSERT_ELEM_TYPE (Base*, s4);

			// Try more overloads:
			auto s5 = Concat<Base&>({ &d1, &b1 }, bases1, { &d2 });
			ASSERT_EQ		 (5, s5.Count());
			ASSERT_ELEM_TYPE (Base&, s5);

			auto s6 = Concat<Base&>({ &d1, &b1 }, { &d1 }, {&d2});
			ASSERT_EQ		 (4, s6.Count());
			ASSERT_ELEM_TYPE (Base&, s6);

			// Fluent init-lists have fixed type of TElem:
			auto f1 = Enumerate(bases1).Concat({ &d1, &b1 });
			ASSERT_EQ		 (4, f1.Count());
			ASSERT_EQ		 (&bases1[0], &f1.First());
			ASSERT_EQ		 (&b1,		  &f1.Last());
			ASSERT_ELEM_TYPE (Base&, f1);

			// because TElem is forced, in this special situation appending pointers is possible without specifying explicitly
			auto f2 = Enumerate(bases1).Addresses().Concat({ nullptr, &d1, &b1 });
			ASSERT_EQ		 (&bases1[0], f2.First());
			ASSERT_EQ		 (&b1,		  f2.Last());
			ASSERT_EQ		 (1,		  f2.Count(nullptr));
			ASSERT_ELEM_TYPE (Base*, f2);
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
		SeededConstructionAdvanced();
		CollectionBasics();
		BracedInit();
		BracedInitWithAllocator();
		ConcatenationsSimple();
		ConcatenationsInheritance();
		ConstructionByMove();
	}

}	// namespace EnumerableTests
