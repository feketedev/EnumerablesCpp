
#include "Tests.hpp"
#include "TestUtils.hpp"
#include "Enumerables.hpp"
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <string>

#ifdef __clang__
#	pragma clang diagnostic ignored "-Wold-style-cast"
#endif



namespace EnumerableTests {


	constexpr unsigned MeasurementCount  = 6;
	constexpr size_t   DefaultComplexity = 50000;
#ifdef _DEBUG
	constexpr char	   GreetTxt[]    = "Performance [Debug]";
	constexpr unsigned DefaultCycles = 10;
#else
	constexpr char	   GreetTxt[]    = "Performance [Release]";
	constexpr unsigned DefaultCycles = 2000;
#endif




#pragma region Testcases

	template <class Inp, class Res = Inp>
	struct ScalarTestBase {
		static std::vector<Inp> GenerateInput(size_t size)
		{
			std::vector<Inp> input;
			input.reserve(size);
			for (size_t i = 0; i < size; i++)
				input.push_back(static_cast<Inp>(std::rand() - RAND_MAX / 2));
			return input;
		}

		static void MixbackElem(Inp& nextInput, Res res)
		{
			nextInput += static_cast<Inp>(res / 2);
		}
	};



	struct PairTestBase {
		using Pair = std::pair<int, char>;

		static std::vector<Pair> GenerateInput(size_t size)
		{
			std::vector<Pair> input;
			input.reserve(size);
			for (size_t i = 0; i < size; i++) {
				int r = std::rand();
				input.emplace_back(r - RAND_MAX / 2, 'a' + (char)(r % ('z' - 'a')));
			}

			return input;
		}

		static void MixbackElem(Pair& nextInput, const Pair& res)
		{
			nextInput.first  += res.first / 2;
			nextInput.second = res.second;
		}
	};


	
	template <bool PosFiltered = false>
	struct PairMinimumsTestBase : public PairTestBase {

		// ensure multiple minima, in some non-leading location
		static std::vector<Pair> GenerateInput(size_t size)
		{
			std::vector<Pair> input = PairTestBase::GenerateInput(size);

			const size_t trgCount = 2 + size / 7;
			const int	 minValue = PosFiltered ? 2
				: std::minmax(input.begin(), input.end(), [](const auto& l, const auto& r) {
					return l->first < r->first;
				  }).first->first;

			// 1st is not to contain minValue => exercise .Minimums()
			size_t slice = size / (trgCount + 1);
			for (size_t i = 0, j = slice + slice / 2; i < slice && j < size; i++) {
				if (input[i].first == minValue) {
					while (j + 1 < size && input[j].first == minValue)
						j++;
					input[i].swap(input[j++]);
				}
			}

			// make result less trivial
			for (size_t i = 0; PosFiltered && i < size; i++) {
				int& n = input[i].first;
				n += (n == 1) ? 5 : 0;
			}

			// implant some minimums
			for (size_t c = 1; c <= trgCount; c++)
				input[c * slice].first = minValue;

			return input;
		}
	};




	struct DirectCopy : public PairTestBase {
		static constexpr const char Name[] = "Small POD iteration";


		static std::vector<Pair> RunClassic(const std::vector<Pair>& in, size_t /*resHint*/)
		{
			std::vector<Pair> cpy;	// = in;  would produce some optimized assembly, but...
			cpy.reserve(in.size());

			// ...let's use iterators -> point is to compare traversal,
			// copying is just a requirement of the current test system
			for (const Pair& e : in)
				cpy.push_back(e);

			return cpy;
		}


		static auto CreateQuery(const std::vector<Pair>& in)
		{
			return Enumerate(in);
		}
	};



	struct ConversionCopy : public ScalarTestBase<int, double> {
		static constexpr const char Name[] = "Convert: int -> double";


		static std::vector<double> RunClassic(const std::vector<int>& in, size_t /*resHint*/)
		{
			std::vector<double> cpy;
			cpy.reserve(in.size());

			for (const int& e : in)
				cpy.push_back(e);

			return cpy;
		}


		static auto CreateQuery(const std::vector<int>& in)
		{
			return Enumerate(in).As<double>();
		}
	};



	struct CopyFromDict {
		static constexpr const char Name[] = "HashMap iteration";


		static std::unordered_map<int, char> GenerateInput(size_t size)
		{
			std::unordered_map<int, char> input;
			for (size_t i = 0; i < size; i++)
				input.emplace((int)i, 'a' + (char)(rand() % ('z' - 'a')));
			return input;
		}

		static void MixbackElem(char& nextInput, char res)
		{
			nextInput = 'a' + (char)((nextInput + res) % ('z' - 'a'));
		}


