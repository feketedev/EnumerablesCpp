
#include "Tests.hpp"
#include "TestUtils.hpp"
#include "Enumerables.hpp"
#include <memory>



namespace EnumerableTests {

	using Enumerables::AreEqual;
	using Enumerables::RangeBetween;


	namespace {

		template <class T, class CT = T>
		struct Holder {
			T			data;
			const CT	constData;

			T&&			PassData()					{ return std::move(data); }

			T&&			GetData()		&&			{ return std::move(data); }
			T&			GetData()		&			{ return data; }
			const T&	GetData()		const &		{ return data; }

			const CT&	GetConstData()	const		{ return constData; }


			Holder		Sum(const Holder& other) const
			{
				return {
					data + other.data,
					constData + other.constData };
			}
		};

		using IntHolder		= Holder<int>;
		using IntHeapHolder = Holder<std::unique_ptr<int>, int>;		// avoid a const unique_ptr member -> can move/cpy


		template <class T>
		static T&			ExtractData(Holder<T>& h)					{ return h.data; }

		template <class T>
		static const T&		ExtractData(const Holder<T>& h)				{ return h.data; }

		template <class T>
		static T&&			ExtractData(Holder<T>&& h)					{ return std::move(h.data); }

		template <class T>
		static const T&		ExtractConstData(const Holder<T>& h)		{ return h.constData; }

		template <class T>
		static Holder<T>	operator+(const Holder<T>& l, const Holder<T>& r)	{ return l.Sum(r); }

	}	// namespace



