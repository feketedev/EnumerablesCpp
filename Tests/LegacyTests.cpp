
#include "Tests.hpp"
#include "TestUtils.hpp"
#include "Enumerables.hpp"

#include <chrono>
#include <fstream>
#include <sstream>
#include <memory>



namespace EnumerableTests {


#pragma region Old but still useful

namespace Legacy {

	using namespace Enumerables;

	template <typename C>
	static void Print(const char* label, const C& collection, bool leaveSpace = true)
	{
		if (leaveSpace)
			std::cout << std::endl << std::endl;
		std::cout << label;
		for (const auto& v : collection)
			std::cout << v << ", ";
		std::cout << std::endl;
	}


	struct RecB {
		virtual void Print() const = 0;

		virtual ~RecB()   = default;

		RecB(const RecB&) = default;

	protected:
		RecB() = default;
	};

	struct Rec : public RecB {
		int n;
		char c;

		char& GetChar() const { return const_cast<char&>(c); }
		virtual void Print() const override { std::cout << '{' << n << ' ' << c << "}, "; }
		Rec(int n, char c) : n {n}, c {c} {}
	};

	class CollectionOwner {
		std::vector<Rec> records;

	public:
		auto Records() /*const*/ { return Enumerate(records); }
		auto PositiveRecords() const { return Enumerate(records).Where(FUN(r,  r.n > 0)); }
		auto PositiveLetters()
		{
			return Enumerate(records)
				.Where(FUN(r, r.n > 0))
				.MapTo<char&>(FUN(r, r.GetChar()));
		}

		Enumerable<const RecB&> Bases() const {
			auto bases = Enumerate(records).As<const RecB&>();
			ASSERT_ELEM_TYPE (const RecB&, bases);
			return bases;
		}

		Enumerable<const Rec&> EnumFirst() const { return Once(records.front());}

		CollectionOwner(std::vector<Rec> records) : records{ std::move(records) } {}
	};



	enum MaskInstanceSelector { MaskedInstance };

	struct Noisy {
		const int content;
		const bool masked = false;

		void Print(const char* action) const
		{
			if (!masked)
				std::cout << action << '(' << content << ')' << std::endl;
		}
		void PrintNonconst(const char* action)
		{
			std::cout << action << '(' << content << ')' << std::endl;
		}

		// NOTE: stating noexcept is necessary to get equal effect with MSVC and Clang during std::vector::resize
		//		 (MS doesn't provide strong exception guarantee despite the standard demands it)
		Noisy(int c, MaskInstanceSelector)	noexcept : content { c }, masked { true }	{}
		Noisy(int c)								 : content { c }					{ Print("ctor"); }
		Noisy(const Noisy& src)						 : content { src.content }			{ Print("copy"); }
		Noisy(Noisy&& src)					noexcept : content { src.content }			{ Print("move"); }
		virtual ~Noisy()																{ Print("dtor"); }
	};

	struct NoisyNoMove : public Noisy
	{
		using Noisy::Noisy;
		NoisyNoMove(NoisyNoMove&&) = delete;
		NoisyNoMove(const NoisyNoMove&) = default;
	};

	struct NoisyMoveOnly : public Noisy
	{
		using Noisy::Noisy;
		NoisyMoveOnly(const NoisyMoveOnly&) = delete;
		NoisyMoveOnly(NoisyMoveOnly&&) = default;
	};

	static std::ostream& operator <<(std::ostream& os, const Noisy& n)
	{
		return os << n.content;
	}