		static std::vector<char> RunClassic(const std::unordered_map<int, char>& in, size_t /*resHint*/ = 0)
		{
			std::vector<char> res;
			res.reserve(in.size());

			for (auto& kv : in)
				res.push_back(kv.second);
			return res;
		}


		static auto CreateQuery(const std::unordered_map<int, char>& in)
		{
			return Enumerate(in).Select(FUN(kv,  kv.second));
		}
	};




	template <class N>
	struct Squares : public ScalarTestBase<N> {
		static const char Name[];


		static std::vector<N> RunClassic(const std::vector<N>& in, size_t /*resHint*/ = 0)
		{
			std::vector<N> res;
			res.reserve(in.size());
			for (const N& n : in)
				res.push_back(n * n);
			return res;
		}


		static auto CreateQuery(const std::vector<N>& in)
		{
			return Enumerate(in).Map(FUN(x,  x * x));
		}
	};




	struct SubrangeFiltered : public ScalarTestBase<int> {
		static constexpr const char Name[] = "Subrange of filtered int";


		static std::vector<int> RunClassic(const std::vector<int>& in, size_t resHint = 0)
		{
			std::vector<int> cpy;
			cpy.reserve(resHint);

			const size_t skip = SkipCount(in.size());
			const size_t take = TakenLen(in.size());

			unsigned found = 0;
			for (const int& e : in) {
				if (IsAccepted(e)) {
					++found;
					if (skip < found && found <= skip + take)
						cpy.push_back(e);
				}
			}
			return cpy;
		}


		static auto CreateQuery(const std::vector<int>& in)
		{
			const size_t s = SkipCount(in.size());
			const size_t t = TakenLen(in.size());

			return Enumerate(in).Where(&IsAccepted).Skip(s).Take(t);
		}

	private:
		static size_t SkipCount(size_t inp)	{ return inp / 5; }
		static size_t TakenLen(size_t inp)	{ return inp / 6; }
		static bool IsAccepted(int n)		{ return n % 47 < 21; }
	};



	struct IntSort : public ScalarTestBase<int> {
		static constexpr const char Name[] = "Sorted copy of ints";


		static std::vector<int> RunClassic(const std::vector<int>& in, size_t /*resHint*/ = 0)
		{
			std::vector<int> res = in;
			std::sort(res.begin(), res.end());
			return res;
		}


		static auto CreateQuery(const std::vector<int>& in)
		{
			// Enumerating decayed int is important to avoid duplicated copy in ToList!
			// ~20% extra overhead if missed
			return Enumerate<int>(in).Order();
		}
	};



	// TEMPORARY for CachingEnumerator iteration
	struct IntSort2 : public ScalarTestBase<int> {
		static constexpr const char Name[] = "Sorted copy of int&";


		static std::vector<int> RunClassic(const std::vector<int>& in, size_t /*resHint*/ = 0)
		{
			std::vector<int> res = in;
			std::sort(res.begin(), res.end());
			return res;
		}


		static auto CreateQuery(const std::vector<int>& in)
		{
			return Enumerate(in).Order();
		}
	};



	struct DblPositiveMinimums : public ScalarTestBase<double> {
		static constexpr const char Name[] = "Positive Min. doubles";


		static std::vector<double> RunClassic(const std::vector<double>& in, size_t resHint = 0)
		{
			std::vector<double> res;
			res.reserve(resHint);

			double min = std::numeric_limits<double>::max();
			for (const double& x : in) {
				if (x <= 0 || min < x)
					continue;

				if (x < min) {
					min = x;
					res.clear();
				}
				res.push_back(x);
			}
			return res;
		}


		static auto CreateQuery(const std::vector<double>& in)
		{
			// by prvalue: exercise ObtainCachedResults (avoid copy)
			return Enumerate<double>(in).Where(FUN(x, x > 0))
										.Minimums();
		}
	};


	template <class N>
	struct NeighborDiffs : public ScalarTestBase<N> {
		static const char Name[];


		static std::vector<N> RunClassic(const std::vector<N>& in, size_t /*resHint*/ = 0)
		{
			std::vector<N> res;
			res.reserve(in.capacity() - 1);
			for (size_t i = 0; i + 1 < in.size(); i++)
				res.push_back(in[i + 1] - in[i]);
			return res;
		}


		static auto CreateQuery(const std::vector<N>& in)
		{
			return Enumerate(in).MapNeighbors(FUN(p,n,  n - p));
		}
	};



	struct OrderByOtherField : public PairTestBase {
		static constexpr const char Name[] = "OrderBy a, Select b";


