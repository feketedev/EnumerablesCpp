
#include "Tests.hpp"
#include "TestUtils.hpp"
#include "Enumerables_InterfaceTypes.hpp"
#include <memory>


#define ASSERT_OPT_TYPE(T, optional)	static_assert (std::is_same<T, decltype(*optional)>(),  "Optional payload type check failed.")



namespace EnumerableTests {

	using Enumerables::OptResult;
	using Enumerables::StopReason;


	template <class T, class In = T>
	OptResult<T> MakeOptVal(In&& src)			{ return { std::forward<In>(src) }; }

	template <class T>
	OptResult<T> MakeOptVal(StopReason err)		{ return { err }; }

	template <class T>
	OptResult<T&> MakeOptRef(T& val)			{ return { val }; }

	template <class T>
	OptResult<T&> MakeOptRef(StopReason err)	{ return { err }; }



	static void BasicUsage()
	{
		int			a = 5;
		const int	c = 10;

		// as value
		{
			OptResult<int> optA { a };
			OptResult<int> optC { c };

			ASSERT	  (optA.HasValue() && optC.HasValue());
			ASSERT_EQ (a, optA);
			ASSERT_EQ (c, optC);

			ASSERT_OPT_TYPE (int&, optA);
			ASSERT_OPT_TYPE (int&, optC);

			OptResult<int>	copyA = optA;
			OptResult<int>	copyC = optC;

			OptResult<MoveOnly<int>> convA = optA;		// might be questionable, but (non-narrowing)
														// converting ctor is supplied
			ASSERT_EQ (a, copyA);
			ASSERT_EQ (c, copyC);
			ASSERT_EQ (a, convA->data);
			ASSERT_EQ (0, convA->moveCount);

			OptResult<MoveOnly<int>> movdA = std::move(convA);
			ASSERT_EQ (a, movdA->data);
			ASSERT_EQ (1, movdA->moveCount);
			ASSERT_OPT_TYPE (MoveOnly<int>&, movdA);
		}

		// as ref
		{
			OptResult<int&>		  optA { a };
			OptResult<const int&> optC { c };
			OptResult<const int&> optAc { a };
		 //	OptResult<int&>		  optCE { c };			// CTE ofc.

			ASSERT	  (optA.HasValue() && optC.HasValue() && optAc.HasValue());
			ASSERT_EQ (&a, &optA.Value());
			ASSERT_EQ (&a, &optAc.Value());
			ASSERT_EQ (&c, &optC.Value());

			ASSERT_OPT_TYPE (int&,		 optA);
			ASSERT_OPT_TYPE (const int&, optC);
			ASSERT_OPT_TYPE (const int&, optAc);


			OptResult<int&>			copyA = optA;
			OptResult<const int&>	copyC = optC;

			OptResult<int>		 valCopyA = optA;		// again converting...
			OptResult<int>		 valCopyC = optC;

			ASSERT_EQ (&a, &copyA.Value());
			ASSERT_EQ (&c, &copyC.Value());
			ASSERT_EQ (a,  valCopyA.Value());
			ASSERT_EQ (c,  valCopyC.Value());

			// various op ==
			ASSERT_EQ (a,  valCopyA);
			ASSERT_EQ (c,  valCopyC);
			ASSERT_EQ (valCopyA, a);
			ASSERT_EQ (valCopyC, c);
			ASSERT_EQ (optA,  valCopyA);				// int& vs. int - unconventional,
			ASSERT_EQ (optC,  valCopyC);				// but usage is expected
			ASSERT_EQ (valCopyA, optA);
			ASSERT_EQ (valCopyC, optC);

			OptResult<int&>			movdA = std::move(copyA);
			OptResult<const int&>	movdC = std::move(copyC);

			ASSERT_EQ (optA,  movdA);
			ASSERT_EQ (optC,  movdC);
		}

		// errors
		{
			OptResult<int> num { 5 };
			OptResult<int> err { StopReason::Empty };

			ASSERT (!err.HasValue());
			ASSERT (err != num);
			ASSERT (num != err);
			ASSERT (err != 0);
			ASSERT (0 != err);
			ASSERT (err == nullptr);
			ASSERT (nullptr == err);
			ASSERT (num != nullptr);
			ASSERT (nullptr != num);

			OptResult<int> copyErr = err;
			ASSERT (err == copyErr);
			OptResult<int> movdErr = std::move(copyErr);
			ASSERT (err == movdErr);

			ASSERT_EQ (StopReason::Empty, movdErr.ReasonOfMiss());
			ASSERT	  (nullptr != movdErr.ReasonOfMissText());
			ASSERT	  (nullptr == num.ReasonOfMissText());
		}
	}