	static void SimpleProjectionOverloads()
	{
		std::vector<IntHolder> intHoldersVec { { 1, 5 }, { 2, 5 }, { 3, 5 }, { 4, 5 } };

		// expected results
		std::vector<int*>		dataPtrs;
		std::vector<const int*> constDataPtrs;
		for (IntHolder& h : intHoldersVec) {
			dataPtrs.push_back(&h.data);
			constDataPtrs.push_back(&h.constData);
		}

		// reusable test inputs
		auto holders      = Enumerate(intHoldersVec);
		auto holderCopies = Enumerate<IntHolder>(intHoldersVec);
		auto holdersConst = Enumerate<const IntHolder&>(intHoldersVec);

		ASSERT_ELEM_TYPE (IntHolder&,		holders);
		ASSERT_ELEM_TYPE (IntHolder,		holderCopies);
		ASSERT_ELEM_TYPE (const IntHolder&,	holdersConst);


		////  A)	Project with automatic type

		// 1. from &
		{
			int& (IntHolder::* nonconstGetter)() &	= &IntHolder::GetData;
			int& (*nonconstExtractor)(IntHolder&)	= &ExtractData<int>;

			auto datas			= holders.Select(&IntHolder::data);
			auto constDatas		= holders.Select(&IntHolder::constData);
			auto gotConstDatas	= holders.Select(&IntHolder::GetConstData);
			auto extConstDatas	= holders.Select(&ExtractConstData<int>);
			auto passedDatas	= holders.Select(&IntHolder::PassData);
		//	auto gotDatas		= holders.Select(&IntHolder::GetData);		// Overload-set! Needs explicit Select<T> or preselected overload:
		//	auto extDatas		= holders.Select(&ExtractData<int>);		//
			auto extDatas		= holders.Select(nonconstExtractor);
			auto gotDatas		= holders.Select(nonconstGetter);

			ASSERT_ELEM_TYPE (int&,			datas);
			ASSERT_ELEM_TYPE (const int&,	constDatas);
			ASSERT_ELEM_TYPE (const int&,	gotConstDatas);
			ASSERT_ELEM_TYPE (const int&,	extConstDatas);
			ASSERT_ELEM_TYPE (int,			passedDatas);
			ASSERT_ELEM_TYPE (int&,			gotDatas);
			ASSERT_ELEM_TYPE (int&,			extDatas);

			ASSERT (AreEqual(dataPtrs,		datas.Addresses()));
			ASSERT (AreEqual(dataPtrs,		gotDatas.Addresses()));
			ASSERT (AreEqual(constDataPtrs,	constDatas.Addresses()));
			ASSERT (AreEqual(constDataPtrs,	gotConstDatas.Addresses()));
			ASSERT (AreEqual(constDataPtrs,	extConstDatas.Addresses()));
			ASSERT (AreEqual(gotDatas,		passedDatas));
			ASSERT (AreEqual(gotDatas,		extDatas));
		}

		// 2. from const &
		{
			const int& (IntHolder::* constGetter)() const & = &IntHolder::GetData;
			const int& (*constExtractor)(const IntHolder&)	= &ExtractData<int>;

			auto datas			= holdersConst.Select(&IntHolder::data);
			auto constDatas		= holdersConst.Select(&IntHolder::constData);
			auto gotConstDatas	= holdersConst.Select(&IntHolder::GetConstData);
		 // auto passedDatas	= holdersConst.Select(&IntHolder::PassData);	// const => CTE
		 // auto gotDatas		= holdersConst.Select(&IntHolder::GetData);		// Overload-set! Still needs explicit type (even if const)
			auto extDatas		= holdersConst.Select(constExtractor);
			auto gotDatas		= holdersConst.Select(constGetter);

			ASSERT_ELEM_TYPE (const int&,	datas);			// constness propagated
			ASSERT_ELEM_TYPE (const int&,	constDatas);
			ASSERT_ELEM_TYPE (const int&,	gotConstDatas);
			ASSERT_ELEM_TYPE (const int&,	gotDatas);
			ASSERT_ELEM_TYPE (const int&,	extDatas);

			ASSERT (AreEqual(dataPtrs,		datas.Addresses()));
			ASSERT (AreEqual(dataPtrs,		gotDatas.Addresses()));
			ASSERT (AreEqual(dataPtrs,		extDatas.Addresses()));
			ASSERT (AreEqual(constDataPtrs,	constDatas.Addresses()));
			ASSERT (AreEqual(constDataPtrs,	gotConstDatas.Addresses()));
		}

		// 3. from prvalue
		{
			const int&	(IntHolder::*  constGetter)() const &	= &IntHolder::GetData;
			int&&		(IntHolder::* rvalGetter)() &&			= &IntHolder::GetData;
			const int&	(*constExtractor)(const IntHolder&)		= &ExtractData<int>;
			int&&		(*rvalExtractor)(IntHolder&&)			= &ExtractData<int>;

			auto datas			= holderCopies.Select(&IntHolder::data);
			auto constDatas		= holderCopies.Select(&IntHolder::constData);
			auto gotConstDatas	= holderCopies.Select(&IntHolder::GetConstData);
			auto passedDatas	= holderCopies.Select(&IntHolder::PassData);
		 //	auto gotDatas	= holderCopies.Select(&IntHolder::GetData);			// Overload-set! Still needs explicit type (even if const)
			auto gotDatas		= holderCopies.Select(constGetter);				// const& overload is applicable as a language rule!
			auto extDatas		= holderCopies.Select(constExtractor);
			auto gotRvalDatas	= holderCopies.Select(rvalGetter);				// && applicable naturally
			auto extRvalDatas	= holderCopies.Select(rvalExtractor);

			// Auto-Materialize each & / && to ensure subobject lifetime!
			ASSERT_ELEM_TYPE (int,	datas);
			ASSERT_ELEM_TYPE (int,	constDatas);
			ASSERT_ELEM_TYPE (int,	gotConstDatas);
			ASSERT_ELEM_TYPE (int,	gotDatas);
			ASSERT_ELEM_TYPE (int,	gotRvalDatas);
			ASSERT_ELEM_TYPE (int,	passedDatas);
			ASSERT_ELEM_TYPE (int,	extDatas);
			ASSERT_ELEM_TYPE (int,	extRvalDatas);

			ASSERT (AreEqual(Enumerate(dataPtrs).Dereference(),			datas));
			ASSERT (AreEqual(Enumerate(constDataPtrs).Dereference(),	constDatas));
			ASSERT (AreEqual(datas,										gotDatas));
			ASSERT (AreEqual(datas,										gotRvalDatas));
			ASSERT (AreEqual(datas,										extDatas));
			ASSERT (AreEqual(datas,										extRvalDatas));
			ASSERT (AreEqual(datas,										passedDatas));
			ASSERT (AreEqual(constDatas,								gotConstDatas));
		}


		// 4. from * --> same as & case with member-pointers, just syntactic shortcut following the language
		{
			auto pointers = holders.Addresses();
			ASSERT_ELEM_TYPE (IntHolder*, pointers);

			int& (IntHolder::* nonconstGetter)() & = &IntHolder::GetData;

			auto datas			= pointers.Select(&IntHolder::data);
			auto constDatas		= pointers.Select(&IntHolder::constData);
			auto gotConstDatas	= pointers.Select(&IntHolder::GetConstData);
			auto passedDatas	= pointers.Select(&IntHolder::PassData);
		 //	auto gotDatas	= pointers.Select(&IntHolder::GetData);			// Overload-set! Still...
			auto gotDatas		= pointers.Select(nonconstGetter);

			ASSERT_ELEM_TYPE (int&,			datas);
			ASSERT_ELEM_TYPE (const int&,	constDatas);
			ASSERT_ELEM_TYPE (const int&,	gotConstDatas);
			ASSERT_ELEM_TYPE (int,			passedDatas);
			ASSERT_ELEM_TYPE (int&,			gotDatas);

			ASSERT (AreEqual(dataPtrs,		datas.Addresses()));
			ASSERT (AreEqual(dataPtrs,		gotDatas.Addresses()));
			ASSERT (AreEqual(constDataPtrs,	constDatas.Addresses()));
			ASSERT (AreEqual(constDataPtrs,	gotConstDatas.Addresses()));
			ASSERT (AreEqual(gotDatas,		passedDatas));
		}

		// 5. from const * --> same as const & case with member-pointers, just syntactic shortcut following the language
		{
			auto pointers = holdersConst.Addresses();
			ASSERT_ELEM_TYPE (const IntHolder*, pointers);

			const int& (IntHolder::* constGetter)() const & = &IntHolder::GetData;

			auto datas			= pointers.Select(&IntHolder::data);
			auto constDatas		= pointers.Select(&IntHolder::constData);
			auto gotConstDatas	= pointers.Select(&IntHolder::GetConstData);
		 // auto passedDatas	= pointers.Select(&IntHolder::PassData);		// const => CTE
		 // auto gotDatas		= pointers.Select(&IntHolder::GetData);			// Overload-set! Still...
			auto gotDatas		= pointers.Select(constGetter);

			ASSERT_ELEM_TYPE (const int&,	datas);			// constness propagated
			ASSERT_ELEM_TYPE (const int&,	constDatas);
			ASSERT_ELEM_TYPE (const int&,	gotConstDatas);
			ASSERT_ELEM_TYPE (const int&,	gotDatas);

			ASSERT (AreEqual(dataPtrs,		datas.Addresses()));
			ASSERT (AreEqual(dataPtrs,		gotDatas.Addresses()));
			ASSERT (AreEqual(constDataPtrs,	constDatas.Addresses()));
			ASSERT (AreEqual(constDataPtrs,	gotConstDatas.Addresses()));
		}


		////  B)	Project with explicit type => limited overload-resolution supported!

		// 1. from &
		{
			auto datas			= holders.Select<int&>		(&IntHolder::data);
			auto constDatas		= holders.Select<const int&>(&IntHolder::constData);
			auto gotConstDatas	= holders.Select<const int&>(&IntHolder::GetConstData);
			auto passedDatas	= holders.Select<int&&>		(&IntHolder::PassData);		// && possible explicitly, but generally discouraged
			auto passedDataCpys	= holders.Select<int>		(&IntHolder::PassData);		// -> better to materialize

			auto gotDatas		= holders.Select<int&>		(&IntHolder::GetData);		// & overload resolved
			auto extDatas		= holders.Select<int&>		(&ExtractData);				// + free template instance deduced

			auto gotDataCpys	= holders.Select<int>		(&IntHolder::GetData);		// prvalue materialization is supported aside overload-resolution
																						// - but no other conversions (e.g. use .As / .AsConst afterwards)

			// CAUTION: These will resolve to the const overloads due to return type!
			auto gotDatasConst	= holders.Select<const int&>(&IntHolder::GetData);
			auto extDatasConst	= holders.Select<const int&>(&ExtractData<int>);

			ASSERT_ELEM_TYPE (int&,			datas);
			ASSERT_ELEM_TYPE (const int&,	constDatas);
			ASSERT_ELEM_TYPE (const int&,	gotConstDatas);
			ASSERT_ELEM_TYPE (int&&,		passedDatas);
			ASSERT_ELEM_TYPE (int,			passedDataCpys);
			ASSERT_ELEM_TYPE (int&,			gotDatas);
			ASSERT_ELEM_TYPE (int&,			extDatas);
			ASSERT_ELEM_TYPE (int,			gotDataCpys);
			ASSERT_ELEM_TYPE (const int&,	gotDatasConst);
			ASSERT_ELEM_TYPE (const int&,	extDatasConst);

			ASSERT (AreEqual(dataPtrs,		datas.Addresses()));
			ASSERT (AreEqual(dataPtrs,		gotDatas.Addresses()));
			ASSERT (AreEqual(dataPtrs,		extDatas.Addresses()));
			ASSERT (AreEqual(constDataPtrs,	constDatas.Addresses()));
			ASSERT (AreEqual(constDataPtrs,	gotConstDatas.Addresses()));
			ASSERT (AreEqual(dataPtrs,		passedDatas.Addresses()));
			ASSERT (AreEqual(gotDatas,		passedDataCpys));
		}

		// 2. from const &
		{
			auto datas			= holdersConst.Select<const int&>(&IntHolder::data);
			auto constDataCpys	= holdersConst.Select<int>		 (&IntHolder::GetConstData);
			auto gotDatas		= holdersConst.Select<const int&>(&IntHolder::GetData);		// const & overload resolved
			auto extDatas		= holdersConst.Select<const int&>(&ExtractData);			// + free template instance deduced
			auto gotDataCpys	= holdersConst.Select<int>		 (&IntHolder::GetData);

			ASSERT_ELEM_TYPE (const int&,	datas);
			ASSERT_ELEM_TYPE (const int&,	gotDatas);
			ASSERT_ELEM_TYPE (const int&,	extDatas);
			ASSERT_ELEM_TYPE (int,			constDataCpys);
			ASSERT_ELEM_TYPE (int,			gotDataCpys);

			ASSERT (AreEqual(dataPtrs,									datas.Addresses()));
			ASSERT (AreEqual(dataPtrs,									gotDatas.Addresses()));
			ASSERT (AreEqual(dataPtrs,									extDatas.Addresses()));
			ASSERT (AreEqual(Enumerate(constDataPtrs).Dereference(),	constDataCpys));
			ASSERT (AreEqual(gotDatas,									gotDataCpys));
		}

		// 3. from prvalue
		{
			auto datas			= holderCopies.Select<int>(&IntHolder::data);
			auto constDatas		= holderCopies.Select<int>(&IntHolder::constData);
			auto gotConstDatas	= holderCopies.Select<int>(&IntHolder::GetConstData);
			auto passedDatas	= holderCopies.Select<int>(&IntHolder::PassData);
			auto gotDatas		= holderCopies.Select<int>(&IntHolder::GetData);		// && overload resolved
			auto extDatas		= holderCopies.Select<int>(&ExtractData);				//

			// These below should be CTE due to .Select lifetime enforcement!
			// .Map impose no such check -> could yield danglings
			//
			// auto gotConstDatas2	= holderCopies.Select<const int&>(&IntHolder::GetConstData);
			// auto gotDatas2		= holderCopies.Select<int&&>(&IntHolder::GetData);
			// auto gotDatas3		= holderCopies.MapTo<int&&>(&IntHolder::GetData);
			// auto extDatas2		= holderCopies.Select<int&&>(&ExtractData<int>);

			ASSERT_ELEM_TYPE (int,	datas);
			ASSERT_ELEM_TYPE (int,	constDatas);
			ASSERT_ELEM_TYPE (int,	gotConstDatas);
			ASSERT_ELEM_TYPE (int,	passedDatas);
			ASSERT_ELEM_TYPE (int,	gotDatas);
			ASSERT_ELEM_TYPE (int,	extDatas);

			ASSERT (AreEqual(Enumerate(dataPtrs).Dereference(),			datas));
			ASSERT (AreEqual(Enumerate(constDataPtrs).Dereference(),	constDatas));
			ASSERT (AreEqual(datas,										gotDatas));
			ASSERT (AreEqual(datas,										extDatas));
			ASSERT (AreEqual(datas,										passedDatas));
			ASSERT (AreEqual(constDatas,								gotConstDatas));
		}


		////  C)	Project with type conversion - not supported with overload-sets
		{
			auto datas			= holderCopies.Select<double>(&IntHolder::data);
			auto constDatas		= holderCopies.Select<double>(&IntHolder::constData);
			auto gotConstDatas	= holderCopies.Select<double>(&IntHolder::GetConstData);
			auto passedDatas	= holderCopies.Select<double>(&IntHolder::PassData);
			auto gotDatas		= holderCopies.Select<int>(&IntHolder::GetData)			// overload => exact (or decayed) type needed
											  .As<double>();							// .As can work around

			auto extConstDatas	= holderCopies.Select<double>(&ExtractConstData<int>);	// explicit instance is not an overload-set
			auto extDatas		= holderCopies.Select<int>(&ExtractData).As<double>();	// <- this one is

			ASSERT_ELEM_TYPE (double,	datas);
			ASSERT_ELEM_TYPE (double,	constDatas);
			ASSERT_ELEM_TYPE (double,	extConstDatas);
			ASSERT_ELEM_TYPE (double,	extDatas);
			ASSERT_ELEM_TYPE (double,	gotConstDatas);
			ASSERT_ELEM_TYPE (double,	passedDatas);
			ASSERT_ELEM_TYPE (double,	gotDatas);
		}

		// MAYBE: Same tests for OptResult
	}