		static std::vector<char> RunClassic(const std::vector<Pair>& in, size_t /*resHint*/ = 0)
		{
			std::vector<const Pair*> ordered;
			ordered.reserve(in.size());
			for (const Pair& p : in)
				ordered.push_back(&p);

			std::sort(ordered.begin(), ordered.end(), [](const Pair* a, const Pair* b) {
				return a->first < b->first;
			});

			std::vector<char> res;
			res.reserve(in.size());
			for (const Pair* p : ordered)
				res.push_back(p->second);

			return res;
		}


		static auto CreateQuery(const std::vector<Pair>& in)
		{
			return Enumerate(in).OrderBy(&Pair::first)
								.Select(&Pair::second);
		}


		static void MixbackElem(Pair& nextInput, char res)
		{
			nextInput.first  += res;
			nextInput.second = res;
		}
	};


	struct FilterByField : public PairTestBase {
		static constexpr const char Name[] = "Filtered pairs";


		static std::vector<Pair> RunClassic(const std::vector<Pair>& in, size_t resHint = 0)
		{
			std::vector<Pair> res;
			res.reserve(resHint);

			for (const Pair& p : in) {
				if (p.first > 0)
					res.push_back(p);
			}

			return res;
		}


		static auto CreateQuery(const std::vector<Pair>& in)
		{
			return Enumerate(in).Where(FUN(p, p.first > 0));
		}
	};



	// from LegacyTests
	struct ProjectFiltered : public PairTestBase {
		static constexpr const char Name[] = "Filter and Project";


		static std::vector<char> RunClassic(const std::vector<Pair>& in, size_t resHint = 0)
		{
			std::vector<char> res;
			res.reserve(resHint);

			for (const Pair& p : in) {
				if ((p.second + p.first) % 3 == 0)
					res.push_back(p.second);
			}
			return res;
		}


		static auto CreateQuery(const std::vector<Pair>& in)
		{
			return Enumerate(in).Where(FUN(p, (p.second + p.first) % 3 == 0))
								.Select(&Pair::second);
		}


		static void MixbackElem(Pair& nextInput, char res)
		{
			nextInput.first += res;
			nextInput.second = res;
		}
	};




	struct PositiveMinimumsOrdered : public PairMinimumsTestBase<true> {
		static constexpr const char Name[] = "Pos.Min. a, OrderBy b";


		static std::vector<Pair> RunClassic(const std::vector<Pair>& in, size_t resHint = 0)
		{
			std::vector<const Pair*> minimums;
			minimums.reserve(resHint);

			int min = INT_MAX;
			for (const Pair& p : in) {
				if (p.first <= 0 || min < p.first)
					continue;

				if (p.first < min) {
					min = p.first;
					minimums.clear();
				}
				minimums.push_back(&p);
			}

			std::sort(minimums.begin(), minimums.end(), [](const Pair* a, const Pair* b) {
				return a->second < b->second;
			});

			// MAYBE Improve test system: currently requires self-contained copy as result
			std::vector<Pair> res;
			res.reserve(minimums.size());
			for (const Pair* p : minimums)
				res.push_back(*p);

			return res;
		}


		static auto CreateQuery(const std::vector<Pair>& in)
		{
			return Enumerate(in).Where		(FUN(p, p.first > 0))
								.MinimumsBy	(&Pair::first)
								.OrderBy	(&Pair::second);
		}
	};



	struct PositiveMinimums : public PairMinimumsTestBase<true> {
		static constexpr const char Name[] = "Pos. Minimums by field";


		static std::vector<Pair> RunClassic(const std::vector<Pair>& in, size_t resHint = 0)
		{
			std::vector<const Pair*> minimums;
			minimums.reserve(resHint);

			int min = INT_MAX;
			for (const Pair& p : in) {
				if (p.first <= 0 || min < p.first)
					continue;

				if (p.first < min) {
					min = p.first;
					minimums.clear();
				}
				minimums.push_back(&p);
			}

			// MAYBE Improve test system: currently requires self-contained copy as result
			std::vector<Pair> res;
			res.reserve(minimums.size());
			for (const Pair* p : minimums)
				res.push_back(*p);

			return res;
		}


		static auto CreateQuery(const std::vector<Pair>& in)
		{
			return Enumerate(in).Where		(FUN(p, p.first > 0))
								.MinimumsBy	(FUN(p, p.first));
		}
	};


	struct MinSearch : public PairMinimumsTestBase<false> {
		static constexpr const char Name[] = "Minimums by field";


		static std::vector<Pair> RunClassic(const std::vector<Pair>& in, size_t resHint = 0)
		{
			std::vector<Pair> minimums;
			minimums.reserve(resHint);

			int min = INT_MAX;
			for (const Pair& p : in) {
				if (min < p.first)
					continue;

				if (p.first < min) {
					min = p.first;
					minimums.clear();
				}
				minimums.push_back(p);
			}

			return minimums;
		}


