
#include "Tests.hpp"
#include "TestUtils.hpp"
#include "TestAllocator.hpp"
#include "Enumerables.hpp"
#include <string>



namespace EnumerableTests {

	namespace {

		// has no std::hash => Require custom template parameters!
		struct Person {
			unsigned		id;
			std::string		name;
			unsigned		age;

			unsigned			GetId()   const	   { return id; }
			std::string&		GetName()		&  { return name; }
			const std::string&	GetName() const	&  { return name; }
			std::string&&		GetName()		&& { return std::move(name); }

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


			Vector2D<int>  GetLocation() const
			{
				return Vector2D<int>(static_cast<int>(id),			// random mock idea
									 static_cast<int>(age / 10));
			}
		};


		// -- Related default-constructible function objects to be used as type-only arguments --

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

		
		// ----

		struct VecYHasher {
			size_t operator ()(const Vector2D<int>& v) const { return std::hash<int> {}(v.y); }
		};

		struct VecYComparer {
			bool operator ()(const Vector2D<int>& l, const Vector2D<int>& r) const { return l.y == r.y; }
		};
	}



	static void CustomHashes()
	{
		const Person groupA[] = {
			{ 1, "Aldo",	23 },
			{ 2, "Charlie", 42 },
			{ 5, "Dave",	44 },
			{ 1, "Aldo",	23 },	// intentional duplicate
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

		// Lambdas can be passed in easily, but designating the deduced SetType will require types
		using HasherLambda	   = decltype(hashLambda);
		using DataHasherLambda = decltype(dataHashLambda);
		using IdHasherLambda   = decltype(idHashLambda);
		using EqualIdsLambda  = decltype(idsEqualLambda);
		using EqualDataLambda = decltype(dataEqualLambda);


		// 1. Simple terminal operations
		{
			auto allMembers = Enumerables::Concat(groupA, groupB);

			// Using standard op ==
			auto distincts0 = allMembers.ToSet<Hasher>();
			{
				auto distincts2 = allMembers.ToSet(0, hashLambda);

				ASSERT_TYPE (std::unordered_set<Person COMMA Hasher>, distincts0);
				ASSERT_TYPE (std::unordered_set<Person COMMA HasherLambda>, distincts2);

				ASSERT_EQ (5u, distincts0.size());			// both groups having Aldo
				ASSERT    (EqualSets(distincts0, distincts2));
			}

			// Full comparison still, just with worse hash
			{
				auto distincts1 = allMembers.ToSet<IdHasher>();
				auto distincts2 = allMembers.ToSet(0, idHashLambda);

				ASSERT_TYPE (std::unordered_set<Person COMMA IdHasher>,		  distincts1);
				ASSERT_TYPE (std::unordered_set<Person COMMA IdHasherLambda>, distincts2);

				ASSERT (EqualSets(distincts0, distincts1));
				ASSERT (EqualSets(distincts0, distincts2));
			}

			// Resorting to "id" field
			{
				auto uniqueIdd1 = allMembers.ToSet<IdHasher, IdComparer>();
				auto uniqueIdd2 = allMembers.ToSet(0, idHashLambda, idsEqualLambda);

				ASSERT_TYPE (std::unordered_set<Person COMMA IdHasher COMMA IdComparer>,		   uniqueIdd1);
				ASSERT_TYPE (std::unordered_set<Person COMMA IdHasherLambda COMMA EqualIdsLambda>, uniqueIdd2);
			
				auto idsAsc = Enumerate(uniqueIdd1).Select(&Person::id).Order();
			
				ASSERT (AreEqual({ 1, 2, 3, 5 }, idsAsc));
				ASSERT (EqualSets(uniqueIdd1, uniqueIdd2));
			}

			// By content, ignoring "id"
			{
				auto uniqueData1 = allMembers.ToSet<DataHasher, DataComparer>();
				auto uniqueData2 = allMembers.ToSet(0, dataHashLambda, dataEqualLambda);

				ASSERT_TYPE (std::unordered_set<Person COMMA DataHasher COMMA DataComparer>,		  uniqueData1);
				ASSERT_TYPE (std::unordered_set<Person COMMA DataHasherLambda COMMA EqualDataLambda>, uniqueData2);

				ASSERT_EQ (4, uniqueData1.size());
				ASSERT_EQ (1, Enumerate(uniqueData1).Count(FUN(p,  p.name == "Charlie")));
				ASSERT_EQ (2, Enumerate(uniqueData1).Count(FUN(p,  p.id   == 5)));

				ASSERT (EqualSets(uniqueData1, uniqueData2));
			}

			// With custom allocator
			{
				NO_MORE_HEAP;

				std::aligned_storage_t<sizeof(Person), alignof(Person)>  buffer[30];
				TestAllocator<Person, 10> fixedAlloc { buffer };

				auto distincts2 = allMembers.ToSet(0, hashLambda, std::equal_to<>{}, fixedAlloc);

				ASSERT_TYPE (std::unordered_set<Person COMMA HasherLambda COMMA std::equal_to<> COMMA decltype(fixedAlloc)>, distincts2);

				ASSERT (EqualSets(distincts0, distincts2));
			}
		}

		// 2. Internal usage in filters
		{
			// Using standard op ==
			auto diff0 = Enumerate(groupA).Except<Hasher>(groupB);
			auto isec0 = Enumerate(groupA).Intersect<Hasher>(groupB);
			{
				auto diff2 = Enumerate(groupA).Except(groupB, hashLambda);
				auto isec2 = Enumerate(groupA).Intersect(groupB, hashLambda);

				ASSERT_ELEM_TYPE (const Person&, diff0);
				ASSERT_ELEM_TYPE (const Person&, diff2);
				ASSERT_ELEM_TYPE (const Person&, isec0);
				ASSERT_ELEM_TYPE (const Person&, isec2);

				ASSERT (AreEqual(diff0.Addresses(), diff2.Addresses()));
				ASSERT (AreEqual(isec0.Addresses(), isec2.Addresses()));

				ASSERT (AreEqual(Enumerate(groupA).Skip(1).Take(2),		diff0));	// 1st and Last is Aldo, being in both groups
				ASSERT (AreEqual(Enumerate({ &groupB[0], &groupB[0] }),	isec0));	// value-eq

				// duplicate items pass through filter in original order
				ASSERT (AreEqual({ &groupA[0], &groupA[3] }, isec0.Addresses()));
			}

			// Full comparison still, worse hash
			{
				auto diff1 = Enumerate(groupA).Except<IdHasher>(groupB);
				auto isec1 = Enumerate(groupA).Intersect<IdHasher>(groupB);
				auto diff2 = Enumerate(groupA).Except(groupB, idHashLambda);
				auto isec2 = Enumerate(groupA).Intersect(groupB, idHashLambda);

				ASSERT (AreEqual(diff0.Addresses(), diff1.Addresses()));
				ASSERT (AreEqual(diff0.Addresses(), diff2.Addresses()));
				ASSERT (AreEqual(isec0.Addresses(), isec1.Addresses()));
				ASSERT (AreEqual(isec0.Addresses(), isec2.Addresses()));
			}

			// Resorting to "id" field
			{
				auto idDiff1 = Enumerate(groupA).Except<IdHasher, IdComparer>(groupB);
				auto idIsec1 = Enumerate(groupA).Intersect<IdHasher, IdComparer>(groupB);
				auto idDiff2 = Enumerate(groupA).Except(groupB, idHashLambda, idsEqualLambda);
				auto idIsec2 = Enumerate(groupA).Intersect(groupB, idHashLambda, idsEqualLambda);

				ASSERT (AreEqual(idDiff1.Addresses(), idDiff2.Addresses()));
				ASSERT (AreEqual(idIsec1.Addresses(), idIsec2.Addresses()));
				
				// only Charlie has unused id
				ASSERT_EQ (&groupA[1], &idDiff1.Single());
				ASSERT    (AreEqual({ &groupA[0], &groupA[2], &groupA[3] }, idIsec1.Addresses()));
			}

			// By content, ignoring "id"
			{
				auto dataDiff1 = Enumerate(groupA).Except<DataHasher, DataComparer>(groupB);
				auto dataIsec1 = Enumerate(groupA).Intersect<DataHasher, DataComparer>(groupB);
				auto dataDiff2 = Enumerate(groupA).Except(groupB, dataHashLambda, dataEqualLambda);
				auto dataIsec2 = Enumerate(groupA).Intersect(groupB, dataHashLambda, dataEqualLambda);
				
				ASSERT (AreEqual(dataDiff1.Addresses(), dataDiff2.Addresses()));
				ASSERT (AreEqual(dataIsec1.Addresses(), dataIsec2.Addresses()));
				
				// only Dave has unique data
				ASSERT_EQ (&groupA[2], &dataDiff1.Single());
				ASSERT    (AreEqual({ &groupA[0], &groupA[1], &groupA[3] }, dataIsec1.Addresses()));
			}

			// With custom allocator
			{
				std::vector<const Person*> expectDiff = diff0.Addresses().ToList();
				std::vector<const Person*> expectIsec = isec0.Addresses().ToList();

				NO_MORE_HEAP;

				std::aligned_storage_t<sizeof(Person), alignof(Person)>  buffer[30];
				TestAllocator<Person, 10> fixedAlloc { buffer };

				auto diff2 = Enumerate(groupA).Except(groupB, hashLambda, std::equal_to<>{}, fixedAlloc);
				auto isec2 = Enumerate(groupA).Intersect(groupB, hashLambda, std::equal_to<>{}, fixedAlloc);

				ASSERT (AreEqual(expectDiff, diff2.Addresses()));
				ASSERT (AreEqual(expectIsec, isec2.Addresses()));
			}
		}
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


	// Similar to CustomHashes(), but a more complicated terminal operator
	static void CustomDictionaries()
	{
		const Person persons[] = {
			{ 1, "Aldo",	23 },
			{ 2, "Charlie", 42 },
			{ 3, "Dave",	44 },
			{ 1, "Aldo",	23 },	// duplicate
			{ 5, "Dorothy",	44 },
		};

		auto uniqueIndices = Enumerate({ 0u, 1u, 2u, 4u });
		
		// TODO: Investigate! Enumerate<unsigned>({ 0, 1, 2, 4 });	
		//		 choses "deduced" overload, only clang emits warning for conversion!

		// Basic usage
		std::unordered_map<unsigned, Person> byId;
		{
			auto byId0	   = Enumerate(persons).ToDictionary(&Person::id);
			auto namesById = Enumerate(persons).ToDictionary(&Person::id, &Person::name);
			auto ptrsById  = Enumerate(persons).Addresses().ToDictionary(&Person::id);
			
			ASSERT_TYPE (std::unordered_map<unsigned COMMA Person>,			byId0);
			ASSERT_TYPE (std::unordered_map<unsigned COMMA const Person*>,	ptrsById);
			ASSERT_TYPE (std::unordered_map<unsigned COMMA std::string>,	namesById);

			byId = std::move(byId0);

			ASSERT_EQ (4u, byId.size());		// Aldo was duplicated
			ASSERT_EQ (4u, namesById.size());
			ASSERT_EQ (4u, ptrsById.size());
		
			ASSERT_EQ (persons[0],  byId[1]);
			ASSERT_EQ (persons + 0, ptrsById[1]);
			ASSERT_EQ ("Aldo",		namesById[1]);
			ASSERT_EQ ("Dave",		namesById[3]);

			ASSERT (uniqueIndices.All(FUN(i,  persons[i] == byId[persons[i].id])));
		}

		// With custom hasher
		auto hashByY = [](const Vector2D<int>& v) { return std::hash<int>{}(v.y); };
		{
			auto byLocation1 = Enumerate(persons).ToDictionary(&Person::GetLocation, 0u, hashByY);
			auto byLocation2 = Enumerate(persons).ToDictionary<VecYHasher>(&Person::GetLocation);

			// explicit types syntax (enables overload-resolver - not needed here)
			auto byLocation3 = Enumerate(persons).ToDictionaryOf<Vector2D<int>, VecYHasher>(&Person::GetLocation);

			ASSERT_TYPE (std::unordered_map<Vector2D<int> COMMA Person COMMA decltype(hashByY)>, byLocation1);
			ASSERT_TYPE (std::unordered_map<Vector2D<int> COMMA Person COMMA VecYHasher>,		 byLocation2);
			ASSERT_TYPE (std::unordered_map<Vector2D<int> COMMA Person COMMA VecYHasher>,		 byLocation2);

			ASSERT_EQ (4u, byLocation1.size());
			ASSERT_EQ (4u, byLocation2.size());
			ASSERT_EQ (byLocation2, byLocation3);

			for (const auto& kv : byId) {
				Vector2D<int> coords = kv.second.GetLocation();
				ASSERT_EQ (kv.second, byLocation1[coords]);
				ASSERT_EQ (kv.second, byLocation2[coords]);
			}
		}

		// With custom (wrong) key-comparer
		{
			auto haveSameY = [](const Vector2D<int>& l, const Vector2D<int>& r) { return l.y == r.y; };

			auto byLocY1 = Enumerate(persons).ToDictionary(&Person::GetLocation, 0u, hashByY, haveSameY);
			auto byLocY2 = Enumerate(persons).ToDictionary<VecYHasher, VecYComparer>(&Person::GetLocation);
			// NOTE: bit weird syntax omitting Value type - due to it being fixed
			auto byLocY3 = Enumerate(persons).ToDictionaryOf<Vector2D<int>, VecYHasher, VecYComparer>(&Person::GetLocation);

			auto namesByLocY1 = Enumerate(persons).ToDictionaryOf<Vector2D<int>, std::string, VecYHasher, VecYComparer>(&Person::GetLocation, &Person::GetName);
			auto namesByLocY2 = Enumerate(persons).ToDictionaryOf<Vector2D<int>, std::string>(&Person::GetLocation, &Person::GetName, 0u, hashByY, haveSameY);

			ASSERT_TYPE (std::unordered_map<Vector2D<int> COMMA Person COMMA decltype(hashByY) COMMA decltype(haveSameY)>,	byLocY1);
			ASSERT_TYPE (std::unordered_map<Vector2D<int> COMMA Person COMMA VecYHasher COMMA VecYComparer>,				byLocY2);
			ASSERT_TYPE (std::unordered_map<Vector2D<int> COMMA Person COMMA VecYHasher COMMA VecYComparer>,				byLocY3);
			ASSERT_TYPE (std::unordered_map<Vector2D<int> COMMA std::string COMMA VecYHasher COMMA VecYComparer>,				namesByLocY1);
			ASSERT_TYPE (std::unordered_map<Vector2D<int> COMMA std::string COMMA decltype(hashByY) COMMA decltype(haveSameY)>,	namesByLocY2);

			ASSERT_EQ (2u, byLocY1.size());		// having 20- and 40-somethings only
			ASSERT_EQ (2u, byLocY2.size());
			ASSERT_EQ (2u, namesByLocY1.size());
			ASSERT_EQ (2u, namesByLocY2.size());
			ASSERT_EQ (byLocY2, byLocY3);

			ASSERT_EQ (persons[0], byLocY1[Vector2D<int>(1, 2)]);	// Aldo
			ASSERT_EQ (persons[1], byLocY1[Vector2D<int>(2, 4)]);	// Charlie

			for (const auto& kv : byLocY1) {
				const Vector2D<int>& coords	= kv.first;
				const Person&		 p		= kv.second;
				ASSERT_EQ (p,		byLocY2[coords]);
				ASSERT_EQ (p,		byLocY3[coords]);
				ASSERT_EQ (p.name,	namesByLocY1[coords]);
				ASSERT_EQ (p.name,	namesByLocY2[coords]);
			}
		}

		// With custom allocator
		{
			NO_MORE_HEAP;

			using KVPair = std::pair<unsigned, Person>;

			std::aligned_storage_t<sizeof(KVPair), alignof(KVPair)>  buffer[30];
			TestAllocator<KVPair, 10> fixedAlloc { buffer };

			auto byId1 = Enumerate(persons).ToDictionary(&Person::id, 0u, std::hash<int>{}, std::equal_to<>{}, fixedAlloc);

			ASSERT_TYPE (std::unordered_map<unsigned COMMA Person COMMA std::hash<int> COMMA std::equal_to<> COMMA decltype(fixedAlloc)>,  byId1);
			for (const auto& kv : byId)
				ASSERT_EQ (kv.second, byId1[kv.first]);
		}

		// Moving tests
		{
			auto copiesToMove = Enumerate(persons).As<MoveOnly<Person>>();

			ASSERT_ELEM_TYPE (MoveOnly<Person>, copiesToMove);		// uncopiable rvalues from here

			// only name is copyable - thus, values are moved:
			std::unordered_map<std::string, MoveOnly<Person>> byName = copiesToMove.ToDictionary(FUN(mp, mp.data.GetName()));

			ASSERT_EQ (4u, byName.size());

			// ...while the keys (names) were copied - i.e. FUN didn't receive an rvalue
			for (const auto& kv : byName) {
				const std::string& name = kv.first;
				ASSERT (Enumerate(persons).Select(&Person::name).Contains(name));

				const MoveOnly<Person>& p = kv.second;
				ASSERT_EQ (name, p.data.name);
			}

			// With overload-selection (have to avoid MoveOnly wrapper):
			auto copies = Enumerate(persons).Copy();

			std::unordered_map<std::string, Person> byName2 = copies.ToDictionaryOf<std::string>(&Person::GetName);
			for (const auto& kv : byName2) {
				const Person& p = byName.at(kv.first).data;
				ASSERT_EQ (p, kv.second);
			}

			// when names serve as values, they are moved
			// TODO tests: current proof is solely the mitigated "unused function" warning
			std::unordered_map<unsigned, std::string> namesById = copies.ToDictionaryOf<unsigned, std::string>(&Person::GetId, &Person::GetName);
			for (const auto& kv : namesById)
				ASSERT_EQ (byId[kv.first].name, kv.second);
		}

		// Obtaining cached items through interface
		{
			constexpr size_t N = 5;

			// let's have some heavy objects
			std::vector<int> vectors[N] = {
				{ 6, 3, 2 },
				{ 2, 1, 3 },
				{ 0, 2, 1 },
				{ 1, 2, 4 },
				{ 3, 2, 6 }
			};

			// OrderBy: won't make sense, but allocates deterministically ahead
			auto						 vectorsAsc = Enumerate(vectors).Copy().OrderBy<int>(&std::vector<int>::back);
			Enumerable<std::vector<int>> ifacedAsc  = vectorsAsc;

			static_assert (sizeof(decltype(vectorsAsc)::TEnumerator) <= ENUMERABLES_INTERFACED_ETOR_INLINE_SIZE, "Invalid test setup.");

			AllocationCounter allocations;
		
			size_t dictAssembly;
			{
				std::vector<std::vector<int>> ascList = ifacedAsc.ToList();	
				allocations.AssertFreshCount(N + 1);							// 1 cont.buffer + 4 vectors

				std::unordered_map<int, std::vector<int>> exRes;
				exRes.reserve(4);
				for (auto& vec : ascList)
					exRes.emplace(vec.front(), std::move(vec));

				dictAssembly = allocations.Count();
				allocations.Reset();
			}

#		if defined(_DEBUG) && !defined(__clang__)
			// Yet again: MSVC doesn't apply NRVO in debug + its move ctor does allocate!
			constexpr size_t dbgExtra = 4;
#		else
			constexpr size_t dbgExtra = 0;
#		endif

			std::unordered_map<int, std::vector<int>> fromTemplated = vectorsAsc.ToDictionaryOf<int>(&std::vector<int>::front);
			allocations.AssertFreshCount(N + 1 + dictAssembly + dbgExtra);

			std::unordered_map<int, std::vector<int>> fromInterfaced = ifacedAsc.ToDictionaryOf<int>(&std::vector<int>::front);
			allocations.AssertFreshCount(N + 1 + dictAssembly + dbgExtra);
			
			// would be N more if cache were not obtainable to move contents e.g:
			std::unordered_map<int, std::vector<int>> disrupted = vectorsAsc.Select(FUN(v, move(v)))
																			.ToDictionaryOf<int>(&std::vector<int>::front);
			allocations.AssertFreshCount(N + 1 + dictAssembly + N + dbgExtra);

			ASSERT_EQ (N,			  fromTemplated.size());
			ASSERT_EQ (vectors[0],	  fromTemplated[6]);
			ASSERT_EQ (vectors[2],	  fromTemplated[0]);
			ASSERT_EQ (vectors[4],	  fromTemplated[3]);
			ASSERT_EQ (fromTemplated, fromInterfaced);
			ASSERT_EQ (fromTemplated, disrupted);
		}
	}


	void TestCollectionCustomizability()
	{
		Greet("Custom collection parameters");

		CustomHashes();
		CustomLists();
		CustomDictionaries();
	}

}	// namespace EnumerableTests