	static void OverloadResolverUsage()
	{
		// fundamentals
		{
			NO_MORE_HEAP;

			IntHolder intHoldersArr[] = { { 1, 3 }, { 2, 4 }, { 3, 5 } };

			auto holders = Enumerate(intHoldersArr);

			auto constDatas = holders.Select(&IntHolder::GetConstData);
			auto datasOvrld = holders.Select<int>(&IntHolder::GetData);

			static_assert (sizeof(datasOvrld) == sizeof(constDatas) + sizeof(void*),
						   "Overload resolution expected to incur +1 ptr size only!");

			// lambdas/selectors are contained by the Enumerable only, Enumerator only refers them
			constexpr size_t simplEtorSize = sizeof(constDatas.GetEnumerator());
			constexpr size_t ovrldEtorSize = sizeof(datasOvrld.GetEnumerator());

			static_assert (simplEtorSize == ovrldEtorSize, "Enumerators shouldn't store an own copy!");


			auto constExplicitDatas = holders.Select<int>(&IntHolder::GetConstData);

			static_assert (sizeof(constExplicitDatas) == sizeof(constDatas),
						   "Exact cases should not incur extra costs even when OverloadResolver could be constructed!");
			static_assert (sizeof(constExplicitDatas.GetEnumerator()) == simplEtorSize, "Should be the same.");

			ASSERT_ELEM_TYPE (int,		  constExplicitDatas);
			ASSERT_ELEM_TYPE (int,		  datasOvrld);
			ASSERT_ELEM_TYPE (const int&, constDatas);

			auto datasOvrld2 = holders.Select<int&>(&IntHolder::GetData);
			auto datasOvrld3 = holders.Select<const int&>(&IntHolder::GetData);
			ASSERT_ELEM_TYPE (int&,		  datasOvrld2);
			ASSERT_ELEM_TYPE (const int&, datasOvrld3);

			ASSERT (5 == constDatas.Last());
			ASSERT (5 == constExplicitDatas.Last());
			ASSERT (3 == datasOvrld.Last());
			ASSERT (3 == datasOvrld2.Last());
			ASSERT (3 == datasOvrld3.Last());
		}

		// lifetime ensurance is a separate concern (e.g. Map vs Select, OverloadResolver allows what the language does)
		{
			auto holders = Enumerables::Range(1, 3).Map(FUN(x, (IntHeapHolder { std::make_unique<int>(x), 10 })));

			ASSERT_ELEM_TYPE (IntHeapHolder, holders);

			auto datasSimpl = holders.Select(&IntHeapHolder::PassData);
			auto datasField = holders.Select(&IntHeapHolder::data);
			auto datasOvrld = holders.Select<std::unique_ptr<int>>(&IntHeapHolder::GetData);		 // data passed

		 //	auto datasOvrldR = holders.Select<std::unique_ptr<int>&&>(&IntHeapHolder::GetData);		 // CTE for Select forbids dangling ref
		 //	auto datasOvrldC = holders.Select<const std::unique_ptr<int>&>(&IntHeapHolder::GetData); //
		 //	auto datasOvrldL = holders.Select<std::unique_ptr<int>&>(&IntHeapHolder::GetData);		 // - CTE by language anyway

		 	auto dangling = holders.MapTo<std::unique_ptr<int>&&>(&IntHeapHolder::GetData);			 // Compiles, Map = programmer's responsibility
			UNUSED (dangling);

			auto constsField = holders.Select(&IntHeapHolder::constData);
			auto constsSimpl = holders.Select(&IntHeapHolder::GetConstData);

			ASSERT_ELEM_TYPE (std::unique_ptr<int>, datasSimpl);				// Select deduces decayed type
			ASSERT_ELEM_TYPE (std::unique_ptr<int>, datasField);				// instead of dangling ref!
			ASSERT_ELEM_TYPE (int,					constsField);				//
			ASSERT_ELEM_TYPE (int,					constsSimpl);				//

			ASSERT_ELEM_TYPE (int,					datasOvrld.Dereference());	// similarly

			AllocationCounter allocations;

			std::vector<int> datas = datasOvrld.Dereference().ToList();
			ASSERT_EQ (1, datas[0]);
			ASSERT_EQ (2, datas[1]);
			ASSERT_EQ (3, datas[2]);

			allocations.AssertFreshCount(4);		// 3 x unique_ptr + 1 vector [capacity deduced]
			//
			// TODO: Could RangeBetween also predict capacity via some specialization? What about defined overflow?

			auto simpHolders = Enumerables::Range(1, 3).Map(FUN(x, (IntHolder { x, 10 })));
			ASSERT_ELEM_TYPE (IntHolder, simpHolders);

			// Some methods don't need lifetime ensurences, operating within a single expression internally (temporaries held)
			IntHolder max = simpHolders.MaximumsBy<const int&>(&IntHolder::GetData).Single();
			ASSERT_EQ (3, max.data);

			// NOTE: CachingEnumerator currently requires copy-constructible element, even though executing the copy can be avoided in ToList()
			//		 Solution would be a change from .Current() to .ConsumeCurrent() allowing to move out elements. (Somewhat extreme case.)
			//
			//auto tmp = holders.MaximumsBy<const std::unique_ptr<int>&>(&IntHeapHolder::GetData);		// define an op< to test
			//ASSERT_ELEM_TYPE (IntHeapHolder, tmp);
		}
	}



