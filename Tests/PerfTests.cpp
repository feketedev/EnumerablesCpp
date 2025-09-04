
#include "Tests.hpp"
#include "TestUtils.hpp"
#include "Enumerables.hpp"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <ranges>
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


		static auto CreateRange(const std::vector<Pair>& in)
		{
			return in | std::views::all;
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


		static auto CreateRange(const std::vector<int>& in)
		{
			return in | std::views::transform([](int i) -> double { return i; });
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


		static auto CreateRange(const std::unordered_map<int, char>& in)
		{
			return in | std::views::values;
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


		static auto CreateRange(const std::vector<N>& in)
		{
			return in | std::views::transform([](N n) { return n * n; });
		}


		static auto CreateQuery(const std::vector<N>& in)
		{
			return Enumerate(in).Map(FUN(x,  x * x));
		}
	};

	template <>		const char Squares<int>::Name[]		= "Square integers";
	template <>		const char Squares<double>::Name[]	= "Square doubles";



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


		static auto CreateRange(const std::vector<int>& in)
		{
			const size_t s = SkipCount(in.size());
			const size_t t = TakenLen(in.size());

			return in | std::views::filter(&IsAccepted)
					  | std::views::drop(s)
					  | std::views::take(t);
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


		static auto CreateRange(const std::vector<int>& in)
		{
			// Not fully equivalent - works eagerly, like Classic
			std::vector<int> copy = in;
			std::ranges::sort(copy);
			return copy;
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


		static auto CreateRange(const std::vector<double>& in)
		{
			auto accepted = in | std::views::filter([](double x) { return x > 0; });

			// Not fully equivalent - not a lazy/updateable view from here
			auto minVal = std::ranges::min(accepted);
			return in | std::views::filter([=](double x) { return x == minVal; });
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

#if CPP23_ENABLED
		static auto CreateRange(const std::vector<N>& in)
		{
			return in | std::views::pairwise_transform([](auto&& p, auto&& n) { 
							return n - p;
						});
		}
#endif

		static auto CreateQuery(const std::vector<N>& in)
		{
			return Enumerate(in).MapNeighbors(FUN(p,n,  n - p));
		}
	};

	template <>		const char NeighborDiffs<int>::Name[]	 = "Pairwise diffs int";
	template <>		const char NeighborDiffs<double>::Name[] = "Pairwise diffs double";
	


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


		static auto CreateRange(const std::vector<Pair>& in)
		{
			// Not fully equivalent - can't sort within pipeline
			auto copy = in;
			std::ranges::sort(copy, std::less<>(), &Pair::first);
			return std::move(copy) | std::views::values;
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


		static auto CreateRange(const std::vector<Pair>& in)
		{
			return in | std::views::filter([](const Pair& p) { return p.first > 0; });
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

		
		static auto CreateRange(const std::vector<Pair>& in)
		{
			return in | std::views::filter([](const Pair& p) {
							return (p.second + p.first) % 3 == 0;
						})
					  | std::views::transform(&Pair::second);
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


		static auto CreateRange(const std::vector<Pair>& in)
		{
			// Not fully equivalent - not a lazy/updateable view
			auto filtered = in | std::views::filter([](const Pair& p) { return p.first > 0; });
			int  min	  = std::ranges::min(filtered | std::views::transform(&Pair::first));
			auto wanted	  = filtered | std::views::filter([=](const Pair& p) { return p.first == min; });
			
			std::vector<Pair> sorted (wanted.begin(), wanted.end());
			std::ranges::sort(sorted, std::less<>(), &Pair::second);
			return sorted;
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


		static auto CreateRange(const std::vector<Pair>& in)
		{
			// Not fully equivalent - not a lazy/updateable view
			auto filtered = in | std::views::filter([](const Pair& p) { return p.first > 0; });
			int  min	  = std::ranges::min(filtered | std::views::transform(&Pair::first));
			return filtered | std::views::filter([=](const Pair& p) { return p.first == min; });
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

		
		static auto CreateRange(const std::vector<Pair>& in)
		{
			// Not fully equivalent - not a lazy/updateable view
			int min = std::ranges::min(in, std::less<>(), &Pair::first).first;
			return in | std::views::filter([=](const Pair& p) { return p.first == min; });
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


#if CPP23_ENABLED
		static auto& CreateRange(const std::vector<Pair>& in)
		{
			return in;
		}

		static auto AggregateRange(const std::vector<Pair>& in)
		{
			return std::ranges::fold_left(in | std::views::transform(&Pair::first),
										  0,
										  std::plus<>());
		}
#endif
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


#if CPP23_ENABLED
		static auto& CreateRange(const std::vector<int>& in)
		{
			return in;
		}

		static auto AggregateRange(const std::vector<int>& in)
		{
			return std::ranges::fold_left(in, 0, std::plus<>());
		}
#endif
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


#if CPP23_ENABLED
		static auto AggregateRange(const std::vector<double>& in)
		{
			return std::ranges::fold_left(in, 0, std::plus<>());
		}

		static auto& CreateRange(const std::vector<double>& in)
		{
			return in;
		}
#endif
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

#pragma endregion




#pragma region Main Templates

	using Enumerables::TypeHelpers::None;


	// common part of sceleton for every perftest
	template <class TestCase, bool UseInterface = false>
	struct Test {
		static constexpr auto&	Name	   = TestCase::Name;
		static constexpr bool	Interfaced = UseInterface;

		using InCollection	= decltype(TestCase::GenerateInput(1u));
		using Res			= decltype(TestCase::RunClassic(std::declval<InCollection>(), 0u));

		static constexpr bool	HasRanges  = requires (const InCollection& in) { TestCase::CreateRange(in); };

		static InCollection	GenerateInput(size_t s)									{ return TestCase::GenerateInput(s);  }
		static Res			RunClassic(const InCollection& in, size_t s)			{ return TestCase::RunClassic(in, s); }
		static auto			CreateRange(const InCollection& in)	requires(HasRanges)	{ return TestCase::CreateRange(in);   }
		static None			CreateRange(const InCollection& in)	requires(!HasRanges);
		static auto			CreateQuery(const InCollection& in)
		{
			if constexpr (UseInterface)		return TestCase::CreateQuery(in).ToInterfaced();
			else							return TestCase::CreateQuery(in);
		}

		using Range = decltype(CreateRange(std::declval<InCollection>()));
		using Query = decltype(CreateQuery(std::declval<InCollection>()));
	};



	// common methods of tests producing vector<R>
	template <class TestCase, bool UseInterface = false>
	struct ListingTest : public Test<TestCase, UseInterface>  {
		using typename Test<TestCase, UseInterface>::InCollection;
		using typename Test<TestCase, UseInterface>::Query;
		using typename Test<TestCase, UseInterface>::Range;
		using typename Test<TestCase, UseInterface>::Res;
		using ResElem = std::decay_t<Enumerables::TypeHelpers::IterableT<Res>>;

		using Test<TestCase, UseInterface>::HasRanges;

		// Fun fact: this line crashes VS2022 compiler - fine on 2015.
		// using ResElem = typename Res::value_type;


		static std::vector<ResElem>   ExecuteRange(Range&& r, size_t allocHint) requires(HasRanges)
		{
			// some algorithms require a copy by their own (e.g. sort), avoid duplicated copy
			if constexpr (std::is_same<Range, std::vector<ResElem>>()) {
				return std::move(r);
			}

			using std::ranges::size;

			std::vector<ResElem> v;

			// Mimic having proper ranges::to<std::vector> from C++23
			if constexpr (requires { size(r); })
				v.reserve(std::max(size(r), allocHint));
			else
				v.reserve(allocHint);
			
			// Sticking to conventional iteration!
			for (auto&& elem : r)
				v.push_back(elem);

			// NOTE: Interesting performance deviations:
			//		 The below form leads to great improvement for vector-based inputs [even 80%!],
			//		 but serious drops [2x time] with more composed algorithms (along with allocations)!
			//
			//	auto adapted = r | std::views::common;
			//	v.assign(adapted.begin(), adapted.end());

			return v;
		}


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
		using typename Test<TestCase, UseInterface>::Range;
		using typename Test<TestCase, UseInterface>::Res;

		using Test<TestCase, UseInterface>::HasRanges;


		static Res ExecuteQuery(const Query& q, size_t /*hint*/ = 0)
		{
			return TestCase::Aggregate(q);
		}

		static Res ExecuteRange(Range&& r, size_t /*hint*/ = 0) requires(HasRanges)
		{
			return TestCase::AggregateRange(std::forward<Range>(r));
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
		std::ostream&	output;
		const char		separator;
		const unsigned	widths[Columns];

		template <class... Args>
		constexpr TableWriter(std::ostream& out, Args... args) :
			TableWriter { out.fill(), out, args...}
		{
		}


		template <class... Args>
		constexpr TableWriter(char sep, std::ostream& out, Args... args) :
			output	  { out },
			separator { sep },
			widths	  { (unsigned)args... }
		{
		}


		template <class T, size_t N = 1>
		unsigned PutCell(const T& val, const char (&suffix)[N] = "", bool forRewrite = false)
		{
			if (waitRewrite) {
				// oversimplified - just works for this tiny feedback here...
				for (unsigned i = 0; i < widths[col]; i++)
					output << '\b';
			}
			if (col == 0) {
				// 0 serves as indentation only
				ASSERT (!waitRewrite);
				output << std::setw(widths[col++]) << "";
			}

			output << std::setw(widths[col] - N + 1) << val << suffix;
			if (waitRewrite = forRewrite)
				return col;

			const unsigned filled = col++;
			if (col == Columns) {
				output << std::endl;
				col = 0;
			}
			else {
				output << separator;
			}
			return filled;
		}


		void CloseRow()
		{
			if (col > 0)
				output << std::endl;
			col = 0;
			waitRewrite = false;
		}


		template <class... T>
		void PutRow(const T&... vals)
		{
			static_assert (sizeof...(T) <= Columns, "Not enough columns defined.");

			(..., PutCell(vals));
			CloseRow();
		}
	};


	static void SectionBreak(const char* title, unsigned hrWidth, char hrSymbol = '-')
	{
		std::string hr = "  ";
		hr.append(hrWidth, hrSymbol);
		Greet(hr.c_str());			// preserve base indentation
		Greet(title);
	}

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
		Microseconds	runtime		= Microseconds::zero();
		size_t			allocations = 0;

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
		PerfResult		ranges;
		PerfResult		enumerate;
		PerfResult		constructAndEnumerate;

		Microseconds	RangesOverhead()	const	{ return ranges.runtime - classic.runtime; }
		Microseconds	EnumerateOverhead() const	{ return enumerate.runtime - classic.runtime; }
		Microseconds	TotalOverhead()		const	{ return constructAndEnumerate.runtime - classic.runtime; }

		double	RangesOverheadPercent()	   const	{ return 100.0 * RangesOverhead().count() / classic.runtime.count(); }
		double	EnumerateOverheadPercent() const	{ return 100.0 * EnumerateOverhead().count() / classic.runtime.count(); }
		double	TotalOverheadPercent()	   const	{ return 100.0 * TotalOverhead().count() / classic.runtime.count(); }

		PerfComparison& operator +=(const PerfComparison& add)
		{
			classic				  += add.classic;
			ranges				  += add.ranges;
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
	static PerfComparison RunTestcase(TableWriter<6>& tab, size_t elements, unsigned cycles, size_t allocForResults = 0, unsigned tries = MeasurementCount)
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

		PerfResult  rangesResult {};
		if constexpr (TestCase::HasRanges && !TestCase::Interfaced) {
			rangesResult = MeasureBest(tries, tab, [&]()
			{
				auto	input = gendInput;
				Output	output;

				auto testStep = [&]() { return TestCase::ExecuteRange(TestCase::CreateRange(input), allocForResults); };
				PerfResult pr = MeasureQuery(testStep, TestCase::MixBack, input, output, cycles);

				ASSERT_EQ (referenceOutput, output);
				return pr;
			});
		}
		else {
			tab.PutCell("-");
		}

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
		const Microseconds diffR = rangesResult.runtime - classicResult.runtime;
		const Microseconds diffE = execOnlyResult.runtime - classicResult.runtime;
		const Microseconds diffC = createExecResult.runtime - classicResult.runtime;
		if (rangesResult.runtime > Microseconds::zero())
			tab.PutCell(100.0 * diffR.count() / classicResult.runtime.count(), " %");
		else
			tab.PutCell("-");
		tab.PutCell(100.0 * diffE.count() / classicResult.runtime.count(), " %");
		tab.PutCell(100.0 * diffC.count() / classicResult.runtime.count(), " %");
		if (classicResult.allocations > 1 || rangesResult.allocations > 1 || execOnlyResult.allocations > 1 || createExecResult.allocations > 1) {
			tab.PutCell("mallocs:");
			tab.PutCell(classicResult.allocations);
			if (rangesResult.HasRun())
				tab.PutCell(rangesResult.allocations);
			else
				tab.PutCell('-');
			tab.PutCell(execOnlyResult.allocations);
			tab.PutCell(createExecResult.allocations);
		}
		bool validComparison = !TestCase::Interfaced;
		return { caseName, validComparison, classicResult, rangesResult, execOnlyResult, createExecResult };
	}

#pragma endregion




#pragma region Summaries

	static TableWriter<7>  BeginSummaryTab()
	{
		TableWriter<7> tb { std::cout, 11, 17, 13, 12, 16, 19, 13 };
		tb.output << std::right;
		tb.PutRow("", "Handwritten", "Ranges", "Repeated Query", "Construct + Query", "Malloc diff");
		return tb;
	}


	static TableWriter<7>  BeginCompactTab(bool showOverheads)
	{
		TableWriter<7> tb { std::cout, 5, 37, 15, 17, 16, 16, 12 };

		std::string leftpadHead1 = "Testcase";
		leftpadHead1.append(tb.widths[1] - leftpadHead1.size(), tb.output.fill());

		tb.output << std::right;
		if (showOverheads)
			tb.PutRow(leftpadHead1, "Baseline [us]", "Ranges ovrhd [%]", "Query ovrhd [%]", "Total ovrhd [%]", "Malloc diff");
		else
			tb.PutRow(leftpadHead1, "Baseline [us]", "Ranges time [us]", "Query time [us]", "Total time [us]", "Malloc diff");

		return tb;
	}


	static TableWriter<7>  BeginCsv(std::ostream& output, bool showOverheads)
	{
		TableWriter<7> tb { ',', output, 0, 0, 0, 0, 0, 0, 0 };

		if (showOverheads)
			tb.PutRow("Testcase", R"("Baseline [us]")", R"("Ranges ovrhd [%]")", R"("Query ovrhd [%]")", R"("Total ovrhd [%]")", R"("Malloc diff")");
		else
			tb.PutRow("Testcase", R"("Baseline [us]")", R"("Ranges time [us]")", R"("Query time [us]")", R"("Total time [us]")", R"("Malloc diff")");

		return tb;
	}


	static void PrintSummary(TableWriter<7>& tb, const std::vector<PerfComparison>& res)
	{
		using namespace std::chrono;
		using Millis = duration<double, std::milli>;

		PerfComparison sum		 { "TOTALs" };
		PerfComparison rangesSum { "Ranges TOTALs" };

		for (const PerfComparison& r : res) {
			if (r.isValidComparison)
				sum += r;
			if (r.ranges.HasRun())
				rangesSum += r;
		}

		auto extraMallocs = (long long)sum.constructAndEnumerate.allocations
						  - (long long)sum.classic.allocations;

		auto extraMallocsRangesCompare = (long long)rangesSum.constructAndEnumerate.allocations
									   - (long long)rangesSum.classic.allocations;

		auto extraMallocsRanges = (long long)rangesSum.constructAndEnumerate.allocations
								- (long long)rangesSum.ranges.allocations;

		tb.PutCell("ALL comparable:");
		tb.PutCell(duration_cast<Millis>(sum.classic.runtime),				 " ms");
		tb.PutCell("n/a");
		tb.PutCell(duration_cast<Millis>(sum.enumerate.runtime),			 " ms");
		tb.PutCell(duration_cast<Millis>(sum.constructAndEnumerate.runtime), " ms");
		tb.output << std::fixed << std::setprecision(0) << (extraMallocs ? std::showpos : std::noshowpos);
		tb.PutCell((double)extraMallocs);

		tb.output << std::noshowpos << std::fixed << std::setprecision(1);
		tb.PutCell("Overhead:");
		tb.PutCell("-");
		tb.PutCell("n/a");
		tb.PutCell(sum.EnumerateOverheadPercent(), " %");
		tb.PutCell(sum.TotalOverheadPercent(), " %");
		tb.CloseRow();

		tb.output << std::noshowpos;
		tb.PutCell("ALL with ranges:");
		tb.PutCell(duration_cast<Millis>(rangesSum.classic.runtime),			   " ms");
		tb.PutCell(duration_cast<Millis>(rangesSum.ranges.runtime),				   " ms");
		tb.PutCell(duration_cast<Millis>(rangesSum.enumerate.runtime),			   " ms");
		tb.PutCell(duration_cast<Millis>(rangesSum.constructAndEnumerate.runtime), " ms");
		tb.output << std::fixed << std::setprecision(0) << (extraMallocs ? std::showpos : std::noshowpos);
		tb.PutCell((double)extraMallocsRangesCompare);

		tb.output << std::noshowpos << std::fixed << std::setprecision(1);
		tb.PutCell("Overhead:");
		tb.PutCell("-");
		tb.PutCell(rangesSum.RangesOverheadPercent(), " %");
		tb.PutCell(rangesSum.EnumerateOverheadPercent(), " %");
		tb.PutCell(rangesSum.TotalOverheadPercent(), " %");
		tb.CloseRow();

		tb.PutCell("Against ranges:");
		tb.PutCell(-100.0 * rangesSum.RangesOverhead().count() / rangesSum.ranges.runtime.count(), " %");
		tb.PutCell("-");
		tb.PutCell(100.0 * (rangesSum.EnumerateOverhead() - rangesSum.RangesOverhead()).count() / rangesSum.ranges.runtime.count(), " %");
		tb.PutCell(100.0 * (rangesSum.TotalOverhead() - rangesSum.RangesOverhead()).count() / rangesSum.ranges.runtime.count(), " %");
		std::cout << std::fixed << std::setprecision(0) << (extraMallocsRanges ? std::showpos : std::noshowpos);
		tb.PutCell((double)extraMallocsRanges);
		std::cout << std::noshowpos << std::setprecision(1);
	}


	static void PrintCompact(TableWriter<7>& tb, const std::vector<PerfComparison>& res, bool showOverheads, const char* groupPrefix = nullptr)
	{
		for (const PerfComparison& r : res) {
			tb.output << std::left;
			std::string quoted { '"' };
			if (groupPrefix)
				quoted += groupPrefix;
			quoted += r.testCase;
			quoted += '"';
			tb.PutCell(quoted);
			tb.output << std::right;

			tb.PutCell(r.classic.runtime);

			if (showOverheads) {
				tb.output << std::fixed << std::setprecision(1);
				if (r.ranges.HasRun())
					tb.PutCell(r.RangesOverheadPercent());
				else
					tb.PutCell("-");
				tb.PutCell(r.EnumerateOverheadPercent());
				tb.PutCell(r.TotalOverheadPercent());
			}
			else {
				if (r.ranges.HasRun())
					tb.PutCell(r.ranges.runtime);
				else
					tb.PutCell("-");
				tb.PutCell(r.enumerate.runtime);
				tb.PutCell(r.constructAndEnumerate.runtime);
			}

			auto extraMallocs = (long long)r.constructAndEnumerate.allocations
							  - (long long)r.classic.allocations;

			// no showpos for ints?
			tb.output << std::setprecision(0) << (extraMallocs ? std::showpos : std::noshowpos);
			tb.PutCell((double)extraMallocs);
			tb.output << std::noshowpos;
		}
	}


	static void SummarizeOnScreen(bool showTimes, bool showOvrhds, const std::vector<PerfComparison>& results)
	{
		bool details = (showTimes || showOvrhds);
		auto tabWriter = details ? BeginCompactTab(showOvrhds) : BeginSummaryTab();
		if (details) {
			PrintCompact(tabWriter, results, showOvrhds);
			std::cout << std::endl;
		}
		PrintSummary(tabWriter, results);
	}


	static void SummarizeCsv(bool showOvrhds, const std::string& path, const std::vector<PerfComparison>& longResults,
																	   const std::vector<PerfComparison>& shortResults)
	{
		std::cout << "  Saving results to '" << path << "'.";
		std::ofstream file { path, std::ios::out };
		if (!file) {
			std::cerr << "Cannot write to file!" << std::endl;
			return;
		}
		auto tabWriter = BeginCsv(file, showOvrhds);
		PrintCompact(tabWriter, longResults,  showOvrhds, "Long - ");
		PrintCompact(tabWriter, shortResults, showOvrhds, "Short - ");
		file.close();
		if (file)
			std::cout << std::endl;
		else
			std::cerr << "Error writing to file." << std::endl;
	}

#pragma endregion



	static std::vector<PerfComparison>	RunAllWith(size_t complexity, unsigned cycles)
	{
		std::string cyclesTxt = std::to_string(MeasurementCount) + " * " + std::to_string(cycles);
		std::cout << "    Complexity: " << std::setw(10) << std::left << complexity
				  << " Cycles: " << cyclesTxt << std::endl;

		TableWriter<6> tb { std::cout, 5, 35, 12, 12, 14, 20 };

		std::cout << std::right;
		tb.PutRow("", "Handwritten", "Ranges", "Enumerable", "    Enumerable   ");
		tb.PutRow("", "",			 "",	   "  query   ", "construct + query");

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


	void NewPerfTests(int argc, const char* argv[])
	{
		auto [sumTimes, timesPath]     = FindCmdOption('T', argc, argv);
		auto [sumOverheads, ovrhdPath] = FindCmdOption('O', argc, argv);
		
		bool printTimes = sumTimes && timesPath.empty();
		bool printOvrhd = sumOverheads && ovrhdPath.empty() && !printTimes;

		Greet(GreetTxt);
		SectionBreak("  Long sequences...", 98);
		auto results1 = RunAllWith(DefaultComplexity, DefaultCycles);
		std::cout << std::endl;

		SectionBreak("  Short sequences...", 98);
		auto results2 = RunAllWith(10, DefaultComplexity / 50 * DefaultCycles);
		std::cout << std::endl;
		
		SectionBreak("  Long sequences summary:", printTimes || printOvrhd ? 119 : 102, '=');
		SummarizeOnScreen(printTimes, printOvrhd, results1);
		std::cout << std::endl;
		SectionBreak("  Short sequences summary:", printTimes || printOvrhd ? 119 : 102);
		SummarizeOnScreen(printTimes, printOvrhd, results2);
		std::cout << std::endl;

		if (!timesPath.empty())
			SummarizeCsv(false, timesPath, results1, results2);
		if (!ovrhdPath.empty())
			SummarizeCsv(true, ovrhdPath, results1, results2);
	}

}	// namespace EnumerableTests
