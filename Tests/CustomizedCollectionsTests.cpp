
#include "Tests.hpp"
#include "TestUtils.hpp"
#include "TestAllocator.hpp"
#include "Enumerables.hpp"



namespace EnumerableTests {

	namespace {

		// has no std::hash => Require custom template parameters!
		struct Person {
			unsigned		id;
			std::string		name;
			unsigned		age;


			bool HasEqualData(const Person& rhs) const
			{
				return name == rhs.name
					&& age == rhs.age;
			}

			bool operator ==(const Person& rhs) const
			{
				return id == rhs.id
					&& HasEqualData(rhs);
			}


			size_t HashData() const
			{
				std::hash<std::string> hs;
				return 17 * age + hs(name);
			}

			size_t Hash() const
			{
				return id + HashData();
			}
		};
	}



	static void CustomHashes()
	{
		const Person groupA[] = {
			{ 1, "Aldo",	23 },
			{ 2, "Charlie", 42 },
			{ 5, "Dave",	44 },
		};

		const Person groupB[] = {
			{ 1, "Aldo",	23 },
			{ 3, "Charlie", 42 },
			{ 5, "Dorothy",	44 },
		};

		auto hashLambda		= [](const Person& p) { return p.Hash(); };
		auto dataHashLambda	= [](const Person& p) { return p.HashData(); };
		auto idHashLambda	= [](const Person& p) { return std::hash<unsigned>{}(p.id); };
		
		auto idsEqualLambda = [](const Person& l, const Person& r)
		{
			return l.id == r.id;
		};

		auto dataEqualLambda = [](const Person& l, const Person& r)
		{
			return l.HasEqualData(r);
		};

		// A) Lambdas can be passed in easily, but designating the deduced set will require types
		using HasherLambda	   = decltype(hashLambda);
		using DataHasherLambda = decltype(dataHashLambda);
		using IdHasherLambda   = decltype(idHashLambda);
		using EqualIdsLambda  = decltype(idsEqualLambda);
		using EqualDataLambda = decltype(dataEqualLambda);


		// B) Default-constructible function objects to be used as type-only arguments
		struct Hasher {
			size_t operator ()(const Person& p) const { return p.Hash(); }
		};

		struct DataHasher {
			size_t operator ()(const Person& p) const { return p.HashData(); }
		};

		struct IdHasher {
			std::hash<decltype(Person::id)> h;

			size_t operator ()(const Person& p) const { return h(p.id); }
		};


		struct IdComparer {
			bool operator ()(const Person& l, const Person& r) const
			{
				return l.id == r.id;
			}
		};

		struct DataComparer {
			bool operator ()(const Person& l, const Person& r) const
			{
				return l.HasEqualData(r);
			}
		};


		// 1. Simple terminal operations
		{
			auto allMembers = Enumerables::Concat(groupA, groupB);

			// Using standard op ==
			auto distincts1 = allMembers.ToHashSet<Hasher>();
			auto distincts2 = allMembers.ToHashSet(0, hashLambda);
			
			ASSERT_TYPE (std::unordered_set<Person COMMA Hasher>,		distincts1);
			ASSERT_TYPE (std::unordered_set<Person COMMA HasherLambda>, distincts2);

			ASSERT_EQ (distincts1.size(), 5u);			// both groups having Aldo
			ASSERT    (EqualSets(distincts1, distincts2));


			// Full comparison still, just with worse hash
			auto distincts3 = allMembers.ToHashSet<IdHasher>();
			auto distincts4 = allMembers.ToHashSet(0, idHashLambda);

			ASSERT_TYPE (std::unordered_set<Person COMMA IdHasher>,		  distincts3);
			ASSERT_TYPE (std::unordered_set<Person COMMA IdHasherLambda>, distincts4);

			ASSERT (EqualSets(distincts1, distincts3));
			ASSERT (EqualSets(distincts1, distincts4));


			// Resorting to "id" field
			auto uniqueIdd1 = allMembers.ToHashSet<IdHasher, IdComparer>();
			auto uniqueIdd2 = allMembers.ToHashSet(0, idHashLambda, idsEqualLambda);

			ASSERT_TYPE (std::unordered_set<Person COMMA IdHasher COMMA IdComparer>,		   uniqueIdd1);
			ASSERT_TYPE (std::unordered_set<Person COMMA IdHasherLambda COMMA EqualIdsLambda>, uniqueIdd2);
			
			auto idsAsc = Enumerate(uniqueIdd1).Select(&Person::id).Order();
			
			ASSERT (AreEqual({ 1, 2, 3, 5 }, idsAsc));
			ASSERT (EqualSets(uniqueIdd1, uniqueIdd2));


			// By content, ignoring "id"
			auto uniqueData1 = allMembers.ToHashSet<DataHasher, DataComparer>();
			auto uniqueData2 = allMembers.ToHashSet(0, dataHashLambda, dataEqualLambda);

			ASSERT_TYPE (std::unordered_set<Person COMMA DataHasher COMMA DataComparer>,		  uniqueData1);
			ASSERT_TYPE (std::unordered_set<Person COMMA DataHasherLambda COMMA EqualDataLambda>, uniqueData2);

			ASSERT_EQ (4, uniqueData1.size());
			ASSERT_EQ (1, Enumerate(uniqueData1).Count(FUN(p,  p.name == "Charlie")));
			ASSERT_EQ (2, Enumerate(uniqueData1).Count(FUN(p,  p.id   == 5)));

			ASSERT (EqualSets(uniqueData1, uniqueData2));

			// With custom allocator
			{
				NO_MORE_HEAP;

				std::aligned_storage_t<sizeof(Person), alignof(Person)>  buffer[30];
				TestAllocator<Person, 10> fixedAlloc { buffer };

				auto distincts5 = allMembers.ToHashSet(0, hashLambda, std::equal_to<>{}, fixedAlloc);

				ASSERT_TYPE (std::unordered_set<Person COMMA HasherLambda COMMA std::equal_to<> COMMA decltype(fixedAlloc)>, distincts5);

				ASSERT (EqualSets(distincts1, distincts5));
			}

		}

		// 2. Internal usage in filters
		// TODO
	}