	// These are a bit simpler, currently have no support for overload-sets.
	static void BinaryMappers()
	{
		std::vector<IntHolder> vecA { { 1, 2 }, { 2, 5 }, { 3, 5 }, { 4, 5 } };
		std::vector<IntHolder> vecB { { 5, 2 }, { 4, 5 }, { 3, 5 }, { 2, 5 } };

		auto seqA = Enumerate(vecA).AsConst();
		auto seqB = Enumerate(vecB).AsConst();

		// Zip
		{
			auto memberSums	= seqA.Zip(seqB, &IntHolder::Sum);
			auto freeSums	= seqA.Zip(seqB, &operator+<int>);
			auto dataSums1	= seqA.Zip(seqB, FUN(a, b,  a.data + b.data + 1));

			ASSERT_ELEM_TYPE (IntHolder,	memberSums);
			ASSERT_ELEM_TYPE (IntHolder,	freeSums);
			ASSERT_ELEM_TYPE (int,			dataSums1);

			ASSERT (memberSums.Select(&IntHolder::data).AllEqual(6));
			ASSERT (freeSums.Select(&IntHolder::data).AllEqual(6));
			ASSERT (dataSums1.AllEqual(7));

			ASSERT_EQ (4, memberSums.Select(&IntHolder::constData).First());
			ASSERT    (memberSums.Skip(1).Select(&IntHolder::constData).AllEqual(10));
		}

		// MapNeighbors
		{
			auto memberSums = seqA.MapNeighbors(&IntHolder::Sum);
			auto freeSums   = seqA.MapNeighbors(&operator+<int>);
			auto dataSums1  = seqA.MapNeighbors(FUN(p, n,  p.data + n.data + 1));

			ASSERT_ELEM_TYPE (IntHolder,	memberSums);
			ASSERT_ELEM_TYPE (IntHolder,	freeSums);
			ASSERT_ELEM_TYPE (int,			dataSums1);

			ASSERT_EQ (vecA.size() - 1, memberSums.Count());
			ASSERT_EQ (vecA.size() - 1, freeSums.Count());
			ASSERT_EQ (vecA.size() - 1, dataSums1.Count());

			ASSERT (AreEqual(Enumerate({ 3, 5, 7 }), memberSums.Select(&IntHolder::data)));
			ASSERT (AreEqual(Enumerate({ 3, 5, 7 }), freeSums.Select(&IntHolder::data)));
			ASSERT (AreEqual(Enumerate({ 4, 6, 8 }), dataSums1));
		}
	}