	static void objectTest()
	{
		CollectionOwner wrap({ { 5, 'a' },{ 6, 'b' },{ -4, 'z' },{ 10, 'g' },{-4, 'a'},{ 10, 'f' },{5, 'x'} });
		for (const Rec& r : wrap.Records())
			r.Print();
		std::cout << std::endl;
		for (const RecB& b : wrap.Bases())
			b.Print();
		std::cout << std::endl;
		for (const Rec& r : wrap.EnumFirst())
			r.Print();
		std::cout << std::endl;
		for (const Rec& r : wrap.PositiveRecords())
			r.Print();

		Print("Letters w/positive: ", wrap.PositiveLetters());

		std::cout << std::endl << "With max ints:      ";
		for (Rec& r : wrap.Records().MaximumsBy(FUN(r, r.n)))
			r.Print();
		std::cout << std::endl << std::endl;
		std::cout << std::endl << "With min ints:      ";
		for (Rec& r : wrap.Records().MinimumsBy(FUN(r, r.n)))
			r.Print();
		std::cout << std::endl << std::endl;


		auto incremented = wrap.Records()
			.MapTo<const int&>(FUN(r, r.n))
			.MapTo<int>(FUN(i, i+1));
		Print("Incremented:        ", incremented);

		auto incremented2 = wrap.Records()
			.Select<int&>	(&Rec::n)
			.MapTo<int>		(FUN(i, i + 1))
			.Map			(FUN(i, i + 1));
		Print("Double Incremented: ", incremented2);

		std::cout << std::endl << "Zipped Incr.-pos.:  ";
		auto rezipped = incremented.ZipTo<Rec>(wrap.PositiveLetters(), FUN(i,c, Rec(i, c)));
		for (auto r : rezipped)
			r.Print();
		std::cout << std::endl << std::endl;
	}


	struct OwnerRec {
		std::unique_ptr<int> i;
		char c;

		OwnerRec(std::unique_ptr<int> i, char c = 'a') : i { std::move(i) }, c { c }	{}
	};

	static void ownerTest()
	{
		std::vector<int> numbers { 1, 2, 3 };
		auto numsFactory = Enumerate(numbers).MapTo<std::unique_ptr<int>>(FUN(x, std::make_unique<int>(x)));

		std::vector<std::unique_ptr<int>> ownedNumbers = numsFactory.ToList();
		auto ownedNums = Enumerate(ownedNumbers).Dereference();

		std::vector<OwnerRec> ownerRecs = numsFactory
			.MapTo<OwnerRec> (FUN(i, OwnerRec(std::move(i))))
			.ToList();

		Enumerable<OwnerRec&> enRecs = Enumerate(ownerRecs);
		//auto selector = [](auto&& r) -> decltype(auto) { return (*r.i); };
		//OwnerRec& first = enRecs.First();
		//int& firstN = selector (first);
		auto recNums = enRecs.MapTo<int&>(FUN(r, *r.i));

		std::cout << std::endl;
		for (int n : ownedNums)
			std::cout << n << ", ";
		std::cout << std::endl;
		for (int n : recNums)
			std::cout << n << ", ";
		std::cout << std::endl;

		const auto& cownerRecs = ownerRecs;
		Enumerable<const OwnerRec&> clonables = Enumerate(cownerRecs);
		std::vector<std::unique_ptr<int>> owned3 = clonables.MapTo<std::unique_ptr<int>>(FUN(cr, std::make_unique<int>(*cr.i))).ToList();

		for (const auto& n : owned3)
			std::cout << *n << ", ";
		std::cout << std::endl;
	}