	static void ValueFallbacks()
	{
		OptResult<int> err1 = StopReason::Empty;
		OptResult<int> err2 = StopReason::Ambiguous;
		OptResult<int> val  = 5;

		const OptResult<int>& cerr1 = err1;
		const OptResult<int>& cval  = val ;


		// A) Fallbacks by reference
		{
			// mutable
			{
				OptResult<int>& e2 = err1.OrFallback(err2);
				OptResult<int>& va = err1.OrFallback(err2).OrFallback(val);
				OptResult<int>& vb = err1.OrFallback(val).OrFallback(err2);

				ASSERT_EQ (&err2, &e2);
				ASSERT_EQ (&val,  &va);
				ASSERT_EQ (&val,  &vb);
			}

			// const
			{
				const OptResult<int>& e1 = err2.OrFallback(cerr1);
				const OptResult<int>& e2 = cerr1.OrFallback(err2);
				const OptResult<int>& va = cerr1.OrFallback(err2).OrFallback(val);
				const OptResult<int>& vb = err1.OrFallback(cval).OrFallback(err2);

				ASSERT_EQ (&err1, &e1);
				ASSERT_EQ (&err2, &e2);
				ASSERT_EQ (&val,  &va);
				ASSERT_EQ (&val,  &vb);
			}

			// rvalue chaining
			{
				// homogene chain: just chose xvalue &&
				ASSERT_TYPE (OptResult<int>&&,	MakeOptVal<int>(1).OrFallback(MakeOptVal<int>(2)));

				OptResult<MoveOnly<int>> res1 = MakeOptVal<MoveOnly<int>>(1)
												.OrFallback(MakeOptVal<MoveOnly<int>>(2))
												.OrFallback(MakeOptVal<MoveOnly<int>>(5));

				OptResult<MoveOnly<int>> res2 = MakeOptVal<MoveOnly<int>>(StopReason::Empty)
												.OrFallback(MakeOptVal<MoveOnly<int>>(2))
												.OrFallback(StopReason::Ambiguous);
				ASSERT_EQ (1, res1->data);
				ASSERT_EQ (1, res1->moveCount);
				ASSERT_EQ (2, res2->data);
				ASSERT_EQ (1, res2->moveCount);
			}
		}


		// B) mixed expressions: must materialize!
		{
			ASSERT_TYPE (OptResult<int>,	MakeOptVal<int>(1).OrFallback(val));
			ASSERT_TYPE (OptResult<int>,	MakeOptVal<int>(1).OrFallback(cval));
			ASSERT_TYPE (OptResult<int>,	val.OrFallback(MakeOptVal<int>(2)));
			ASSERT_TYPE (OptResult<int>,	cval.OrFallback(MakeOptVal<int>(2)));

			{
				OptResult<int> res1 = MakeOptVal<int>(StopReason::Empty)
										.OrFallback(val)
										.OrFallback(StopReason::Ambiguous);

				OptResult<int> res2 = val.OrFallback(MakeOptVal<int>(StopReason::Empty))
										 .OrFallback(StopReason::Ambiguous);
				ASSERT_EQ (5, res1);
				ASSERT_EQ (5, res2);
			}
		}


		// A') Defaulting by reference
		{
			int		  m = 9;
			const int c = 7;

			// payload is not reference
			{
				int&		res1 = val.OrDefault(m);
				const int&	res2 = val.OrDefault(c);
				const int&	res3 = cval.OrDefault(m);

				ASSERT_EQ (&val.Value(),  &res1);
				ASSERT_EQ (&val.Value(),  &res2);
				ASSERT_EQ (&cval.Value(), &res3);

				int&		def1 = err1.OrDefault(m);
				const int&	def2 = err1.OrDefault(c);
				const int&	def3 = cerr1.OrDefault(m);

				ASSERT_EQ (&m, &def1);
				ASSERT_EQ (&c, &def2);
				ASSERT_EQ (&m, &def3);

				// should not be able to get dangling here
				ASSERT_TYPE (int, MakeOptVal<int>(3).OrDefault(m));
				ASSERT_TYPE (int, MakeOptVal<int>(3).OrDefault(c));
				ASSERT_TYPE (int, MakeOptVal<int>(3).OrDefault(6));
			}

			// payload is reference
			{
				int		  b = 7;
				const int k = 8;

				OptResult<int&>			ref   { b };
				OptResult<const int&>	cref  { k };
				OptResult<int&>			rerr  { StopReason::Empty };
				OptResult<const int&>	crerr { cerr1 };				// CONSIDER: forbid questionable conversion?

				int&		res1 = ref.OrDefault(m);
				const int&	res2 = ref.OrDefault(k);
				const int&	res3 = cref.OrDefault(m);
				const int&	res4 = cref.OrDefault(k);

				ASSERT_EQ (&b, &res1);
				ASSERT_EQ (&b, &res2);
				ASSERT_EQ (&k, &res3);
				ASSERT_EQ (&k, &res4);

				int&		def1 = rerr.OrDefault(b);
				const int&	def2 = rerr.OrDefault(k);
				const int&	def3 = crerr.OrDefault(b);
				const int&	def4 = crerr.OrDefault(k);

				ASSERT_EQ (&b, &def1);
				ASSERT_EQ (&k, &def2);
				ASSERT_EQ (&b, &def3);
				ASSERT_EQ (&k, &def4);

				// rvalues: payload is still an lvalue ref
				//			-> allow it to pass, when the alternative is ref too
				//			-> force materialize when alternative is rvalue
				{
					int&		r1  = MakeOptRef(b).OrDefault(m);
				 	const int&	cr1 = MakeOptRef(b).OrDefault(c);
				 	const int&	cr2 = MakeOptRef(k).OrDefault(m);
					int			v1  = MakeOptRef(b).OrDefault(6);
					int			v2  = MakeOptRef(k).OrDefault(8);

					// don't let dangle
					ASSERT_TYPE		(int, MakeOptRef(b).OrDefault(6));
					ASSERT_TYPE		(int, MakeOptRef(k).OrDefault(6));
					ASSERT_OPT_TYPE	(int&,		 MakeOptRef(b));
					ASSERT_OPT_TYPE	(const int&, MakeOptRef(k));

					ASSERT_EQ (&b,	&r1);
					ASSERT_EQ (&b,	&cr1);
					ASSERT_EQ (&k,	&cr2);
					ASSERT_EQ (b,	v1);
					ASSERT_EQ (k,	v2);
				}

				// try the same with errors
				{
					int&		r1 = MakeOptRef<int>(StopReason::Empty).OrDefault(m);
					const int& cr1 = MakeOptRef<int>(StopReason::Empty).OrDefault(c);
					const int& cr2 = MakeOptRef<int>(StopReason::Empty).OrDefault(m);
					int			v1 = MakeOptRef<int>(StopReason::Empty).OrDefault(6);
					int			v2 = MakeOptRef<int>(StopReason::Empty).OrDefault(3);

					// don't let dangle
					ASSERT_TYPE		(int, MakeOptRef(b).OrDefault(6));
					ASSERT_TYPE		(int, MakeOptRef(k).OrDefault(6));
					ASSERT_OPT_TYPE	(int&, MakeOptRef(b));
					ASSERT_OPT_TYPE	(const int&, MakeOptRef(k));

					ASSERT_EQ (&m, &r1);
					ASSERT_EQ (&c, &cr1);
					ASSERT_EQ (&m, &cr2);
					ASSERT_EQ (6, v1);
					ASSERT_EQ (3, v2);
				}
			}


			// B') Defaulting by value   (mixing ref kinds -> prval)
			{
				// iayload is not reference
				{
					ASSERT_TYPE (int, MakeOptVal<int>(3).OrDefault(m));
					ASSERT_TYPE (int, MakeOptVal<int>(3).OrDefault(c));
					ASSERT_TYPE (int, MakeOptVal<int>(3).OrDefault(6));
					ASSERT_TYPE (int, val.OrDefault(6));
					ASSERT_TYPE (int, cval.OrDefault(6));

					ASSERT_EQ (3,	  MakeOptVal<int>(3).OrDefault(m));
					ASSERT_EQ (3,	  MakeOptVal<int>(3).OrDefault(c));
					ASSERT_EQ (3,	  MakeOptVal<int>(3).OrDefault(6));
					ASSERT_EQ (*val,  val.OrDefault(6));
					ASSERT_EQ (*cval, cval.OrDefault(6));
					ASSERT_EQ (6,	  err1.OrDefault(6));
					ASSERT_EQ (6,	  cerr1.OrDefault(6));

					ASSERT_EQ (m, MakeOptVal<int>(StopReason::Empty).OrDefault(m));
					ASSERT_EQ (c, MakeOptVal<int>(StopReason::Empty).OrDefault(c));
					ASSERT_EQ (6, MakeOptVal<int>(StopReason::Empty).OrDefault(6));
				}

				// payload is reference -> materialization can only be needed for the parameter
				{
					OptResult<int&>			ref   { m };
					OptResult<const int&>	cref  { c };

					ASSERT_TYPE (int&,		 MakeOptRef(m).OrDefault(m));	// lvalue referred inside!
					ASSERT_TYPE (const int&, MakeOptRef(m).OrDefault(c));	//
					ASSERT_TYPE (int,		 ref.OrDefault(6));
					ASSERT_TYPE (int,		 cref.OrDefault(6));

					ASSERT_EQ (m, ref.OrDefault(6));
					ASSERT_EQ (c, cref.OrDefault(6));

					OptResult<int&>			rerr  { StopReason::Empty };
					OptResult<const int&>	crerr { StopReason::Ambiguous };

					ASSERT_EQ (6, rerr.OrDefault(6));
					ASSERT_EQ (6, crerr.OrDefault(6));
				}
			}
		}
	}



