
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



	void TestMisc()
	{
		Greet("Misc");

		Flattening();
		ClosingWithFirsts();
		Rotate();
		ElemAccess();
	}

}	// namespace EnumerableTests