		static auto CreateQuery(const std::vector<Pair>& in)
		{
			return Enumerate<Pair>(in).MinimumsBy(&Pair::first);
		}
	};




	struct SumField : public PairTestBase {
		static constexpr const char Name[] = "Sum int field";


		static void MixbackElem(Pair& nextIn, long long res)
		{
			nextIn.first = res % nextIn.first + 100;
		}


		static long long RunClassic(const std::vector<Pair>& in, size_t /*resHint*/ = 0)
		{
			long long s = 0;
			for (const Pair& p : in)
				s += p.first;

			return s;
		}


		static auto CreateQuery(const std::vector<Pair>& in)
		{
			return Enumerate(in).Select(&Pair::first);
		}

		template <class Query>
		static long long Aggregate(const Query& q)
		{
			return q.template Sum<long long>();
		}
	};


	struct SumInts : public ScalarTestBase<int> {
		static constexpr const char Name[] = "Sum simple ints";


		static void MixbackElem(int& nextIn, long long res)
		{
			nextIn = res % nextIn + 100;
		}


		static long long RunClassic(const std::vector<int>& in, size_t /*resHint*/ = 0)
		{
			long long s = 0;
			for (const int& i : in)
				s += i;

			return s;
		}

		static auto CreateQuery(const std::vector<int>& in)
		{
			return Enumerate(in);
		}

		template <class Query>
		static long long Aggregate(const Query& q)
		{
			return q.template Sum<long long>();
		}
	};


	struct SumDoubles : public ScalarTestBase<double> {
		static constexpr const char Name[] = "Naive-sum doubles";

		static void MixbackElem(double& nextIn, double res)
		{
			nextIn = res / nextIn * 100.0;
		}


		static double RunClassic(const std::vector<double>& in, size_t /*resHint*/ = 0)
		{
			double s = 0;
			for (const double& d : in)
				s += d;

			return s;
		}

		static auto CreateQuery(const std::vector<double>& in)
		{
			return Enumerate(in);
		}

		template <class Query>
		static double Aggregate(const Query& q)
		{
			// .Sum() provides compensated summation!
			return q.Aggregate(std::plus<>());
		}
	};


	struct SumDoubles2 : public ScalarTestBase<double> {
		static constexpr const char Name[] = "Compensated Sum doubles";

		static void MixbackElem(double& nextIn, double res)
		{
			nextIn = res / nextIn * 100.0;
		}


		static double RunClassic(const std::vector<double>& in, size_t /*resHint*/ = 0)
		{
			double s   = 0;
			double err = 0;
			for (const double& d : in)
				Enumerables::Def::NeumaierSum2(s, d, err);

			return s + err;
		}

		static auto CreateQuery(const std::vector<double>& in)
		{
			return Enumerate(in);
		}

		template <class Query>
		static double Aggregate(const Query& q)
		{
			return q.Sum();
		}
	};




	template <>		const char Squares<int>::Name[]		= "Square integers";
	template <>		const char Squares<double>::Name[]	= "Square doubles";

	template <>		const char NeighborDiffs<int>::Name[]	 = "Pairwise diffs int";
	template <>		const char NeighborDiffs<double>::Name[] = "Pairwise diffs double";


	// C++17 can get rid of these
	constexpr const char DirectCopy::Name[];
	constexpr const char CopyFromDict::Name[];
	constexpr const char ConversionCopy::Name[];
	constexpr const char SubrangeFiltered::Name[];
	constexpr const char IntSort::Name[];
	constexpr const char IntSort2::Name[];
	constexpr const char OrderByOtherField::Name[];
	constexpr const char FilterByField::Name[];
	constexpr const char ProjectFiltered::Name[];
	constexpr const char PositiveMinimumsOrdered::Name[];
	constexpr const char PositiveMinimums::Name[];
	constexpr const char DblPositiveMinimums::Name[];
	constexpr const char MinSearch::Name[];
	constexpr const char SumField::Name[];
	constexpr const char SumInts::Name[];
	constexpr const char SumDoubles::Name[];
	constexpr const char SumDoubles2::Name[];

#pragma endregion




#pragma region Main Templates

	// common part of sceleton for every perftest
	template <class TestCase, bool UseInterface = false>
	struct Test {
		static constexpr auto&	Name	   = TestCase::Name;
		static constexpr bool	Interfaced = UseInterface;

		using InCollection	= decltype(TestCase::GenerateInput(1u));
		using Res			= decltype(TestCase::RunClassic(std::declval<InCollection>(), 0u));

