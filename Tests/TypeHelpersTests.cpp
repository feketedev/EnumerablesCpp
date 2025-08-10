#include "Tests.hpp"
#include "TestUtils.hpp"
#include "TestAllocator.hpp"
#include "Enumerables_TypeHelpers.hpp"
#include "Enumerables_ConfigDefaults.hpp"	// access bindings instead of repeating them
#include <cmath>
#include <tuple>
#include <memory>



namespace EnumerableTests {

	using namespace Enumerables::TypeHelpers;


	namespace {

		struct GetterTester
		{
			char c			= 'u';
			char cLeft		= 'l';
			char cRight		= 'r';
			char cConst		= 'c';
			char cLeftConst = 'k';

			const char&	GetCharConst()	const	{ return cConst; }

			char&&		PassChar()				{ return std::move(c); }

			char&		GetChar2()				{ return c; }
			const char&	GetChar2()		const	{ return cConst; }

			char&		GetChar3()		&		{ return cLeft; }
			const char&	GetChar3()		const&	{ return cLeftConst; }
			char&&		GetChar3()		&&		{ return std::move(cRight); }


			char	GetCopy2()				{ return c; }
			char	GetCopy2()		const	{ return cConst; }

			char	GetCopy3()		&		{ return cLeft; }
			char	GetCopy3()		const&	{ return cLeftConst; }
			char	GetCopy3()		&&		{ return cRight; }

			// mutable & can be missing from ref-qualified triple - and even makes sense
			char	GetCopy32()		const&	{ return cLeftConst; }
			char	GetCopy32()		&&		{ return cRight; }
		};



		static const char&	FreeExtractor(const GetterTester& obj)	{ return obj.cConst; }
		static char&		FreeExtractor(GetterTester& obj)		{ return obj.cLeft; }
		static char&&		FreeExtractor(GetterTester&& obj)		{ return std::move(obj.cRight); }

		static char			FreeExtractorValParam(GetterTester obj)	{ return obj.c; }


		static char			GlobChar = 'g';

		static char&		MapFunction1(const GetterTester&)		{ return GlobChar; }
		static char&&		MapFunction1(GetterTester&& obj)		{ return std::move(obj.cRight); }

		static char			MapFunction2(const GetterTester&)		{ return GlobChar; }
		static char			MapFunction2(GetterTester&& obj)		{ return std::move(obj.cRight); }

	}	// namespace