	static void basicTest()
	{
		std::cout << "Testing Enumerables...\n" << std::endl;

		std::cout << std::endl << "Simple ranges:          ";
		for (int n : Enumerables::RangeBetween(2, 6))
			std::cout << n << ", ";
		std::cout << std::endl;
		for (int n : Enumerables::RangeDownBetween(10, 7))
			std::cout << n << ", ";


		std::vector<int> arr{ 12, 0, -5, -6, 84, 6, 7 };
		auto numbers = Enumerate(arr);

		Print("Numbers:          ", numbers);
		Print("Let's test again: ", numbers);
		std::cout << std::endl;


		std::cout << std::endl << "Same assessment through interfaces:" << std::endl;
		static_assert (sizeof (decltype(numbers.GetEnumerator())) == 4 * sizeof(int*), "error");	// 2 ptrs + vptr + padded bool
		Enumerable<int> iNumbers = numbers.Decay();
		for (int n : iNumbers)
			std::cout << n << ", ";
		std::cout << std::endl;
		for (int n : iNumbers)
			std::cout << n << ", ";
		std::cout << std::endl;

		auto positives = numbers.Where(FUN(x, x > 0)).Where(FUN(x, x % 2 == 0));
		Print("Positive evens:     ", positives);

		std::cout << std::endl << "Through interfaces: ";
		Enumerable<int&> iPositives = positives;
		for (int n : iPositives)
			std::cout << n << ", ";
		std::cout << std::endl;

		auto positiveDups = positives.MapTo<int>(FUN(x, 2 * x));
		Print("Positive evens *2:  ", positiveDups);

		std::cout << std::endl << "Through interfaces: ";
		Enumerable<int> iPositiveDups = positiveDups;
		for (int n : iPositiveDups)
			std::cout << n << ", ";
		std::cout << std::endl;


		auto skips = numbers.Skip(1).Take(2);
		Print("Skip 1 Take 2:      ", skips);

		std::cout << std::endl << "Through interfaces: ";
		Enumerable<int&> iSkips = skips;
		for (int n : iSkips)
			std::cout << n << ", ";
		std::cout << std::endl;

		auto skips2 = numbers.Skip(3).Take(5);
		Print("Skip 3 Take 5:      ", skips2);

		std::cout << std::endl << "Through interfaces: ";
		Enumerable<int&> iSkips2 = skips2;
		for (int n : iSkips2)
			std::cout << n << ", ";
		std::cout << std::endl;

		auto skips3 = numbers.Skip(10).Take(1);
		Print("Skip 10 Take 1:     ", skips3);

		std::cout << std::endl << "Through interfaces: ";
		Enumerable<int&> iSkips3 = skips3;
		for (int n : iSkips3)
			std::cout << n << ", ";
		std::cout << std::endl;



		Enumerable<int&> single = numbers.Where(FUN(x, x == 7));
		std::cout << std::endl << "Single '7':      ";
		auto opt1 = single.Decay().SingleIfAny();
		if (opt1.HasValue())
			std::cout << opt1.Value();

		Enumerable<int&> single8 = numbers.Where(FUN(x, x == 8));
		std::cout << std::endl << "Single '8':      ";
		auto opt2 = single8.Decay().SingleIfAny();
		if (opt2.HasValue())
			std::cout << opt2.Value();
		std::cout << std::endl;


		Enumerable<int> once = Once(5);
		std::cout << std::endl << "'5' Once:      " << once.Single() << std::endl;

		Enumerable<int> differences = numbers.MapNeighbors<int>([](int prev, int next) { return next - prev; });
		Print("Differences between numbers:  ", differences);
		std::cout << std::endl;

		// For Enumerable<V>
		// V:				Output kind:	Storable Tmp:
		//	 [const] T		rvalue			[const] T
		//   [const] T&		lvalue			[const] T*
		//	 T&& - NA.
		//
		// Enumerate (C& collection) -> Enumerable<V>
		// C:						V:
		// C<[const] T>						[const] T&
		// const C<[const]T>				const T&
		// initializer_list<[const] T>		[const] T
		// initializer_list<[const] T&>		[const] T&
		//
		// Repeat (T val, UInt) / Once (T val)  -> Enumerable<V>
		// T:			V:
		// T&&			V
		// [const] T&	[const] T&

		{
			Enumerable<int> tenFour = Once(10).Concat(Once(4));
			Print("Ten-Four:  ", tenFour);
			std::cout << std::endl;

			int a = 11;
			Enumerable<int&> onceA = Once(a);
			std::cout << std::endl << "a:  ";
			auto enumerator = onceA.GetEnumerator();
			enumerator.FetchNext();
			int& n = enumerator.Current();
			n++;
			for (const auto& n2 : onceA)
				std::cout << n2 << ", ";
			std::cout << std::endl;
		}


		for (int& n : positives)
			n++;
		std::cout << std::endl;
		Print("No positive evens:  ", numbers);
		std::cout << std::endl;

		int& a = numbers.Where(FUN(x, x == 0)).Single();
		a++;
		if (numbers.Where(FUN(x, x == 0)).Any())
			std::cout << "Zero eliminated." << std::endl;


		//auto multiNums = Repeat (numbers, 3);
		Enumerable<Enumerable<int>&> multiNums = Repeat(iNumbers, 3);
		std::cout << std::endl;
		std::cout << std::endl << "Flattened lists:  ";
		for (int n : multiNums.Flatten())
			std::cout << n << ", ";
		std::cout << std::endl;

		// watch out for lifetimes for materialized Enumerables!
		Enumerable<int> materialized = Enumerate(std::vector<int>(arr)).Decay();
		Enumerable<Enumerable<int>> multiNums2 = Repeat(materialized, 3).Decay();
		std::cout << std::endl;
		std::cout << std::endl << "Flattened lists:  ";
		for (int n : multiNums2.Flatten())
			std::cout << n << ", ";
		std::cout << std::endl;

		std::vector<int*> ptrArr;
		auto pointers = Enumerate(ptrArr).Decay();
		Enumerable<int*> iPointers = pointers;

		std::cout << std::endl;
		std::cout << std::endl;

		std::vector<int> list1 = numbers.ToList();
		std::vector<int> list2 = numbers.ToList<10>();	// test SmallList fallback
		ASSERT_EQ (list1, list2);
		ASSERT_EQ (numbers.Count(), list1.size());
		ASSERT_EQ (numbers.First(), list1.front());
		ASSERT_EQ (numbers.Last(), list1.back());
	}


