#pragma once

#include "TestCompileSetup.hpp"
#include <iostream>
#include <utility>


// Workaround for <T1, T2> or { a, b } in macro calls
#define COMMA , 

#define ASSERT_TYPE(T, expr)		static_assert (std::is_same<T, decltype(expr)>(), "Type assertion failed.")

#define ASSERT_ELEM_TYPE(T, eb)		static_assert (std::is_same<T, decltype(eb.First())>() &&		\
												   std::is_same<T, decltype(eb)::TElem>() &&		\
												   std::is_same<T, decltype(*eb.begin())>()  ,		\
												   "Assertion of enumerated type failed!"	 )

// let's stay oversimplified here
#define ASSERT(cond)		::EnumerableTests::Assert((cond), #cond, __FILE__, __LINE__)
#define ASSERT_EQ(x, y)		::EnumerableTests::AssertEq(x, y, #x " == " #y, __FILE__, __LINE__)
#define NO_MORE_HEAP		::EnumerableTests::AllocationGuard forbidHeap(0, __FILE__, __LINE__)

#define ASSERT_THROW(Ex, xpr)	try {														\
									::EnumerableTests::DisableClientBreaks dontAssert;		\
									xpr;													\
									ASSERT (false); 										\
								}															\
								catch (const Ex&) {}										\
								(void)0

// disable allocation checks if running with ResultsView for debug
#define RESULTSVIEW_DISABLES_ALLOCASSERTS													\
	::EnumerableTests::AllocationCounter::EnableAsserts = !ENUMERABLES_USE_RESULTSVIEW		\
													   || !ENUMERABLES_RESULTSVIEW_AUTO_EVAL



namespace EnumerableTests {

	std::pair<bool, std::string>  FindCmdOption(char letter, int argc, const char* argv[]);

	void Greet(const char* testName);

	void PrintFail	(const char* errorTxt, const char* file, long line);
	void AskForBreak(const char* errorTxt, const char* file, long line);

	void MaskableClientBreak(const char* errorTxt);

	struct DisableClientBreaks {
		DisableClientBreaks()  noexcept;
		~DisableClientBreaks() noexcept;
	};

	template <class A, class B>
	bool AssertEq(const A& a, const B& b, const char* txt, const char* file, long line);
	bool Assert  (bool  cond,			  const char* txt, const char* file, long line);

	template <class S1, class S2>
	bool EqualSets(const S1& lhs, const S2& rhs);



	// -----------------------------------------------------------------------------------

	class AllocationCounter	{
		friend void* ::operator new(size_t);

		thread_local static size_t	globalCount;
		size_t						myStart;

	public:
		static bool					EnableAsserts;		// global masking for ResultsView
		
		AllocationCounter();

		size_t Count() const		{ return globalCount - myStart; }
		void   Reset()				{ myStart = globalCount;		}
		
		void AssertFreshCount(size_t		expected,
							  const char*	file   = "unspecified",
							  long			line   = 0			  );
		
		void AssertMaxFreshCount(size_t		 maxExpected,
							     const char* file   = "unspecified",
							     long		 line   = 0			   );
	};



	struct AllocationGuard {
		const char* const	file;
		const long			line;

	public:
		const size_t		Expected;
		AllocationCounter	counter;

		explicit AllocationGuard(size_t			expect = 0,
								 const char*	file   = "unspecified",
								 long			line   = 0)
		: Expected { expect }, file { file }, line { line }
		{
		}

		~AllocationGuard()
		{
			counter.AssertFreshCount(Expected, file, line);
		}
	};



	// -----------------------------------------------------------------------------------

	template <class T>
	struct MoveOnly final {
		T			data;
		unsigned	moveCount = 0;

		const T&	operator *() const	{ return data; }
		T&			operator *()		{ return data; }

		template <class In>
		MoveOnly(In&& init) : data { std::forward<In>(init) } {}
		MoveOnly(const MoveOnly&) = delete;
		MoveOnly(MoveOnly&& src) noexcept(noexcept(T(std::move(src.data)))) :
			data	  { std::move(src.data) },
			moveCount { src.moveCount + 1 }
		{
		}
	};
	

	template <class T>
	struct CountedCopy final {
		T			data;
		unsigned	copyCount = 0;
		unsigned	moveCount = 0;		// preserved cumulatively

		const T&	operator *() const	{ return data; }
		T&			operator *()		{ return data; }

		template <class In, class = std::enable_if_t<!std::is_same<In&, CountedCopy&>::value>>
		CountedCopy(In&& init) : data { std::forward<In>(init) } {}

		CountedCopy(const CountedCopy& src) noexcept(noexcept(T(src.data))) :
			data	  { src.data },
			copyCount { src.copyCount + 1 },
			moveCount { src.moveCount }
		{
		}

		CountedCopy(CountedCopy&& src) noexcept(noexcept(T(std::move(src.data)))) :
			data	  { std::move(src.data) },
			copyCount { src.copyCount },
			moveCount { src.moveCount + 1 }
		{
		}
	};


	template <class N>
	struct Vector2D {
		N	x, y;

		Vector2D() : x {}, y {}
		{
		}

		Vector2D(const N& x, const N& y) : x { x }, y { y }
		{
		}

		bool operator ==(const Vector2D& rhs) const
		{
			return x == rhs.x && y == rhs.y;
		}

		bool operator !=(const Vector2D& rhs) const		{ return !operator==(rhs); }

		Vector2D operator +(const Vector2D& rhs) const	{ return { x + rhs.x, y + rhs.y }; }
		Vector2D operator -(const Vector2D& rhs) const	{ return { x - rhs.x, y - rhs.y }; }
		Vector2D operator *(double c)			 const	{ return { x * c,     y * c }; }

		Vector2D DirectionTo(const Vector2D& trg) const { return trg - *this; }
	};


	// ===================================================================================

	template <class T>
	auto PrintObj(const char* name, const T& obj) -> decltype(std::cout << obj)
	{
		return std::cout << name << obj << std::endl;
	}

	template <class T>
	void PrintObj(const void*, const T&)			// SFINAE fallback
	{
	}


	template <class A, class B>
	auto PrintDiff(const A& a, const B& b)
	{
		if constexpr (std::is_floating_point<B>() && std::is_convertible<A, B>())
			PrintObj("\tDiff =      ",  b - a);
	}


	template <class A, class B>
	bool AssertEq(const A& a, const B& b, const char* txt, const char* file, long line)
	{
		if (a == b)
			return true;

		PrintFail(txt, file, line);
		PrintObj("\tExpected =  ", a);
		PrintObj("\tActual   =  ", b);
		PrintDiff(a, b);
		AskForBreak(txt, file, line);
		return false;
	}


	template <class S1, class S2>
	bool EqualSets(const S1& lhs, const S2& rhs)
	{
		if (lhs.size() != rhs.size())
			return false;

		for (auto& el : rhs) {
			if (lhs.find(el) == lhs.end())
				return false;
		}
		for (auto& el : lhs) {
			if (rhs.find(el) == rhs.end())
				return false;
		}
		return true;
	}

}	// namespace EnumerableTests



namespace std {

	template <class F, class S>
	struct hash<pair<F, S>> {
		size_t operator ()(const pair<F, S>& p) const
		{
			hash<F> h1;
			hash<S> h2;
			return 5 * h1(p.first) + h2(p.second);
		}
	};


	template <class T>
	struct hash<EnumerableTests::Vector2D<T>> {
		size_t operator ()(const EnumerableTests::Vector2D<T>& v) const {
			hash<T> hash;
			return (hash(v.x) << 6) + hash(v.y);
		}
	};

}	// namespace std
