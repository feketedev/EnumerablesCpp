
		/* 				 Welcome to Enumerables for C++!			   *
		 *  														   *
		 *  The annotated tests in Introduction*.cpp files present an  *
		 *  overview of the most important features of this library.   *
		 * 															   *
		 *  Part 1:	Fundamental concepts, differences to .Net,		   *
		 * 			basic creation and consumption of Enumerables.	   */


#include "Tests.hpp"
#include "TestUtils.hpp"
#include "Enumerables.hpp"



namespace EnumerableTests {


	// 1.1		Fundamental concepts - approaching C#'s Linq in C++
	static void Fundamentals()
	{
		// Rather then being a direct interface (like .Net's IEnumerable), our Enumerable is a type-erasure wrapper
		// for anything that is consumable by a range-based for loop - i.e. can be iterated over via begin()/end().
		// This is both for resource-management and to be able to extend existing container types.
		// [Type-erasure is optional, but used throughout this chapter.]
		// 
		// So, as a starter, any container can be wrapped into an Enumerable of its exact iterated type:

		std::vector<int> vec { 1, 2, 3, 3 };
		
		Enumerable<int&> numbers = vec;
		

		// Notice the &, as vector::iterator::operator* supplies references!
		// The simplest way to test access to elements is by .First() - which throws only for empty sequences.
		{
			int& n0 = numbers.First();

			ASSERT_EQ (vec[0],	n0);
			ASSERT_EQ (&vec[0],	&n0);
		}
		

		// By default, Enumerable is not a snapshot. As the assertion suggests, L-VALUE containers (or iterables)
		// are stored BY REFERENCE when wrapped, therefore items in the backing collection can be manipulated via
		// non-const & access. Also, changes to the container itself will be reflected in subsequent enumerations.
		{
			vec[0] = 5;
			ASSERT_EQ (5, numbers.First());

			++numbers.First();							// int &
			ASSERT_EQ (6, vec[0]);

			vec.insert(vec.begin(), 0);
			ASSERT_EQ (0, numbers.First());
			vec.erase(vec.begin());
			ASSERT_EQ (6, numbers.First());
		}


		// Of course, Enumerable itself is iterable too.
		{
			size_t i = 0;

			for (int& x : numbers) {
				int* target = &vec[i++];
				ASSERT_EQ (target, &x);
			}
			ASSERT_EQ (vec.size(), i);
		}


		// Simple but frequent algorithms that evaluate (or reduce) an Enumerable's contents are provided 
		// as "Terminal operations" (in contrast with chainables). Some of the most straightforward include:
		{
			vec[0] = 1;

			ASSERT_EQ (true,  numbers.Any());
			ASSERT_EQ (3,	  numbers.Last());			// iterates all over	=> inefficient! 
			ASSERT_EQ (4,	  numbers.Count());			// count all elements	-> queries size directly in exact cases,
														//  					   but iterates otherwise!
			ASSERT_EQ (2,	  numbers.Count(3));		// count equal elements
			ASSERT_EQ (1,	  numbers.Count(2));
			ASSERT_EQ (false, numbers.AllEqual(3));		// no any different item
			ASSERT_EQ (false, numbers.AllEqual());		// all items are the same (can be empty)
			ASSERT_EQ (1,	  numbers.Min());
			ASSERT_EQ (3,	  numbers.Max());
		}


		// Aside wrapping containers, the library provides some other simple source-creator functions.
		{
			int n = 5;

			Enumerable<int&> noInts  = Enumerables::Empty<int&>();
			Enumerable<int&> repeat3 = Enumerables::Repeat(n, 3);		// these also capture 
			Enumerable<int&> repeat1 = Enumerables::Once(n);			// l-value "n" BY REFERENCE!

			ASSERT    (!noInts.Any());
			ASSERT_EQ (3, repeat3.Count());
			ASSERT_EQ (1, repeat1.Count());
			ASSERT    (noInts.AllEqual());
			ASSERT    (repeat1.AllEqual(n));
			ASSERT    (repeat3.AllEqual(n));

			int s = 5;
			Enumerable<int>		 rangeUp   = Enumerables::RangeBetween(s, 10);
			Enumerable<unsigned> rangeDown = Enumerables::RangeDownBetween(2u, 0u);

			// Range* methods, intended for number-like elements, always capture BY VALUE.
			// Despite not being totally consistent with Once/Repeat, this seems to be the more natural behaviour.
			// [For a more customizable generator supporting reference seed, see Enumerables::Sequence.]
			s = 8;
			ASSERT (AreEqual({ 5, 6, 7, 8, 9, 10 },	rangeUp));
			ASSERT (AreEqual({ 2, 1, 0 },			rangeDown));

			{
				// Plain "Range" yields size_t (indices) - but can be influenced:
				Enumerable<size_t> r = Enumerables::Range(3);
				Enumerable<short> rs = Enumerables::Range<short>(3);
			
				ASSERT (AreEqual({ 0, 1, 2 },	r));
				ASSERT (AreEqual({ 0, 1, 2 },	rs));

				// A custom starting point can be specified too, whose type will be followed:
				Enumerable<unsigned> ru = Enumerables::Range(5u, 3);

				ASSERT (AreEqual({ 5, 6, 7 },	ru));
			}


			// Sometimes having the indices of a container is needed.
			std::vector<int> vec2 { 5, 6, 7 };

			// In these cases the real source is the container "vec2", which is l-value => stored BY REFERENCE
			Enumerable<size_t> indices = Enumerables::IndexRange(vec2);
			Enumerable<size_t> revIdcs = Enumerables::IndexRangeReversed(vec2);
			
			ASSERT (AreEqual({ 0, 1, 2 }, indices));
			ASSERT (AreEqual({ 2, 1, 0 }, revIdcs));
			
			vec2.resize(2);
			ASSERT (AreEqual({ 0, 1 }, indices));
			ASSERT (AreEqual({ 1, 0 }, revIdcs));

			// Value-capturing an index range is just as easy though...
			Enumerable<size_t> savedIndices = Enumerables::Range(vec2.size());
		}


		// Restricting access to const, or materializing the elements (copy) is always possible.
		// [However: no arbitrary/covariant conversions supported implicitly to preserve overloadability.]
		{
			Enumerable<const int&> cnums = numbers;
			Enumerable<int>		   vnums = numbers;			// still refers to "vec" underneeth!
		
			ASSERT_EQ (&vec[0],	&cnums.First());

			numbers.First() = 8;
			ASSERT_EQ (8,	cnums.First());
			ASSERT_EQ (8,	vnums.First());

			// Enumerable acts as a query/builder interface.
			// Neither "cnums" or "vnums" depend on "numbers", but on "vec" directly!
		}


		// Just like in Linq, transformations can be layered on top of sequences via builder-style methods.
		// These often take lambda functions, but some take simple numbers or other sequences (any iterable).
		// 
		// Lambda arguments are always stored BY VALUE - their own capture-block provides fine-grained control anyway -,
		// and so are simple number parameters - for which this feels natural, to avoid a myriad of unexpected mistakes.
		{
			Enumerable<int&> odds	 = numbers.Where([](int x) { return x % 2 > 0; });
			Enumerable<int>  oddSqrs = odds   .Map  ([](int x) { return x * x; });
		
			vec[0] = 5;
			ASSERT_EQ (&vec[0], &odds.First());

			ASSERT    (AreEqual({ 5, 3, 3 },  odds));		// free helper: elem-by-elem equivalence using op ==
			ASSERT    (AreEqual({ 25, 9, 9 }, oddSqrs));

			// Again, "oddSqrs" have no direct dependency to "odds", but to anything captured during it's built:
			// "vec" and references captured by Where's lambda (nothing in this example).
			
			// Of course direct chaining is more common - and also more efficient,
			// moving stored resources under the next layer, instead of copying them:

			Enumerable<int> top2OddSqrs = numbers.Where([](int x) { return x % 2 > 0; })
												 .Map  ([](int x) { return x * x; })
												 .Take (2);

			ASSERT (AreEqual({ 25, 9 }, top2OddSqrs));

			// Optionally, to get closer to C#, the above can be written in a more concise form, using the FUN macro:
			//    FUN(x, r) acts similarly to (x => r) in C#,
			//    FUN(x, y, r) acts like ((x, y) => r) etc.
			// These expand to the most generic form of lambda available in C++14, returning & whenever possible
			// [forwarding && parameters, decltype(auto) result].

			Enumerable<int> top2WithFun = numbers.Where(FUN(x,  x % 2 > 0))
												 .Map  (FUN(x,  x * x))
												 .Take (2);

			ASSERT (AreEqual(top2OddSqrs, top2WithFun));

			// While at first glance might seem horrific to long-time C++ devs, FUN can enhance readability
			// quite a bit when multiple parameters or reference return types are about to clutter the code.
			// Look out though: basic FUN uses [&] capture, but there's a FUNV variant too with [=] capture.
		}


		// The results of a query can be saved to a container [of preconfigured type] just like in .Net:
		{
			Enumerable<int&> approved = numbers.Where(FUN(x,  x > 2));
			
			std::vector<int>		list = approved.ToList();			// Decays element automatically
			std::unordered_set<int> set  = approved.ToHashSet();		//
			
			ASSERT	  (Enumerables::AreEqual({ 5, 3, 3 },	list));
			ASSERT_EQ ((std::unordered_set<int>{ 5, 3 }),	set);

			// The container bindings (Enumerables::ListType, Enumerables::SetType) can be set 
			// in your Enumerables.hpp, before the inclusion of library implementation.
		}


		// Managing lifetimes is an unavoidable addition in C++. Enumerables leaves this burden
		// at the programmer, but attempts to provide good defaults and concise syntax to help.
		// 
		// The fundamental example is when the result element of a transformation is tied to the input.
		{
			// Let's have 2 point-sources: an l-value and an r-value!

			std::unordered_set<Vector2D<double>>  pointSet { { 1.0, 0.0 },
															 { 2.0, 0.0 },
															 { 2.0, 1.0 } };

			Enumerable<const Vector2D<double>&> pointsL = pointSet;
			Enumerable<Vector2D<double>>		pointsR = Enumerables::Range(1.0, 3)
																	  .Map(FUN(y, Vector2D<double>(0.0, y)));

			// Notice the usage of ".Map" so far despite Linq has ".Select".
			// That's not an arbitrary rename. We have both!
			//	* .Map expresses the intent to map each element to something arbitrary
			//	  -> basically no restriction, usually a pr-value, but can be an Item& of some global container either
			//  * .Select expresses an SQL-like projection, zooming in to subobjects tied to their owner elements
			//	  -> Enumerables can decide implicitly wether & is viable or the subobject must be copied!
			//
			// [The intention is to keep & elements whenever possible, both for performance and allow access to originals.]

			Enumerable<const double&> heightsL = pointsL.Select(FUN(v, v.y));		// explicit typea are for clarity only
			Enumerable<double>        heightsR = pointsR.Select(FUN(v, v.y));		// - see next chapter

			const double& hL = heightsL.Last();
			double        hR = heightsR.Last();

			ASSERT_EQ (1.0, hL);
			ASSERT_EQ (3.0, hR);

			// A copy (or even a conversion) can be requested in-line explicitly:
			Enumerable<double> xsL = pointsL.Select<double>(FUN(v, v.x));

			// The contrary - force-selecting & from an r-value - generates a compile-time error
			Enumerable<const double&> xRefsL = pointsL.Select<const double&>(FUN(v, v.x));
		//	Enumerable<const double&> xRefsR = pointsR.Select<const double&>(FUN(v, v.x));		// CTE!

			// If however .Map is called, the programmer takes full responsibility for whatever the lambda returns.
			// FUN prefers & whenever possible [decltype(auto)], specificly to help dealing with subobjects -
			// that's why .Select can decide to "rematerialize" or not.
			auto danglingHeights = pointsR.Map   (FUN(v, v.y));
			auto copiedHeights   = pointsR.Select(FUN(v, v.y));

			ASSERT_TYPE (double &,	danglingHeights.First());	// valid for lambda, not during iteration
			ASSERT_TYPE (double,	copiedHeights.First());

			// An example for FUN being in control:
			// Subobjects live on, but a pr-value expression can never get & obviously.
			auto oneRight  = pointsL.Select(FUN(v, v.x + 1.0));
			
			ASSERT_TYPE (double,	oneRight.First());
		}
	}