	static void TestOverloadResolver()
	{
		static_assert (sizeof(OverloadResolver<GetterTester, int>) == sizeof(void*) * 2, "Surprise");

		{
			GetterTester obj;

			// --- Non-overloaded ---
			{
				OverloadResolver<const GetterTester&,	const char&>	c1 { &GetterTester::GetCharConst };
				OverloadResolver<GetterTester&,			const char&>	c2 { &GetterTester::GetCharConst };
				OverloadResolver<GetterTester,			const char&>	c3 { &GetterTester::GetCharConst };
				OverloadResolver<GetterTester&&,		const char&>	c4 { &GetterTester::GetCharConst };
				OverloadResolver<const GetterTester,	const char&>	c5 { &GetterTester::GetCharConst };

			//	OverloadResolver<GetterTester, char&>			c6 { &GetterTester::GetCharConst };		// CTE!

				OverloadResolver<GetterTester&,  char&&>		m1 { &GetterTester::PassChar };
				OverloadResolver<GetterTester,   char&&>		m2 { &GetterTester::PassChar };
				OverloadResolver<GetterTester&&, char&&>		m3 { &GetterTester::PassChar };
			//	OverloadResolver<const GetterTester, char&&>	m4 { &GetterTester::PassChar };			// CTE
			//	OverloadResolver<const GetterTester&, char&&>	m5 { &GetterTester::PassChar };			// CTE

				ASSERT(&obj.cConst == &c1(obj));
				ASSERT(&obj.cConst == &c2(obj));
				ASSERT(&obj.cConst == &c3(std::move(obj)));
				ASSERT(&obj.cConst == &c4(std::move(obj)));
				ASSERT(&obj.cConst == &c5(std::move(obj)));
				char&& cPassed1 = m1(obj);
				char&& cPassed2 = m2(std::move(obj));
				char&& cPassed3 = m3(std::move(obj));
				ASSERT(&obj.c == &cPassed1);
				ASSERT(&obj.c == &cPassed2);
				ASSERT(&obj.c == &cPassed3);
			}

			// --- Triple-qualified overload ---
			{
				OverloadResolver<const GetterTester&,	const char&>	cl1 { &GetterTester::GetChar3 };
				OverloadResolver<GetterTester&,			const char&>	cl2 { &GetterTester::GetChar3 };  // - const overload because of return type!
				OverloadResolver<GetterTester,			const char&>	cl3 { &GetterTester::GetChar3 };	// Legit calls according to language
				OverloadResolver<GetterTester&&,		const char&>	cl4 { &GetterTester::GetChar3 };	// despite lifetime issues!
				OverloadResolver<const GetterTester,	const char&>	cl5 { &GetterTester::GetChar3 };	// (Errors in .Select to prevent dangling!)

				OverloadResolver<GetterTester,		  char&&>		r1 { &GetterTester::GetChar3 };
				OverloadResolver<GetterTester&&,	  char&&>		r2 { &GetterTester::GetChar3 };

				OverloadResolver<GetterTester&,		  char&>		l1 { &GetterTester::GetChar3 };
			//	OverloadResolver<GetterTester,		  char&>		l2 { &GetterTester::GetChar3 };			// CTE! (& call on rvalue)
			//	OverloadResolver<const GetterTester&, char&>		l3 { &GetterTester::GetChar3 };			// CTE! (constness)

				ASSERT(&obj.cLeftConst == &cl1(obj));
				ASSERT(&obj.cLeftConst == &cl2(obj));
				ASSERT(&obj.cLeftConst == &cl3(std::move(obj)));
				ASSERT(&obj.cLeftConst == &cl4(std::move(obj)));
				ASSERT(&obj.cLeftConst == &cl5(std::move(obj)));
				char&& rr1 = r1(std::move(obj));
				char&& rr2 = r2(std::move(obj));
				ASSERT(&obj.cRight == &rr1);
				ASSERT(&obj.cRight == &rr2);
				ASSERT(&obj.cLeft  == &l1(obj));
			}

			// --- Const/Mutable overload ---
			{
				OverloadResolver<const GetterTester&,	const char&>	c1 { &GetterTester::GetChar2 };
				OverloadResolver<GetterTester&,			const char&>	c2 { &GetterTester::GetChar2 };
				OverloadResolver<GetterTester,			const char&>	c3 { &GetterTester::GetChar2 };		//
				OverloadResolver<GetterTester&&,		const char&>	c4 { &GetterTester::GetChar2 };		// OK here, error in .Select to prevent dangling!
				OverloadResolver<const GetterTester,	const char&>	c5 { &GetterTester::GetChar2 };		//

				OverloadResolver<GetterTester&,	 char&>	m1 { &GetterTester::GetChar2 };
				OverloadResolver<GetterTester,	 char&>	m2 { &GetterTester::GetChar2 };				// OK here, error in .Select to prevent dangling!
				OverloadResolver<GetterTester&&, char&>	m3 { &GetterTester::GetChar2 };				//

				ASSERT(&obj.cConst == &c1(obj));
				ASSERT(&obj.cConst == &c2(obj));
				ASSERT(&obj.cConst == &c3(std::move(obj)));
				ASSERT(&obj.cConst == &c4(std::move(obj)));
				ASSERT(&obj.cConst == &c5(std::move(obj)));
				ASSERT(&obj.c == &m1(obj));
				ASSERT(&obj.c == &m2(std::move(obj)));
				ASSERT(&obj.c == &m3(std::move(obj)));
			}


			// ====== Converting to prvalue ======
			// --- Non-overloaded ---
			{
				OverloadResolver<const GetterTester&,	char>	c1 { &GetterTester::GetCharConst };
			//	OverloadResolver<GetterTester&,			char>	c2 { &GetterTester::GetCharConst };		// Limitation:	N/A to prevent ambiguity
			//	OverloadResolver<GetterTester,			char>	c3 { &GetterTester::GetCharConst };		// But if not an overload set,
			//	OverloadResolver<GetterTester&&,		char>	c4 { &GetterTester::GetCharConst };		// general .Select handles it fine!
				OverloadResolver<const GetterTester,	char>	c5 { &GetterTester::GetCharConst };

				ASSERT(obj.cConst == c1(obj));
				ASSERT(obj.cConst == c5(std::move(obj)));
			}

			// --- Triple-qualified overload ---
			{
				OverloadResolver<const GetterTester,	char>	cl1 { &GetterTester::GetChar3 };	// const & -> copy
				OverloadResolver<const GetterTester&,	char>	cl2 { &GetterTester::GetChar3 };
				OverloadResolver<GetterTester&,			char>	l   { &GetterTester::GetChar3 };	// & -> copy

				OverloadResolver<GetterTester,	 char>			r1 { &GetterTester::GetChar3 };		// && -> materialize prval
				OverloadResolver<GetterTester&&, char>			r2 { &GetterTester::GetChar3 };

				ASSERT(obj.cLeftConst == cl1(std::move(obj)));
				ASSERT(obj.cLeftConst == cl2(obj));
				ASSERT(obj.cLeft  == l(obj));
				ASSERT(obj.cRight == r1(std::move(obj)));
				ASSERT(obj.cRight == r2(std::move(obj)));
			}

			// --- Const-Mutable overload ---
			{
				OverloadResolver<const GetterTester&,	char>	c1 { &GetterTester::GetChar2 };
				OverloadResolver<GetterTester&,			char>	m1 { &GetterTester::GetChar2 };
				OverloadResolver<GetterTester,			char>	m2 { &GetterTester::GetChar2 };
				OverloadResolver<GetterTester&&,		char>	m3 { &GetterTester::GetChar2 };
				OverloadResolver<const GetterTester,	char>	c2 { &GetterTester::GetChar2 };

				ASSERT(obj.cConst == c1(obj));
				ASSERT(obj.c == m1(obj));
				ASSERT(obj.c == m2(std::move(obj)));
				ASSERT(obj.c == m3(std::move(obj)));
				ASSERT(obj.cConst == c2(std::move(obj)));
			}


			// ====== Methods returning prvalue ======
			// --- Triple-qualified overload ---
			{
				OverloadResolver<const GetterTester,	char>	cl1 { &GetterTester::GetCopy3 };
				OverloadResolver<const GetterTester&,	char>	cl2 { &GetterTester::GetCopy3 };
				OverloadResolver<GetterTester&,			char>	l   { &GetterTester::GetCopy3 };

				OverloadResolver<GetterTester,	 char>			r1 { &GetterTester::GetCopy3 };
				OverloadResolver<GetterTester&&, char>			r2 { &GetterTester::GetCopy3 };

				ASSERT(obj.cLeftConst == cl1(std::move(obj)));
				ASSERT(obj.cLeftConst == cl2(obj));
				ASSERT(obj.cLeft  == l(obj));
				ASSERT(obj.cRight == r1(std::move(obj)));
				ASSERT(obj.cRight == r2(std::move(obj)));
			}

			// --- Incomplete ref-qualified set ---
			{
				OverloadResolver<const GetterTester,	char>	cl1 { &GetterTester::GetCopy32 };
				OverloadResolver<const GetterTester&,	char>	cl2 { &GetterTester::GetCopy32 };
				OverloadResolver<GetterTester&,			char>	cl3 { &GetterTester::GetCopy32 };

				OverloadResolver<GetterTester,	 char>			r1 { &GetterTester::GetCopy32 };
				OverloadResolver<GetterTester&&, char>			r2 { &GetterTester::GetCopy32 };

				ASSERT(obj.cLeftConst == cl1(std::move(obj)));
				ASSERT(obj.cLeftConst == cl2(obj));
				ASSERT(obj.cLeftConst == cl3(obj));
				ASSERT(obj.cRight == r1(std::move(obj)));
				ASSERT(obj.cRight == r2(std::move(obj)));
			}

			// --- Const-Mutable overload ---
			{
				OverloadResolver<const GetterTester&,	char>	c1 { &GetterTester::GetCopy2 };
				OverloadResolver<GetterTester&,			char>	m1 { &GetterTester::GetCopy2 };
				OverloadResolver<GetterTester,			char>	m2 { &GetterTester::GetCopy2 };
				OverloadResolver<GetterTester&&,		char>	m3 { &GetterTester::GetCopy2 };
				OverloadResolver<const GetterTester,	char>	c2 { &GetterTester::GetCopy2 };

				ASSERT(obj.cConst == c1(obj));
				ASSERT(obj.c == m1(obj));
				ASSERT(obj.c == m2(std::move(obj)));
				ASSERT(obj.c == m3(std::move(obj)));
				ASSERT(obj.cConst == c2(std::move(obj)));
			}


			// ====== Free functions ======
			// --- Non-overloaded - prval param ---
			{
				OverloadResolver<const GetterTester&,	char>	c1 { &FreeExtractorValParam };
				OverloadResolver<GetterTester&,			char>	c2 { &FreeExtractorValParam };
				OverloadResolver<GetterTester,			char>	c3 { &FreeExtractorValParam };
				OverloadResolver<GetterTester&&,		char>	c4 { &FreeExtractorValParam };
				OverloadResolver<const GetterTester,	char>	c5 { &FreeExtractorValParam };

				ASSERT(obj.c == c1(obj));
				ASSERT(obj.c == c2(obj));
				ASSERT(obj.c == c3(std::move(obj)));
				ASSERT(obj.c == c4(std::move(obj)));
				ASSERT(obj.c == c5(std::move(obj)));
			}

			// --- Triple-qualified overload ---
			{
				OverloadResolver<const GetterTester&,	const char&>	c1 { &FreeExtractor };
				OverloadResolver<GetterTester&,			const char&>	c2 { &FreeExtractor };	// WARNING: const overload selected by return type!
				OverloadResolver<GetterTester,			const char&>	c3 { &FreeExtractor };	// <
				OverloadResolver<GetterTester&&,		const char&>	c4 { &FreeExtractor };	// < Gets dangling if part of object
				OverloadResolver<const GetterTester,	const char&>	c5 { &FreeExtractor };  // < (.Select forbids this, can be ok with .Map)

				OverloadResolver<GetterTester,			char&&>		r1 { &FreeExtractor };
				OverloadResolver<GetterTester&&,		char&&>		r2 { &FreeExtractor };
			//	OverloadResolver<GetterTester&,			char&&>		r3 { &FreeExtractor };		// CTE

				OverloadResolver<GetterTester&,		char&>		l1 { &FreeExtractor };
			//	OverloadResolver<GetterTester,		char&>		l2 { &FreeExtractor };			// CTE
			//	OverloadResolver<GetterTester&&,	char&>		l3 { &FreeExtractor };			// CTE

				ASSERT(&obj.cConst == &c1(obj));
				ASSERT(&obj.cConst == &c2(obj));
				ASSERT(&obj.cConst == &c4(std::move(obj)));		// moved but not dangling yet (no sense IRL)
				UNUSED(c3);									// would get dangling in this test
				UNUSED(c5);                                 //
				char&& rr1 = r1(std::move(obj));
				char&& rr2 = r2(std::move(obj));
				ASSERT(&obj.cRight == &rr1);
				ASSERT(&obj.cRight == &rr2);
				ASSERT(&obj.cLeft  == &l1(obj));
			}

			// --- Const/Rval -> ref overloads (uncommon) ---
			{
				OverloadResolver<const GetterTester&,	char&>	c1 { &MapFunction1 };
				OverloadResolver<GetterTester&,			char&>	c2 { &MapFunction1 };
				OverloadResolver<GetterTester,			char&>	c3 { &MapFunction1 };	// WARNING: const overload selected by return type
				OverloadResolver<GetterTester&&,		char&>	c4 { &MapFunction1 };   //			in case of MapFunction1!
				OverloadResolver<const GetterTester,	char&>	c5 { &MapFunction1 };   //

				ASSERT(&GlobChar == &c1(obj));
				ASSERT(&GlobChar == &c2(obj));
				ASSERT(&GlobChar == &c3(std::move(obj)));
				ASSERT(&GlobChar == &c4(std::move(obj)));
				ASSERT(&GlobChar == &c5(std::move(obj)));

				OverloadResolver<GetterTester,			char&&>	r1 { &MapFunction1 };
				OverloadResolver<GetterTester&&,		char&&>	r2 { &MapFunction1 };
			//	OverloadResolver<GetterTester&,			char&&>	r3 { &MapFunction1 };	// CTE
			//	OverloadResolver<const GetterTester,	char&&>	r4 { &MapFunction1 };   // CTE

				ASSERT(obj.cRight == r1(std::move(obj)));
				ASSERT(obj.cRight == r2(std::move(obj)));
			}

			// --- Const/Rval -> prval overloads ---
			{
				OverloadResolver<const GetterTester&,	char>	c1 { &MapFunction2 };
				OverloadResolver<GetterTester&,			char>	c2 { &MapFunction2 };
				OverloadResolver<GetterTester,			char>	c3 { &MapFunction2 };
				OverloadResolver<GetterTester&&,		char>	c4 { &MapFunction2 };
				OverloadResolver<const GetterTester,	char>	c5 { &MapFunction2 };

				ASSERT(GlobChar == c1(obj));
				ASSERT(GlobChar == c2(obj));
				ASSERT(obj.cRight == c3(std::move(obj)));
				ASSERT(obj.cRight == c4(std::move(obj)));
				ASSERT(GlobChar == c5(std::move(obj)));

				OverloadResolver<GetterTester,			char>	r1 { &MapFunction2 };
				OverloadResolver<GetterTester&&,		char>	r2 { &MapFunction2 };
			 //	OverloadResolver<GetterTester&,			char&&>	r3 { &MapFunction1 };	// CTE
			 //	OverloadResolver<const GetterTester,	char&&>	r4 { &MapFunction1 };   // CTE

				ASSERT(obj.cRight == r1(std::move(obj)));
				ASSERT(obj.cRight == r2(std::move(obj)));
			}

			// Pointer syntax + Scalars vs. MemberPointer enable_if
			{
				OverloadResolver<const GetterTester*,	char>	c1 { &GetterTester::GetChar2 };
				OverloadResolver<GetterTester*,			char>	m1 { &GetterTester::GetChar2 };
			 // OverloadResolver<GetterTester*,			char>	c2 { &MapFunction2 };			// CTE, no dereference for free functions

				ASSERT(obj.cConst == c1(&obj));
				ASSERT(obj.c == m1(&obj));

				OverloadResolver<int, double>			i1 { &std::sqrt<int> };
				OverloadResolver<const int&, double>	i2 { &std::sqrt<int> };
				OverloadResolver<int&, double>			i3 { &std::sqrt<int> };
				OverloadResolver<int&&, double>			i4 { &std::sqrt<int> };

				int			n = 4;
				const int  cn = 16;
				ASSERT(2.0 == i1(4));
				ASSERT(2.0 == i2(4));
				ASSERT(2.0 == i2(n));
				ASSERT(4.0 == i2(cn));
				ASSERT(2.0 == i3(n));
				ASSERT(2.0 == i4(4));
				ASSERT(2.0 == i4(std::move(n)));

				OverloadResolver<double, double>			d1 { &std::sqrt };
				OverloadResolver<const double&, double>		d2 { &std::sqrt };
				OverloadResolver<double&, double>			d3 { &std::sqrt };
				OverloadResolver<double&&, double>			d4 { &std::sqrt };

				double		 d  = 4.0;
				const double cd = 16.0;
				ASSERT(2.0 == d1(4.0));
				ASSERT(2.0 == d2(4.0));
				ASSERT(2.0 == d2(d));
				ASSERT(4.0 == d2(cd));
				ASSERT(2.0 == d3(d));
				ASSERT(2.0 == d4(4.0));
				ASSERT(2.0 == d4(std::move(d)));
			}
		}
	}