	static void FunctionFallbacks()
	{
		// payload is not ref
		{
			auto gimmeFive		= []() -> int			 { return 5; };
			auto gimmeOptFive	= []() -> OptResult<int> { return 5; };
			auto dontCallInt	= []() -> int			 { ASSERT (false); return 1; };
			auto dontCallOpt	= []() -> OptResult<int> { ASSERT (false); return 1; };

			OptResult<int> err1 = StopReason::Empty;
			OptResult<int> val = 5;

			const OptResult<int>& cerr1 = err1;
			const OptResult<int>& cval = val;

			// - these overloads always produce exact T type of payload
			// - lambda should also produce exactly T
			ASSERT_TYPE (OptResult<int>,	val.OrFallback(gimmeOptFive));
			ASSERT_TYPE (OptResult<int>,	cval.OrFallback(gimmeOptFive));
			ASSERT_TYPE (OptResult<int>,	MakeOptVal<int>(1).OrFallback(gimmeOptFive));

			ASSERT_TYPE (int,				val.OrDefault(gimmeFive));
			ASSERT_TYPE (int,				cval.OrDefault(gimmeFive));
			ASSERT_TYPE (int,				MakeOptVal<int>(1).OrDefault(gimmeFive));

			ASSERT_EQ (MakeOptVal<int>(5), val.OrFallback(dontCallOpt));
			ASSERT_EQ (MakeOptVal<int>(5), cval.OrFallback(dontCallOpt));
			ASSERT_EQ (cval,			   MakeOptVal<int>(5).OrFallback(dontCallOpt));

			ASSERT_EQ (5,				   val.OrDefault(dontCallInt));
			ASSERT_EQ (5,				   cval.OrDefault(dontCallInt));
			ASSERT_EQ (5,				   MakeOptVal<int>(5).OrDefault(dontCallInt));

			ASSERT_EQ (5, err1.OrDefault(gimmeFive));
			ASSERT_EQ (5, cerr1.OrDefault(gimmeFive));
			ASSERT_EQ (5, MakeOptVal<int>(StopReason::Empty).OrDefault(gimmeFive));

			int res1 = err1.OrFallback([]() { return MakeOptVal<int>(StopReason::Empty); })
						   .OrFallback([]() { return MakeOptVal<int>(4); })
						   .OrFallback(dontCallOpt)
						   .OrDefault (dontCallInt);
			ASSERT_EQ (4, res1);

			int res2 = err1.OrFallback(gimmeOptFive)
						   .OrDefault (dontCallInt);
			ASSERT_EQ (5, res2);

			int res3 = cerr1.OrFallback([]() { return MakeOptVal<int>(StopReason::Empty); })
						    .OrDefault ([]() { return 10; });
			ASSERT_EQ (10, res3);

			int res5 = MakeOptVal<int>(StopReason::Empty).OrFallback([]() { return MakeOptVal<int>(4); })
														 .OrDefault(dontCallInt);
			ASSERT_EQ (4, res5);
		}

		// same with Ref payload
		{
			int one  = 1;
			int five = 5;

			auto gimmeFive		= [&]() -> int&			   { return five; };
			auto gimmeOptFive	= [&]() -> OptResult<int&> { return five; };
			auto dontCallInt	= [&]() -> int&			   { ASSERT (false); return one; };
			auto dontCallOpt	= [&]() -> OptResult<int&> { ASSERT (false); return one; };

			OptResult<int&> err1 = StopReason::Empty;
			OptResult<int&> val = five;

			const OptResult<int&>& cerr1 = err1;
			const OptResult<int&>& cval  = val;

			// - these overloads always produce exact T type of payload
			// - lambda should also produce exactly T
			ASSERT_TYPE (OptResult<int&>,	val.OrFallback(gimmeOptFive));
			ASSERT_TYPE (OptResult<int&>,	cval.OrFallback(gimmeOptFive));
			ASSERT_TYPE (OptResult<int&>,	MakeOptRef(one).OrFallback(gimmeOptFive));

			ASSERT_TYPE (int&,				val.OrDefault(gimmeFive));
			ASSERT_TYPE (int&,				cval.OrDefault(gimmeFive));
			ASSERT_TYPE (int&,				MakeOptRef(one).OrDefault(gimmeFive));

			ASSERT_EQ (&five,	&val.OrFallback(dontCallOpt).Value());
			ASSERT_EQ (&five,	&cval.OrFallback(dontCallOpt).Value());
			ASSERT_EQ (&five,	&MakeOptRef(five).OrFallback(dontCallOpt).Value());

			ASSERT_EQ (&five,	&val.OrDefault(dontCallInt));
			ASSERT_EQ (&five,	&cval.OrDefault(dontCallInt));
			ASSERT_EQ (&five,	&MakeOptRef(five).OrDefault(dontCallInt));

			ASSERT_EQ (&five,	&err1.OrDefault(gimmeFive));
			ASSERT_EQ (&five,	&cerr1.OrDefault(gimmeFive));
			ASSERT_EQ (&five,	&MakeOptRef<int>(StopReason::Empty).OrDefault(gimmeFive));

			int& res1 = err1.OrFallback([]()  { return MakeOptRef<int>(StopReason::Empty); })
							.OrFallback([&]() { return MakeOptRef<int>(one); })
							.OrFallback(dontCallOpt)
							.OrDefault (dontCallInt);
			ASSERT_EQ (&one, &res1);

			int& res2 = err1.OrFallback(gimmeOptFive)
							.OrDefault (dontCallInt);
			ASSERT_EQ (&five, &res2);

			int& res3 = cerr1.OrFallback([]() { return MakeOptRef<int>(StopReason::Empty); })
						     .OrDefault ([&]() -> int& { return one; });
			ASSERT_EQ (&one, &res3);

			int& res5 = MakeOptRef<int>(StopReason::Empty).OrFallback([&]() { return MakeOptRef<int>(five); })
														  .OrDefault(dontCallInt);
			ASSERT_EQ (&five, &res5);
		}
	}