		static InCollection	GenerateInput(size_t s)							{ return TestCase::GenerateInput(s); }
		static Res			RunClassic(const InCollection& in, size_t s)	{ return TestCase::RunClassic(in, s); }

		template <bool I = UseInterface, std::enable_if_t<!I, int> = 0>
		static auto			CreateQuery(const InCollection& in)		{ return TestCase::CreateQuery(in); }

		template <bool I = UseInterface, std::enable_if_t<I, int> = 0>
		static auto			CreateQuery(const InCollection& in)		{ return TestCase::CreateQuery(in).ToInterfaced(); }

		using Query	= decltype(CreateQuery<UseInterface>(std::declval<InCollection>()));
	};



	// common methods of tests producing vector<R>
	template <class TestCase, bool UseInterface = false>
	struct ListingTest : public Test<TestCase, UseInterface>  {
		using typename Test<TestCase, UseInterface>::InCollection;
		using typename Test<TestCase, UseInterface>::Query;
		using typename Test<TestCase, UseInterface>::Res;
		using ResElem = std::decay_t<Enumerables::TypeHelpers::IterableT<Res>>;

		// Fun fact: this line crashes VS2022 compiler - fine on 2015.
		// using ResElem = typename Res::value_type;


		static std::vector<ResElem>   ExecuteQuery(const Query& q, size_t allocHint)
		{
			return q.ToList(allocHint);
		}


		// prevent optimizations with some turmoil - prepared templates for alternative input kinds
		template <class V>
		static void MixBack(std::vector<V>& nextInput, const std::vector<ResElem>& res)
		{
			TestCase::MixbackElem(nextInput.front(), res[res.size()/2]);
			TestCase::MixbackElem(nextInput[nextInput.size()/2], res.back());
		}

		template <class K, class V>
		static void MixBack(std::unordered_map<K, V>& nextInput, const std::vector<ResElem>& res)
		{
			unsigned i = 0;
			for (auto& kv : nextInput) {
				if (i == 2)
					TestCase::MixbackElem(kv.second, res.front());
				if (i == 4)
					TestCase::MixbackElem(kv.second, res[res.size()/2]);
				if (++i > 4)
					break;
			}
		}
	};


	// common methods of tests aggregating to single scalar - e.g. .Min(), .Aggregate(), ...
	template <class TestCase, bool UseInterface = false>
	struct AggregationTest : public Test<TestCase, UseInterface> {
		using typename Test<TestCase, UseInterface>::InCollection;
		using typename Test<TestCase, UseInterface>::Query;
		using typename Test<TestCase, UseInterface>::Res;


		static Res ExecuteQuery(const Query& q, size_t /*hint*/ = 0)
		{
			return TestCase::Aggregate(q);
		}


		// prevent optimizations with some turmoil - prepared templates for alternative input kinds
		template <class V>
		static void MixBack(std::vector<V>& nextInput, const Res& aggr)
		{
			TestCase::MixbackElem(nextInput.front(), aggr);
			TestCase::MixbackElem(nextInput[nextInput.size()/2], aggr);
		}

		template <class K, class V>
		static void MixBack(std::unordered_map<K, V>& nextInput, const Res& aggr)
		{
			unsigned i = 0;
			for (auto& kv : nextInput) {
				if (++i == 4) {
					TestCase::MixbackElem(*kv->value, aggr);
					break;
				}
			}
		}
	};

#pragma endregion




#pragma region Formatting Utils

	template <class Period>
	std::ostream& operator <<(std::ostream& os, const std::chrono::duration<double, Period>& t)
	{
		if (t.count() > 1000.0)
			os << std::fixed << std::setprecision(0);
		else
			os << std::defaultfloat << std::setprecision(4);

		return os << t.count();
	}



	template <size_t Columns>
	class TableWriter {
		unsigned		col			= 0;
		bool			waitRewrite	= false;

	public:
		const unsigned	Widths[Columns];

		template <class... Args>
		constexpr TableWriter(Args... args) : Widths { (unsigned)args... }
		{
		}


		template <class T, size_t N = 1>
		unsigned PutCell(const T& val, const char (&suffix)[N] = "", bool forRewrite = false)
		{
			if (waitRewrite) {
				// oversimplified - just works for this tiny feedback here...
				for (unsigned i = 0; i < Widths[col]; i++)
					std::cout << '\b';
			}
			if (col == 0) {
				// 0 serves as indentation only
				ASSERT (!waitRewrite);
				std::cout << std::setw(Widths[col++]) << ' ';
			}

			std::cout << std::setw(Widths[col] - N + 1) << val << suffix;
			if (waitRewrite = forRewrite)
				return col;

			const unsigned filled = col++;
			if (col == Columns) {
				std::cout << std::endl;
				col = 0;
			}
			return filled;		// return anything for template below :)
		}