	static void conversionsTest()
	{
		struct Generator {
			std::vector<int> mNumbers { 2, 3, 4, 5 };

			Enumerable<const int&> IntConstRefs()
			{
				std::cout << std::endl << "Given by const &..." << std::endl;
				return Enumerate(std::vector<int>(mNumbers));
			}

			Enumerable<int&> IntRefs()
			{
				std::cout << std::endl << "Given by &..." << std::endl;
				return Enumerate(mNumbers);
			}

			Enumerable<int> IntValues()
			{
				std::cout << std::endl << "Given by prvalue..." << std::endl;
				return Range(2, 4);
			}

			Enumerable<int*> IntPointers()
			{
				std::cout << std::endl << "Given by ptr..." << std::endl;
				return Enumerate(mNumbers).Addresses();
			}

			Enumerable<int&> CachedResults()
			{
				int max = 4;
				return Enumerate(mNumbers)
					.Where(FUN(x,  x <= max))
					.ToSnapshot();
			}

			Enumerable<const int&> MaterializedResults()
			{
				std::vector<int> locals { 2, 3, 4, 5 };
				int max = 4;

				return Enumerate(locals)
					.Where(FUN(x, x <= max))
					.AsConst()
					.ToMaterialized();
			}

			Enumerable<const int&> MaterializedAutoConverted()
			{
				// Note that returning Enumerable<int&> should NOT work here,
				// as the Enumerable itself should be immutable (in reality readonly).
				return std::vector<int> { 2, 3, 4, 5 };
			}

			Enumerable<const int&> MaterializedAutoConvertedLocal()
			{
				std::vector<int> locals { 2, 3, 4, 5 };
				locals[2] *= 2;
				locals[3] *= 2;
				return locals;
				// fortunately 'locals' must be treated as rvalue as of C++14!
				// https://stackoverflow.com/questions/25875596/can-returning-a-local-variable-by-value-in-c11-14-result-in-the-return-value-b
			}

			Enumerable<int> MaterializedAutoConvertedByvalue()
			{
				std::vector<int> locals { 2, 3, 4, 5 };
				locals[2] *= 2;
				locals[3] *= 2;
				return locals;
			}
		} generate;

		struct Consumer {
			void ByConstRef(Enumerable<const int&> nums)
			{
				Print("Taken by const &:   ", nums, false);
			}

			void ByRef(Enumerable<int&> nums)
			{
				Print("Taken by &:         ", nums, false);
			}

			void ByPtr(Enumerable<int*> nums)
			{
				Print("Taken by ptr:       ", nums.Dereference(), false);
			}

			void ByConstPtr(Enumerable<const int*> nums)
			{
				Print("Taken by const ptr: ", nums.Dereference(), false);
			}

			void ByValue(Enumerable<int> nums)
			{
				Print("Taken by prvalue:   ", nums, false);
			}
		} consume;

		Print("Wrapped array:                 ", generate.IntConstRefs());
		Print("Cached enumeration (max 4):    ", generate.CachedResults());
		Print("Materialized enumerationarray: ", generate.MaterializedResults());

		auto copied = Enumerate(std::vector<int>(generate.mNumbers));
		auto moved  = Enumerate(generate.IntValues().ToList());
		Print("Copied self-contained:         ", copied);
		Print("Moved  self-contained:         ", moved);

		// implicitly:
		consume.ByConstRef(generate.IntValues());
		consume.ByConstRef(generate.IntConstRefs());
		consume.ByValue(generate.IntValues());
		consume.ByValue(generate.IntConstRefs());
		consume.ByValue(generate.IntRefs());
		consume.ByRef(generate.IntRefs());
	 // consume.ByRef(generate.IntValues());					// CTE
	 // consume.ByRef(generate.IntConstRefs());				// CTE
		consume.ByConstRef(generate.IntRefs());
		consume.ByConstPtr(generate.IntPointers());
		consume.ByPtr(generate.IntPointers());
	 // consume.ByPtr(generate.IntPointers().AsConst());		// CTE


		std::vector<int> nums2 { 6, 7, 8, 9 };
		Enumerable<int> prnums = Enumerate<const int&> (nums2);
		Print("PRnums:  ", prnums);

		int max1 = prnums.Maximums().Single();
		int max2 = *prnums.Max();
		Print("Max:     ", prnums.Maximums());
		std::cout << " == " << max1 << " == " << max2 << std::endl;


		std::cout << std::endl << "Testing implicit conversion of container..." << std::endl;
		std::vector<int> vector { 5, 6, 7, 8 };
		consume.ByRef(vector);
		consume.ByConstRef(vector);
		consume.ByValue(vector);

		std::cout << std::endl << "From const..." << std::endl;
		//consume.ByRef(const_cast<const std::vector<int>&>(vector));		// CTE
		consume.ByConstRef(const_cast<const std::vector<int>&>(vector));
		consume.ByValue(const_cast<const std::vector<int>&>(vector));
		std::cout << std::endl << "From rvalue..." << std::endl;
		consume.ByConstRef(generate.MaterializedAutoConverted());
		consume.ByConstRef(generate.MaterializedAutoConvertedLocal());
		consume.ByValue(generate.MaterializedAutoConvertedByvalue());
	}