	static void TestGenStorage()
	{
		// StorableT / RefHolder
		{
			std::vector<int>			nums { 1, 2, 3 };
			std::vector<RefHolder<int>> refs;

			refs.push_back(nums.front());
			refs.push_back(nums[1]);
			refs.push_back(nums[2]);

			static_assert(is_same<int&,			decltype(Revive(refs.front()))>(), "type check");
			static_assert(is_same<const int&,	decltype(ReviveConst(refs[1]))>(), "type check");
			static_assert(is_same<int&,			decltype(PassRevived(refs[2]))>(), "type check");	// refs are lvalues

			int&		i0 = Revive(refs.front());
			const int&	i1 = ReviveConst(refs[1]);
			int&		i2 = PassRevived(refs[2]);

			ASSERT_EQ (1, i0);
			ASSERT_EQ (2, i1);
			ASSERT_EQ (3, i2);
			++nums[0];
			++nums[1];
			++nums[2];
			ASSERT_EQ (2, i0);
			ASSERT_EQ (3, i1);
			ASSERT_EQ (4, i2);

			static_assert(is_same<int&,			decltype(Revive(nums.front()))>(), "type check");
			static_assert(is_same<const int&,	decltype(ReviveConst(nums[1]))>(), "type check");
			static_assert(is_same<int&&,		decltype(PassRevived(nums[2]))>(), "type check");
		}


		// In old MS STL, pair has no comparison operators for compatible (different) types,
		// e.g. std::pair<int&, char&>{x, y} == std::pair<int, char>{x, y}
		//
		// The same works with tuples (see below endif)!
		// Still kept the original pair version, showing that these tests depend on STL capability.
#if !defined(_MSC_VER) || (_MSC_VER > 1900)

		// RefHolder transparent equality
		{
			std::pair<int, char> obj1 { 1, 'a' };		// type with templated op==
			std::pair<int, char> cpy1 { 1, 'a' };		// => conversion operator won't help!
			std::pair<int, char> obj2 { 2, 'a' };

			RefHolder<std::pair<int, char>> ref1 = obj1;

			ASSERT (ref1 == cpy1);
			ASSERT (ref1 != obj2);
			ASSERT (cpy1 == ref1);
			ASSERT (obj2 != ref1);

			ASSERT (ref1.operator==({ 1, 'a'}));		// edge-case of default type argument


			RefHolder<std::pair<int, char>> ref2   = obj2;
			RefHolder<std::pair<int, char>> refcpy = ref1;

			ASSERT (ref1 != ref2);
			ASSERT (ref1 == refcpy);

			// holding different, but equatable type
			std::pair<int&, char&> refPair { obj1.first, obj1.second };

			ASSERT (refPair == obj1);
			ASSERT (obj1 == refPair);

			RefHolder<std::pair<int&, char&>> refPairRef = refPair;

			ASSERT (refPairRef == refPair);
			ASSERT (refPairRef == obj1);
			ASSERT (refPair	== refPairRef);
			ASSERT (obj1	== refPairRef);

			int*			ptr	   = &obj1.first;
			RefHolder<int*> ptrRef = ptr;

			ASSERT (ptrRef != nullptr);
			ASSERT (nullptr != ptrRef);
		}


		// RefHolder transparent compare (for tree-sets)
		{
			std::pair<int, char> obj1 { 1, 'a' };		// type with templated op<
			std::pair<int, char> cpy1 { 1, 'a' };		// => conversion operator won't help!
			std::pair<int, char> obj2 { 2, 'a' };

			RefHolder<std::pair<int, char>> ref1 = obj1;

			ASSERT_EQ (false, obj1 < cpy1);
			ASSERT_EQ (true,  obj1 < obj2);

			ASSERT_EQ (false, ref1 < cpy1);
			ASSERT_EQ (false, cpy1 < ref1);
			ASSERT_EQ (true,  ref1 < obj2);
			ASSERT_EQ (false, obj2 < ref1);

			ASSERT (ref1.operator<({1, 'b'}));			// edge-case of default type argument


			RefHolder<std::pair<int, char>> ref2   = obj2;
			RefHolder<std::pair<int, char>> refcpy = ref1;

			ASSERT_EQ (true,  ref1 < ref2);
			ASSERT_EQ (false, ref1 < refcpy);

			// holding different, but comparable type
			std::pair<int&, char&> refPair { obj1.first, obj1.second };

			ASSERT_EQ (true,  refPair < obj2);
			ASSERT_EQ (false, refPair < obj1);
			ASSERT_EQ (false, obj1 < refPair);

			RefHolder<std::pair<int&, char&>> refPairRef = refPair;

			ASSERT_EQ (false, refPairRef < refPair);
			ASSERT_EQ (true,  refPairRef < obj2);
			ASSERT_EQ (false, refPair	 < refPairRef);
			ASSERT_EQ (false, obj2		 < refPairRef);
		}

#endif


		// RefHolder transparent equality
		{
			std::tuple<int, char> obj1 { 1, 'a' };		// type with templated op==
			std::tuple<int, char> cpy1 { 1, 'a' };		// => conversion operator won't help!
			std::tuple<int, char> obj2 { 2, 'a' };

			RefHolder<std::tuple<int, char>> ref1 = obj1;

			ASSERT (ref1 == cpy1);
			ASSERT (ref1 != obj2);
			ASSERT (cpy1 == ref1);
			ASSERT (obj2 != ref1);

			ASSERT (ref1.operator==({ 1, 'a'}));		// edge-case of default type argument


			RefHolder<std::tuple<int, char>> ref2   = obj2;
			RefHolder<std::tuple<int, char>> refcpy = ref1;

			ASSERT (ref1 != ref2);
			ASSERT (ref1 == refcpy);

			// holding different, but equatable type
			std::tuple<int&, char&> refPair { std::get<0>(obj1), std::get<1>(obj1) };

			ASSERT (refPair == obj1);
			ASSERT (obj1 == refPair);

			RefHolder<std::tuple<int&, char&>> refPairRef = refPair;

			ASSERT (refPairRef == refPair);
			ASSERT (refPairRef == obj1);
			ASSERT (refPair	== refPairRef);
			ASSERT (obj1	== refPairRef);

			int*			ptr	   = &std::get<0>(obj1);
			RefHolder<int*> ptrRef = ptr;

			ASSERT (ptrRef != nullptr);
			ASSERT (nullptr != ptrRef);
		}

		// RefHolder transparent compare (for tree-sets)
		{
			std::tuple<int, char> obj1 { 1, 'a' };		// type with templated op<
			std::tuple<int, char> cpy1 { 1, 'a' };		// => conversion operator won't help!
			std::tuple<int, char> obj2 { 2, 'a' };

			RefHolder<std::tuple<int, char>> ref1 = obj1;

			ASSERT_EQ (false, obj1 < cpy1);
			ASSERT_EQ (true,  obj1 < obj2);

			ASSERT_EQ (false, ref1 < cpy1);
			ASSERT_EQ (false, cpy1 < ref1);
			ASSERT_EQ (true,  ref1 < obj2);
			ASSERT_EQ (false, obj2 < ref1);

			ASSERT (ref1.operator<({1, 'b'}));			// edge-case of default type argument


			RefHolder<std::tuple<int, char>> ref2   = obj2;
			RefHolder<std::tuple<int, char>> refcpy = ref1;

			ASSERT_EQ (true,  ref1 < ref2);
			ASSERT_EQ (false, ref1 < refcpy);

			// holding different, but comparable type
			std::tuple<int&, char&> refPair = { std::get<0>(obj1), std::get<1>(obj1) };

			ASSERT_EQ (true,  refPair < obj2);
			ASSERT_EQ (false, refPair < obj1);
			ASSERT_EQ (false, obj1 < refPair);

			RefHolder<std::tuple<int&, char&>> refPairRef = refPair;

			ASSERT_EQ (false, refPairRef < refPair);
			ASSERT_EQ (true,  refPairRef < obj2);
			ASSERT_EQ (false, refPair	 < refPairRef);
			ASSERT_EQ (false, obj2		 < refPairRef);
		}


		struct alignas(64) ConstStruct {
			const int				id;
			std::unique_ptr<double> payload;

			ConstStruct(int id, double pl) : id { id }, payload { std::make_unique<double>(pl) }
			{
			}
		};

		auto getStruct = []() { return ConstStruct { 4, 4.4 }; };


		// Reassignable
		{
			Reassignable<ConstStruct> s1 { ForwardParams, 1, 1.1 };
			ASSERT_EQ (1,	s1->id);
			ASSERT_EQ (1.1,	*s1->payload);

			static_assert(is_same<ConstStruct&,  decltype(*s1)>(),			  "type check");
			static_assert(is_same<ConstStruct&&, decltype(*std::move(s1))>(), "type check");

			Reassignable<ConstStruct> s2 = std::move(s1);
			ASSERT_EQ (1,	s2->id);
			ASSERT_EQ (1.1,	*s2->payload);

			s2.Reconstruct(2, 2.2);
			s1.Reconstruct(3, 3.3);
			ASSERT_EQ (3,	s1->id);
			ASSERT_EQ (3.3,	*s1->payload);
			ASSERT_EQ (2,	s2->id);
			ASSERT_EQ (2.2,	*s2->payload);

			ConstStruct so = s2.PassValue();
			ASSERT_EQ (2,	so.id);
			ASSERT_EQ (2.2,	*so.payload);

			Reassignable<ConstStruct> s4 { InvokeFactory, getStruct };
			s2.AcceptRvo(getStruct);
			ASSERT_EQ (4,	s4->id);
			ASSERT_EQ (4.4,	*s4->payload);
			ASSERT_EQ (4,	s2->id);
			ASSERT_EQ (4.4,	*s2->payload);

			Reassignable<ConstStruct&> r1 { ForwardParams, *s1 };
			ASSERT_EQ (3.3, *r1->payload);
		 // r1.Reconstruct(5, 5.5);			// CTE
			r1 = so;
			ASSERT_EQ (2.2, *r1->payload);

			// But if assignment is available, it should be used!
			std::vector<int> nums1 { 1, 2, 3, 4, 5 };
			std::vector<int> nums2 { 1, 2, 3, 4 };
			std::vector<int> nums3 { 1, 2, 3, 4, 5, 6 };

			Reassignable<std::vector<int>> v1 { ForwardParams, nums1 };

			NO_MORE_HEAP;

			Reassignable<std::vector<int>> v2 { ForwardParams, std::move(nums1) };
			ASSERT_EQ (5, v2->size());
			ASSERT_EQ (5, v1->size());

			v2 = nums2;
			v1 = v2;
			ASSERT_EQ (4, v2->size());	// size reduced -> no realloc, just copy
			ASSERT_EQ (4, v1->size());

			v2 = std::move(nums3);		// size increased, but move -> no realloc
			ASSERT_EQ (6, v2->size());

			v1 = std::move(v2);
			ASSERT_EQ (6, v1->size());

			static_assert (!std::is_default_constructible<Reassignable<int>>(), "Should be forbidden.");
		}

		// Deferred
		{
			Deferred<ConstStruct> s;
			ASSERT (!s.IsInitialized());

			static_assert(is_same<ConstStruct&,  decltype(*s)>(),			 "type check");
			static_assert(is_same<ConstStruct&&, decltype(*std::move(s))>(), "type check");

			s = ConstStruct { 1, 1.1 };
			ASSERT (s.IsInitialized());
			ASSERT_EQ (1,	s->id);
			ASSERT_EQ (1.1,	*s->payload);

			s.AcceptRvo(getStruct);
			ASSERT_EQ (4,	s->id);
			ASSERT_EQ (4.4,	*s->payload);

			ConstStruct so = s.PassValue();
			ASSERT_EQ (4,	so.id);
			ASSERT_EQ (4.4,	*so.payload);
			ASSERT (s.IsInitialized());		// only moved!

			// Same checks for assignment as for Reassignable
			std::vector<int> nums1 { 1, 2, 3, 4, 5 };
			std::vector<int> nums2 { 1, 2, 3, 4 };
			std::vector<int> nums3 { 1, 2, 3, 4, 5, 6 };

			Deferred<std::vector<int>> v1;
			v1 = nums1;

			NO_MORE_HEAP;

			Deferred<std::vector<int>> v2;
			v2 = std::move(nums1);

			ASSERT_EQ (5, v2->size());
			ASSERT_EQ (5, v1->size());

			v2 = nums2;
			v1 = v2;
			ASSERT_EQ (4, v2->size());	// size reduced -> no realloc, just copy
			ASSERT_EQ (4, v1->size());

			v2 = std::move(nums3);		// size increased, but move -> no realloc
			ASSERT_EQ (6, v2->size());

			v1 = std::move(v2);
			ASSERT_EQ (6, v1->size());
		}
	}