		void CloseRow()
		{
			if (col > 0)
				std::cout << std::endl;
			col = 0;
			waitRewrite = false;
		}


		template <class... T>
		void PutRow(const T&... vals)
		{
			static_assert (sizeof...(T) <= Columns, "Not enough columns defined.");

			std::initializer_list<unsigned> ress { PutCell(vals)... };
			UNUSED (ress);
			CloseRow();
		}
	};

#pragma endregion




#pragma region Runner

	using Enumerables::TypeHelpers::InvokeResultT;

	template <size_t>
	class TableWriter;

	using Microseconds = std::chrono::duration<double, std::micro>;


	// mixes up input in each cycle in attempt to prevent optimizations
	template <class Input, class Query>
	using MixBackFun = void (Input&, const InvokeResultT<Query>&);


	struct PerfResult {
		Microseconds	runtime;
		size_t			allocations;

		PerfResult(Microseconds	runtime = Microseconds::zero(), size_t allocations = 0)
			: runtime { runtime }, allocations { allocations }
		{
				
		}

		bool HasRun() const
		{
			return runtime > Microseconds::zero();
		}

		void operator += (const PerfResult& part)
		{
			runtime		+= part.runtime;
			allocations	+= part.allocations;
		}
	};


	struct PerfComparison {
		std::string		testCase;
		bool			isValidComparison = true;
		PerfResult		classic;
		PerfResult		enumerate;
		PerfResult		constructAndEnumerate;

		Microseconds	EnumerateOverhead() const	{ return enumerate.runtime - classic.runtime; }
		Microseconds	TotalOverhead()		const	{ return constructAndEnumerate.runtime - classic.runtime; }

		double	EnumerateOverheadPercent() const	{ return 100.0 * EnumerateOverhead().count() / classic.runtime.count(); }
		double	TotalOverheadPercent()	   const	{ return 100.0 * TotalOverhead().count() / classic.runtime.count(); }

		PerfComparison& operator +=(const PerfComparison& add)
		{
			classic				  += add.classic;
			enumerate			  += add.enumerate;
			constructAndEnumerate += add.constructAndEnumerate;
			return *this;
		}
	};



	template <class Q, class InpStore>
	static PerfResult	MeasureQuery(Q&& runQuery, MixBackFun<InpStore, Q>& mixback, InpStore& inputStore, InvokeResultT<Q>& lastRes, unsigned cycles)
	{
		using namespace std::chrono;

		AllocationCounter	allocations;
		const auto			startTime  = high_resolution_clock::now();
		auto				firstRes   = runQuery();
		const size_t		allocCount = allocations.Count();		// measured are the allocations of the 1st run
		lastRes = std::move(firstRes);

		for (unsigned c = 1; c < cycles; c++) {
			auto res = runQuery();
			mixback(inputStore, res);
			std::swap(lastRes, res);
		}
		auto cycleTime = (high_resolution_clock::now() - startTime) / cycles;

		return { duration_cast<Microseconds>(cycleTime), allocCount };
	}


	template <class M, size_t C>
	static PerfResult	MeasureBest(unsigned n, TableWriter<C>& tab,  M&& measurer)
	{
		PerfResult best { Microseconds::max(), SIZE_MAX };

		for (unsigned i = 0; i < n; i++) {
			PerfResult res = measurer();
			if (res.runtime < best.runtime)
				best = res;

			ASSERT_EQ (best.allocations, res.allocations);
			bool isTemp = i + 1 < n;
			tab.PutCell(best.runtime, " us", isTemp);
		}
		return best;
	}