	// 1.2		Opting out type-erasure (inspired by Dlang)
	//			  -> Stay in template-world while can to save resources
	//			  -> Shift to interfaces only when needed
	static void AutoTyping()
	{
		// Declaring Enumerable<T> explicitly is convenient and readable, but requires
		// type erasure, which can incur heap allocation and virtual calls to evaluate.
		// It also provides automatic wrapping conversions - as seen in chapter 1. -
		// which makes it convenient as method parameter or return type.
		// However,
		// the main entry-point to this library is really the Enumerate<T>(..) function,
		// which does the wrapping in many forms. Use it with auto! This way everything
		// is templated and can be inlined by the compiler up to the point when it gets
		// hidden behind an explicit Enumerable<V> interfaced wrapper, if needed.

		AllocationCounter allocations;		// track new's

		int arr[] = { 1, 2, 3, 3 };

		auto nums = Enumerate(arr);

		ASSERT_ELEM_TYPE (int&,	nums);
		ASSERT_EQ		 (4,	nums.Count());
		allocations.AssertFreshCount(0);

		// The element type can be deduced like above, but it can also be specified explicitly - even altered,
		// as far as conversions allow. Some might find it readable simply to denote types despite using auto.
		{
			auto nums2     = Enumerate<int&>(arr);
			auto constNums = Enumerate<const int&>(arr);
			auto numCopies = Enumerate<int>(arr);

			// "nums", "numCopies" and "constNums" all store the same: a reference to "arr".
			// Only difference is their yielded element type.
			ASSERT_ELEM_TYPE (int,			 numCopies);
			ASSERT_ELEM_TYPE (int&,			 nums2);
			ASSERT_ELEM_TYPE (const int&,	 constNums);
			ASSERT_EQ		 (&nums.First(), &constNums.First());

			// These tests use the above macro, but the enumerated type can be assessed as follows:
			static_assert (std::is_same<int&, decltype(nums.First())>(), "Type deduction error");
			static_assert (std::is_same<int&, decltype(nums)::TElem>(),  "Type deduction error");

			ASSERT_EQ (3, nums.Last());

			nums2.Last() = 6;

			ASSERT_EQ (6, nums.Last());
			ASSERT_EQ (6, numCopies.Last());
			ASSERT_EQ (6, nums.Last());

			// Note that a common pattern in this library is that the 1st, optional template argument
			// [defaulting to void = "deduce elem type implicitly"] is available to force an explicit
			// element type - on points where it can change. 
			// (In some methods it can also denote the type of a property selected for comparison.)
		}


		// Any implicit conversion of the enumerated type is possible at any later point:
		{
			auto constNums2 = nums.As<const int&>();
			auto constNums3 = nums.AsConst();			// for short
			
			auto doubles	= nums.As<double>();
			
			ASSERT (AreEqual(nums, constNums2));
			ASSERT (AreEqual(nums, constNums3));

			ASSERT_ELEM_TYPE (const int&,	constNums2);
			ASSERT_ELEM_TYPE (const int&,	constNums3);
			ASSERT_ELEM_TYPE (double,		doubles);
		}


		// As shown earlier, .Select can also add an extra conversion "for free".
		// For readability reasons .Map<R> is invalid, .MapTo<R> is available instead!
		{
			Vector2D<int> vecStore[] = { { 1, 0 }, { 2, 0 }, { 3, 0 } };

			auto vectors = Enumerate(vecStore);

			ASSERT_ELEM_TYPE (Vector2D<int>&,	vectors);

			auto xRefs   = vectors.Select		 (FUN(v, v.x));
			auto xRefs2  = vectors.Select<int&>	 (FUN(v, v.x));
			auto xCopies = vectors.Select<int>	 (FUN(v, v.x));
			auto xReals  = vectors.Select<double>(FUN(v, v.x));

			ASSERT_ELEM_TYPE (int&,		xRefs);
			ASSERT_ELEM_TYPE (int&,		xRefs2);
			ASSERT_ELEM_TYPE (int,		xCopies);
			ASSERT_ELEM_TYPE (double,	xReals);

			ASSERT (AreEqual({ 1, 2, 3 },		xRefs));
			ASSERT (AreEqual({ 1, 2, 3 },		xCopies));
			ASSERT (AreEqual({ 1.0, 2.0, 3.0 }, xReals));

			auto toManhattan = FUN(v,  v.x + v.y);
			auto dists		 = vectors.Map(toManhattan);
			auto dists2		 = vectors.MapTo<int>(toManhattan);
			auto distsReal	 = vectors.MapTo<double>(toManhattan);

			ASSERT_ELEM_TYPE (int,			dists);
			ASSERT_ELEM_TYPE (int,			dists2);
			ASSERT_ELEM_TYPE (double,		distsReal);

			ASSERT (AreEqual({ 1, 2, 3 },	dists));
			ASSERT (AreEqual({ 1, 2, 3 },	dists2));
			ASSERT (AreEqual({ 1, 2, 3 },	distsReal));
		}


		// We also have a few shorthands to facilitate C++ nuances
		{
			auto pointers = nums.Addresses();			// x => &x
			auto nums2	  = pointers.Dereference();		// x => *x
			auto copies1  = nums.Decay();				// decayed type
			auto copies2  = nums.Copy();				// same, but asserts that source was &

			ASSERT_ELEM_TYPE (int*,		pointers);
			ASSERT_ELEM_TYPE (int&,		nums2);
			ASSERT_ELEM_TYPE (int,		copies1);
			ASSERT_ELEM_TYPE (int,		copies2);
			
			ASSERT (AreEqual(nums, nums2));

			auto constPtrs = pointers.AsConst();		// injects under & or * (if wasn't &)
			
			ASSERT_ELEM_TYPE (const int*,	constPtrs);
		}

		
		// Everything so far were fully templated (and no complex algorithms were involved).
		// No allocation was required.
		allocations.AssertFreshCount(0);


		// When to switch to interfaced mode?
		// Aside public parameters and return types: a reason can be a conditional transformation.
		{
			// Let's have a simple multi-step query.
			auto oddSquared = nums.Where	 (FUN(x,  x % 2 > 0))
								  .MapTo<int>(FUN(x,  x * x));

			// Have some condition for refinement of the query
			bool wantResultsMod3 = std::rand() % 2 == 1;

			auto mod3Res = oddSquared.Where(FUN(s,  s % 3 == 0));

			// Still no allocations yet.
			allocations.AssertFreshCount(0);

			// Now we add the refinement based on the condition. With the 2 different queries being
			// two different types, we must hide their exact type to make them interchangable and
			// continue on a single execution path
			auto myResults = wantResultsMod3 ? mod3Res.ToInterfaced()
											 : oddSquared.ToInterfaced();

			ASSERT_TYPE (Enumerable<int>, myResults);

			// The ToInterfaced method is the same that's called during automatic conversion to interfaced wrapper.
			// The below code is equivalent, except that the unused option also gets wrapped (possibly allocating):
			Enumerable<int> mod3Option = mod3Res;
			Enumerable<int> baseOption = oddSquared;

			// [A query object is always const; avoid unnecessary copy]
			const Enumerable<int>& myResults2 = wantResultsMod3 ? mod3Option : baseOption;

			ASSERT (AreEqual(myResults, myResults2));

			if (wantResultsMod3) {
				ASSERT_EQ (9, myResults.Single());
			}
			else {
				ASSERT (AreEqual({ 1, 9 }, myResults));
			}
		}


		// Of course viable &-conversions are also enabled during conversion to interfaced:
		{
			Enumerable<int>			copies	  = nums;
			Enumerable<const int&>	constRefs = nums;

			ASSERT (AreEqual(nums, copies));
			ASSERT (AreEqual(nums, constRefs));
			ASSERT (AreEqual(nums.Addresses(), constRefs.Addresses()));
		}
	}