	namespace TestConstPropagation		// static test
	{
		// & -> decayed is allowed
		static_assert (is_same< int,					CompatResultT<int,					int 	  	>>(), "Err");
		static_assert (is_same< int,					CompatResultT<const int,			int 	  	>>(), "Err");
		static_assert (is_same< int,					CompatResultT<int &,				int 	  	>>(), "Err");
		static_assert (is_same< int,					CompatResultT<int,					int const 	>>(), "Err");	// top cv dropped (will copy object)
		static_assert (is_same< int,					CompatResultT<int const & ,			int 	  	>>(), "Err");

		static_assert (is_same< int		  &,			CompatResultT<int 		& ,			int 	  &	>>(), "Err");
		static_assert (is_same< int		  *,			CompatResultT<int 		* ,			int 	  *	>>(), "Err");
		static_assert (is_same< int	const &,			CompatResultT<int const & ,			int 	  &	>>(), "Err");
		static_assert (is_same< int	const *,			CompatResultT<int const * ,			int 	  *	>>(), "Err");
		static_assert (is_same< int	const &,			CompatResultT<int const & ,			int const &	>>(), "Err");
		static_assert (is_same< int	const *,			CompatResultT<int const * ,			int const *	>>(), "Err");
		static_assert (is_same< int	const &,			CompatResultT<int 		& ,			int const &	>>(), "Err");
		static_assert (is_same< int	const *,			CompatResultT<int 		* ,			int const *	>>(), "Err");

		static_assert (is_same< int		  *,			CompatResultT<int 		* &,		int 	  *	>>(), "Err");
		static_assert (is_same< int	const *,			CompatResultT<int const * &,		int 	  *	>>(), "Err");
		static_assert (is_same< int	const *,			CompatResultT<int const * &,		int const *	>>(), "Err");
		static_assert (is_same< int	const *,			CompatResultT<int 		* &,		int const *	>>(), "Err");