	static void storableTest()
	{
		using namespace Enumerables::TypeHelpers;

		std::cout << std::endl << "noise test" << std::endl;
		{
			NoisyMoveOnly toMove { 1 };
			NoisyMoveOnly n1 = std::move(toMove);
			NoisyNoMove n2 { 2 };
			NoisyNoMove n2b = n2;
			Noisy n3 = Noisy { 3 };
		}

		std::cout << std::endl << std::endl << "-- Basic storable operation --" << std::endl;
		{
			auto makePRval = []() -> Noisy { return Noisy { 2, MaskedInstance }; };

			{
				auto pass = [](Noisy n) -> Noisy { return n; };

				Noisy n1 = makePRval();
				StorableT<Noisy> n2 = makePRval();
				StorableT<Noisy> n3 = pass(makePRval());
			}

			std::cout << std::endl << "Storing as value..." << std::endl;
			{
				Noisy leftVal { 1, MaskedInstance };

				StorableT<Noisy> s1 = leftVal;
				StorableT<Noisy> s2 = makePRval();
				StorableT<Noisy> s3 = Noisy { 3, MaskedInstance };
				Noisy xval { 4, MaskedInstance };
				StorableT<Noisy> s4 = std::move(xval);

				std::cout << std::endl << "Accessing..." << std::endl;
				ReviveConst<Noisy> (s1).Print("lval access");
				Noisy c1 = Revive<Noisy> (s1);
				ReviveConst<Noisy> (s2).Print("const lval access");
				Noisy c2 = ReviveConst<Noisy> (s2);
				// ReviveConst<Noisy>(s2).PrintNonconst("");			// should be CTE!
				ReviveConst<Noisy> (s3).Print("move access");
				Noisy c3 = PassRevived<Noisy> (s3);
				std::cout << std::endl << "Exiting scope" << std::endl;
			}

			std::cout << std::endl << "Storing as ref..." << std::endl;
			{
				Noisy leftVal { 1 };

				StorableT<Noisy&> s1 = leftVal;
				// StorableT<const Noisy&> s2 = makePRval();			//
				// StorableT<Noisy&> s3 = Noisy{ 3, MaskedInstance };	// should be CTE!

				std::cout << std::endl << "Accessing..." << std::endl;
				ReviveConst(s1).Print("lval access");
				Noisy a1 = Revive(s1);
				ReviveConst(s1).Print("const lval access");
				Noisy b1 = ReviveConst(s1);
				// ReviveConst (s1).PrintNonconst("");					// should be CTE!
				Revive(const_cast<const StorableT<Noisy&>&> (s1))
					.PrintNonconst("nonconst access");					// no error, storage remains const
				ReviveConst(s1).Print("move access");
				Noisy c1 = PassRevived(s1);
				std::cout << std::endl << "Exiting scope" << std::endl;
			}
		}

		{
			std::cout << std::endl << "-- Constructing list --" << std::endl;
			std::initializer_list<Noisy> list { 1, 2, 3 };

			auto testEnumerating = [](const auto& noisies)
			{
				Print("direct iteration: ", noisies, false);

				std::cout << std::endl << "converting ToList" << std::endl;
				std::vector<Noisy> lis = noisies.ToList();
				Print("list iteration: ", lis, false);

				std::cout << std::endl << "converting ToMaterialized" << std::endl;
				auto mat = noisies.ToMaterialized();
				Print("materialized iteration: ", mat, false);

				std::cout << std::endl << "taking Minimums" << std::endl;
				auto mins = noisies.MinimumsBy(FUN(n,  n.content));
				Print("minimums iteration:\n", mins, false);

				std::cout << std::endl << "taking Maximums" << std::endl;
				auto maxes = noisies.MaximumsBy(FUN(n,  n.content));
				Print("maximums iteration: ", maxes, false);

				std::cout << std::endl << "Exiting scope" << std::endl;
			};
			auto testEnumeratingCached = [](const auto& noisies)
			{
				std::cout << std::endl << "converting ToSnapshot" << std::endl;
				auto cached = noisies.ToSnapshot();
				Print("cached iteration: ", cached, false);

				std::cout << std::endl << "Exiting scope" << std::endl;
			};


			std::cout << std::endl << "-- Testing Filter by reference --" << std::endl;
			auto filteredRef = Enumerate(list).Where(FUN(n, n.content != 2));
			testEnumerating(filteredRef);
			testEnumeratingCached(filteredRef);

			std::cout << std::endl << "-- Testing Filter by value --" << std::endl;
			auto filteredVal = Enumerate<Noisy> (list).Where(FUN(n, n.content != 2));
			testEnumerating(filteredVal);

			std::cout << std::endl << "Exiting scope" << std::endl;
		}
	}