	// 1.3		Internals of interfaced wrappers (Enumerable<T>)
	static void InterfacedTyping()
	{
		using Enumerables::IEnumerator;
		using Enumerables::InterfacedEnumerator;

		// Enumerable<T> is a wrapper to support existing containers. Algorithms however, just like in Linq,
		// are simply implementations of the IEnumerator<T> interface, being created by some Enumerable<T>
		// - or, to be more precise, some AutoEnumerable<F> - any templated enumerable type:

		int arr[] = { 1, 2, 3 };

		auto nums1 = Enumerate<int&>(arr);
		auto nums2 = Enumerables::Range<int>(3);		// 0..2

		// Create Enumerators the same way as in .Net:
		{
			auto et1 = nums1.GetEnumerator();
			auto et2 = nums2.GetEnumerator();

			// Now we are in template-land, so their exact type is implementation-specified.
			// But they implement the familiar interface:

			IEnumerator<int&>& iface1 = et1;
			IEnumerator<int>&  iface2 = et2;

			while (iface1.FetchNext() && iface2.FetchNext()) {			// known same length
				int& n1 = iface1.Current();
				int	 n2 = iface2.Current();

				ASSERT_EQ (n1, n2 + 1);
			}

			// FetchNext should be tolerant to redundant calls once depleted
			ASSERT (!iface1.FetchNext());
			ASSERT (!iface2.FetchNext());

			// [Of course .FetchNext and .Current could be called on "et1" and "et2" directly.
			//  But now they are depleted, would need to be recreated.]
		}


		// Rather then being a wrapper for collections, Enumerable<T> (and AutoEnumerable<F> in general)
		// is a wrapper for IEnumerator factories, providing the query/builder interface to chain them.
		// "F" is that factory, which produces some IEnumerator<T> implementation.
		// Check out the definition: Enumerable<T> is the case when F = std::function<.>
		{
			Enumerable<int&> numsIfaced1 = nums1;
			Enumerable<int>  numsIfaced2 = nums2;

			// The produced type isn't a plain pointer though, but a dedicated Enumerator holder:
			InterfacedEnumerator<int&> et1 = numsIfaced1.GetEnumerator();
			InterfacedEnumerator<int>  et2 = numsIfaced2.GetEnumerator();

			// It holds a responding IEnumerator<T> implementation - possibly on heap -,
			// to which itself delegates operations...
			IEnumerator<int&>& iface1 = et1;
			IEnumerator<int>&  iface2 = et2;

			while (iface1.FetchNext()) {
				ASSERT	  (iface2.FetchNext());
				ASSERT_EQ (iface1.Current(), iface2.Current() + 1);
			}

			// InterfacedEnumerator can posess an inline buffer to avoid using the heap for holding
			// the implementor. Its size can be set through ENUMERABLES_INTERFACED_ETOR_INLINE_SIZE 
			// (refer to Enumerables.hpp and Enumerables_ConfigDefaults.hpp).
			// This contains the full paused algorithm state during iteration.

			// Enumerable<T> potentially also applies inline buffer optimization, but that currently
			// depends on the actual std::function implementation. It only holds the factory though,
			// which is usually smaller then the algorithm state.
		}


		// About allocation costs:
		{
			AllocationCounter allocations;

			auto evens = nums1.Where(FUN(x,  x % 2 == 0));

			ASSERT_EQ (1, evens.Count());
			allocations.AssertFreshCount(0);		// mandatory (templated)

			Enumerable<int&> evensIfaced = evens;

			allocations.AssertFreshCount(0);		// depending on std::function's inline optimization
													// needs: array ptr + lambda = 16 bytes 
													// passes with MS STL

			// Creation of InterfacedEnumerators use heap depending on concrete size:
			using ConcreteEnumerator = decltype(evens)::TEnumerator;
			
			// IteratorEnumerator: vptr + 2 iter + padded bool
			// FilterEnumerator:   vptr + lambda
			constexpr size_t size = 6 * sizeof(void*);
			static_assert (sizeof(ConcreteEnumerator) == size, "Enumerator unexpectedly large.");
			static_assert (ENUMERABLES_INTERFACED_ETOR_INLINE_SIZE >= size, "Wrong test setup.");
			
			// Simple transformations shouldn't need heap:
			ASSERT_EQ (1, evensIfaced.Count());
			allocations.AssertFreshCount(0);

			// More complex, or double wrapped interfaced queries might need heap allocation:
			Enumerable<int> doubleWrapped = evensIfaced.Map(FUN(x,  x * x));
			allocations.AssertFreshCount(1);

			ASSERT_EQ (4, doubleWrapped.Single());
			allocations.AssertFreshCount(1);
		}
	}