	template <class TestCase>
	static PerfComparison RunTestcase(TableWriter<5>& tab, size_t elements, unsigned cycles, size_t allocForResults = 0, unsigned tries = MeasurementCount)
	{
		using Output = typename TestCase::Res;

		std::srand(56);
		std::cout << std::endl << std::left;

		std::string caseName = TestCase::Name;
		caseName +=	  (TestCase::Interfaced && allocForResults) ? " [interfaced, reserve]"
					: (TestCase::Interfaced)					? " [interfaced]"
					: (allocForResults)							? " [reserve]"
					: "";

		tab.PutCell(caseName);
		std::cout << std::right;

		const auto gendInput = TestCase::GenerateInput(elements);

		Output		referenceOutput;
		PerfResult  classicResult = MeasureBest(tries, tab, [&]()
		{
			auto	input    = gendInput;
			auto	testStep = [&]() { return TestCase::RunClassic(input, allocForResults); };
			return	MeasureQuery(testStep, TestCase::MixBack, input, referenceOutput, cycles);
		});

		PerfResult execOnlyResult = MeasureBest(tries, tab, [&]()
		{
			auto	input = gendInput;
			auto	query = TestCase::CreateQuery(input);
			Output	output;

			auto testStep = [&]() { return TestCase::ExecuteQuery(query, allocForResults); };
			PerfResult pr = MeasureQuery(testStep, TestCase::MixBack, input, output, cycles);

			ASSERT_EQ (referenceOutput, output);
			return pr;
		});

		PerfResult createExecResult = MeasureBest(tries, tab, [&]()
		{
			auto		inputStore = gendInput;
			const auto&	constInput = inputStore;
			Output		output;

			auto testStep = [&]() { return TestCase::ExecuteQuery(TestCase::CreateQuery(constInput), allocForResults); };
			PerfResult pr = MeasureQuery(testStep, TestCase::MixBack, inputStore, output, cycles);

			ASSERT_EQ (referenceOutput, output);
			return pr;
		});

		std::cout << std::fixed << std::setprecision(1);
		tab.PutCell("overhead:");
		tab.PutCell('-');
		const Microseconds diffE = execOnlyResult.runtime - classicResult.runtime;
		const Microseconds diffC = createExecResult.runtime - classicResult.runtime;
		tab.PutCell(100.0 * diffE.count() / classicResult.runtime.count(), " %");
		tab.PutCell(100.0 * diffC.count() / classicResult.runtime.count(), " %");
		if (classicResult.allocations > 1 || execOnlyResult.allocations > 1 || createExecResult.allocations > 1) {
			tab.PutCell("mallocs:");
			tab.PutCell(classicResult.allocations);
			tab.PutCell(execOnlyResult.allocations);
			tab.PutCell(createExecResult.allocations);
		}
		bool validComparison = !TestCase::Interfaced;
		return { caseName, validComparison, classicResult, execOnlyResult, createExecResult };
	}

#pragma endregion




	static std::vector<PerfComparison>	RunAllWith(size_t complexity, unsigned cycles)
	{
		std::string cyclesTxt = std::to_string(MeasurementCount) + " * " + std::to_string(cycles);
		std::cout << "    Complexity: " << std::setw(10) << std::left << complexity
				  << " Cycles: " << cyclesTxt << std::endl;

		TableWriter<5> tb { 5, 35, 16, 15, 20 };

		std::cout << std::endl << std::right;
		tb.PutRow("", "Handwritten", "Enumerable", "    Enumerable   ");
		tb.PutRow("", "",			  "  query   ", "construct + query");

		std::vector<PerfComparison> results;

		results.push_back(RunTestcase<ListingTest<DirectCopy>>				(tb, complexity, cycles));
		results.push_back(RunTestcase<ListingTest<DirectCopy, true>>		(tb, complexity, cycles));
		results.push_back(RunTestcase<ListingTest<CopyFromDict>>			(tb, complexity, cycles));
		results.push_back(RunTestcase<ListingTest<CopyFromDict, true>>		(tb, complexity, cycles));
		results.push_back(RunTestcase<ListingTest<ConversionCopy>>			(tb, complexity, cycles));
		results.push_back(RunTestcase<ListingTest<Squares<int>>>			(tb, complexity, cycles));
		results.push_back(RunTestcase<ListingTest<Squares<double>>>			(tb, complexity, cycles));
		results.push_back(RunTestcase<ListingTest<Squares<int>, true>>		(tb, complexity, cycles));
		results.push_back(RunTestcase<ListingTest<Squares<double>, true>>	(tb, complexity, cycles));
		results.push_back(RunTestcase<ListingTest<NeighborDiffs<int>>>		(tb, complexity, cycles));
		results.push_back(RunTestcase<ListingTest<NeighborDiffs<double>>>	(tb, complexity, cycles));

		results.push_back(RunTestcase<ListingTest<FilterByField>>			(tb, complexity, cycles));
		results.push_back(RunTestcase<ListingTest<SubrangeFiltered>>		(tb, complexity, cycles));
		results.push_back(RunTestcase<ListingTest<SubrangeFiltered>>		(tb, complexity, cycles, complexity / 5));
		results.push_back(RunTestcase<ListingTest<ProjectFiltered>>			(tb, complexity, cycles));
		results.push_back(RunTestcase<ListingTest<ProjectFiltered, true>>	(tb, complexity, cycles));

		results.push_back(RunTestcase<ListingTest<IntSort>>					(tb, complexity / 10, cycles));
		results.push_back(RunTestcase<ListingTest<IntSort, true>>			(tb, complexity / 10, cycles));
		results.push_back(RunTestcase<ListingTest<IntSort2>>				(tb, complexity / 10, cycles));
		results.push_back(RunTestcase<ListingTest<OrderByOtherField>>		(tb, complexity / 10, cycles));

		results.push_back(RunTestcase<ListingTest<MinSearch>>				 (tb, complexity, 2 * cycles));
		results.push_back(RunTestcase<ListingTest<MinSearch, true>>			 (tb, complexity, 2 * cycles));
		results.push_back(RunTestcase<ListingTest<PositiveMinimums>>		 (tb, complexity, cycles));   // from refs => needs copy
		results.push_back(RunTestcase<ListingTest<PositiveMinimums, true>>	 (tb, complexity, cycles));
		results.push_back(RunTestcase<ListingTest<DblPositiveMinimums>>		 (tb, complexity, cycles));   // from values => optimized
		results.push_back(RunTestcase<ListingTest<DblPositiveMinimums, true>>(tb, complexity, cycles));
		results.push_back(RunTestcase<ListingTest<PositiveMinimumsOrdered>>	 (tb, complexity, cycles));

		results.push_back(RunTestcase<AggregationTest<SumField>>	(tb, complexity, 2 * cycles));
		results.push_back(RunTestcase<AggregationTest<SumInts>>		(tb, complexity, 4 * cycles));
		results.push_back(RunTestcase<AggregationTest<SumDoubles>>	(tb, complexity, cycles));
		results.push_back(RunTestcase<AggregationTest<SumDoubles2>>	(tb, complexity, cycles));

		return results;
	}


