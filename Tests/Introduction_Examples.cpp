
		/* 				 Welcome to Enumerables for C++!			   *
		 *  														   *
		 *  The annotated tests in Introduction*.cpp files present an  *
		 *  overview of the most important features of this library.   *
		 * 															   *
		 *  Part 3:	Simple examples for possible real-world usage.	   */


#include "Tests.hpp"
#include "TestUtils.hpp"
#include "Enumerables.hpp"

#include <map>
#include <string>



namespace EnumerableTests {

	
	// 3.1. Provide queries using internal index structures.
	static void Example1()
	{
		struct Person {
			std::string name;
			unsigned	age;
		};


		class Registry {
			std::list<Person>							persons;
			std::map<unsigned, std::vector<Person*>>	byAge;
		
		public:
			// Simple const getter - hiding container implementation.
			// The main goal of implicit interfaced conversions is to have such succinct getters.
			Enumerable<const Person&>	Persons() const		{ return persons; }

			
			// With inline functions, staying templated is an option - although reduces readability.
			// Explicit element type might help a bit + guards against unintended interface changes!
			auto						Persons2() const	{ return Enumerate<const Person&>(persons); }


			// Queries can be formed using hidden internal index structures.
			// (This result stays valid only until a modification of the Registry - usually fine in
			//  practice, but note that this is in contrast to the simple getters above, which remain
			//  valid for the entire lifetime of Registry.)
			Enumerable<const Person&>	PersonsMinOld(unsigned minAge) const
			{
				return Enumerate(byAge.lower_bound(minAge), byAge.end())
					.Select		(FUN(p, p.second))
					.Flatten	()
					.Dereference();
			}

			Enumerable<const Person&>	PersonsInAge(unsigned age) const
			{
				auto it = byAge.find(age);
				if (it == byAge.end())
					return Enumerables::Empty<const Person&>();

				const std::vector<Person*>& bucket = it->second;
				return Enumerate(bucket).Dereference();
			}

			void Insert(Person&& p)
			{
				std::vector<Person*>& sameAge = byAge.insert({ p.age, {} }).first->second;	 // omg
				persons.push_back(std::move(p));
				sameAge.push_back(&persons.back());
			}
		};


		Registry reg;
		reg.Insert({ "Adam",	25 });
		reg.Insert({ "Annie",	25 });
		reg.Insert({ "Bob",		41 });
		reg.Insert({ "Charlie", 34 });

		ASSERT_EQ (4, reg.Persons().Count());
		ASSERT_EQ (2, reg.PersonsMinOld(34).Count());
		ASSERT_EQ (1, reg.PersonsInAge(34).Count());
		ASSERT_EQ (2, reg.PersonsInAge(25).Count());
		ASSERT_EQ (0, reg.PersonsInAge(26).Count());
		ASSERT    (AreEqual(reg.Persons().Select(&Person::name), reg.Persons2().Select(&Person::name)));
	}



	// 3.2.	Oftentimes, derived sequences enable having a more normalized data-structure.
	static void Example2()
	{
		struct Interval {
			const double start, end;

			bool IsDisjoint(const Interval& other) const
			{
				return end < other.start || other.end < start;
			}

			Interval(double s, double e) : start { s }, end { e }
			{
				ASSERT (s <= e);
			}
		};


		using Segment2D = std::pair<Vector2D<double>, Vector2D<double>>;


		class SegmentedLine {
			Vector2D<double>		basePoint;
			Vector2D<double>		direction;
			std::vector<Interval>	endParams;

		public:
			const Vector2D<double>&	BasePoint()		const	{ return basePoint; }
			const Vector2D<double>&	Direction()		const	{ return direction; }
			Enumerable<Interval>	SegmentParams() const	{ return endParams; }
			
			// safe to expose
			Vector2D<double>&		BasePoint()				{ return basePoint; }


			// Guard needed: direction can't be a null-vector
			void SetDirection(const Vector2D<double>& dir)
			{
				constexpr double minLenSqr = 10e-8;	// some app-specific constant
				ASSERT (minLenSqr <= dir.x * dir.x + dir.y * dir.y);
				direction = dir;
			}

			void AddSegment(Interval params)
			{
				// often simplifies expressing preconditions
				ASSERT (SegmentParams().All(FUN(iv, iv.IsDisjoint(params))));

				// order ignored in this example
				endParams.push_back(params);
			}

			// Merely derived data - follows basepoint/direction changes
			Enumerable<Segment2D>	Segments() const
			{
				auto to2D = [this](const Interval& params) -> Segment2D
				{
					return { basePoint + direction * params.start,
							 basePoint + direction * params.end   };
				};

				return Enumerate(endParams).Map(to2D);
			}

			// maybe a bit wild
			Enumerable<Vector2D<double>>	Vertices() const
			{
				return Segments()
					.Map	(FUN(s, Enumerate({ s.first, s.second })))
					.Flatten();
			}


			SegmentedLine(const Vector2D<double>& base, const Vector2D<double>& dir) : basePoint { base }
			{
				SetDirection(dir);
			}
		};


		SegmentedLine lin { { 0.0, 0.0 }, { 1.0, 0.0 } };

		ASSERT (!lin.Segments().Any());

		lin.AddSegment({ 0.5, 1.0 });
		lin.AddSegment({ 1.5, 2.0 });

		ASSERT_EQ (2, lin.Segments().Count());		// actually doesn't iterate, being a simple .Map
		ASSERT_EQ (4, lin.Vertices().Count());		// does iterate!

		ASSERT    (lin.Vertices().All(FUN(v, v.y == 0.0)));

		ASSERT_EQ (Vector2D<double>(0.5, 0.0), lin.Segments().First().first);

		// safe to shift
		lin.BasePoint().y = 1.0;

		ASSERT_EQ (Vector2D<double>(0.5, 1.0), lin.Segments().First().first);
		ASSERT    (lin.Vertices().All(FUN(v, v.y == 1.0)));

		// or rotete
		lin.SetDirection({ 0.0, 1.0 });

		ASSERT_EQ (Vector2D<double>(0.0, 1.5), lin.Segments().First().first);
		ASSERT    (lin.Vertices().All(FUN(v, v.x == 0.0)));

		// just to silence unused warnings:
		const SegmentedLine& clin = lin;
		ASSERT_EQ (0.0, clin.BasePoint().x);
		ASSERT_EQ (0.0, clin.Direction().x);
	}




	void RunIntroduction3()
	{
		Greet("Introduction 3");

		Example1();
		Example2();
	}

}	// namespace EnumerableTests