	static void TestMinMaxOverloads()
	{
		std::vector<IntHolder> intHoldersVector { { 1, 6 }, { 2, 5 }, { 3, 4 } };

		auto holders = Enumerate(intHoldersVector);

		ASSERT_EQ (4, holders.Select(&IntHolder::GetConstData).Min());
		ASSERT_EQ (1, holders.Select<int>(&IntHolder::GetData).Min());
		ASSERT_EQ (1, holders.Select<int&>(&IntHolder::GetData).Min());
		ASSERT_EQ (1, holders.Select<const int&>(&IntHolder::GetData).Min());

		// NOTE: MinimumsBy provides const access only to elements
		ASSERT_EQ (6, holders.MinimumsBy<const int&>(&ExtractData)		 .Single().constData);
		ASSERT_EQ (6, holders.MinimumsBy<const int&>(&IntHolder::GetData).Single().constData);
		ASSERT_EQ (6, holders.MinimumsBy<const int&>(&IntHolder::data)	 .Single().constData);
		ASSERT_EQ (6, holders.MinimumsBy			(&IntHolder::data)	 .Single().constData);
		ASSERT_EQ (6, holders.MinimumsBy<const int&>(FUN(h, h.GetData())).Single().constData);
		ASSERT_EQ (6, holders.MinimumsBy			(FUN(h, h.GetData())).Single().constData);

		ASSERT_EQ (3, holders.MinimumsBy<const int&>(&ExtractConstData)		  .Single().data);	// needs OverloadResolver for being template
		ASSERT_EQ (3, holders.MinimumsBy<const int&>(&IntHolder::GetConstData).Single().data);
		ASSERT_EQ (3, holders.MinimumsBy			(&IntHolder::GetConstData).Single().data);
		ASSERT_EQ (3, holders.MinimumsBy<const int&>(&IntHolder::constData)	  .Single().data);
		ASSERT_EQ (3, holders.MinimumsBy			(&IntHolder::constData)	  .Single().data);
		ASSERT_EQ (3, holders.MinimumsBy<const int&>(FUN(h, h.GetConstData())).Single().data);
		ASSERT_EQ (3, holders.MinimumsBy			(FUN(h, h.GetConstData())).Single().data);

		// member-pointers should work just as fine with pointer sequences
		auto holderPtrs = holders.Addresses();

		ASSERT_EQ (4, holderPtrs.Select(&IntHolder::GetConstData).Min());
		ASSERT_EQ (1, holderPtrs.Select<int>(&IntHolder::GetData).Min());
		ASSERT_EQ (1, holderPtrs.Select<int&>(&IntHolder::GetData).Min());
		ASSERT_EQ (1, holderPtrs.Select<const int&>(&IntHolder::GetData).Min());

		ASSERT_EQ (6, holderPtrs.MinimumsBy<const int&>(&IntHolder::GetData) .Single()->constData);
		ASSERT_EQ (6, holderPtrs.MinimumsBy<const int&>(&IntHolder::data)	 .Single()->constData);
		ASSERT_EQ (6, holderPtrs.MinimumsBy			   (&IntHolder::data)	 .Single()->constData);
		ASSERT_EQ (6, holderPtrs.MinimumsBy<const int&>(FUN(h, h->GetData())).Single()->constData);
		ASSERT_EQ (6, holderPtrs.MinimumsBy			   (FUN(h, h->GetData())).Single()->constData);

		ASSERT_EQ (3, holderPtrs.MinimumsBy<const int&>(&IntHolder::GetConstData) .Single()->data);
		ASSERT_EQ (3, holderPtrs.MinimumsBy			   (&IntHolder::GetConstData) .Single()->data);
		ASSERT_EQ (3, holderPtrs.MinimumsBy<const int&>(&IntHolder::constData)	  .Single()->data);
		ASSERT_EQ (3, holderPtrs.MinimumsBy			   (&IntHolder::constData)	  .Single()->data);
		ASSERT_EQ (3, holderPtrs.MinimumsBy<const int&>(FUN(h, h->GetConstData())).Single()->data);
		ASSERT_EQ (3, holderPtrs.MinimumsBy			   (FUN(h, h->GetConstData())).Single()->data);

		auto prHolders = Enumerables::Range(1, 3).Map(FUN(i, (IntHolder { i, 7 - i })));
		ASSERT_ELEM_TYPE (IntHolder, prHolders);

		ASSERT_EQ (4, prHolders.Select(&IntHolder::GetConstData).Min());
		ASSERT_EQ (1, prHolders.Select<int>(&IntHolder::GetData).Min());

		// temporaries persist during comparison in MinimumsBy
		ASSERT_EQ (6, prHolders.MinimumsBy<const int&>(&ExtractData)	   .Single().constData);
		ASSERT_EQ (6, prHolders.MinimumsBy<const int&>(&IntHolder::GetData).Single().constData);
		ASSERT_EQ (6, prHolders.MinimumsBy<const int&>(&IntHolder::data)   .Single().constData);
		ASSERT_EQ (6, prHolders.MinimumsBy			  (&IntHolder::data)   .Single().constData);
		ASSERT_EQ (6, prHolders.MinimumsBy<const int&>(FUN(h, h.GetData())).Single().constData);
		ASSERT_EQ (6, prHolders.MinimumsBy			  (FUN(h, h.GetData())).Single().constData);

		ASSERT_EQ (3, prHolders.MinimumsBy<const int&>(&ExtractConstData)		.Single().data);
		ASSERT_EQ (3, prHolders.MinimumsBy<const int&>(&IntHolder::GetConstData).Single().data);
		ASSERT_EQ (3, prHolders.MinimumsBy			  (&IntHolder::GetConstData).Single().data);
		ASSERT_EQ (3, prHolders.MinimumsBy<const int&>(&IntHolder::constData)	.Single().data);
		ASSERT_EQ (3, prHolders.MinimumsBy			  (&IntHolder::constData)	.Single().data);
		ASSERT_EQ (3, prHolders.MinimumsBy<const int&>(FUN(h, h.GetConstData())).Single().data);
		ASSERT_EQ (3, prHolders.MinimumsBy			  (FUN(h, h.GetConstData())).Single().data);
	}