		static_assert (is_same< int		  &&,			CompatResultT<int 		&&,			int 	  &&	>>(), "Err");
		static_assert (is_same< int	const &&,			CompatResultT<int const &&,			int 	  &&	>>(), "Err");
		static_assert (is_same< int	const &&,			CompatResultT<int const &&,			int const &&	>>(), "Err");
		static_assert (is_same< int	const &&,			CompatResultT<int 		&&,			int const &&	>>(), "Err");
		static_assert (is_same< int		  *,			CompatResultT<int 		* &&,		int 	  *		>>(), "Err");
		static_assert (is_same< int	const *,			CompatResultT<int const * &&,		int 	  *		>>(), "Err");
		static_assert (is_same< int	const *,			CompatResultT<int const * &&,		int const *		>>(), "Err");
		static_assert (is_same< int	const *,			CompatResultT<int 		* &&,		int const *		>>(), "Err");
		static_assert (is_same< int		  *,			CompatResultT<int 		*,			int 	  * &&	>>(), "Err");
		static_assert (is_same< int	const *,			CompatResultT<int const *,			int 	  * &&	>>(), "Err");
		static_assert (is_same< int	const *,			CompatResultT<int const *,			int const * &&	>>(), "Err");
		static_assert (is_same< int	const *,			CompatResultT<int 		*,			int const * &&	>>(), "Err");
		static_assert (is_same< int		  * &&,			CompatResultT<int 		* &&,		int 	  * &&	>>(), "Err");
		static_assert (is_same< int	const * &&,			CompatResultT<int const * &&,		int const * &&	>>(), "Err");

#if defined(_MSC_VER) && !defined(__clang__)
		static_assert (is_same< int		  *&,			CompatResultT<int 		* &,		int 	  * & >>(), "Err");
		static_assert (is_same< int	const *&,			CompatResultT<int const * &,		int 	  * & >>(), "Err");
		static_assert (is_same< int	const *&,			CompatResultT<int const * &,		int const * & >>(), "Err");
		static_assert (is_same< int	const *&,			CompatResultT<int 		* &,		int const * & >>(), "Err");

		static_assert (is_same< int		  * *,			CompatResultT<int 		* *,		int 	  * * >>(), "Err");
		static_assert (is_same< int	const * *,			CompatResultT<int const * *,		int 	  * * >>(), "Err");
		static_assert (is_same< int	const * *,			CompatResultT<int const * *,		int const * * >>(), "Err");
		static_assert (is_same< int	const * *,			CompatResultT<int 		* *,		int const * * >>(), "Err");

		static_assert (is_same< int	const * &&,			CompatResultT<int const * &&,		int 	  * &&	>>(), "Err");
		static_assert (is_same< int	const * &&,			CompatResultT<int 		* &&,		int const * &&	>>(), "Err");

#else	// strange but standard behaviour of std::common_type:
		// (Probably to prevent pointing an int* to a const int indirectly)
		static_assert (is_same< int		  *&,			CompatResultT<int 		* &,		int 	  * & >>(), "Err");
		static_assert (is_same< int	const * const &,	CompatResultT<int const * &,		int 	  * & >>(), "Err");
		static_assert (is_same< int	const *&,			CompatResultT<int const * &,		int const * & >>(), "Err");
		static_assert (is_same< int	const * const &,	CompatResultT<int 		* &,		int const * & >>(), "Err");

		static_assert (is_same< int		  * *,			CompatResultT<int 		* *,		int 	  * * >>(), "Err");
		static_assert (is_same< int	const * const *,	CompatResultT<int const * *,		int 	  * * >>(), "Err");
		static_assert (is_same< int	const * *,			CompatResultT<int const * *,		int const * * >>(), "Err");
		static_assert (is_same< int	const * const *,	CompatResultT<int 		* *,		int const * * >>(), "Err");

		static_assert (is_same< int	const * const &&,	CompatResultT<int const * &&,		int 	  * &&	>>(), "Err");
		static_assert (is_same< int	const * const &&,	CompatResultT<int 		* &&,		int const * &&	>>(), "Err");
#endif

 		static_assert (is_same< int		  * const &,	CompatResultT<int 		*  const &,	int		  * &		>>(), "Err");
		static_assert (is_same< int	const * const &,	CompatResultT<int const *  const &,	int		  * &		>>(), "Err");
		static_assert (is_same< int	const * const &,	CompatResultT<int const *  const &,	int const * &		>>(), "Err");
		static_assert (is_same< int	const * const &,	CompatResultT<int 		*  const &,	int const * &		>>(), "Err");
		static_assert (is_same< int		  * const &,	CompatResultT<int 		* &,		int		  * const &	>>(), "Err");
		static_assert (is_same< int	const * const &,	CompatResultT<int const * &,		int		  * const &	>>(), "Err");
		static_assert (is_same< int	const * const &,	CompatResultT<int const * &,		int const * const &	>>(), "Err");
		static_assert (is_same< int	const * const &,	CompatResultT<int 		* &,		int const * const &	>>(), "Err");

		static_assert (is_same< int	const * const *,	CompatResultT<int const * *,		int		  * const *	>>(), "Err");


		static_assert (is_same< int,					CompatResultT<int,					double			>>(),	"Err");
		static_assert (is_same< int,					CompatResultT<int,					double		 &	>>(),	"Err");
		static_assert (is_same< int,					CompatResultT<int,					double const &	>>(),	"Err");
		static_assert (is_same< int,					CompatResultT<int		&,			double			>>(),	"Err");
		static_assert (is_same< int,					CompatResultT<int const &,			double			>>(),	"Err");
		static_assert (is_same< double,					CompatResultT<double,				int				>>(),	"Err");
		static_assert (is_same< double,					CompatResultT<double,				int		  &		>>(),	"Err");
		static_assert (is_same< double,					CompatResultT<double,				int	const &		>>(),	"Err");
		static_assert (is_same< double,					CompatResultT<double 	   &,		int				>>(),	"Err");
		static_assert (is_same< double,					CompatResultT<double const &,		int				>>(),	"Err");

		// incompatibles
		static_assert (is_same< void,					CompatResultT<int,		double *	>>(),	"Err");
		static_assert (is_same< void,					CompatResultT<double *,	int			>>(),	"Err");
		static_assert (is_same< void,					CompatResultT<int,		int *		>>(),	"Err");
		static_assert (is_same< void,					CompatResultT<int *,	int			>>(),	"Err");
		static_assert (is_same< void,					CompatResultT<int &,	int *		>>(),	"Err");
		static_assert (is_same< void,					CompatResultT<int *,	int	&		>>(),	"Err");

		// passing free-functions should work
		static_assert (is_same<  float (*)(float),		CompatResultT<float (*)(float),	decltype(&std::fabsf)	>>(),	"Err");
		static_assert (is_same<  float (&)(float),		CompatResultT<float (&)(float),	decltype(std::fabsf) &	>>(),	"Err");

		// pointers to arrays
		static_assert (is_same< int	(*)[3],				CompatResultT<int (*)[3] ,			int (*)[3]	>>(), "Err");
		static_assert (is_same< int	(&)[3],				CompatResultT<int (&)[3] ,			int (&)[3]	>>(), "Err");
		static_assert (is_same< const int (*)[3],		CompatResultT<int (*)[3] ,	  const int (*)[3]	>>(), "Err");
		static_assert (is_same< const int (&)[3],		CompatResultT<int (&)[3] ,	  const int (&)[3]	>>(), "Err");
		static_assert (is_same< void,					CompatResultT<int (*)[3] ,			int (*)[4]	>>(), "Err");
		static_assert (is_same< void,					CompatResultT<int (&)[3] ,			int (&)[4]	>>(), "Err");

		// plain arrays are not valid return types anyway
		static_assert (is_same< void,					CompatResultT<int [3] ,			int [3]	>>(), "Err");
		static_assert (is_same< void,					CompatResultT<int [] ,			int []	>>(), "Err");

		// inheritance must work
		struct Base {
			int data;
		};
		struct Derived1 : public Base {};
		struct Derived2 : public Base {};

		static_assert (is_same< Base 	   &,			CompatResultT<Derived1 		 &,	Base 	   &	>>(), "Err");
		static_assert (is_same< Base const &,			CompatResultT<Derived1 const &,	Base 	   &	>>(), "Err");
		static_assert (is_same< Base const &,			CompatResultT<Derived1 const &,	Base const &	>>(), "Err");
		static_assert (is_same< Base const &,			CompatResultT<Derived1 		 &,	Base const &	>>(), "Err");

		static_assert (is_same< Base 	   *,			CompatResultT<Derived1 		 *,	Base *			>>(), "Err");
		static_assert (is_same< Base const *,			CompatResultT<Derived1 const *,	Base *			>>(), "Err");
		static_assert (is_same< Base const *,			CompatResultT<Derived1 const *,	Base const * &	>>(), "Err");	// + decay
		static_assert (is_same< Base const *,			CompatResultT<Derived1 		 *,	Base const * &	>>(), "Err");	//

		// incompatible
		static_assert (is_same< void,					CompatResultT<Derived1 	* &,	Base	* &		>>(), "Err");

		// member pointers should pass too
		static_assert (is_same< int	Base::*,			CompatResultT<int	Base::*,	decltype(&Derived1::data) >>(), "Err");
	}

	
	
	namespace TestDeepConst {		// static test
		