	// 1.4		Learn the various overloads of Enumerate - the primary way to start a query 
	static void EnumerateAlternatives()
	{
		// i) Single, templated parameter
		{
			// As shown already: l-value container -> wrap BY REFERENCE
			int arr[] = { 1, 2, 3 };

			auto numsRef = Enumerate(arr);

			// But there's support for r-values too -> wrap BY VALUE (move in)
			auto numsVal = Enumerate(std::vector<int> { 1, 2, 3 });

			// A value-wrapping Enumerable is:
			//	* safe to return from method scope
			
			//	* restricted to const& or pr-value access, due to an Enumerable must be const
			{
				ASSERT_ELEM_TYPE (int&,			numsRef);
				ASSERT_ELEM_TYPE (const int&,	numsVal);

				auto numsDecayedVal = Enumerate<int>(std::vector<int> { 1, 2, 3 });

				ASSERT_ELEM_TYPE (int,	numsDecayedVal);
			}

			//	* to copy, must store a copyable iterable
			//	* convertible to Enumerable only if stores a copiable iterable
			{
				auto numsValCopy = numsVal;

				ASSERT_ELEM_TYPE (const int&,	numsValCopy);

				ASSERT (AreEqual(numsVal, numsValCopy));
				ASSERT (&numsVal.First() != &numsValCopy.First());

				std::vector<MoveOnly<int>> movec;		// no initializer list...
				movec.push_back(1);
				movec.push_back(2);
				movec.push_back(3);

				auto moveOnly = Enumerate(std::move(movec)).Dereference();	// * -> const int&
				auto moved	  = std::move(moveOnly);
			 //	auto copy     = moved;										// CTE
			 //	Enumerable<const MoveOnly<int>&> moved = std::move(moved);	// CTE
				// ^ Due to std::function and Enumerable being copyable wrappers ^

				ASSERT_ELEM_TYPE (const int&, moved);
				ASSERT_EQ		 (1, moved.First());
				ASSERT_EQ		 (3, moved.Last());
				ASSERT_EQ		 (3, moved.Count());
			}
		}

		// ii) Braced initializer [as a compact notation to store a short Enumerables::ListType]
		{
			// Intended as a quick way to form a sequence of few simple elements - VALUE capture
			{
				auto uints = Enumerate({ 3u, 5u, 4u });
					
				ASSERT_ELEM_TYPE (unsigned, uints);				// yields pr-values
				ASSERT (AreEqual({ 3, 5, 4 }, uints));

				// BY VALUE storage
				auto getNums = []()
				{ 
					return Enumerate({ 3, 5, 4 }); 
				};

				Enumerable<int> returned = getNums();

				ASSERT (AreEqual({ 3, 5, 4 }, returned));
			}

			// In case of ambiguity, explicit type can guide the initializer list
			{
				auto uints2 = Enumerate<unsigned>({ 3, 5u, 4 });
				auto reals  = Enumerate<double>  ({ 3, 5u, 4 });

				ASSERT_ELEM_TYPE (unsigned, uints2);
				ASSERT_ELEM_TYPE (double,	reals);

				ASSERT (AreEqual({ 3, 5, 4 },		uints2));
				ASSERT (AreEqual({ 3.0, 5.0, 4.0 }, reals));

			}
				
			// Often sequencing a few l-value objects are needed.
			// Sequence of their references can be formed via "capture-syntax" - i.e. pointers :)
			{
				Vector2D<double> v1 { 5.0, 1.5 };
				Vector2D<double> v2 { 5.5, 0.0 };

				auto points = Enumerate({ &v2, &v1 });
				auto point1 = Enumerate({ &v1 });
					
				// It's a special rule only for initializer_lists, to convert pointers to references.
				// Of course the underlying ListType of pointers is stored BY VALUE.
				ASSERT_ELEM_TYPE (Vector2D<double>&, points);
				ASSERT_ELEM_TYPE (Vector2D<double>&, point1);

				ASSERT    (AreEqual({ &v2, &v1 },	points.Addresses()));
				ASSERT_EQ (&v1,						&point1.Single());
			}

			// It's possible also with "capture-syntax" to guide the initializer
			{
				struct Base {
					int data; 
					Base(int d) : data { d } {}
				};
				struct Derived : public Base {
					using Base::Base;
				};

				Base	b1 { 1 };
				Derived d1 { 2 };
				Derived d2 { 3 };

				auto asBases = Enumerate<Base&>({ &b1, &d1, &d2 });

				ASSERT_ELEM_TYPE (Base&, asBases);
				ASSERT_EQ (1, asBases.First().data);
				ASSERT_EQ (3, asBases.Last().data);
			}

			// To store pointers plainly (override "capture-syntax"), specify explicit pointer type
			{
				int a = 5;
				int b = 6;

				auto ptrs = Enumerate<int*>({ &a, &b });

				ASSERT_ELEM_TYPE (int*,	ptrs);
				ASSERT_EQ		 (&a,	ptrs.First());
				ASSERT_EQ		 (&b,	ptrs.Last());
			}

			// BUT: l-value initializer_lists aren't treated like direct braced initializer!
			//		Those are ordinary iterables - sometimes useful for template param pack expansion...
			{
				std::initializer_list<int> list { 1, 2, 3 };

				auto nums = Enumerate(list);
					
				ASSERT_ELEM_TYPE (const int&, nums);		// wrapped BY REFERENCE, not self-contained
			}
		}

		// iii) Itarator pair - stored directly
		{
			std::vector<int> vec { 1, 2, 3 };

			// The 2 parameter overload expects a (begin, end) pair - storing them BY VALUE.
			auto nums12  = Enumerate(&vec[0],	   &vec[2]);
			auto numsRev = Enumerate(vec.rbegin(), vec.rend());

			ASSERT_ELEM_TYPE (int&,	nums12);
			ASSERT_ELEM_TYPE (int&,	numsRev);

			ASSERT (AreEqual({ 1, 2 },    nums12));
			ASSERT (AreEqual({ 3, 2, 1 }, numsRev));

			// conversions are still supported
			auto copiesRev = Enumerate<int>(vec.rbegin(), vec.rend());
				
			ASSERT_ELEM_TYPE (int, copiesRev);

			// The caveat can be that iterators invalidate if the container is mutated.
		}
	}