	// csv-like output to diff/collect
	static void							PrintCompact(const std::vector<PerfComparison>& res)
	{
		TableWriter<6> tb { 5, 37, 15, 17, 17, 13 };

		std::cout << std::left;
		tb.PutRow("Testcase", "  Baseline [us]", "  Query ovrhd [%]", "  Total ovrhd [%]", "  Malloc diff");

		PerfComparison sum { "TOTALs" };

		for (const PerfComparison& r : res) {
			std::cout << std::left;
			std::string quoted { '"' };
			quoted += r.testCase;
			quoted += '"';
			tb.PutCell(quoted);
			std::cout << std::right;

			tb.PutCell(r.classic.runtime);

			std::cout << std::fixed << std::setprecision(1);
			tb.PutCell(r.EnumerateOverheadPercent());
			tb.PutCell(r.TotalOverheadPercent());

			auto extraMallocs = (long long)r.constructAndEnumerate.allocations
							  - (long long)r.classic.allocations;

			// no showpos for ints?
			std::cout << std::setprecision(0) << (extraMallocs ? std::showpos : std::noshowpos);
			tb.PutCell((double)extraMallocs);
			std::cout << std::noshowpos;

			if (r.isValidComparison)
				sum += r;
		}
		std::cout << std::endl;

		using namespace std::chrono;
		using Millis = duration<double, std::milli>;

		auto extraMallocs = (long long)sum.constructAndEnumerate.allocations
						  - (long long)sum.classic.allocations;

		tb.PutCell("ALL comparable:");
		tb.PutCell(duration_cast<Millis>(sum.classic.runtime),				 " ms");
		tb.PutCell(duration_cast<Millis>(sum.enumerate.runtime),			 " ms");
		tb.PutCell(duration_cast<Millis>(sum.constructAndEnumerate.runtime), " ms");
		std::cout << std::fixed << std::setprecision(0) << (extraMallocs ? std::showpos : std::noshowpos);
		tb.PutCell((double)extraMallocs);

		std::cout << std::noshowpos << std::fixed << std::setprecision(1);
		tb.PutCell("Overhead:");
		tb.PutCell("-");
		tb.PutCell(sum.EnumerateOverheadPercent(), " %");
		tb.PutCell(sum.TotalOverheadPercent(), " %");
		tb.CloseRow();
	}



	void NewPerfTests()
	{
		Greet(GreetTxt);
		Greet("  Long sequences...");
		auto results1 = RunAllWith(DefaultComplexity, DefaultCycles);
		std::cout << std::endl;

		Greet("  ---------------------------------------------------------------------------------------");
		Greet("  Short sequences...");
		auto results2 = RunAllWith(10, DefaultComplexity / 50 * DefaultCycles);
		std::cout << std::endl;

		Greet("  ====================================================================================================");
		Greet("  Long sequences summary:");
		PrintCompact(results1);
		std::cout << std::endl;
		Greet("  ----------------------------------------------------------------------------------------------------");
		Greet("  Short sequences summary:");
		PrintCompact(results2);
	}

}	// namespace EnumerableTests