	static void initializerTest()
	{
		Enumerable<int> ints = Enumerables::Empty<int>();
		{
			auto localInts = Enumerate({ 1, 2, 3, 4 });
			ints = localInts;
			static_assert (std::is_same<decltype(localInts)::TElem, int>::value, "error");
		}
		Print("Init by-val ints:  ", ints);
		{
			ints = Enumerate<int> ({ 1, 2u, 3, 4 });
		}
		Print("Init explicit by-val ints:  ", ints);
	 // ints = Enumerate<int&> ({ 1, 2, 3, 4 });		// CTE


		int a = 5, b = 6, c = 7;
		auto ints2 = Enumerate({ &c, &b, &a });
		Print("Init by-ref ints:  ", ints2);
		c = 8;
		Print("Init by-ref ints:  ", ints2);

		auto ints3 = Enumerate<int&> ({ &c, &b, &a });
		ints3.First() = 1;
		auto ints4 = Enumerate<const int&> ({ &c, &b, &a });
		Print("Init explicit by-ref ints:  ", ints4);

	 // auto bad = Enumerate<int> ({ &c, &b, &a });		// CTE

		auto ptrs = Enumerate<int*> ({ &c, &b, &a });
		*ptrs.Last() = 10;
		Print("Init explicit pointers:     ", ptrs.Dereference());


		std::initializer_list<int> lvalList { 10, 11u, 12, 13, 14 };
		auto intsReferred = Enumerate(lvalList);
		Print("Wrap referred init-list:  ", intsReferred);
	}


	static void scannerTest()
	{
		auto numbers = Enumerables::RangeBetween(0, 10);

		int sum1a = numbers.Aggregate				(5, FUN(a,n,  a + n));
		int sum1b = numbers.Aggregate<int>			(5, FUN(a,n,  a + n));
	//	int sum1c = numbers.Aggregate<const int&>	(5, FUN(a,n,  a + n));		// CTE
		std::cout << sum1a << ' ' << sum1b << std::endl;

		int sum2a = numbers.Aggregate				(FUN(a,n,  a + n));
		int sum2b = numbers.Aggregate<int>			(FUN(a,n,  a + n));
	//	int sum2c = numbers.Aggregate<const int&>	(FUN(a,n,  a + n));		// CTE
		std::cout << sum2a << ' ' << sum2b << std::endl;


		std::vector<int> numberCont { 1, 2, 3, 4, 5 };
		auto numberRefs = Enumerate<const int&> (numberCont);

		int sum3a = numberRefs.Aggregate				(5, FUN(a,n,  a + n));
		int sum3b = numberRefs.Aggregate<int>			(5, FUN(a,n,  a + n));
	//	int sum3c = numberRefs.Aggregate<const int&>	(5, FUN(a,n,  a + n));		// CTE
		std::cout << sum3a << ' ' << sum3b << std::endl;

		int sum4a = numberRefs.Aggregate				(FUN(a,n,  a + n));
		int sum4b = numberRefs.Aggregate<int>			(FUN(a,n,  a + n));
	//	int sum4c = numberRefs.Aggregate<const int&>	(FUN(a,n,  a + n));		// CTE
		std::cout << sum4a << ' ' << sum4b << std::endl;
	}