	// 1.5		Explore alternative ways to express a lambda function.
	//			Will demonstrate on the demo class below:
	class Rectangle {
		unsigned w, h;
	public:
		Rectangle(unsigned a)			  : Rectangle { a, a } {}
		Rectangle(unsigned w, unsigned h) : w { w }, h { h }   {}

		unsigned& 		Width() 		{ return w; }
		unsigned& 		Height()		{ return h; }
		const unsigned& Width()  const 	{ return w; }
		const unsigned& Height() const 	{ return h; }

		unsigned		Area() const	{ return w * h; }
		bool		IsSquare() const	{ return w == h; }
	};
	

	static bool		IsSquare(Rectangle&)					{ return ASSERT(false); /* non-const: shouldn't get called*/ }
	static bool		IsSquare(const Rectangle& r)			{ return r.Width() == r.Height(); }
	static unsigned CalcCircumference(const Rectangle& r)	{ return 2 * (r.Width() + r.Height()); }
	 

	// Unfolding 1.5:
	static void LambdaUsage()
	{
		// The programmer can chose from 4 options to express a simple function that maps 1 input object:
		//	* classic C++ lambda (can wrap anything, but wordy and hard to read)
		//	* FUN / FUNV macros  (automatic, concise for simple cases, but not very customizable,
		//						  also "just macros" - don't tolerate {} initialization directly)
		//	* free-function pointer accepting taking TElem
		//	* pointer-to-member of TElem
		//		-> to subobject, or
		//		-> to parameterless mebmer function (~ a getter)

		std::vector<Rectangle>	 rectangleStore { { 2 }, { 5, 4 }, { 4 }, { 3, 5 } };

		auto rectangles = Enumerate(rectangleStore);

		// Filtering predicates are the simplest - they must return bool
		{
			// Showing all possible variations:
			auto squares1 = rectangles.Where([](const Rectangle& r) { return r.IsSquare(); });
			auto squares2 = rectangles.Where(FUN(r, r.IsSquare()));
			auto squares3 = rectangles.Where(&Rectangle::IsSquare);
			auto squares4 = rectangles.Where(&IsSquare);

			ASSERT_ELEM_TYPE (Rectangle&, squares1);
			ASSERT_ELEM_TYPE (Rectangle&, squares2);
			ASSERT_ELEM_TYPE (Rectangle&, squares3);
			ASSERT_ELEM_TYPE (Rectangle&, squares4);

			ASSERT_EQ (&rectangleStore[0], &squares1.First());
			ASSERT_EQ (&rectangleStore[2], &squares1.Last());
			ASSERT_EQ (2,				   squares1.Count());

			ASSERT (AreEqual(squares1.Addresses(), squares2.Addresses()));
			ASSERT (AreEqual(squares1.Addresses(), squares3.Addresses()));
			ASSERT (AreEqual(squares1.Addresses(), squares4.Addresses()));
		}

		// Simple mapping (no &) looks quite similar
		{
			auto areas1  = rectangles.Map([](const Rectangle& r) { return r.Area(); });
			auto areas2  = rectangles.Map(FUN(r, r.Area()));
			auto areas3  = rectangles.Map(&Rectangle::Area);
			auto circums = rectangles.Map(&CalcCircumference);		// for a change :)

			ASSERT_ELEM_TYPE (unsigned, areas1);
			ASSERT_ELEM_TYPE (unsigned, circums);

			ASSERT (AreEqual({ 4, 20, 16, 15 }, areas1));
			ASSERT (AreEqual({ 4, 20, 16, 15 }, areas2));
			ASSERT (AreEqual({ 4, 20, 16, 15 }, areas3));
			ASSERT (AreEqual({ 8, 18, 16, 16 }, circums));
		}


		// As it's shown, function-pointer syntax can give very readable solution for simple cases,
		// without resorting to macros. However: the situation is more intricate when the identifier
		// designates an overload-set!
		{
			// First we can see how much clutter the regular lambda syntax can cause
			// in contrast to rational automatisms:

			auto hs1 = rectangles.Select([](Rectangle& r) -> unsigned& { return r.Height(); });
			auto hs2 = rectangles.Select(FUN(r, r.Height()));

			// Resolving a function-pointer's overload-set is supported only if the result type is specified.
			// This case no arbitrary conversions supported, only decaying the result at most.

			auto hs3	 = rectangles.Select<unsigned&>(&Rectangle::Height);
			auto hCopies = rectangles.Select<unsigned>(&Rectangle::Height);

			// The library is prepared to resolve the usual overload-sets (const/mutable, &/&&/const&)
			// at the cost of storing 1 extra pointer. [Same applies to overloaded free-functions!]

			// Alternative is to resolve the pointer beforehand -  
			// probably being worst then lambdas readability-wise:

			unsigned& (Rectangle::* accessHeight)() = &Rectangle::Height;
			auto hs4    = rectangles.Select(accessHeight);

			// at least a resolved pointer behaves the same as a lambda - conversions supported
			auto hReals = rectangles.Select<double>(accessHeight);

			ASSERT_ELEM_TYPE (unsigned&, hs1);
			ASSERT_ELEM_TYPE (unsigned&, hs2);
			ASSERT_ELEM_TYPE (unsigned&, hs3);
			ASSERT_ELEM_TYPE (unsigned&, hs4);
			ASSERT_ELEM_TYPE (unsigned,  hCopies);
			ASSERT_ELEM_TYPE (double,    hReals);

			ASSERT_EQ (&rectangleStore[0].Height(), &hs1.First());
			ASSERT_EQ (&rectangleStore[3].Height(), &hs1.Last());

			ASSERT (AreEqual({ 2, 4, 4, 5 },   hs1));
			ASSERT (AreEqual(hs1.Addresses(),  hs2.Addresses()));
			ASSERT (AreEqual(hs1.Addresses(),  hs3.Addresses()));
			ASSERT (AreEqual(hs1.Addresses(),  hs4.Addresses()));
			ASSERT (AreEqual(hs1,			   hCopies));
			ASSERT (AreEqual(hs1.As<double>(), hReals));
		}


		// The overload-resolution capability (along with its cost + restrictions) is secondary
		// - can't be triggered with an already well-defined function-pointer.
		{
			auto roots1  = Enumerables::Range<double>(5).MapTo<double>(&std::sqrt);
		 //	auto roots1b = Enumerables::Range<double>(5).MapTo<float>(&std::sqrt);			// CTE: no such overload
			
			auto roots2   = Enumerables::Range<float>(5).Map(&std::sqrtf);
			auto roots2b  = Enumerables::Range<float>(5).MapTo<double>(&std::sqrtf);		// just a conversion

			auto roots3  = Enumerables::Range(5).Map(&std::sqrt<size_t>);		// exact template instance (prefer!)
			auto roots3b = Enumerables::Range(5).MapTo<double>(&std::sqrt);		// via OverloadResolver

			ASSERT_ELEM_TYPE (double, roots1);
			ASSERT_ELEM_TYPE (float,  roots2);
			ASSERT_ELEM_TYPE (double, roots2b);
			ASSERT_ELEM_TYPE (double, roots3);
			ASSERT_ELEM_TYPE (double, roots3b);

			// Also note that only certain transformation methods offer using
			// the OverloadResolver - for which it's a reasonably common need.
			
			// Predicates expect const member-functions to be exact,
			// and support a limited (but free) resolution for free-functions.
			// -> see IsSquare(Rectangle&)

			bool (*shouldntCall)(Rectangle&) = &IsSquare;
			UNUSED (shouldntCall);
		}


		// ---------------------------------------------------------------------------------------
		// Some transformations work with binary lambda functions.
		// These also support function-pointers - albiet without overload resolution capabilities.

		// Zip
		{
			Vector2D<double> arr1[] = { { 0.0, 0.0 }, { 1.0, 0.5 },  { 0.0, 1.5 } };
			Vector2D<double> arr2[] = { { 0.0, 1.0 }, { -1.0, 0.5 }, { 0.0, -0.5 } };

			auto points1 = Enumerate(arr1);
			auto points2 = Enumerate(arr2);

			auto sums1 = points1.Zip(points2, [](auto& p1, auto& p2) { return p1 + p2; });
			auto sums2 = points1.Zip(points2, FUN(p1, p2,  p1 + p2));
			auto sums3 = points1.Zip(points2, &Vector2D<double>::operator+);

			ASSERT_EQ (3, sums1.Count());
			ASSERT    (sums1.AllEqual({ 0.0, 1.0 }));
			ASSERT    (AreEqual(sums1, sums2));
			ASSERT    (AreEqual(sums1, sums3));
		}

		// MapNeighbors, Scan - let's get the direction of edges of a simple rectangle:
		{
			Vector2D<double> corners[] = { { 0.0, 0.0 }, 
										   { 2.0, 0.0 },
										   { 2.0, 3.5 },
										   { 0.0, 3.5 } };
			
			auto edgeDirs = Enumerate(corners).CloseWithFirst()
											  .MapNeighbors(&Vector2D<double>::DirectionTo);

			// the results of '+' will start from corners[1]
			auto cornersRotated = edgeDirs.Scan(corners[0], &Vector2D<double>::operator+);
		
			auto cornersFrom1 = Enumerate(corners).CloseWithFirst().Skip(1);

			ASSERT (AreEqual(cornersFrom1, cornersRotated));
			// or
			ASSERT (cornersRotated.Counted().All(FUN(x, x.value == corners[(x.index + 1) % 4])));
		}


		// ---------------------------------------------------------------------------------------
		// A note about the FUN/FUNV macros

		// returning lambdas
		{
			// suppose a getter returning a query to object state:
			auto getValidRectangles = [&rectangles](unsigned minWidth) -> Enumerable<const Rectangle&>
			{
				return rectangles.Where(FUNV(r,  r.Width() >= minWidth));
			};

			// Use FUNV (or [=] capture) for the scoped parameter -
			// it's probably the easiest error to miss value capture!

			ASSERT_EQ (4, getValidRectangles(1).Count());
			ASSERT_EQ (2, getValidRectangles(4).Count());
		}
	}





	void RunIntroduction1()
	{
		Greet("Introduction 1");

		Fundamentals();
		AutoTyping();
		InterfacedTyping();
		EnumerateAlternatives();
		LambdaUsage();
	}

}	// namespace EnumerableTests