		// Result is "Deep-only const", top-level is left intact, easy to qualify manually
		static_assert (is_same< int,				DeepConstT<int>				  >(),	"Err");
		static_assert (is_same< int	const,			DeepConstT<int const>		  >(),	"Err");
		static_assert (is_same< int	const &,		DeepConstT<int 		 &>		  >(),	"Err");
		static_assert (is_same< int	const &,		DeepConstT<int const &>		  >(),	"Err");
		static_assert (is_same< int	const *,		DeepConstT<int 		 *>		  >(),	"Err");
		static_assert (is_same< int	const *,		DeepConstT<int const *>		  >(),	"Err");
		static_assert (is_same< int	const * const,	DeepConstT<int const * const> >(),	"Err");

		static_assert (is_same< int volatile const *,					DeepConstT<int volatile 	  *>				>(),	"Err");
		static_assert (is_same< int volatile const *,					DeepConstT<int volatile const *>				>(),	"Err");
		static_assert (is_same< int volatile const * const,				DeepConstT<int volatile const * const>			>(),	"Err");
		static_assert (is_same< int volatile const * volatile,			DeepConstT<int volatile 	  * volatile>		>(),	"Err");
		static_assert (is_same< int volatile const * volatile,			DeepConstT<int volatile const * volatile>		>(),	"Err");
		static_assert (is_same< int volatile const * volatile const,	DeepConstT<int volatile const * volatile const>	>(),	"Err");

		static_assert (is_same< int volatile const * const &,			DeepConstT<int volatile 	  * &>				>(),	"Err");
		static_assert (is_same< int volatile const * const &,			DeepConstT<int volatile const * &>				>(),	"Err");
		static_assert (is_same< int volatile const * const &,			DeepConstT<int volatile const * const &>		>(),	"Err");
		static_assert (is_same< int volatile const * volatile const &,	DeepConstT<int volatile 	  * volatile &>		>(),	"Err");
		static_assert (is_same< int volatile const * volatile const &,	DeepConstT<int volatile const * volatile &>		>(),	"Err");
		static_assert (is_same< int volatile const * volatile const &,	DeepConstT<int volatile const * volatile const&>>(),	"Err");

		// implementation is the same for &&
		static_assert (is_same< int	const &&,							DeepConstT<int 		 &&>						>(),	"Err");
		static_assert (is_same< int volatile const * volatile const &&,	DeepConstT<int volatile 	  * volatile &&>	>(),	"Err");


		static_assert (is_same< int volatile const * const *,			DeepConstT<int volatile 	  * *>				>(),	"Err");
		static_assert (is_same< int volatile const * const *,			DeepConstT<int volatile const * *>				>(),	"Err");
		static_assert (is_same< int volatile const * const *,			DeepConstT<int volatile const * const *>		>(),	"Err");
		static_assert (is_same< int volatile const * volatile const *,	DeepConstT<int volatile 	  * volatile *>		>(),	"Err");
		static_assert (is_same< int volatile const * volatile const *,	DeepConstT<int volatile const * volatile *>		>(),	"Err");
		static_assert (is_same< int volatile const * volatile const *,	DeepConstT<int volatile const * volatile const*>>(),	"Err");

		static_assert (is_same< int volatile const * volatile const * volatile,	DeepConstT<int volatile const * volatile	   * volatile>>(), "Err");
		static_assert (is_same< int volatile const * volatile const * const,	DeepConstT<int volatile		  * volatile const * const>	  >(), "Err");
		static_assert (is_same< int const		   * volatile const *,			DeepConstT<int				  * volatile const *>		  >(), "Err");


		static_assert (is_same< int	const	  [],		DeepConstT<int			 []>	 >(),	"Err");
		static_assert (is_same< int	const (&) [],		DeepConstT<int (&)		 []	 >	 >(),	"Err");
		static_assert (is_same< int	const (&) [],		DeepConstT<int const (&) []>	 >(),	"Err");
		static_assert (is_same< int	const * const [],	DeepConstT<int 		 *		 []> >(),	"Err");
		static_assert (is_same< int	const * const [],	DeepConstT<int const *		 []> >(),	"Err");
		static_assert (is_same< int	const * const [],	DeepConstT<int const * const []> >(),	"Err");

		static_assert (is_same< int	volatile const	   [],				DeepConstT<int volatile			  []>			   >(),	"Err");
		static_assert (is_same< int	volatile const (&) [],				DeepConstT<int volatile (&)		  []>			   >(),	"Err");
		static_assert (is_same< int	volatile const (&) [],				DeepConstT<int volatile const (&) []>			   >(),	"Err");
		static_assert (is_same< int	volatile const * const [],			DeepConstT<int volatile 	  *		  []>		   >(),	"Err");
		static_assert (is_same< int	volatile const * const [],			DeepConstT<int volatile const *		  []>		   >(),	"Err");
		static_assert (is_same< int	volatile const * const [],			DeepConstT<int volatile const * const []>		   >(),	"Err");
		static_assert (is_same< int	volatile const * volatile const [],	DeepConstT<int volatile 	  *	volatile 	   []> >(),	"Err");
		static_assert (is_same< int	volatile const * volatile const [],	DeepConstT<int volatile const *	volatile 	   []> >(),	"Err");
		static_assert (is_same< int	volatile const * volatile const [],	DeepConstT<int volatile const * volatile const []> >(),	"Err");

		// not full coverage, quite exponential...
		static_assert (is_same< int	const (*) [],							DeepConstT<int 		 (*)	   []>				     >(), "Err");
		static_assert (is_same< int	const (*) [],							DeepConstT<int const (*)	   []>				     >(), "Err");
		static_assert (is_same< int	const (* const) [],						DeepConstT<int const (* const) []>				     >(), "Err");
		static_assert (is_same< int	volatile const (* volatile)		  [],	DeepConstT<int volatile 	  (* volatile)  	 []> >(), "Err");
		static_assert (is_same< int	volatile const (* volatile)		  [],	DeepConstT<int volatile const (* volatile)  	 []> >(), "Err");
		static_assert (is_same< int	volatile const (* volatile const) [],	DeepConstT<int volatile const (* volatile const) []> >(), "Err");

		static_assert (is_same< int	const	  [4],		DeepConstT<int			 [4]>	 >(),	"Err");
		static_assert (is_same< int	const (&) [4],		DeepConstT<int (&)		 [4]>	 >(),	"Err");
		static_assert (is_same< int	const (&) [4],		DeepConstT<int const (&) [4]>	 >(),	"Err");
		static_assert (is_same< int	const	  [][4],	DeepConstT<int			 [][4]>	 >(),	"Err");
		static_assert (is_same< int	const (&) [][4],	DeepConstT<int (&)		 [][4]>	 >(),	"Err");
		static_assert (is_same< int	const (&) [][4],	DeepConstT<int const (&) [][4]>	 >(),	"Err");
		
		static_assert (is_same< int	const		   (*) [4],		DeepConstT<int			(*)	[4]>   >(),	"Err");
		static_assert (is_same< int	const		   (*) [4],		DeepConstT<int const	(*) [4]>   >(),	"Err");
		static_assert (is_same< int	const volatile (*) [4],		DeepConstT<int volatile (*) [4]>   >(),	"Err");
		static_assert (is_same< int	const		   (*) [][4],	DeepConstT<int			(*)	[][4]> >(),	"Err");
		static_assert (is_same< int	const		   (*) [][4],	DeepConstT<int const	(*) [][4]> >(),	"Err");
		static_assert (is_same< int	const volatile (*) [][4],	DeepConstT<int volatile (*) [][4]> >(),	"Err");

		// function pointers are "just pointers"
		static_assert (is_same< int	(*)(char),	DeepConstT<int (*)(char)> >(),	"Err");

		// member-pointers should pass intact
		static_assert (is_same< char	std::pair<int, char>::*,		DeepConstT<decltype(&std::pair<int, char>::second)> >(), "Err");
		static_assert (is_same< size_t	(std::vector<int>::*) () const,	DeepConstT<decltype(&std::vector<int>::size)>		>(), "Err");
		static_assert (is_same< void	(std::vector<int>::*) (),		DeepConstT<decltype(&std::vector<int>::clear)>		>(), "Err");
	}



	namespace TestCallabilityChecks {		// static test

		using V2   = Vector2D<double>;
		using Plus = decltype(&V2::operator+);

		static_assert(IsCallableMember<V2,		  Plus, V2>::value,	"Error");
		static_assert(IsCallableMember<V2*,		  Plus, V2>::value,	"Error");
		static_assert(IsCallableMember<V2&,		  Plus, V2>::value,	"Error");
		static_assert(IsCallableMember<V2&&,	  Plus, V2>::value,	"Error");
		static_assert(IsCallableMember<const V2,  Plus, V2>::value,	"Error");
		static_assert(IsCallableMember<const V2*, Plus, V2>::value,	"Error");
		static_assert(IsCallableMember<const V2&, Plus, V2>::value,	"Error");