	static void perfTest(int cycles, bool prealloc)
	{
		using namespace std::chrono;

		std::vector<Rec> recordsIn;
		recordsIn.reserve(100000);
		for (int i = 0; i < recordsIn.capacity(); i++)
			recordsIn.push_back(Rec(i, 'a' + (char)(i % ('z' - 'a'))));

		std::cout << "Perftest " << cycles << " cycles on " << recordsIn.size() << " elements." << std::endl;
		std::cout << "Preallocation for results: " << (prealloc ? "YES" : "NO") << std::endl;

		std::chrono::milliseconds legacyRangebasedMs;
		{
			std::vector<Rec>  records = recordsIn;
			std::vector<char> lastSelected;

			auto legacyStart = system_clock::now();
			for (int i = 0; i < cycles; i++) {
				std::vector<char> selected;
				if (prealloc)
					selected.reserve(records.size());

				for (const Rec& r : records) {
					if ((r.GetChar() + r.n) % 3 != 0)
						continue;
					selected.push_back(r.c);
				}

				records[1].c = selected[2];	// prevent optimizations
				lastSelected.swap(selected);
			}
			legacyRangebasedMs = duration_cast<milliseconds>(system_clock::now() - legacyStart);

			std::cout << "Legacy iteration took: \t\t\t" << legacyRangebasedMs.count() << " ms" << std::endl;
			std::cout << std::endl << std::endl;
		}

		std::chrono::milliseconds legacyIterMs;
		{
			std::vector<Rec>  records = recordsIn;
			std::vector<char> lastSelected;
			if (prealloc)
				lastSelected.reserve(records.size());

			auto legacyStart = system_clock::now();
			for (int i = 0; i < cycles; i++) {
				std::vector<char> selected;
				if (prealloc)
					selected.reserve(records.size());

				for (auto it = records.cbegin(); it != records.cend(); ++it) {
					const Rec& r = *it;
					if ((r.GetChar() + r.n) % 3 != 0)
						continue;
					selected.push_back(r.c);
				}

				records[1].c = selected[2];	// prevent optimizations
				lastSelected.swap(selected);
			}
			legacyIterMs = duration_cast<milliseconds>(system_clock::now() - legacyStart);

			std::cout << "Legacy iterator iteration took: \t" << legacyIterMs.count() << " ms" << std::endl;
			auto diff = legacyIterMs.count() - legacyRangebasedMs.count();
			std::cout << "Iterator overhead:              \t" << (double)diff*100/(double)legacyRangebasedMs.count() << '%' << std::endl;
			std::cout << std::endl << std::endl;
		}


		std::chrono::milliseconds enumeratedQueryMs;
		{
			std::vector<Rec> records = recordsIn;
			auto query = Enumerate(records)
				.Where (FUN(r, (r.GetChar() + r.n) % 3 == 0))
				.Select(&Rec::c);

			std::vector<char> lastSelected;

			auto enumeratedQueryStart = system_clock::now();
			for (int i = 0; i < cycles; i++) {
				std::vector<char> selected = query.ToList(prealloc ? records.size() : 0);

				records[1].c = selected[2];		// prevent optimizations
				selected.swap(lastSelected);
			}
			enumeratedQueryMs = duration_cast<milliseconds>(system_clock::now() - enumeratedQueryStart);

			std::cout << "Enumerated query iteration took: \t" << enumeratedQueryMs.count() << " ms" << std::endl;
			auto diff = enumeratedQueryMs.count() - legacyIterMs.count();
			std::cout << "Query overhead:                  \t" << (double)diff*100/(double)legacyIterMs.count() << '%' << std::endl;
			std::cout << std::endl << std::endl;
		}


		std::chrono::milliseconds enumeratedMs;
		{
			std::vector<Rec>  records = recordsIn;
			std::vector<char> lastSelected;

			auto enumeratedStart = system_clock::now();
			for (int i = 0; i < cycles; i++) {
				std::vector<char> selected = Enumerate(records)
					.Where (FUN(r, (r.GetChar() + r.n) % 3 == 0))
					.Select(&Rec::c)
					.ToList(prealloc ? records.size() : 0);

				records[1].c = selected[2];		// prevent optimizations
				selected.swap(lastSelected);
			}
			enumeratedMs = duration_cast<milliseconds>(system_clock::now() - enumeratedStart);

			std::cout << "Enumerated iteration took:           \t"  << enumeratedMs.count() << " ms" << std::endl;
			auto diff = enumeratedMs.count() - legacyIterMs.count();
			std::cout << "Enumerable assembly + query overhead:\t" << (double)diff*100/(double)legacyIterMs.count() << '%' << std::endl;
			std::cout << std::endl << std::endl;
		}
	}

}	// namespace Legacy

#pragma endregion




#pragma region Reference checking helpers