	static void NonPureProjections()
	{
		std::vector<IntHeapHolder> holderVec;
		holderVec.push_back({ std::make_unique<int>(1), 5 });
		holderVec.push_back({ std::make_unique<int>(2), 6 });
		holderVec.push_back({ std::make_unique<int>(3), 7 });

		auto holders = Enumerate<IntHeapHolder&>(holderVec);

		ASSERT (AreEqual(RangeBetween(1, 3),	holders.Select(FUN(h,  *h.data))));
		ASSERT (AreEqual(RangeBetween(5, 7),	holders.Select(FUN(h,  h.constData))));

		// Behaviour with xvalues (&&) is generally unspecified, but should work in this simple case:
		auto xvalues = holders.Select<std::unique_ptr<int>&&>(&IntHeapHolder::PassData);
		auto ptrs    = holders.Select(&IntHeapHolder::data).Addresses();

		ASSERT_ELEM_TYPE (std::unique_ptr<int>&&, xvalues);
		ASSERT_ELEM_TYPE (std::unique_ptr<int>*,  ptrs);
		ASSERT			 (AreEqual(ptrs, xvalues.Addresses()));

		// Now the bit more realistic version, actually mutating the objects:
		auto passed = holders.Select(&IntHeapHolder::PassData);

		// automatic result type becomes materialized (prvalue)
		ASSERT_ELEM_TYPE (std::unique_ptr<int>, passed);

		// This can be evaluated only once! But till then, it can be built upon.
		// Dereference - similarly to Select - treats the result as a subobject
		// of an rvalue -> materializes int& to int.
		auto passedInts = passed.Dereference();
		ASSERT_ELEM_TYPE (int, passedInts);

		ASSERT (AreEqual(RangeBetween(1, 3), passedInts));
		ASSERT (xvalues.AllEqual(nullptr));					// consumed
	}