		static_assert(IsCallableMember<V2,		  Plus, V2&>::value, "Error");
		static_assert(IsCallableMember<V2*,		  Plus, V2&>::value, "Error");
		static_assert(IsCallableMember<V2&,		  Plus, V2&>::value, "Error");
		static_assert(IsCallableMember<V2&&,	  Plus, V2&>::value, "Error");
		static_assert(IsCallableMember<const V2,  Plus, V2&>::value, "Error");
		static_assert(IsCallableMember<const V2*, Plus, V2&>::value, "Error");
		static_assert(IsCallableMember<const V2&, Plus, V2&>::value, "Error");

		static_assert(IsCallableMember<V2,		  Plus, V2&&>::value, "Error");
		static_assert(IsCallableMember<V2*,		  Plus, V2&&>::value, "Error");
		static_assert(IsCallableMember<V2&,		  Plus, V2&&>::value, "Error");
		static_assert(IsCallableMember<V2&&,	  Plus, V2&&>::value, "Error");
		static_assert(IsCallableMember<const V2,  Plus, V2&&>::value, "Error");
		static_assert(IsCallableMember<const V2*, Plus, V2&&>::value, "Error");
		static_assert(IsCallableMember<const V2&, Plus, V2&&>::value, "Error");

		// ----

		static_assert(!IsCallableMember<V2,			Plus, V2*>::value,	"Error");
		static_assert(!IsCallableMember<V2*,		Plus, V2*>::value,	"Error");
		static_assert(!IsCallableMember<V2&,		Plus, V2*>::value,	"Error");
		static_assert(!IsCallableMember<V2&&,		Plus, V2*>::value,	"Error");
		static_assert(!IsCallableMember<const V2,	Plus, V2*>::value,	"Error");
		static_assert(!IsCallableMember<const V2*,	Plus, V2*>::value,	"Error");
		static_assert(!IsCallableMember<const V2&,	Plus, V2*>::value,	"Error");

		static_assert(!IsCallableMember<V2,			Plus, int>::value,	"Error");
		static_assert(!IsCallableMember<V2*,		Plus, int>::value,	"Error");
		static_assert(!IsCallableMember<V2&,		Plus, int>::value,	"Error");
		static_assert(!IsCallableMember<V2&&,		Plus, int>::value,	"Error");
		static_assert(!IsCallableMember<const V2,	Plus, int>::value,	"Error");
		static_assert(!IsCallableMember<const V2*,	Plus, int>::value,	"Error");
		static_assert(!IsCallableMember<const V2&,	Plus, int>::value,	"Error");

	}



	namespace TestBasicTypeHelpers {		// static test

		// somewhat exotic cases
		static_assert(is_same<const void *,			ConstValueT<void *>>(),			 "Err");
		static_assert(is_same<float (*)(float),		ConstValueT<decltype(&sinf)>>(), "Err");
		static_assert(is_same<float (&)(float),		ConstValueT<decltype(sinf)&>>(), "Err");
		static_assert(is_same<const int [],			ConstValueT<int []>>(),			 "Err");
		static_assert(is_same<const int (&) [3],	ConstValueT<int (&)[3]>>(),		 "Err");
		static_assert(is_same<const int (*) [3],	ConstValueT<int (*)[3]>>(),		 "Err");

		// Member-pointers: consting is not planned, only follows the "default" branch.
		//					But don't qualify the function itself!
		using Pair = std::pair<int, char>;

		static_assert(is_same<int   Pair::* const,			ConstValueT<decltype(&Pair::first)>>(),	"Err");
		static_assert(is_same<void (Pair::* const)(Pair&),	ConstValueT<decltype(&Pair::swap)>>(),	"Err");
	
	}



	namespace TestHashTypes {				// static test

		// RefHolder with hash-enabled type
		static_assert (std::is_default_constructible<std::hash<RefHolder<long>>>::value, "Err");
		static_assert (std::is_copy_constructible<std::hash<RefHolder<long>>>::value,	 "Err");
		static_assert (std::is_move_constructible<std::hash<RefHolder<long>>>::value,	 "Err");
		static_assert (std::is_copy_assignable<std::hash<RefHolder<long>>>::value,		 "Err");
		static_assert (std::is_move_assignable<std::hash<RefHolder<long>>>::value,		 "Err");

		ASSERT_TYPE (size_t, std::hash<RefHolder<long>> {}(declval<RefHolder<long>>()));
		ASSERT_TYPE (size_t, std::hash<RefHolder<long>> {}(declval<const RefHolder<long>&>()));

		static_assert (noexcept(std::hash<long> {}(5l)) == noexcept(std::hash<RefHolder<long>> {}(declval<RefHolder<long>>())),			"Err");
		static_assert (noexcept(std::hash<long> {}(5l)) == noexcept(std::hash<RefHolder<long>> {}(declval<const RefHolder<long>&>())),	"Err");


		// RefHolder with hash-disabled type  (2015 STL didn't have SFINAE-friendly implementation)
#if !defined(_MSC_VER) || (_MSC_VER > 1900)
		using Tup = std::tuple<int, long>;

		static_assert (!std::is_default_constructible<std::hash<RefHolder<Tup>>>::value, "Err");
		static_assert (!std::is_copy_constructible<std::hash<RefHolder<Tup>>>::value,	 "Err");
		static_assert (!std::is_move_constructible<std::hash<RefHolder<Tup>>>::value,	 "Err");
		static_assert (!std::is_copy_assignable<std::hash<RefHolder<Tup>>>::value,		 "Err");
		static_assert (!std::is_move_assignable<std::hash<RefHolder<Tup>>>::value,		 "Err");
#endif

	}



	namespace TestTypeArgManipulation {				// static test

		using std::tuple;


		static_assert(is_same<tuple<char>,						OverriddenNthArgT<tuple<int>, 0, char>>(), "Err");
		static_assert(is_same<tuple<int>,						OverriddenNthArgT<tuple<int>, 0, None>>(), "Err");
		static_assert(is_same<tuple<char, int, double>,			OverriddenNthArgT<tuple<unsigned, int, double>, 0, char>>(), "Err");
		static_assert(is_same<tuple<unsigned, char, double>,	OverriddenNthArgT<tuple<unsigned, int, double>, 1, char>>(), "Err");
		static_assert(is_same<tuple<unsigned, int, char>,		OverriddenNthArgT<tuple<unsigned, int, double>, 2, char>>(), "Err");
		static_assert(is_same<tuple<unsigned, int, double>,		OverriddenNthArgT<tuple<unsigned, int, double>, 2, None>>(), "Err");
		
	 // Friendly assertions (CTE):
	 //	using Bad1 = OverriddenNthArgT<tuple<unsigned, int, double>, 3, char>;
	 //	using Bad2 = OverriddenNthArgT<tuple<>, 0, char>;
	 //	using Bad3 = OverriddenNthArgT<int, 0, char>;


		struct TestBindingNoAllocVal {
		};
		struct TestBindingCustomAllocVal {
			template <class V, class... Args>
			using AllocatedValueT = std::tuple<V, Args...>;
		};


		static_assert(is_same<std::tuple<int>,			CustomAllocatedT<TestBindingCustomAllocVal,	TypeList<int>,		 SizeList<>>>(), "Err");
		static_assert(is_same<std::tuple<int, char>,	CustomAllocatedT<TestBindingCustomAllocVal,	TypeList<int, char>, SizeList<>>>(), "Err");
		static_assert(is_same<std::tuple<int>,			CustomAllocatedT<TestBindingCustomAllocVal,	TypeList<int>,		 SizeList<>>>(), "Err");
		static_assert(is_same<void,						CustomAllocatedT<TestBindingNoAllocVal,		TypeList<int>,		 SizeList<>>>(), "Err");
		static_assert(is_same<void,						CustomAllocatedT<TestBindingNoAllocVal,		TypeList<int, char>, SizeList<>>>(), "Err");

		static_assert(is_same<std::tuple<int>,			RequiredAllocatedT<TestBindingCustomAllocVal,	TypeList<int>,		 SizeList<>>>(), "Err");
		static_assert(is_same<std::tuple<int, char>,	RequiredAllocatedT<TestBindingCustomAllocVal,	TypeList<int, char>, SizeList<>>>(), "Err");
		static_assert(is_same<std::tuple<int>,			RequiredAllocatedT<TestBindingCustomAllocVal,	TypeList<int>,		 SizeList<>>>(), "Err");
		static_assert(is_same<int,						RequiredAllocatedT<TestBindingNoAllocVal,		TypeList<int>,		 SizeList<>>>(), "Err");
		static_assert(is_same<char,						RequiredAllocatedT<TestBindingNoAllocVal,		TypeList<int, char>, SizeList<>>>(), "Err");
							// ^-- arbitrary default: last component, as for V in Dictionary<K, V>


		// Simplified invokation for common cases
		template <class A, class... Components>
		using AdjustedWithDefaultT = AdjustedAllocatorT<A, TestBindingNoAllocVal, TypeList<Components...>, SizeList<>>;

		template <class A, class... Components>
		using AdjustedWithCustomT = AdjustedAllocatorT<A, TestBindingCustomAllocVal, TypeList<Components...>, SizeList<>>;


