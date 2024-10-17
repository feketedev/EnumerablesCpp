
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



	namespace {

		struct CollectionOwner {
			std::vector<int>	myElements;

			const std::vector<int>&		GetElements()	const	{ return myElements; }
			std::vector<int>&			GetElements()			{ return myElements; }

			auto						Elements()		const	{ return Enumerate(myElements); }
			auto						Elements()				{ return Enumerate(myElements); }

			Enumerable<int>				ValuesIface()	const	{ return myElements; }
			Enumerable<const int&>		ElementsIface()	const	{ return myElements; }
			Enumerable<int&>			ElementsIface()			{ return myElements; }
		};


		struct SequenceOwner {
			const size_t count;

			auto				Range()		 const { return Enumerables::Range<size_t>(1, count); }
			Enumerable<size_t>  RangeIface() const { return Enumerables::Range<size_t>(1, count); }
		};


		// ----------------------------------------------------------------------------------------


		struct Base1 {
			int  i;
		
			Base1(int i) : i { i }  {}
			virtual ~Base1() = default;
		};

		struct Base2 {
			char c;

			explicit Base2(char c) : c { c }  {}
			virtual ~Base2() = default;
		};

		struct Derived1 : public Base1 {
			unsigned u;

			Derived1(int ib, unsigned u) : Base1 { ib }, u { u }  {}
		};

		struct Derived2 : public Base2 {
			int i2;

			Derived2(char cb, int i2) : Base2 { cb }, i2 { i2 }  {}
		};

		struct Derived12 : public Base1, public Base2 {
			Derived12(int ib, char cb) : Base1 { ib }, Base2 { cb }  {}
		};

	}


	static void Flattening()
	{
		{
			std::vector<CollectionOwner> collections;
			collections.push_back({ { } });
			collections.push_back({ { 1, 2 } });
			collections.push_back({ { 4 } });
			collections.push_back({ { } });
			collections.push_back({ { 5, 6, 7 } });

			auto cs = Enumerate(collections);

			auto viaVectors			 = cs.Select					(&CollectionOwner::myElements)	 .Flatten();
			auto viaVectorGetters	 = cs.Select<std::vector<int>&>	(&CollectionOwner::GetElements)	 .Flatten();
			auto viaAutoEnumerable   = cs.Select					(FUN(x, x.Elements()))			 .Flatten();
			auto viaIfaceEnumerable  = cs.Select<Enumerable<int&>>	(&CollectionOwner::ElementsIface).Flatten();
			auto viaValuesEnumerable = cs.Select					(&CollectionOwner::ValuesIface)	 .Flatten();

			ASSERT_ELEM_TYPE (int&, viaVectors);
			ASSERT_ELEM_TYPE (int&, viaVectorGetters);
			ASSERT_ELEM_TYPE (int&, viaAutoEnumerable);
			ASSERT_ELEM_TYPE (int&, viaIfaceEnumerable);
			ASSERT_ELEM_TYPE (int,  viaValuesEnumerable);

			// don't leave const overloads unused...
			auto viaConstVectorGetters	 = cs.AsConst().Select<const std::vector<int>&>	(&CollectionOwner::GetElements)  .Flatten();
			auto viaConstAutoEnumerable  = cs.AsConst().Select							(FUN(x, x.Elements()))			 .Flatten();
			auto viaConstIfaceEnumerable = cs.AsConst().Select<Enumerable<const int&>>	(&CollectionOwner::ElementsIface).Flatten();

			ASSERT_ELEM_TYPE (const int&, viaConstVectorGetters);
			ASSERT_ELEM_TYPE (const int&, viaConstAutoEnumerable);
			ASSERT_ELEM_TYPE (const int&, viaConstIfaceEnumerable);

			auto checkElements = [&collections](auto& elems)
			{
				ASSERT_EQ (6,							  elems.Count());
				ASSERT_EQ (&collections[1].myElements[0], &elems.First());
				ASSERT_EQ (&collections[4].myElements[2], &elems.Last());
			};

			checkElements(viaVectors);
			checkElements(viaVectorGetters);
			checkElements(viaAutoEnumerable);
			checkElements(viaIfaceEnumerable);
			checkElements(viaConstVectorGetters);
			checkElements(viaConstAutoEnumerable);
			checkElements(viaConstIfaceEnumerable);

			ASSERT_EQ (6, viaValuesEnumerable.Count());
			ASSERT_EQ (1, viaValuesEnumerable.First());
			ASSERT_EQ (7, viaValuesEnumerable.Last());

			// no change after adding an extra empty:
			collections.push_back({});

			checkElements(viaVectors);
			checkElements(viaVectorGetters);
			checkElements(viaAutoEnumerable);
			checkElements(viaIfaceEnumerable);

			ASSERT_EQ (6, viaValuesEnumerable.Count());
			ASSERT_EQ (1, viaValuesEnumerable.First());
			ASSERT_EQ (7, viaValuesEnumerable.Last());
		}

		{
			auto seqOwners = Enumerate({ SequenceOwner { 0 }, SequenceOwner { 3 }, SequenceOwner { 2 } });

			auto catSeq	   = seqOwners.Select(&SequenceOwner::Range).Flatten();
			auto catSeqIf  = seqOwners.Select(&SequenceOwner::RangeIface).Flatten();

			ASSERT_ELEM_TYPE (size_t, catSeq);
			ASSERT_ELEM_TYPE (size_t, catSeqIf);

			ASSERT_EQ (5, catSeq.Count());
			ASSERT_EQ (5, catSeqIf.Count());

			ASSERT_EQ (1, catSeq.First());
			ASSERT_EQ (1, catSeqIf.First());
			ASSERT_EQ (2, catSeq.Last());
			ASSERT_EQ (2, catSeqIf.Last());
		}
	}



	static void ClosingWithFirsts()
	{
		int nums[] = { 1, 2, 3, 4 };

		// make it prvalue to test
		auto squares = Enumerate(nums).Map(FUN(x, x * x));

		auto squaresClosed  = squares.CloseWithFirst();
		auto squaresClosed2 = squares.CloseWithFirst(2);
		auto squaresClosed3 = squares.CloseWithFirst(6);	// insufficient elems
		auto squaresClosed0 = squares.CloseWithFirst(0);

		AssertEndsLength(squares,		 1, 16, 4);
		AssertEndsLength(squaresClosed,	 1, 1,  5);
		AssertEndsLength(squaresClosed2, 1, 4,  6);
		AssertEndsLength(squaresClosed3, 1, 16, 8);
		AssertEndsLength(squaresClosed0, 1, 16, 4);
	}



	// Such Rotate function was considered on the interface, but that will need a proper Enumerator for n > Count!
	// Nevertheless, it's a usable idea for when the sequence length is known.
	static void Rotate()
	{
		std::vector<int> vec { 1, 2, 3, 4 };

		auto rot0 = Enumerate(vec).CloseWithFirst(0).Skip(0);
		auto rot1 = Enumerate(vec).CloseWithFirst(1).Skip(1);
		auto rot2 = Enumerate(vec).CloseWithFirst(2).Skip(2);
		auto rot4 = Enumerate(vec).CloseWithFirst(4).Skip(4);
		auto rot5 = Enumerate(vec).CloseWithFirst(5).Skip(5);

		auto addrs = Enumerate(vec).Addresses();

		ASSERT (Enumerables::AreEqual(addrs, rot0.Addresses()));

		ASSERT_EQ (4, rot1.Count());
		ASSERT_EQ (4, rot2.Count());
		ASSERT_EQ (4, rot4.Count());
		ASSERT_EQ (3, rot5.Count());	// Skip truncates with this oversimplified method
	}



	static void ElemAccess()
	{
		NO_MORE_HEAP;

		int arr[] = { 1, 2, 3, 4 };

		auto nums = Enumerate(arr);

		int& first  = *nums.ElementAt(0);
		int& second = *nums.ElementAt(1);
		int& fourth = *nums.ElementAt(3);

		ASSERT_EQ (arr + 0, &first);
		ASSERT_EQ (arr + 1, &second);
		ASSERT_EQ (arr + 3, &fourth);
	}



	static void CastConvertElements()
	{
		auto integers = Enumerables::Range<int>(5);

		// Implicit conversion and static_cast operations are no-op for exactly matching type:
		{
			auto shortcut1 = integers.As<int>();
			auto shortcut2 = integers.Cast<int>();

			static_assert (std::is_same<decltype(integers), decltype(shortcut1)>::value, "Unnecessary conversion!");
			static_assert (std::is_same<decltype(integers), decltype(shortcut2)>::value, "Unnecessary conversion!");

			ASSERT (AreEqual(integers, shortcut1));
			ASSERT (AreEqual(integers, shortcut2));
		}

		// These have similar effect, as the language provides:
		{
			auto ushorts1 = integers.As<unsigned short>();				// works, but narrowing warning
			auto ushorts2 = integers.Cast<unsigned short>();			// silent

			ASSERT_ELEM_TYPE (unsigned short, ushorts1);
			ASSERT_ELEM_TYPE (unsigned short, ushorts2);
			ASSERT			 (AreEqual(integers, ushorts1));
			ASSERT			 (AreEqual(integers, ushorts2));
		
			auto convertedObjs1 = integers.As<Base1>();					// using conversion ctor
			auto convertedObjs2 = integers.Cast<Base1>();				//
			
			ASSERT (AreEqual(integers, convertedObjs1.Select(&Base1::i)));
			ASSERT (AreEqual(integers, convertedObjs2.Select(&Base1::i)));

			auto letters = Enumerables::Range('a', 5);

		 //	auto explicitObjs1 = letters.As<Base2>();					// CTE: explicit ctor!
			auto explicitObjs2 = letters.Cast<Base2>();
			
			ASSERT    (AreEqual(letters, explicitObjs2.Select(&Base2::c)));
			ASSERT_EQ ('e', explicitObjs2.Last().c);
		}

		// Avoiding temporaries
		{
		 /*	int	 arr[] = { 1, 2, 3 };
			auto intRefs = Enumerate(arr);

			// Without explicit checks, these would compile with warnings.
			// Keep them CTE!
		 	auto shorts1 = intRefs.As<const short&>();
		 	auto shorts2 = intRefs.Cast<const short&>(); */
		}
	}



	static void CastRelatedElements()
	{
		Derived1	objs1[]	 = { { -1, 1u }, { -5, 2u } };
		Derived2	objs2[]	 = { { 'a', 1 }, { 'c', 2 } };
		Derived12	objs12[] = { { 8, 'e' }, { 9, 'f' } };

		auto ofBase1 = Enumerables::Concat<Base1&>(objs1, objs12);

		auto ptrs1  = Enumerate(objs1).Addresses();
		auto ptrs2  = Enumerate(objs2).Addresses();
		auto ptrs12 = Enumerate(objs12).Addresses();

		// static upcast / reference conversion
		{
			auto bases1 = Enumerate(objs1).As<Base1&>();
			auto bases2 = Enumerate(objs2).Cast<Base2&>();

			ASSERT_ELEM_TYPE (Base1&, bases1);
			ASSERT_ELEM_TYPE (Base2&, bases2);

			AssertEndsLength(bases1.Select(&Base1::i), -1,  -5,  2);
			AssertEndsLength(bases2.Select(&Base2::c), 'a', 'c', 2);
			
			auto basePtrs1 = Enumerate(objs1).Addresses().As<Base1*>();
			auto basePtrs2 = Enumerate(objs2).Addresses().Cast<Base2*>();

			ASSERT_ELEM_TYPE (Base1*, basePtrs1);
			ASSERT_ELEM_TYPE (Base2*, basePtrs2);

			ASSERT (AreEqual(ptrs1, basePtrs1));
			ASSERT (AreEqual(ptrs2, basePtrs2));
		}

		// assured downcast (static/dynamic)
		{
			auto bases    = Enumerate<Base1&>(objs1);
			auto basePtrs = bases.Addresses();

			ASSERT_ELEM_TYPE (Base1&, bases);
			ASSERT_ELEM_TYPE (Base1*, basePtrs);

			auto derivedsA = bases.Cast<Derived1&>();
			auto derivedsB = bases.DynamicCast<Derived1&>();
		
			ASSERT_ELEM_TYPE (Derived1&, derivedsA);
			ASSERT_ELEM_TYPE (Derived1&, derivedsB);
			ASSERT (AreEqual(ptrs1, derivedsA.Addresses()));
			ASSERT (AreEqual(ptrs1, derivedsB.Addresses()));
		}

		// uninformed/filtered dynamic downcast
		{
			auto asDerived1 = ofBase1.Addresses().DynamicCast<Derived1*>();

			ASSERT (AreEqual(ptrs1, asDerived1.NonNulls()));
			ASSERT_EQ (ptrs1.Count() + ptrs12.Count(),	asDerived1.Count());
			ASSERT_EQ (ptrs12.Count(),					asDerived1.Count(nullptr));
			
			auto onlyDerived1 = ofBase1.OfType<Derived1&>();
			auto onlyDerived2 = ofBase1.OfType<Derived2&>();

			ASSERT (AreEqual(ptrs1, onlyDerived1.Addresses()));
			ASSERT (!onlyDerived2.Any());

			auto ptrsBase1 = ofBase1.Addresses();
			ASSERT_ELEM_TYPE (Base1*, ptrsBase1);

			auto ptrsOnlyDerived1 = ptrsBase1.OfType<Derived1*>();
			auto ptrsOnlyDerived2 = ptrsBase1.OfType<Derived2*>();
			
			ASSERT (AreEqual(ptrs1, ptrsOnlyDerived1));
			ASSERT (!ptrsOnlyDerived2.Any());
		}

		// cross cast
		{
			auto someBase1 = Enumerate<Base1&>(objs12);
			auto asBase2   = someBase1.DynamicCast<Base2&>();

			ASSERT_ELEM_TYPE (Base2&, asBase2);
			ASSERT (AreEqual(ptrs12, asBase2.Addresses()));

			auto mixedAsBase2 = ofBase1.Addresses().DynamicCast<Base2*>();
			auto onlyBase2	  = ofBase1.OfType<Base2&>();

			ASSERT_ELEM_TYPE (Base2*, mixedAsBase2);
			ASSERT_ELEM_TYPE (Base2&, onlyBase2);

			ASSERT (AreEqual(ptrs12, mixedAsBase2.NonNulls()));
			ASSERT_EQ (ptrs1.Count() + ptrs12.Count(),	mixedAsBase2.Count());
			ASSERT_EQ (ptrs12.Count(),					mixedAsBase2.Count(nullptr));
		}
	}



	void TestMisc()
	{
		Greet("Misc");

		Flattening();
		ClosingWithFirsts();
		Rotate();
		ElemAccess();
		CastConvertElements();
		CastRelatedElements();
	}

}	// namespace EnumerableTests