	// Predicate parameters support limited overload-resolution of free functions, which is cost-free
	static bool HasEvenData(int x)					{ return x % 2 == 0; }
	static bool HasEvenData(const IntHolder& h)		{ return h.data % 2 == 0; }
	static bool HasEvenData(const IntHeapHolder& h)	{ return *h.data % 2 == 0; }

	static void Predicates()
	{
		std::vector<int>			directInts	{ 1, 2, 3 };
		std::vector<IntHolder>		simpHolders { { 3, 0 }, { 4, 0 }, { 1, 0 }, { 2, 0 } };
		std::vector<IntHeapHolder>	heapHolders;
		heapHolders.push_back({ std::make_unique<int>(5), 0 });
		heapHolders.push_back({ std::make_unique<int>(6), 0 });
		heapHolders.push_back({ std::make_unique<int>(7), 0 });

		NO_MORE_HEAP;

		auto intEvens  = Enumerate(directInts).Where(&HasEvenData);
		auto simpEvens = Enumerate(simpHolders).Where(&HasEvenData);
		auto heapEvens = Enumerate(heapHolders).Where(&HasEvenData);

		ASSERT_ELEM_TYPE (int&,			  intEvens);
		ASSERT_ELEM_TYPE (IntHolder&,	  simpEvens);
		ASSERT_ELEM_TYPE (IntHeapHolder&, heapEvens);

		ASSERT_EQ (2, intEvens.Single());
		ASSERT_EQ (4, simpEvens.First().data);
		ASSERT_EQ (2, simpEvens.Last().data);
		ASSERT_EQ (6, *heapEvens.Single().data);

		// should also work with shorthands:
		ASSERT_EQ (&intEvens.Single(),  &Enumerate(directInts).Single(&HasEvenData));
		ASSERT_EQ (&simpEvens.First(),  &Enumerate(simpHolders).First(&HasEvenData));
		ASSERT_EQ (&simpEvens.Last(),   &Enumerate(simpHolders).Last(&HasEvenData));
		ASSERT_EQ (&heapEvens.Single(), &Enumerate(heapHolders).Single(&HasEvenData));

		ASSERT_EQ (&intEvens.Single(),  &Enumerables::SingleFrom(directInts, &HasEvenData));
		ASSERT_EQ (&intEvens.Single(),  &Enumerables::SingleFrom(directInts, &HasEvenData));
	}