		static_assert(is_same<std::allocator<int>,					AdjustedWithDefaultT<std::allocator<int>,			int>						>(), "Err");
		static_assert(is_same<std::allocator<int*>,					AdjustedWithDefaultT<std::allocator<int*>,			int*>						>(), "Err");
		static_assert(is_same<std::allocator<RefHolder<int>>,		AdjustedWithDefaultT<std::allocator<int*>,			RefHolder<int>>				>(), "Err");
		static_assert(is_same<std::allocator<RefHolder<const int>>,	AdjustedWithDefaultT<std::allocator<const int*>,	RefHolder<const int>>		>(), "Err");
		static_assert(is_same<std::allocator<char>,					AdjustedWithDefaultT<std::allocator<char>,			int, char>					>(), "Err");
		static_assert(is_same<std::allocator<char*>,				AdjustedWithDefaultT<std::allocator<char*>,			int, char*>					>(), "Err");
		static_assert(is_same<std::allocator<RefHolder<int>>,		AdjustedWithDefaultT<std::allocator<int*>,			char, RefHolder<int>>		>(), "Err");
		static_assert(is_same<std::allocator<RefHolder<const int>>,	AdjustedWithDefaultT<std::allocator<const int*>,	char, RefHolder<const int>>	>(), "Err");

		static_assert(is_same<std::allocator<std::tuple<int>>,						  AdjustedWithCustomT<std::allocator<std::tuple<int>>,				int>						>(), "Err");
		static_assert(is_same<std::allocator<std::tuple<int*>>,						  AdjustedWithCustomT<std::allocator<std::tuple<int*>>,				int*>						>(), "Err");
		static_assert(is_same<std::allocator<std::tuple<RefHolder<int>>>,			  AdjustedWithCustomT<std::allocator<std::tuple<int*>>,				RefHolder<int>>				>(), "Err");
		static_assert(is_same<std::allocator<std::tuple<RefHolder<const int>>>,		  AdjustedWithCustomT<std::allocator<std::tuple<const int*>>,		RefHolder<const int>>		>(), "Err");
		static_assert(is_same<std::allocator<std::tuple<int, char>>,				  AdjustedWithCustomT<std::allocator<std::tuple<int, char>>,		int, char>					>(), "Err");
		static_assert(is_same<std::allocator<std::tuple<int*, char>>,				  AdjustedWithCustomT<std::allocator<std::tuple<int*, char>>,		int*, char>					>(), "Err");
		static_assert(is_same<std::allocator<std::tuple<RefHolder<int>, char>>,		  AdjustedWithCustomT<std::allocator<std::tuple<int*, char>>,		RefHolder<int>, char>		>(), "Err");
		static_assert(is_same<std::allocator<std::tuple<RefHolder<const int>, char>>, AdjustedWithCustomT<std::allocator<std::tuple<const int*, char>>,	RefHolder<const int>, char>	>(), "Err");

		static_assert(is_same<TestAllocator<int, 5>,									AdjustedWithDefaultT<TestAllocator<int, 5>,							int>						>(), "Err");
		static_assert(is_same<TestAllocator<int*, 5>,									AdjustedWithDefaultT<TestAllocator<int*, 5>,						int*>						>(), "Err");
		static_assert(is_same<TestAllocator<RefHolder<int>, 5>,							AdjustedWithDefaultT<TestAllocator<int*, 5>,						RefHolder<int>>				>(), "Err");
		static_assert(is_same<TestAllocator<RefHolder<const int>, 5>,					AdjustedWithDefaultT<TestAllocator<const int*, 5>,					RefHolder<const int>>		>(), "Err");
		static_assert(is_same<TestAllocator<char, 5>,									AdjustedWithDefaultT<TestAllocator<char, 5>,							int, char>					>(), "Err");
		static_assert(is_same<TestAllocator<char*, 5>,									AdjustedWithDefaultT<TestAllocator<char*, 5>,						int, char*>					>(), "Err");
		static_assert(is_same<TestAllocator<RefHolder<int>, 5>,							AdjustedWithDefaultT<TestAllocator<int*, 5>,						char, RefHolder<int>>		>(), "Err");
		static_assert(is_same<TestAllocator<RefHolder<const int>, 5>,					AdjustedWithDefaultT<TestAllocator<const int*, 5>,					char, RefHolder<const int>>	>(), "Err");
		static_assert(is_same<TestAllocator<std::tuple<int>, 5>,						AdjustedWithCustomT<TestAllocator<std::tuple<int>, 5>,				int>						>(), "Err");
		static_assert(is_same<TestAllocator<std::tuple<int*>, 5>,						AdjustedWithCustomT<TestAllocator<std::tuple<int*>, 5>,				int*>						>(), "Err");
		static_assert(is_same<TestAllocator<std::tuple<RefHolder<int>>, 5>,				AdjustedWithCustomT<TestAllocator<std::tuple<int*>, 5>,				RefHolder<int>>				>(), "Err");
		static_assert(is_same<TestAllocator<std::tuple<RefHolder<int>>, 5>,				AdjustedWithCustomT<TestAllocator<std::tuple<int*>, 5>,				RefHolder<int>>				>(), "Err");
		static_assert(is_same<TestAllocator<std::tuple<int, char>, 5>,					AdjustedWithCustomT<TestAllocator<std::tuple<int, char>, 5>,		int, char>					>(), "Err");
		static_assert(is_same<TestAllocator<std::tuple<int*, char>, 5>,					AdjustedWithCustomT<TestAllocator<std::tuple<int*, char>, 5>,		int*, char>					>(), "Err");
		static_assert(is_same<TestAllocator<std::tuple<RefHolder<int>, char>, 5>,		AdjustedWithCustomT<TestAllocator<std::tuple<int*, char>, 5>,		RefHolder<int>, char>		>(), "Err");
		static_assert(is_same<TestAllocator<std::tuple<RefHolder<const int>, char>, 5>,	AdjustedWithCustomT<TestAllocator<std::tuple<const int*, char>, 5>,	RefHolder<const int>, char>	>(), "Err");


		// Try a more complex, imaginary binding too (value type not to make sense)
		struct TestBindingSmallDict {
			template <class K, class V, size_t N, class... Args>
			using AllocatedValueT = std::array<std::pair<K, const V>, N>;
		};

		static_assert(is_same<std::allocator<std::array<std::pair<RefHolder<const int>, const char>, 15>>,
							  AdjustedAllocatorT<std::allocator<std::array<std::pair<const int*, const char>, 15>>,
												 TestBindingSmallDict,
												 TypeList<RefHolder<const int>, char>,
												 SizeList<15>,
												 int, None>
					  >(), "Err");



		using Enumerables::ListOperations;
		using Enumerables::SetOperations;

		static_assert(is_same<std::vector<int>,										 AdjustedContainerT<ListOperations, int>>(), "Err");
		static_assert(is_same<std::vector<int>,										 AdjustedContainerT<ListOperations, int, std::allocator<int>>>(), "Err");
		static_assert(is_same<std::vector<int, TestAllocator<int, 4>>,				 AdjustedContainerT<ListOperations, int, TestAllocator<int, 4>>>(), "Err");
		static_assert(is_same<std::vector<const int*>,								 AdjustedContainerT<ListOperations, const int*>>(), "Err");
		static_assert(is_same<std::vector<const int*>,								 AdjustedContainerT<ListOperations, const int*, std::allocator<const int*>>>(), "Err");
		static_assert(is_same<std::vector<const int*, TestAllocator<const int*, 4>>, AdjustedContainerT<ListOperations, const int*, TestAllocator<const int*, 4>>>(), "Err");
		
		// default allocator will be correct ofc.
		static_assert(is_same<std::vector<RefHolder<int>>,							 AdjustedContainerT<ListOperations, RefHolder<int>>>(), "Err");

		// The actual adjustment: simply expect int* value_type from user, hiding internals
		static_assert(is_same<std::vector<RefHolder<int>>,													AdjustedContainerT<ListOperations, RefHolder<int>,		 std::allocator<int*>>>(), "Err");
		static_assert(is_same<std::vector<RefHolder<const int>>,											AdjustedContainerT<ListOperations, RefHolder<const int>, std::allocator<const int*>>>(), "Err");
		static_assert(is_same<std::vector<RefHolder<const int>, TestAllocator<RefHolder<const int>, 4>>,	AdjustedContainerT<ListOperations, RefHolder<const int>, TestAllocator<const int*, 4>>>(), "Err");
		static_assert(is_same<std::vector<RefHolder<int>,		TestAllocator<RefHolder<int>, 4>>,			AdjustedContainerT<ListOperations, RefHolder<int>, TestAllocator<int*, 4>>>(), "Err");

	}



	void TestTypeHelpers()
	{
		Greet("Typehelpers");

		TestOverloadResolver();
		TestGenStorage();
	}

}	// namespace EnumerableTests