	static void Mappers()
	{
		// basic
		{
			OptResult<int> err1 = StopReason::Empty;
			OptResult<int> val1 = 5;

			auto recip = [](int a) -> double { return 1.0 / a; };	// lval func

			OptResult<double> opt1 = err1.MapValue(recip);
			OptResult<double> opt2 = val1.MapValue([](int a) -> double { return 1.0 / a; });

			ASSERT	  (!opt1.HasValue());
			ASSERT	  (opt2.HasValue());
			ASSERT_EQ (1.0 / 5, opt2);
		}

		// simple members
		{
			using Pair = std::pair<int, std::unique_ptr<long>>;

			OptResult<Pair> err1 = StopReason::Empty;
			OptResult<Pair> lval { 5, std::make_unique<long>(15) };

			// CONSIDER: if this ref behaviour is wanted here or not...
			ASSERT_TYPE (OptResult<int&>, err1.MapValue(&Pair::first));
			ASSERT_TYPE (OptResult<int&>, lval.MapValue(&Pair::first));
			ASSERT_TYPE (OptResult<int>,  std::move(lval).MapValue(&Pair::first));

			// ...it's certainly flexible though.
			OptResult<int> opt1 = err1.MapValue(&Pair::first);
			OptResult<int> opt2 = lval.MapValue(&Pair::first);

			ASSERT	  (!opt1.HasValue());
			ASSERT	  (opt2.HasValue());
			ASSERT_EQ (5, opt2);
		}


		// qualifiers
		// (without being complete here -> OverlaodResolver is thouroughly checked by LambdaHandlingTests + TypeHelperTests)
		{
			struct Rec {
				double				 data;
				std::unique_ptr<int> heapData;

				std::unique_ptr<int>	CopyHeapData()	const	{ return std::make_unique<int>(*heapData); }
				const int&				HeapData()		const	{ return *heapData; }
				int&					HeapData()				{ return *heapData; }

				std::unique_ptr<int>	GetHeapData()	const &	{ return CopyHeapData(); }
				std::unique_ptr<int>	GetHeapData()		 &&	{ return std::move(heapData); }

			};

			OptResult<Rec>		 val  { 2.4, std::make_unique<int>(4) };
			const OptResult<Rec> cval { 2.5, std::make_unique<int>(5) };

			// direct lambdas
			{
				OptResult<double&>		 mData = val.MapValue ([](Rec& r)  -> double&			{ return r.data; });
				OptResult<const double&> cData = cval.MapValue([](auto& r) -> const double&		{ return r.data; });

				OptResult<std::unique_ptr<int>>  prData = MakeOptVal<Rec>({ 2.6, std::make_unique<int>(6) })
															.MapValue([](Rec&& r) { return std::move(r.heapData); });

				ASSERT_EQ (&val->data,  &mData.Value());
				ASSERT_EQ (&cval->data, &cData.Value());
				ASSERT_EQ (6,			**prData);
			}

			// exact method
			{
				OptResult<std::unique_ptr<int>> opt1 = val.MapValue(&Rec::CopyHeapData);
				OptResult<std::unique_ptr<int>> opt2 = cval.MapValue(&Rec::CopyHeapData);
				OptResult<std::unique_ptr<int>> opt3 = MakeOptVal<Rec>({ 2.6, std::make_unique<int>(6) }).MapValue(&Rec::CopyHeapData);

				ASSERT_EQ (*val->heapData,	**opt1);
				ASSERT_EQ (*cval->heapData,	**opt2);
				ASSERT_EQ (6,				**opt3);
			}

			// const-nonconst overload
			{
				// CONSIDER: There's no safety here. MakeOptVal().MapValueTo<int&> would compile! Should there be some kind of .Select here too?
				OptResult<int&>		  opt1 = val.MapValueTo<int&>(&Rec::HeapData);
				OptResult<const int&> opt2 = cval.MapValueTo<const int&>(&Rec::HeapData);
				OptResult<int>		  opt3 = MakeOptVal<Rec>({ 2.6, std::make_unique<int>(6) }).MapValueTo<int>(&Rec::HeapData);

				ASSERT_EQ (val->heapData.get(),  &opt1.Value());
				ASSERT_EQ (cval->heapData.get(), &opt2.Value());
				ASSERT_EQ (6,					 *opt3);
			}

			// rval-lval overload
			{
				OptResult<Rec> toMove { 2.6, std::make_unique<int>(6) };

				OptResult<std::unique_ptr<int>> opt1 = val.MapValueTo<std::unique_ptr<int>>(&Rec::GetHeapData);
				OptResult<std::unique_ptr<int>> opt2 = cval.MapValueTo<std::unique_ptr<int>>(&Rec::GetHeapData);
				OptResult<std::unique_ptr<int>> opt3 = std::move(toMove).MapValueTo<std::unique_ptr<int>>(&Rec::GetHeapData);

				ASSERT_EQ (*val->heapData,  **opt1);
				ASSERT_EQ (*cval->heapData, **opt2);
				ASSERT_EQ (6,			    **opt3);
				ASSERT	  (toMove->heapData == nullptr);
			}
		}
	}



	void TestOptResult()
	{
		Greet("Optional Results");

		BasicUsage();
		ValueFallbacks();
		FunctionFallbacks();
		Mappers();
	}

}	// namespace EnumerableTests