	static void CaptureTest()
	{
		std::vector<int> vector { 1, 2, 3, 2 };

		// - Every lambda is stored by value throughout the library
		// - FUNV uses [=] capture itself
		//(- Range generators also store values)
		{
			NO_MORE_HEAP;

			auto queryFromVector = [&vector]()
			{
				int  m = 2;
				auto limited = FUNV(x, x <= m);
				return Enumerate(vector).Where(limited);
			};


			auto queryFromRange = []()
			{
				int  k = 2;
				auto limited = FUNV(x, x <= k);
				auto inc	 = FUNV(x, x + k);
				return RangeBetween(-1, 5).Where(limited).Map(inc);
			};

			auto qv = queryFromVector();
			auto qr = queryFromRange();

			ASSERT_ELEM_TYPE (int&, qv);
			ASSERT_ELEM_TYPE (int,	qr);

			ASSERT_EQ (2, qv.Max());
			ASSERT_EQ (4, qr.Max());
			ASSERT_EQ (+1, qv.Min());
			ASSERT_EQ (+1, qr.Min());
		}


		// Set operations also can store values when receive rvalues
		{
			auto query = [&vector]()
			{
				return Enumerate(vector).Except({ 2, 2, 3 });
			};

			auto remain = query();

			ASSERT_ELEM_TYPE (int&,	remain);
			ASSERT_EQ		 (1,	remain.Single());
		}


		// MAYBE: All lambda operators should be tested - but the Chain mechanism is the same for all.
	}



	void TestLambdaUsage()
	{
		Greet("Lambda usage");
		RESULTSVIEW_DISABLES_ALLOCASSERTS;

		SimpleProjectionOverloads();
		OverloadResolverUsage();
		TestMinMaxOverloads();
		NonPureProjections();
		BinaryMappers();
		Predicates();
		CaptureTest();
	}

}	// namespace EnumerableTests