	static constexpr char TranscriptReferenceFile[]	= "legacyOutput.txt";
	static constexpr char TranscriptDumpFile[]		= "legacyOutputAct.txt";


	struct OutRedirector {
		std::streambuf* const	original;

		OutRedirector(std::ostream& os, std::ostream& file) : original { os.rdbuf(file.rdbuf()) }
		{
		}
		~OutRedirector()	{ std::cout.set_rdbuf(original); }
	};



	static std::string GetDir(const std::string& path)
	{
		size_t pos = path.rfind('/');
		if (pos == std::string::npos)
			pos = path.rfind('\\');
		return path.substr(0, pos == std::string::npos ? 0 : pos + 1);
	}


	static bool CompareResults(std::ifstream& reference, std::stringstream& act)
	{
		constexpr size_t MaxChunk = 300;
		constexpr size_t MaxPrint = 5;
		char			 line[MaxChunk], actLine[MaxChunk];
		long			 ln = 1;
		long			 diffs = 0;

		while (reference.getline(line, MaxChunk)) {
			if (!act.getline(actLine, MaxChunk)) {
				std::cout << "  FAILED Reference comparison: reached end of actual output!" << std::endl;
				return false;
			}

			if (!AssertEq(std::string { line }, std::string { actLine }, "FAILED Reference comparison", TranscriptReferenceFile, ln)) {
				if (++diffs >= MaxPrint) {
					std::cout << "Too many differences...";
					return false;
				}
			}
			++ln;
		}
		if (act.getline(actLine, 2)) {
			std::cout << "  FAILED Reference comparison: reached end of reference output!" << std::endl;
			return false;
		}
		return true;
	}


	static void CheckTranscript(const std::string& directory, std::stringstream& transcript)
	{
		const std::string refPath = directory + TranscriptReferenceFile;
		const std::string actPath = directory + TranscriptDumpFile;

		std::ifstream reference { refPath.c_str() };
		if (!reference.good()) {
			std::cerr << "Reference file '" << refPath << "' not found!\n";
			return;
		}

		if (!CompareResults(reference, transcript)) {
			std::ofstream actual { actPath.c_str() };
			actual << transcript.str();
			std::cout << "    --> See actual results in '" << actPath << "'!" << std::endl;
		}
	}


#pragma endregion



	void LegacyTests(const char myPath[])
	{
		Greet("Legacy tests");

		std::stringstream transcript;
		{
			OutRedirector redirGuard { std::cout, transcript };

			using namespace Legacy;

			basicTest();
			objectTest();
			ownerTest();
			conversionsTest();
			storableTest();
			initializerTest();
			scannerTest();
		}

		CheckTranscript(GetDir(myPath), transcript);
	}



	void LegacyPerfTests()
	{
#	ifdef _DEBUG
		constexpr unsigned speedFactor = 1;
#	else
		constexpr unsigned speedFactor = 30;
#	endif

		Legacy::perfTest(speedFactor * 50, true);
		Legacy::perfTest(speedFactor * 50, false);
	}


}	// EnumerableTests