	// For std::vector the only extra parameter is its allocator.
	static void CustomLists()
	{
		const Person personArray[] = {
			{ 1, "Aldo",	13 },
			{ 2, "Charlie", 42 },
			{ 3, "Dave",	14 },
			{ 4, "Eve",		28 },
			{ 5, "Felix",	19 },
		};


		// Deduced syntax, using TestAllocator (on stack)
		{
			NO_MORE_HEAP;

			auto adults = Enumerables::Filter(personArray, FUN(p, p.age >= 18));

			// vector's resize strategy: will realloc for 4 after capacity of 2 becomes insufficient
			//							 -> buffers for 2 + 4 at once
			std::aligned_storage_t<sizeof(Person), alignof(Person)>  buffer[8];
			TestAllocator<Person, 2> fixedAlloc { buffer };

			auto adultList = adults.ToList(0, fixedAlloc);

			ASSERT_TYPE (std::vector<Person COMMA decltype(fixedAlloc)>, adultList);
			ASSERT_EQ	(3, adultList.size());
		}

		// Type-only syntax, just using the default allocator
		{
			auto adults = Enumerables::Filter(personArray, FUN(p, p.age >= 18));

			using A = std::allocator<Person>;

			auto adultList = adults.ToList<A>();

			ASSERT_TYPE (std::vector<Person COMMA A>, adultList);
			ASSERT_EQ	(3, adultList.size());
		}
	}



	void TestCollectionCustomizability()
	{
		Greet("Custom collection parameters");

		CustomHashes();
		CustomLists();
	}

}	// namespace EnumerableTests
