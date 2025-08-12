#ifndef ENUMERABLES_INTERFACE_HPP
#define ENUMERABLES_INTERFACE_HPP

	/*  --------------------------------------------------------------------------------------------------------  *
	 *  									Enumerables for C++   v2.0.3-17										  *
	 *  																										  *
	 *  A  [less and less]  rudimentary attempt to introduce LINQ-style evaluation of collections to C++, along	  *
	 *  with the fruits of declarative reasoning.																  *
	 *  																										  *
	 *  Copyright 2024 Norbert Fekete 							   Released under MIT licencse [see LICENSE.txt]  *
	 *  --------------------------------------------------------------------------------------------------------  *
	 *  																										  *
	 *  								[Outdated but fun nostalgic disclaimer]									  *
	 *  																										  *
	 *  Similar concepts are spreading across various languages, including:										  *
	 *  - .Net LINQ:         https://msdn.microsoft.com/en-us/library/bb308959.aspx								  *
	 *  - Java Streams:      http://www.oracle.com/technetwork/articles/java/ma14-java-se-8-streams-2177646.html  *
	 *  - D ranges (Phobos): https://dlang.org/phobos/std_range.html											  *
	 *  																										  *
	 *  Noting one interesting implementational difference here:												  *
	 *  .Net's IEnumerable<T> is a factory that creates IEnumerator<T> objects, while a D range is rather like	  *
	 *  an iterator-pair, needs to be cloned explicitly in case the iteration is required multiple times.		  *
	 *  The reason might be easier compiler optimizations, but for now we're going with the factory variant to	  *
	 *  face less unexpected errors - while on the level of Enumerators, still blend templates with interfaces,	  *
	 *  similarly to Phobos.																					  *
	 *  																										  *
	 *  In case of warning 4503 encountered:																	  *
	 *  	- It's safe to issue a warning(disable : 4503) in your cpp											  *
	 *  	- In headers it can be avoided by breaking the chain of nested templated							  *
	 *  	  Enumerators at some points using their interfaces (see .ToInterfaced()).							  *
	 *  																										  *
	 *  --------------------------------------------------------------------------------------------------------  *
	 *  This file contains the main interface of Enumerables, consisting of:									  *
	 *  	- class AutoEnumerable<F>																			  *
	 *  		The generic template implementation, to be used only with implicit typing (auto).				  *
	 *  	- typedef for its interfaced subcase: Enumerable<T>													  *
	 *  		To be used as explicitly, like in C#. Provides type-erasure, applicable for interfaces.			  *
	 *  	- free creator functions																			  *
	 *  		To wrap containers or take a range of numbers.													  *
	 *  	- some famous macros to have FUN.																	  *
	 *  																										  *
	 *  Users should interact only with parts published to the Enumerables namespace by the end of this file,	  *
	 *  along with helpers defined in Enumerables_InterfaceTypes.hpp.											  *
	 *  																										  *
	 *  To instantiate the library inlude		Enumerables_Implementation.hpp									  *
	 *  For available cofiguration options see	Enumerables_ConfigDefaults.hpp									  *
	 *  --------------------------------------------------------------------------------------------------------  */


#include "Enumerables_ChainTool.hpp"
#include "Enumerables_ConfigDefaults.hpp"
#include "Enumerables_Enumerators.hpp"
#include <functional>




#pragma region Fun

// FUN macros: Getting as close as possible to simple arrow-lambda syntax.
// If they're not welcome:  defining ENUMERABLES_NO_FUN before including the library should hide them.


// Arity-based overload for macros - expansion hack is needed for MSVC, which seems to follow standard this case.
// https://stackoverflow.com/questions/11761703/overloading-macro-on-number-of-arguments
#define ENUMERABLES_EXPAND_VARARGS(x) x
#define ENUMERABLES_GET_MACRO(_1, _2, _3, _4, NAME, ...) NAME


/// General shorthand for lambda (similar to C# => arrow)
/// - Only ref capture, be aware when returned!
#define FUN(...)	ENUMERABLES_EXPAND_VARARGS(ENUMERABLES_GET_MACRO(__VA_ARGS__, FUN3, FUN2, FUN1, 0)(__VA_ARGS__))

#define FUN1(x, expression)			[&](auto&& x)						-> decltype(auto)	{ return (expression); }
#define FUN2(x, y, expression)		[&](auto&& x, auto&& y)				-> decltype(auto)	{ return (expression); }
#define FUN3(x, y, z, expression)	[&](auto&& x, auto&& y, auto&& z)	-> decltype(auto)	{ return (expression); }


/// General shorthand for lambda (similar to C# => arrow)
/// - Value capture for locals, ref for members [this].
#define FUNV(...) ENUMERABLES_EXPAND_VARARGS(ENUMERABLES_GET_MACRO(__VA_ARGS__, FUNV3, FUNV2, FUNV1, 0)(__VA_ARGS__))

#define FUNV1(x, expression)		[=](auto&& x)						-> decltype(auto)	{ return (expression); }
#define FUNV2(x, y, expression)		[=](auto&& x, auto&& y)				-> decltype(auto)	{ return (expression); }
#define FUNV3(x, y, z, expression)	[=](auto&& x, auto&& y, auto&& z)	-> decltype(auto)	{ return (expression); }

#pragma endregion




#pragma region Predeclaration + Technical

// Def is an internal namespace of the library to be freely polluted by helper stuff -
// Client code should use its subset exported to ::Enumerables.
namespace Enumerables::Def {

	/// Generic wrapper for Enumerator factories  - to be used with 'auto' => no virtualization overhead.
	template <class TFactory>
	class AutoEnumerable;

	/// Wrapped factory of interfaced enumerators - to be used in explicit declarations.
	template <class V>
	using Enumerable = AutoEnumerable<std::function<InterfacedEnumerator<V> ()>>;


#if ENUMERABLES_USE_RESULTSVIEW

	// defined outside AutoEnumerable to provide accessible type parameter for natvis
	template <class T>
	struct ResultBuffer {
		using TDebugValue = std::remove_cv_t<StorableT<T>>;

#	if ENUMERABLES_RESULTSVIEW_AUTO_EVAL == 0
		const char* Status = "Not evaluated.  Call Test() or Print() from Immediate window, or set ENUMERABLES_RESULTSVIEW_AUTO_EVAL.";
#	else
		const char* Status = "Not evaluated.  Step over first (&) method call, or invoke Test()/Print() from Immediate window.  See ENUMERABLES_RESULTSVIEW_AUTO_EVAL.";
#	endif

		SmallListType<TDebugValue, 4>	Elements;

		template <class Factory>
		void Fill(Factory& getEnumerator, bool isPure, bool autoCall);
	};

#endif

}	// namespace Enumerables::Def


namespace Enumerables::TypeHelpers {

	/// This trait disambiguates between Enumerables and containers
	template <class Fact>
	struct IsAutoEnumerable<Def::AutoEnumerable<Fact>>
	{
		static constexpr bool value = true;
	};

	template <class Fact>
	struct IsAutoEnumerable<const Def::AutoEnumerable<Fact>>
	{
		static constexpr bool value = true;
	};

}

#pragma endregion




namespace Enumerables::Def {

	using namespace Enumerables::TypeHelpers;


	/// Common Enumerable implementation.
	/// @remarks
	///		By default, creates directly nested, templated enumerators.
	///		Can be upgraded to Enumerable<V> on demand, which cuts the chain of template nesting
	///		by producing interfaced IEnumerator<V>, and provides type-erasure for the factory too.
	///		Practically immutable, as queries should be repeatable in general.
	template <class TFactory>
	class AutoEnumerable {
		template <class>	friend class AutoEnumerable;

		/// Factory object of Enumerators. Serves as a direct container
		/// for any dependencies, including nested (upstream) factories.
		/// @remarks
		///		The AutoEnumerable class is merely a builder-style wrapper for this factory-chain.
		TFactory	factory;

		/// Setting false means enumerating has side-effects - a generally discouraged scenario.
		/// Disables auto-evaluation of debug ResultsView, when necessary.
		bool		isPure;


	public:
		using TEnumerator	  = decltype(factory());
		using TElem			  = EnumeratedT<TEnumerator>;
		using TElemDecayed	  = std::decay_t<TElem>;
		using TElemConstParam = LambdaCreators::ConstParamT<TElem>;		// deep-const for predicates


		static_assert(!is_reference<TFactory>(), "AutoEnumerable construction error");


		/// Execute the factory function.
		TEnumerator GetEnumerator()				const	{ ViewTrigger();  return factory(); }
		TEnumerator GetEnumeratorNoDebug()		const	{ return factory(); }

		/// Only to be directly consumable by for ( : ).
		EnumeratorAdapter<TEnumerator>	begin()	const	{ ViewTrigger();  return { factory }; }
		constexpr EnumeratorAdapterEnd	end()	const	{ return {}; }



	// =========== Debug support =====================================================================================================
	#pragma region
	private:

		/// Marks the points of lvalue-only ResultsView evaluation.
		void ViewTrigger() const;


#	if ENUMERABLES_USE_RESULTSVIEW
		mutable ResultBuffer<TElem>		ResultsView;

		// force the compiler to generate those functions for immediate window...
		const char*						(AutoEnumerable::*	testResultsViewPtr)()  const = nullptr;
		decltype(ResultsView.Elements)&	(AutoEnumerable::*	printResultsViewPtr)() const = nullptr;


		/// Fill the debug buffer with yielded values if possible. For immediate window.
		ENUMERABLES_NOINLINE const char* Test()	 const;

		/// Print yielded values if possible. For immediate window.
		ENUMERABLES_NOINLINE auto		 Print() const -> decltype(ResultsView.Elements)&;
#	endif

	public:
	#pragma endregion


	// =========== Constructors ======================================================================================================
	#pragma region
	
		/// Main constructor. Wraps an Enumerator-factory directly. Intended for usage through creator functions.
		/// @param pureSource:  enumerating is sideeffect-free, allows auto-evaluation for debugging.
		/// @param stepToDebug: reasonable to auto-evaluate - e.g. not just a wrapped container.
		AutoEnumerable(TFactory&& factory, bool pureSource = true, [[maybe_unused]] bool stepToDebug = true) :
			factory { move(factory) },
			isPure  { pureSource && is_copy_constructible<TElem>() }
		{
#		if ENUMERABLES_USE_RESULTSVIEW
#			if ENUMERABLES_RESULTSVIEW_AUTO_EVAL >= 2
				if (stepToDebug)
					ResultsView.Fill(factory, isPure, true);
#			endif
			testResultsViewPtr  = &AutoEnumerable::Test;
			printResultsViewPtr = &AutoEnumerable::Print;
#		endif
		}


		/// Wrap container implicitly to an interfaced Enumerable.
		template <
			class Container,
			IfContainerLike<Container&, int> = 0,
			enable_if_t<   !IsSpeciallyTreatedContainer<Container>::value
						&& is_same_v<TEnumerator, InterfacedEnumerator<TElem>>, int> = 0
		>
		AutoEnumerable(Container& c) :
			AutoEnumerable { [&c]() {
				return InterfacedEnumerator<TElem> { [&c]() { return CreateEnumeratorFor<TElem>(c); } };
			}}
		{
			ContainerWrapChecks::ByRefImplicit<IterableT<Container&>, TElem>();
		}


		/// Auto-convert an r-value container to an interfaced, materialized Enumerable.
		/// @remarks:	typical subjects are function results and returned locals [if eligible to NRVO]
		template <
			class Container,
			IfContainerLike<const Container&, int> = 0,
			enable_if_t<   !IsSpeciallyTreatedContainer<Container>::value
						&& !is_lvalue_reference_v<Container>
						&& is_same_v<TEnumerator, InterfacedEnumerator<TElem>>, int> = 0
		>
		AutoEnumerable(Container&& cont) :
			AutoEnumerable { [c = move(cont)]() {
				return InterfacedEnumerator<TElem> { [&c]() { return CreateEnumeratorFor<TElem>(c); } };
			}}
		{
			ContainerWrapChecks::ByValueImplicit<IterableT<Container&>, TElem>();
		}

	#pragma endregion


	// =========== Chaining utils ====================================================================================================
	#pragma region
	private:
		TFactory			CloneFactory() const	{ return factory; }
		TFactory&&			PassFactory()  &&		{ return move(factory); }
		AutoEnumerable&&	Move()					{ return move(*this); }


		/// Creates a new NextEnumerator-factory, using self as the source, then returns it wrapped as AutoEnumerable.
		/// @tparam NextEnumerator:	Algorithm template to be constructed - bound as NextEnumerator<TEnumerator, Args..., TypeArgs...>.
		/// @param  args:			Data to be stored into factory.
		///							First of them can be SteadyParams(...), contents of which are passed to ctor, but not as type arguments.
		/// @tparam TypeArgs:		Optional type-only arguments supplied to NextEnumerator at last.
		template <template <class...> class NextEnumerator, class... TypeArgs, class... Args>
		auto Chain(Args&&... args) const;

		/// Same as chain, but moves current factory inside the chained, instead of copying.
		template <template <class...> class NextEnumerator, class... TypeArgs, class... Args>
		auto MvChain(Args&&... args);


		/// Chains an Enumerable made up of two sources - useful for: Concat, Except, Zip etc.
		/// @param  second:			can be a container (& / &&) or an AutoEnumerable ready to use.
		/// @tparam Enumerable2:	specified explicitly to forward @p second more concisely (!)
		/// @tparam TForced:		forced elem type for @p second, or void - helps with initializers
		/// @tparam NextEnumerator:	Algorithm template to be constructed - bound as NextEnumerator<TEnumerator, TEnumerator2, Args..., TypeArgs...>,
		///							where TEnumerator2 is the enumerator type for @p second.
		template <class Enumerable2, class TForced, template <class...> class NextEnumerator, class... TypeArgs, class... Args>
		auto ChainJoined(Enumerable2& second, Args&&... args) const;

		/// Same as ChainJoined, but moves current factory inside the chained, instead of copying.
		template <class Enumerable2, class TForced, template <class...> class NextEnumerator, class... TypeArgs, class... Args>
		auto MvChainJoined(Enumerable2& second, Args&&... args);


	// ----- Unified mapper utils ----------------------------------------------------------------------------------------------------

		// Shorthands to accept any kind of mapper argument (lambda / free function pointer / member pointer) taking TElem.
		// CAUTION: interface convention is to omit std::forward at call-sites. For that always specify type argument explicitly!
		//			Note the &'s here to accept the callers universal reference w/o copy - pass their deduced type to forward!
		// NOTE:	decltype(auto): if no adjustment needed, the provided lambda gets forwarded as is.
		//			[member-] function pointers always generate a prvalue wrapper.

		template <class Mapper, class VForced = void>
		static decltype(auto) Selector(Mapper& m)		{ return LambdaCreators::Selector<TElem, VForced>(forward<Mapper>(m)); }

		template <class Mapper, class VForced = void>
		static decltype(auto) FreeMapper(Mapper& m)		{ return LambdaCreators::CustomMapper<TElem, VForced>(forward<Mapper>(m)); }

		// Forced l-value variant for ToDictionary
		template <class Mapper>
		static decltype(auto) KeyMapper(Mapper& m)		{ return LambdaCreators::CustomMapper<TElem&>(forward<Mapper>(m)); }

		// Special SFINAE variant for ToDictionary overloads - Not forcing Selector, as target is always decayed.
		template <class Mapper, class = enable_if_t<!is_convertible_v<Mapper, size_t>>>
		static decltype(auto) ValueMapper(Mapper& m)	{ return LambdaCreators::CustomMapper<TElem>(forward<Mapper>(m)); }


		template <class Mapper, class VForced = void>
		static decltype(auto) IndepMapper(Mapper& m)
		{
			static_assert (!is_member_object_pointer<Mapper>(), "Use projection (.Select) to access a member with related lifetime!");
			return LambdaCreators::CustomMapper<TElem, VForced>(forward<Mapper>(m));
		}

		template <class Pred>
		static decltype(auto) Predicate(Pred& p)	{ return LambdaCreators::Predicate<TElem>(forward<Pred>(p)); }

		template <class Pred>
		static decltype(auto) BinPred(Pred& p)		{ return LambdaCreators::BinaryMapper<TElemConstParam, TElemConstParam, bool>(forward<Pred>(p)); }



		// Create binary operation that compares a given property of TElem
		template <class Mapper, class TPropOvrd = void>
		static auto	ComparatorForProperty(Mapper& getProperty)
		{
			using CP = TElemConstParam;
			return [prop = LambdaCreators::CustomMapper<CP, TPropOvrd>(forward<Mapper>(getProperty))](CP l, CP r) -> bool
			{
				return prop(l) < prop(r);
			};
		}


		// Helper to accept pointer to [member-]function syntax for unary functions over TElem, even when it designates an overload-set.
		// Resolution happens based on TElem + manually specified expected result type (Res) for typical cases:
		// Type-conversions (covariance) are not supported, but materializing prvalues from reference results are.
		// Usage notes:
		//	- OverloadResolver stores 1 extra function pointer - language mechanics necessitate a miniature type-erasure there.
		//  - always inactive with implicit result types
		//  - inactive when an exact function is designated (templated operations w/o conversion take precedence)
		template <class Res>
		using OverloadTo = TypeHelpers::OverloadResolver<TElem, Res>;

		template <class Res>
		using LVOverloadTo = TypeHelpers::OverloadResolver<TElem&, Res>;

		template <class Res>
		using ConstOverloadTo = TypeHelpers::OverloadResolver<TElemConstParam, Res>;

		// As a default type, supports limited, but simple overload-resolution for Predicate Functions.
		using PF = TypeHelpers::FreePredicatePtr<TElemDecayed>;


		template <class Res>
		static auto			  SelectorOverload(const OverloadResolver<TElem, Res>& s)   { return LambdaCreators::Selector<TElem, Res>(s);  }


		// Custom binary operation over TElem
		template <class Mapper, class VForced = void>
		static decltype(auto) Combiner(Mapper& m)		{ return LambdaCreators::BinaryMapper<TElem, TElem, VForced>(forward<Mapper>(m));  }

		// Custom binary operation: (TLeft, TElem) -> VForced / auto
		template <class TLeft,  class Mapper, class VForced = void>
		static decltype(auto) CombinerL(Mapper& m)		{ return LambdaCreators::BinaryMapper<TLeft, TElem, VForced>(forward<Mapper>(m));  }

		// Custom binary operation: (TElem, TRight) -> VForced / auto
		template <class TRight, class Mapper, class VForced = void>
		static decltype(auto) CombinerR(Mapper& m)		{ return LambdaCreators::BinaryMapper<TElem, TRight, VForced>(forward<Mapper>(m)); }


		// Forward binary operation with reversed operand order
		template <class BinOp>
		static auto SwappedBinop(BinOp&& opp)
		{
			return [op = forward<BinOp>(opp)](auto&& l, auto&& r)	{ return op(r, l); };
		}


	// ----- Result type shorthands --------------------------------------------------------------------------------------------------

		template <class Mapper>  using DecayedResult   = std::decay_t<MappedT<TElem, Mapper>>;
		template <class Mapper>  using DecayedResultLV = std::decay_t<MappedT<TElem&, Mapper>>;


	// ----- Scan/Aggregate deduction utils ------------------------------------------------------------------------------------------

		template <class ForcedAcc, class Init>
		using IfInitByValue   = enable_if_t<IsAccuInit<TElem, Init, ForcedAcc>::byValue,   int>;

		template <class ForcedAcc, class Init>
		using IfInitByMapping = enable_if_t<IsAccuInit<TElem, Init, ForcedAcc>::byMapping, int>;

		using DeduceAccumulator = AccuDeducer<TElem>;

	public:
	#pragma endregion


	// =========== Transformations ===================================================================================================
	#pragma region
	
		/// Disables ResultsView auto-evaluation for debugging, when really necessary.
		/// @remarks
		///		Enumerables with side-effects are discouraged. However, when really needed,
		///		a NonPure call keeps them functional under debugging by shielding downstream
		///		filters from auto-evaluating ther contets, which could alter program state.
		AutoEnumerable<TEnumerator> NonPure() const &	{ return AutoEnumerable { CloneFactory(), false }; }
		AutoEnumerable<TEnumerator> NonPure() &&		{ return AutoEnumerable { move(factory),  false }; }


	// ----- Filtration / Truncation -------------------------------------------------------------------------------------------------

		/// Elements satisfying the given predicate.
		template <class Pred = PF>	auto Where(Pred&& p) const &	{ return   Chain<FilterEnumerator>(Predicate<Pred>(p)); }
		template <class Pred = PF>	auto Where(Pred&& p) &&			{ return MvChain<FilterEnumerator>(Predicate<Pred>(p)); }


		/// At most @p count consecutive elements from begin.
		auto Take(size_t count) const &	{ return   Chain<CounterEnumerator>(SteadyParams(FilterMode::TakeWhile, count)); }
		auto Take(size_t count) &&		{ return MvChain<CounterEnumerator>(SteadyParams(FilterMode::TakeWhile, count)); }

		/// The remainder after omitting the first @p count elements.
		auto Skip(size_t count) const &	{ return   Chain<CounterEnumerator>(SteadyParams(FilterMode::SkipUntil, count)); }
		auto Skip(size_t count) &&		{ return MvChain<CounterEnumerator>(SteadyParams(FilterMode::SkipUntil, count)); }


		/// Leading elements that consecutively satisfy the predicate.
		template <class Pred = PF>	auto TakeWhile(Pred&& p)  const &		{ return   Chain<FilterUntilEnumerator>(SteadyParams(FilterMode::TakeWhile), Predicate<Pred>(p)); }
		template <class Pred = PF>	auto TakeWhile(Pred&& p)  &&			{ return MvChain<FilterUntilEnumerator>(SteadyParams(FilterMode::TakeWhile), Predicate<Pred>(p)); }

		/// Tail starting with the first element which satisfies the predicate.
		template <class Pred = PF>	auto SkipUntil(Pred&& p) const &		{ return   Chain<FilterUntilEnumerator>(SteadyParams(FilterMode::SkipUntil), Predicate<Pred>(p)); }
		template <class Pred = PF>	auto SkipUntil(Pred&& p) &&				{ return MvChain<FilterUntilEnumerator>(SteadyParams(FilterMode::SkipUntil), Predicate<Pred>(p)); }

		/// Leading elements closed by the first which satisfies the predicate (or by the Last).
		/// @remarks:	in some way similar to LTL Relase operator
		template <class Pred = PF>	auto TakeUntilFinal(Pred&& p) const &	{ return   Chain<FilterUntilEnumerator>(SteadyParams(FilterMode::ReleaseBy), Predicate<Pred>(p)); }
		template <class Pred = PF>	auto TakeUntilFinal(Pred&& p) &&		{ return MvChain<FilterUntilEnumerator>(SteadyParams(FilterMode::ReleaseBy), Predicate<Pred>(p)); }


		// --- Boolean operations capturing readily available sets [can form in-place using braced-initializer] ---
		
		// NOTE: Result is not a set! Duplicated elements of this sequence (where accepted) will pass through.

		template <class... SetOptions>  auto Except(const SetType<TElemDecayed, SetOptions...>& set)	const &	 { return		 Where(FUN(x, !SetOperations::Contains(set, x))); }
		template <class... SetOptions>  auto Except(const SetType<TElemDecayed, SetOptions...>& set)	&&		 { return Move().Where(FUN(x, !SetOperations::Contains(set, x))); }
		template <class... SetOptions>  auto Except(SetType<TElemDecayed, SetOptions...>&&      set)	const &	 { return		 Where([s = move(set)](const auto& x) { return !SetOperations::Contains(s, x); }); }
		template <class... SetOptions>  auto Except(SetType<TElemDecayed, SetOptions...>&&      set)	&&		 { return Move().Where([s = move(set)](const auto& x) { return !SetOperations::Contains(s, x); }); }

		template <class... SetOptions>  auto Intersect(const SetType<TElemDecayed, SetOptions...>& set)	const &	 { return		 Where(FUN(x, SetOperations::Contains(set, x))); }
		template <class... SetOptions>  auto Intersect(const SetType<TElemDecayed, SetOptions...>& set)	&&		 { return Move().Where(FUN(x, SetOperations::Contains(set, x))); }
		template <class... SetOptions>  auto Intersect(SetType<TElemDecayed, SetOptions...>&&	   set)	const &	 { return		 Where([s = move(set)](const auto& x) { return SetOperations::Contains(s, x); }); }
		template <class... SetOptions>  auto Intersect(SetType<TElemDecayed, SetOptions...>&&	   set)	&&		 { return Move().Where([s = move(set)](const auto& x) { return SetOperations::Contains(s, x); }); }


		// --- Boolean operations evaluating other iterables ---
		
		// NOTE: 2nd operand gets evaluated lazily, before enumeration - forming a temporary SetType.

		/// @tparam SetOptions:   Hash/Comparer/etc. strategy types injected directly to SetType used as filter internally
		template <class... SetOptions, class E>	 auto Except   (E&& elems) const &	{ return   ChainJoined<E, TElem, SetFilterEnumerator, SetOptions...>(elems, SteadyParams(false)); }
		template <class... SetOptions, class E>	 auto Except   (E&& elems) &&		{ return MvChainJoined<E, TElem, SetFilterEnumerator, SetOptions...>(elems, SteadyParams(false)); }
		template <class... SetOptions, class E>	 auto Intersect(E&& elems) const &	{ return   ChainJoined<E, TElem, SetFilterEnumerator, SetOptions...>(elems, SteadyParams(true)); }
		template <class... SetOptions, class E>	 auto Intersect(E&& elems) &&		{ return MvChainJoined<E, TElem, SetFilterEnumerator, SetOptions...>(elems, SteadyParams(true)); }
		
		/// @param setOptions:    hash/equal_to/etc. strategy objects injected to the internally constructed SetType used as filter
		template <class E, class... Os>  auto Except   (E&& elems, const Os&... setOptions) const &	{ return   ChainJoined<E, TElem, SetFilterEnumerator>(elems, SteadyParams(false), setOptions...); }
		template <class E, class... Os>  auto Except   (E&& elems, const Os&... setOptions) &&		{ return MvChainJoined<E, TElem, SetFilterEnumerator>(elems, SteadyParams(false), setOptions...); }
		template <class E, class... Os>	 auto Intersect(E&& elems, const Os&... setOptions) const &	{ return   ChainJoined<E, TElem, SetFilterEnumerator>(elems, SteadyParams(true),  setOptions...); }
		template <class E, class... Os>	 auto Intersect(E&& elems, const Os&... setOptions) &&		{ return MvChainJoined<E, TElem, SetFilterEnumerator>(elems, SteadyParams(true),  setOptions...); }

		// NOTE: Union wouldn't make much sense asymmetrically.
		//		 For a proper set result .Concat(s).ToSet() is effective! (No lazy evaluation though.)


		// --- Shorthands for convenience ---

		/// Elements not equal to nullptr
		auto NonNulls() const &		{ return		Where(FUN(x, x != nullptr)); }
		auto NonNulls() &&			{ return Move().Where(FUN(x, x != nullptr)); }

		/// Unbox optional-like types where a value is available
		auto ValuesOnly() const &	{ return		Where(FUN(x, Enumerables::HasValue(x))).Dereference(); }
		auto ValuesOnly() &&		{ return Move().Where(FUN(x, Enumerables::HasValue(x))).Dereference(); }


	// ----- Element transformation / projection -------------------------------------------------------------------------------------

		/// Apply @p mapper as a transformation function to each element.
		template <class Mapper>					auto Map  (Mapper&& mapper)				const &	{ return   Chain<MapperEnumerator>(IndepMapper<Mapper>(mapper)); }
		template <class Mapper>					auto Map  (Mapper&& mapper)				&&		{ return MvChain<MapperEnumerator>(IndepMapper<Mapper>(mapper)); }

		/// Apply @p mapper as a transformation function - yielding TMapped - to each element.
		template <class TMapped, class Mapper>	auto MapTo(Mapper&& mapper)				const &	{ return   Chain<MapperEnumerator>(IndepMapper<Mapper, TMapped>(mapper)); }
		template <class TMapped, class Mapper>	auto MapTo(Mapper&& mapper)				&&		{ return MvChain<MapperEnumerator>(IndepMapper<Mapper, TMapped>(mapper)); }
		template <class TMapped>				auto MapTo(OverloadTo<TMapped> mapper)	const &	{ return   Chain<MapperEnumerator>(mapper); }
		template <class TMapped>				auto MapTo(OverloadTo<TMapped> mapper)	&&		{ return MvChain<MapperEnumerator>(mapper); }


		/// Select a subobject of each element (project 1 field). In contrast with Map/MapTo:
		/// Lifetime of result elements is considered bound to the input elements - therefore a copy to prvalue is automatically applied when necessary.
		/// @param  selector:  points either a data member of elements' class, or a simple getter function with no parameter
		/// @tparam TSelected: explicitly specified result type - usage optional
		template <class TSelected = void, class S>	auto Select(S&& selector)				  const &	{ return   Chain<MapperEnumerator>(Selector<S, TSelected>(selector)); }
		template <class TSelected = void, class S>	auto Select(S&& selector)				  &&		{ return MvChain<MapperEnumerator>(Selector<S, TSelected>(selector)); }
		template <class TSelected>					auto Select(OverloadTo<TSelected> getter) const &	{ return   Chain<MapperEnumerator>(SelectorOverload<TSelected>(getter)); }
		template <class TSelected>					auto Select(OverloadTo<TSelected> getter) &&		{ return MvChain<MapperEnumerator>(SelectorOverload<TSelected>(getter)); }


			// --- Shorthands for convenience ---

		/// Results of & operator applied to elements.
		auto Addresses()	const &	{ return		Map(FUN(x, &x)); }
		auto Addresses()	&&		{ return Move().Map(FUN(x, &x)); }

		/// Results of * operator applied to elements.
		auto Dereference()	const &	{ return		Select(FUN(x, *x)); }
		auto Dereference()	&&		{ return Move().Select(FUN(x, *x)); }


	// ----- Conversions -------------------------------------------------------------------------------------------------------------

		/// Apply an implicit conversion to type R for each element.
		template <class R>	auto As() const &
		{ 
			if constexpr (is_same_v<R, TElem>)	return *this;
			else								return Chain<ConverterEnumerator, R>();
		}
		template <class R>	auto As() &&
		{ 
			if constexpr (is_same_v<R, TElem>)	return Move();
			else								return MvChain<ConverterEnumerator, R>();
		}

		/// Apply static_cast to type R for each element.
		template <class R>	auto Cast() const &
		{
			if constexpr (is_same_v<R, TElem>)	return *this;
			else								return Chain<CastingEnumerator, R>();
		}
		template <class R>	auto Cast() &&
		{ 
			if constexpr (is_same_v<R, TElem>)	return Move();
			else								return MvChain<CastingEnumerator, R>();
		}

		/// Apply dynamic_cast to type R for each element.
		template <class R>	auto DynamicCast()	const &	{ return   Chain<DynCastingEnumerator, R>(); }
		template <class R>	auto DynamicCast()	&&		{ return MvChain<DynCastingEnumerator, R>(); }

		/// Filter and dynamic_cast to type R. (Use exact * or & type.)
		template <class R>	auto OfType()		const &	{ return   Chain<TypeFilterEnumerator, R>(); }
		template <class R>	auto OfType()		&&		{ return MvChain<TypeFilterEnumerator, R>(); }


		/// Same elements with const qualifier injected to top pointed or referenced type.
		auto AsConst()		const &	{ return As<ConstValueT<TElem>>(); }
		auto AsConst()		&&		{ return Move().template As<ConstValueT<TElem>>(); }

		/// Assure a sequence of pr-values (apply copy if needed).
		auto Decay()		const &	{ return As<TElemDecayed>(); }
		auto Decay()		&&		{ return Move().template As<TElemDecayed>(); }

		/// Copy ref elements to form pr-values. (Readability helper.)
		auto Copy()			const &	{ return		Decay();	static_assert(is_reference<TElem>(), "Already copies."); }
		auto Copy()			&&		{ return Move().Decay();	static_assert(is_reference<TElem>(), "Already copies."); }


		// --- Misc. sequence conversions ---

		/// Sequence of (index, value) pairs as Indexed<TElem>
		auto Counted() const &	{ return   Chain<IndexerEnumerator>();   }
		auto Counted() &&		{ return MvChain<IndexerEnumerator>();   }

		/// Flattened sequence of sequences (be they containers or already wrapped Enumerables).
		auto Flatten() const &	{ return   Chain<FlattenerEnumerator>(); }
		auto Flatten() &&		{ return MvChain<FlattenerEnumerator>(); }


	// ----- Multi-element transformations -------------------------------------------------------------------------------------------

		/// Apply a binary mapper function to each neighboring pair  [N-1 calls for input length N]
		template <class TMapped = void, class C>
		auto MapNeighbors(C&& combiner) const &		{ return   Chain<CombinerEnumerator>(Combiner<C, TMapped>(combiner)); }
		template <class TMapped = void, class C>
		auto MapNeighbors(C&& combiner) &&			{ return MvChain<CombinerEnumerator>(Combiner<C, TMapped>(combiner)); }


		/// Repeat the first n elements after the end of sequence. (Useful for circular structures in conjunction with MapNeighbors.)
		/// @remarks  Equivalent to Concat(Take(n)) but holds those n elements instead of double evaluation.
		auto CloseWithFirst(size_t n = 1) const &	{ return   Chain<ReplayEnumerator>(SteadyParams(n)); }
		auto CloseWithFirst(size_t n = 1) &&		{ return MvChain<ReplayEnumerator>(SteadyParams(n)); }


		/// Generic Scan operation - results of consecutive application of a binary operation in left-associative manner.
		/// [N-1 calls for length N;  1st result = 1st input!]
		template <class ForcedAcc = void, class F>
		auto Scan(F&& combiner)		const &
		{
			using Acc = typename DeduceAccumulator::template ForFirstInit<F, ForcedAcc>;
			return Chain<FetchFirstScannerEnumerator, None, Acc>(CombinerL<Acc, F>(combiner));
		}
		template <class ForcedAcc = void, class F>
		auto Scan(F&& combiner)		&&
		{
			using Acc = typename DeduceAccumulator::template ForFirstInit<F, ForcedAcc>;
			return MvChain<FetchFirstScannerEnumerator, None, Acc>(CombinerL<Acc, F>(combiner));
		}


		/// [N calls for length N;  use given value to initialize the accumulator.]
		template <class ForcedAcc = void, class InAcc, class F>
		auto Scan(InAcc&& firstAccValue, F&& combiner, IfInitByValue<ForcedAcc, decay_t<InAcc>> = 0)	const &
		{
			using Acc = typename DeduceAccumulator::template ForDirectInit<InAcc, F, ForcedAcc>;
			return Chain<ScannerEnumerator, Acc>(SteadyParams(StoreAllowingRef<InAcc, Acc>(firstAccValue)), CombinerL<Acc, F>(combiner));
			// SteadyParams: Acc is passed explicitly, don't repeat
		}
		template <class ForcedAcc = void, class InAcc, class F>
		auto Scan(InAcc&& firstAccValue, F&& combiner, IfInitByValue<ForcedAcc, decay_t<InAcc>> = 0)	&&
		{
			using Acc = typename DeduceAccumulator::template ForDirectInit<InAcc, F, ForcedAcc>;
			return MvChain<ScannerEnumerator, Acc>(SteadyParams(StoreAllowingRef<InAcc, Acc>(firstAccValue)), CombinerL<Acc, F>(combiner));
		}


		/// [N-1 combiner calls for length N;  1st call to @p init initializes accumulator by a single input elem.]
		/// @remarks
		///		Citation needed? Can't find the page where I met this idea.
		template <class ForcedAcc = void, class AccInitMap, class F>
		auto Scan(AccInitMap&& init, F&& combiner, IfInitByMapping<ForcedAcc, AccInitMap> = 0)	const &
		{
			using Acc = typename DeduceAccumulator::template ForMappingInit<AccInitMap, F, ForcedAcc>;
			return Chain<FetchFirstScannerEnumerator, Acc>(CombinerL<Acc, F>(combiner), FreeMapper<AccInitMap>(init));
		}
		template <class ForcedAcc = void, class AccInitMap, class F>
		auto Scan(AccInitMap&& init, F&& combiner, IfInitByMapping<ForcedAcc, AccInitMap> = 0)	&&
		{
			using Acc = typename DeduceAccumulator::template ForMappingInit<AccInitMap, F, ForcedAcc>;
			return MvChain<FetchFirstScannerEnumerator, Acc>(CombinerL<Acc, F>(combiner), FreeMapper<AccInitMap>(init));
		}


	// ----- Combine sequences -------------------------------------------------------------------------------------------------------

		template <class Zipper, class Eb2>
		auto Zip  (Eb2&& second, Zipper&& z) const &		{ return   ChainJoined<Eb2, void, ZipperEnumerator>(second, CombinerR<IterableT<Eb2>, Zipper>(z)); }
		template <class Zipper, class Eb2>
		auto Zip  (Eb2&& second, Zipper&& z) &&				{ return MvChainJoined<Eb2, void, ZipperEnumerator>(second, CombinerR<IterableT<Eb2>, Zipper>(z)); }
		template <class TZipped, class Zipper, class Eb2>
		auto ZipTo(Eb2&& second, Zipper&& z) const &		{ return   ChainJoined<Eb2, void, ZipperEnumerator>(second, CombinerR<IterableT<Eb2>, Zipper, TZipped>(z)); }
		template <class TZipped, class Zipper, class Eb2>
		auto ZipTo(Eb2&& second, Zipper&& z) &&				{ return MvChainJoined<Eb2, void, ZipperEnumerator>(second, CombinerR<IterableT<Eb2>, Zipper, TZipped>(z)); }


		template <class E2>	auto Concat(E2&& continuation) const &	{ return   ChainJoined<E2, TElem, ConcatEnumerator>(continuation); }
		template <class E2>	auto Concat(E2&& continuation) &&		{ return MvChainJoined<E2, TElem, ConcatEnumerator>(continuation); }

	#pragma endregion


	// =========== Terminal operations ===============================================================================================
	#pragma region

		// NOTE: Parameters here will be a local to scope -> not even lambdas need to be heavy -> no need to forward them.

		// NOTE: For these operations, ref-qualified overloads could only limit debug overhead with ResultsView evaluation.
		//		 Currently less duplication is preferred over the slight debug cost-cutting.
		//		 In Release, using .ToReferenced() universally prevents not only copying the factory, but even moving it.

		bool				Any()				const;
		TElem				First()				const;
		TElem				Single()			const;
		Optional<TElem>		FirstIfAny()		const;
		Optional<TElem>		SingleIfAny()		const;
		Optional<TElem>		SingleOrNone()		const;

		/// The sequence contains no different elements according to operator ==
		bool				AllEqual()			const;

		// Iterating operations - these are generally inefficient!  [Count is optimized for when the source length is known.]
		size_t				Count()				const;
		TElem				Last()				const;
		Optional<TElem>		LastIfAny()			const;
		Optional<TElem>		ElementAt(size_t i) const						{ return ToReferenced().Skip(i).FirstIfAny(); }

		/// Apply binary predicate to each pair of consequtive elements.
		template <class BinPred>  bool AllNeighbors(BinPred&& p) const		{ return !ToReferenced().template MapNeighbors<bool>(p).Contains(false); }
		template <class BinPred>  bool AnyNeighbors(BinPred&& p) const		{ return ToReferenced() .template MapNeighbors<bool>(p).Contains(true);  }


	// ----- Shorthands taking a predicate -------------------------------------------------------------------------------------------

		template <class Pred = PF>	bool			  All		  (const Pred& p) const;
		template <class Pred = PF>	bool			  Any		  (const Pred& p) const   { return ToReferenced().Where(p).Any();		   }
		template <class Pred = PF>	TElem			  First		  (const Pred& p) const   { return ToReferenced().Where(p).First();		   }
		template <class Pred = PF>	TElem			  Last		  (const Pred& p) const   { return ToReferenced().Where(p).Last();		   }
		template <class Pred = PF>	TElem			  Single	  (const Pred& p) const   { return ToReferenced().Where(p).Single();	   }
		template <class Pred = PF>	Optional<TElem>	  FirstIfAny  (const Pred& p) const   { return ToReferenced().Where(p).FirstIfAny();   }
		template <class Pred = PF>	Optional<TElem>	  LastIfAny   (const Pred& p) const   { return ToReferenced().Where(p).LastIfAny();    }
		template <class Pred = PF>	Optional<TElem>	  SingleIfAny (const Pred& p) const   { return ToReferenced().Where(p).SingleIfAny();  }
		template <class Pred = PF>	Optional<TElem>	  SingleOrNone(const Pred& p) const   { return ToReferenced().Where(p).SingleOrNone(); }
		
		template <class Pred = PF, class = enable_if_t<!is_convertible_v<Pred, TElemConstParam>>>
		size_t	Count(const Pred& p)		  const   { return ToReferenced().Where(p).Count(); }


	// ----- Shorthands comparing to an element --------------------------------------------------------------------------------------

		template <class R = TElemDecayed>
		bool	AllEqual(const R& rhs)		  const   { return ToReferenced().All  (FUN(x, x == rhs)); }
		bool	Contains(TElemConstParam val) const   { return ToReferenced().Any  (FUN(x, x == val)); }
		size_t	Count	(TElemConstParam val) const   { return ToReferenced().Where(FUN(x, x == val)).Count(); }


	// ----- Aggregating operations --------------------------------------------------------------------------------------------------

		/// Classic FoldL (aka. Aggregate/Reduce) - aggregate elements using a binary operation in a left-associative manner.
		/// [N-1 calls for input length N]
		/// @throws		on empty input
		/// @returns	Acc, or in case of implicit accumulator type: result of combiner(TElem, TElem)
		template <class Acc = void, class F>
		decltype(auto) Aggregate(F&& combiner) const	{ return ToReferenced().template Scan<Acc>(forward<F>(combiner)).Last(); }


		/// [N-1 calls for input length N, after mapping First()]
		/// @param initMapper:	TElem -> Acc callable
		/// @throws				on empty input
		/// @returns			Acc, determined by initMapper in implicit case
		template <class Acc = void, class M, class F>
		decltype(auto) Aggregate(M&& initMapper, F&& combiner, IfInitByMapping<Acc, M> = 0) const
		{
			return ToReferenced().template Scan<Acc>(forward<M>(initMapper), forward<F>(combiner)).Last();
		}


		/// [N calls for input length N.]
		/// @param initVal: the initial value for accumulator
		/// @returns		initVal directly in case of an empty sequence
		template <class Acc = void, class Init, class F>
		decltype(auto) Aggregate(Init&& initVal, F&& combiner, IfInitByValue<Acc, Init> = 0) const
		{
			// CONSIDER: Separate implementation could avoid Init copy - along with its whole copyable requirement, which is naturally set by Scan.
			return ToReferenced()
				  .template Scan<Acc>(Init { initVal }, forward<F>(combiner))
				  .LastIfAny()
				  .OrDefault(forward<Init>(initVal));
		}

	#pragma endregion


	// =========== Arithmetics (chaining & terminal) =================================================================================
	#pragma region

		// CONSIDER: This form of min/max search is the most powerful, but uses more memory for a non-uniqie sequence.
		//			 A more performant 'SingleMinimum()' or 'MinimalFirst()' could be added.
		//			 Alternatively: Enumerators could provide some internal trick (FetchFirstOnly?) as an optional feature.

		template <class Comp = std::less<>>		auto Minimums(Comp&& isLess = {}) const &	{ return   Chain<MinSeekEnumerator>(BinPred<Comp>(isLess)); }
		template <class Comp = std::less<>>		auto Minimums(Comp&& isLess = {}) &&		{ return MvChain<MinSeekEnumerator>(BinPred<Comp>(isLess)); }

		template <class Comp = std::less<>>		auto Maximums(Comp&& isLess = {}) const &	{ return		Minimums(SwappedBinop(BinPred<Comp>(isLess))); }
		template <class Comp = std::less<>>		auto Maximums(Comp&& isLess = {}) &&		{ return Move().Minimums(SwappedBinop(BinPred<Comp>(isLess))); }

		template <class TProp = void, class P>	auto MinimumsBy(P&& toMinimize)				  const &	{ return		Minimums(ComparatorForProperty<P, TProp>(toMinimize)); }
		template <class TProp = void, class P>	auto MinimumsBy(P&& toMinimize)				  &&		{ return Move().Minimums(ComparatorForProperty<P, TProp>(toMinimize)); }
		template <class TProp>					auto MinimumsBy(ConstOverloadTo<TProp> toMin) const &	{ return				 MinimumsBy<TProp, ConstOverloadTo<TProp>>(move(toMin)); }
		template <class TProp>					auto MinimumsBy(ConstOverloadTo<TProp> toMin) &&		{ return Move().template MinimumsBy<TProp, ConstOverloadTo<TProp>>(move(toMin)); }

		template <class TProp = void, class P>	auto MaximumsBy(P&& toMaximize)				  const &	{ return		Maximums(ComparatorForProperty<P, TProp>(toMaximize)); }
		template <class TProp = void, class P>	auto MaximumsBy(P&& toMaximize)				  &&		{ return Move().Maximums(ComparatorForProperty<P, TProp>(toMaximize)); }
		template <class TProp>					auto MaximumsBy(ConstOverloadTo<TProp> toMax) const &	{ return				 MaximumsBy<TProp, ConstOverloadTo<TProp>>(move(toMax)); }
		template <class TProp>					auto MaximumsBy(ConstOverloadTo<TProp> toMax) &&		{ return Move().template MaximumsBy<TProp, ConstOverloadTo<TProp>>(move(toMax)); }

		/// Sort elements in order defined by comparison function
		template <class Comp = std::less<>> 	auto Order(Comp&& isLess = {})				const & { return   Chain<SorterEnumerator>(forward<Comp>(isLess)); }
		template <class Comp = std::less<>> 	auto Order(Comp&& isLess = {})				&&		{ return MvChain<SorterEnumerator>(forward<Comp>(isLess)); }

		/// In order by the value of a selected property
		template <class TProp = void, class P>	auto OrderBy(P&& getProperty)				const &	{ return		Order(ComparatorForProperty<P, TProp>(getProperty)); }
		template <class TProp = void, class P>	auto OrderBy(P&& getProperty)				&&		{ return Move().Order(ComparatorForProperty<P, TProp>(getProperty)); }
		template <class TProp>					auto OrderBy(ConstOverloadTo<TProp> getter)	const &	{ return				 OrderBy<TProp, ConstOverloadTo<TProp>>(move(getter)); }
		template <class TProp>					auto OrderBy(ConstOverloadTo<TProp> getter)	&&		{ return Move().template OrderBy<TProp, ConstOverloadTo<TProp>>(move(getter)); }

		/// Find extreme value, if not Empty.
		template <class Comp = std::less<>> 	Optional<TElemDecayed>	Min(Comp&& isLess = {}) const;
		template <class Comp = std::less<>> 	Optional<TElemDecayed>	Max(Comp&& isLess = {}) const	{ return Min(SwappedBinop(isLess)); }

		template <class S = TElemDecayed>		Optional<S>				Avg() const;
		template <class S = TElemDecayed>		S						Sum() const;

	#pragma endregion


	// =========== Materialization / Lifetime-utils ==================================================================================
	#pragma region
		
		// ----- Container creators --------------------------------------------------------------------------------------------------

		// NOTE: For the customizability of the resulting containers, any further constructor arguments
		//		 (e.g. allocators or non-default hash algorithms) can be passed down for construction
		//		 after the commonly accepted "sizeHint".
		//		 Separate overloads are provided to instantiate with actual argument objects (stateful strategies),
		//		 and to specify type arguments only (stateless strategies), which will be default constructed.
		//
		//		 Mixing the two styles (having some arg objects but more type args to be default constructed)
		//		 is not supported - would be solvable by having 2 packs and using auto return type, but that
		//		 would be too ugly for minimal benefit.


		/// Form a List from sequence elements.
		/// @tparam Options:  Additional arguments for ListType
		template <class... Options>
		ListType<TElemDecayed, Options...>			ToList(size_t sizeHint = 0) const;
		
		/// Form a List with predefined inline buffer for N elements.
		/// @tparam N:		  size of inline buffer
		/// @tparam Options:  Additional arguments for SmallListType
		template <size_t N, class... Options>
		SmallListType<TElemDecayed, N, Options...>	ToList(size_t sizeHint = N) const;

		/// Form a Set of distinct elements.
		/// Can be ordered or based on hash, according to configuration.
		/// @tparam Options:  Additional arguments for SetType
		///					  (typ.: Hasher, Equality comparer, Allocator)
		template <class... Options>	
		SetType<TElemDecayed, Options...>			ToSet(size_t sizeHint = 0) const;


		/// Map sequence elements to unique keys, forwarding them as a whole into values of a Dictionary.
		/// @tparam Options:  Additional arguments for DictionaryType
		/// @param  makeKey:  TElem& -> Key mapper function
		template <class... Options, class KeyMap>
		auto ToDictionary(KeyMap&& toKey, size_t sizeHint = 0)			 const -> DictionaryType<DecayedResultLV<decltype(KeyMapper(toKey))>,
																								 TElemDecayed,
																								 Options...>;
		
		/// Map sequence elements to unique keys by a pointer to possibly const-overloaded getter.
		/// @tparam  K:		  Explicit type of keys (required)
		template <class K, class... Options>
		auto ToDictionaryOf(LVOverloadTo<K> getKey, size_t sizeHint = 0) const -> DictionaryType<decay_t<K>, TElemDecayed, Options...>;


		/// Form a custom Dictionary.
		/// @tparam Options:  Additional arguments for DictionaryType
		/// @param  k:		  TElem& -> Key   mapper function
		/// @param  v:		  TElem  -> Value mapper function
		template <class... Options, class KeyMap, class ValMap>
		auto ToDictionary(KeyMap&& k, ValMap&& v, size_t sizeHint = 0)	 const -> DictionaryType<DecayedResultLV<decltype(KeyMapper(k))>,
																								 DecayedResult<decltype(ValueMapper(v))>,
																								 Options...>;

		/// Form a custom Dictionary resolving const/ref-overloaded getters of TElem. [No mix with lambdas atm.]
		/// @tparam Options:  Additional arguments for DictionaryType
		/// @tparam  K:		  Explicit type of keys   (required)
		/// @tparam  V:		  Explicit type of values (required)
		template <class K, class V, class... Options>
		auto ToDictionaryOf(LVOverloadTo<K> getKey, OverloadTo<V> getValue, size_t sizeHint = 0) const -> DictionaryType<decay_t<K>, decay_t<V>, Options...>;



			// ----- Overloads with Options... deduced from (possibly stateful) parameters. -----

		template <class... Options>
		ListType<TElemDecayed, Options...>			ToList(size_t sizeHint, const Options&...) const;
		
		template <size_t N, class... Options>
		SmallListType<TElemDecayed, N, Options...>	ToList(size_t sizeHint, const Options&...) const;

		template <class... Options>
		SetType<TElemDecayed, Options...>			ToSet(size_t sizeHint, const Options&...) const;


		template <class... Options, class KeyMap>
		auto ToDictionary(KeyMap&& k, size_t sizeHint, const Options&...)				const -> DictionaryType<DecayedResultLV<decltype(KeyMapper(k))>,
																												TElemDecayed,
																												Options...>;
		template <class K, class... Options>
		auto ToDictionaryOf(LVOverloadTo<K>, size_t sizeHint, const Options&...)		const -> DictionaryType<decay_t<K>, TElemDecayed, Options...>;


		template <class... Options, class KeyMap, class ValMap>
		auto ToDictionary(KeyMap&& k, ValMap&& v, size_t sizeHint, const Options&...)	const -> DictionaryType<DecayedResultLV<decltype(KeyMapper(k))>,
																												DecayedResult<decltype(ValueMapper(v))>,
																												Options...>;
		template <class K, class V, class... Options>
		auto ToDictionaryOf(LVOverloadTo<K>, OverloadTo<V>, size_t sizeHint, const Options&...) const -> DictionaryType<decay_t<K>, decay_t<V>, Options...>;



		// ----- Lifetime tools ------------------------------------------------------------------------------------------------------

		/// Evaluate current query and pass it as a self-contained enumeration (an abstract collection).
		template <class Output = TElem>
		auto ToMaterialized()  const;
		
		/// Cache calculation results (For & elements => not totally self-contained!)
		auto ToSnapshot()	   const;

		/// Fork a temporary instance referencing this (just like a container), when a heavy copy would be undesired.
		/// @remarks	
		///		Mostly an internal tool for utilizing existing Enumerators to implement terminal operations concisely.
		///		CONSIDER: for usage somehow at parameters, like EnumerableRef<T> ?
		auto ToReferenced() const &  noexcept
		{
			auto proxy = [this]() { return GetEnumerator(); };
			return AutoEnumerable<decltype(proxy)> { move(proxy), isPure, false };
		}

		auto ToReferenced()	&& = delete;

	#pragma endregion


	// =========== Type-erasure ======================================================================================================
	#pragma region

		/// Convert to interfaced type: Enumerable<TElem>
		/// @remarks: this may incur heap allocation, and virtual calls upon enumerating the result.
		Enumerable<TElem> ToInterfaced() const &
		{
			// Should not get called implicitly for matching type, since copy ctor is also available
			static_assert (!IsInterfacedEnumerator<TEnumerator>::value, "Already an Enumerable<TElem>.");

			// We need a copy here!  Alias prevents capturing "this"
			const auto& factoryAlias = factory;
			return Enumerable<TElem> {
				[factoryAlias]() { return InterfacedEnumerator<TElem> { factoryAlias }; },
				isPure
			};
		}

		Enumerable<TElem> ToInterfaced() &&
		{
			static_assert (!IsInterfacedEnumerator<TEnumerator>::value, "Already an Enumerable<TElem>.");

			return Enumerable<TElem> {
				[fact = move(factory)]() { return InterfacedEnumerator<TElem> { fact }; },
				isPure
			};
		}


	// ----- Auto-conversions to Enumerable<T>  [incl. Elem conversions] -------------------------------------------------------------

		// NOTE: Implementing these as operators would be (almost) as simple as:
		//		 operator Enumerable<TElem>() const &	{ return ToInterfaced(); }
		//		 operator Enumerable<TElem>() &&		{ return Move().ToInterfaced(); }
		//
		//		 But Clang refuses to apply the moving versions for returning RVO-eligible locals, complying with the standard
		//		 ==> hence the use of conversion ctors (in couples, else the cont& operator would get preferred in some cases)!


		/// Autoconvert to interfaced type - possibly qualifying or decaying the elements.
		/// @remarks
		///		Supported element conversions:
		///			V& --> const V&,
		///			V* --> const V*,
		///			V  --> const V,
		///			[const] V& --> V (copy as pr-values)
		///
		///		Since the conversion is implicit only to interfaced Enumerable<V> targets,
		///		with type V being well seen, copying the elements should have enough explicitness.
		///
		///		Covariance seems not to be viable, because it hinders overload resolution.
		///
		template <class F, class = enable_if_t<IsInterfacedConversion<InvokeResultT<F>, TEnumerator>::any>>
		AutoEnumerable(const AutoEnumerable<F>& src) :
			AutoEnumerable { src.template As<TElem>().ToInterfaced().PassFactory(), src.isPure }
		{
		}

		template <class F, class = enable_if_t<IsInterfacedConversion<InvokeResultT<F>, TEnumerator>::any>>
		AutoEnumerable(AutoEnumerable<F>&& src) :
			AutoEnumerable { move(src).template As<TElem>().ToInterfaced().PassFactory(), src.isPure }
		{
		}


		/// Autoconvert to Enumerable:	V (prvalue) --> const V&
		/// @remarks
		///								!This conversion is debatable!
		///		Mimicks standard compiler behaviour for const refs, however, we need to "Materialize"
		///		the sequence here, to extend the lifetime of every element (causing heap allocation).
		///
		///		Morover, it is rather unexpected that the referred elements won't survive the Enumerable.
		///
		///		This makes Enumerable<const V&> parameters kind of universal, but
		///		for small types Enumerable<V> should be preferred on consumer side.
		template <
			class V = TElem,
			class   = enable_if_t<is_same_v<V, TElem> && !is_reference_v<V>>
		>
		operator Enumerable<const BaseT<V>&>() const
		{
			return ToMaterialized<const BaseT<TElem>&>().ToInterfaced();
		}

		#pragma endregion

	};

	template <class Fact>	
	AutoEnumerable(Fact&&, bool = true, bool = true) -> AutoEnumerable<Fact>;




#pragma region Source creators

	#pragma region Generate sequences

	/// A 0-length sequence of any type is gladly provided.
	template <class V>
	auto Empty()
	{
		return AutoEnumerable { []() { return EmptyEnumerator<V> {}; } };
	}


	/// Infinitely repeat a value/reference.
	template <class V = void, class Vin>
	auto Repeat(Vin&& val)
	{
		using Storage = typename SeededEnumerationTypes<Vin, V>::SeedStorage;
		using Output  = typename SeededEnumerationTypes<Vin, V>::Output;

		struct InfRepeaterFactory {
			Storage seed;

			RepeaterEnumerator<Storage, Output>   operator ()() const   { return { seed }; }
		};

		return AutoEnumerable { InfRepeaterFactory { forward<Vin>(val) } };
	}


	/// Repeat a value/reference the given number of times.
	template <class V = void, class Vin>
	auto Repeat(Vin&& val, size_t count)
	{
		using Storage = typename SeededEnumerationTypes<Vin, V>::SeedStorage;
		using Output  = typename SeededEnumerationTypes<Vin, V>::Output;

		struct RepeaterFactory {
			Storage	seed;
			size_t	count;

			CounterEnumerator<RepeaterEnumerator<Storage, Output>>  operator ()() const
			{
				auto createRepeater = [this]() { return RepeaterEnumerator<Storage, Output>{ seed }; };
				return { createRepeater, FilterMode::TakeWhile, count };
			}
		};

		return AutoEnumerable { RepeaterFactory { forward<Vin>(val), count } };
	}


	/// A single-element sequence (value/reference).
	template <class V = void, class Vin>
	auto Once(Vin&& val)
	{
		return Repeat<V>(forward<Vin>(val), 1u);
	}


	/// Custom infinite sequence specified by the starting value/reference and a step function.
	/// @remarks
	///		Deduced mode: 	Result = accumulator = result_of @p step
	///		Explicit more:	Result = accumulator = V
	///		Usages:
	///			A ) Sequence(5, x => x*7)						---> store/give prvalue
	///			A') Sequence(n, x => x*7)						---> store & / give prvalue
	///			B ) Sequence(headObj, (auto& o) => o.nNext())	---> store/give &
	template <class V = void, class StepFun, class Vin>
	auto Sequence(Vin&& seed, StepFun&& step)
	{
		using SeedStorage = typename SeededEnumerationTypes<Vin>::SeedStorage;
		using StepStorage = BaseT<StepFun>;
		using StepRes	  = MappedT<OverrideT<V, const Vin&>, const StepStorage&>;
		using Acc		  = OverrideT<V, StepRes>;

		struct SequenceFactory {
			SeedStorage	seed;
			StepStorage	step;

			SequenceEnumerator<Acc, StepStorage>	operator ()() const
			{
				return { static_cast<Acc>(seed), step };	// Narrowing allowed if explicit
			}
		};

		return AutoEnumerable { SequenceFactory { forward<Vin>(seed), forward<StepFun>(step) } };
	}


	/// Range of given length generated by operator++ [value capture only].
	template <class V>
	auto Range(V start, size_t count)
	{
		// using op++ is not very functional, but gives more abstraction than "+ 1" literal
		auto advance = [](auto x) { return ++x; };

		return Sequence<V>(move(start), advance).Take(count);
	}


	/// Simple range of numbers in [0, count).
	template <class V = size_t>
	auto Range(size_t count)
	{
		return Range<V>(V {}, count);
	}


	/// Range of given length generated by operator-- [value capture only].
	template <class V>
	auto RangeDown(V start, size_t count)
	{
		auto advance = [](auto x) { return --x; };

		return Sequence<V>(move(start), advance).Take(count);
	}


	/// Range stepped by operator++ until the given closing element reached (inclusive). [value capture]
	template <class V>
	auto RangeBetween(V first, V last)
	{
		auto advance = []				(V x)			{ return ++x; };
		auto stop	 = [l = move(last)]	(const V& x)	{ return x == l; };

		return Sequence<V>(move(first), advance).TakeUntilFinal(stop);
	}


	/// Range stepped by operator-- until the given closing element reached (inclusive). [value capture]
	template <class V>
	auto RangeDownBetween(V first, V last)
	{
		auto advance = []				(V x)			{ return --x; };
		auto stop	 = [l = move(last)]	(const V& x)	{ return x == l; };

		return Sequence<V>(move(first), advance).TakeUntilFinal(stop);
	}


	/// Indices of a sequence container queried at start of enumeration. [Ref capture]
	/// @remarks	For a snapshot consider Range(list.size()).
	template <class V = size_t, class Container>
	auto IndexRange(Container& list)
	{
		static_assert (!is_reference<V>(), "Index type should not be reference.");

		struct IndexRangeFactory {
			const Container&  list;

			auto operator ()() const
			{
				auto advance  = [](V x)	{ return ++x; };

				using SeqGen  = SequenceEnumerator<V, decltype(advance)>;
				auto startSeq = [&]()	{ return SeqGen { 0, advance }; };

				return CounterEnumerator<SeqGen> {
					startSeq,
					FilterMode::TakeWhile,
					GetSize(list)
				};
			}
		};
		return AutoEnumerable { IndexRangeFactory { list } };
	}


	/// Descending indices of a sequence container according to its size at start of enumeration. [Ref capture]
	/// @remarks	For a snapshot consider RangeDownBetween(list.size() - 1, 0).
	template <class V = size_t, class Container>
	auto IndexRangeReversed(Container& list)
	{
		static_assert (!is_reference<V>(), "Index type should not be reference.");

		struct RevIndexRangeFactory {
			const Container&  list;

			auto operator ()() const
			{
				auto advance  = [](V x)	{ return --x; };

				using SeqGen  = SequenceEnumerator<V, decltype(advance)>;
				auto startSeq = [&]()	{ return SeqGen { GetSize(list) - 1, advance }; };

				return CounterEnumerator<SeqGen> {
					startSeq,
					FilterMode::TakeWhile,
					GetSize(list)
				};
			}
		};
		return AutoEnumerable { RevIndexRangeFactory { list } };
	}

	#pragma endregion




	#pragma region Wrap Containers by Reference

	/// Shortcuts to enable some generic code (to Enumerate either a container or any AutoEnumerable)
	template <class ForcedResult = void, class Fact, class = EnumeratedT<decltype(declval<Fact>()())>>
	auto Enumerate(const AutoEnumerable<Fact>& eb)
	{
		// note: unnecessary conversions are bypassed inside As
		return eb.template As<OverrideT<ForcedResult, typename AutoEnumerable<Fact>::TElem>>();
	}

	template <class ForcedResult = void, class Fact, class = EnumeratedT<decltype(declval<Fact>()())>>
	auto Enumerate(AutoEnumerable<Fact>& eb)
	{
		return eb.template As<OverrideT<ForcedResult, typename AutoEnumerable<Fact>::TElem>>();
	}

	template <class ForcedResult = void, class Fact, class = EnumeratedT<decltype(declval<Fact>()())>>
	auto Enumerate(AutoEnumerable<Fact>&& eb)
	{
		return move(eb).template As<OverrideT<ForcedResult, typename AutoEnumerable<Fact>::TElem>>();
	}



	/// Wrap any container by reference.
	template <class ForcedResult = void, class Container>
	auto Enumerate(Container& cont)
	{
		return AutoEnumerable {
			[&cont]() { return CreateEnumeratorFor<ForcedResult>(cont); },
			true, false
		};
	}


	/// Wrap any container by move. The result is self-contained (Materialized).
	template <
		class ForcedResult = void,
		class Container,
		enable_if_t<!is_lvalue_reference_v<Container>
				 && !IsSpeciallyTreatedContainer<Container>::value, int> = 0
	>
	auto Enumerate(Container&& cont)
	{
		using Elem = IterableT<const Container&>;
		ContainerWrapChecks::ByRef<Elem, OverrideT<ForcedResult, Elem>>();
		return AutoEnumerable {
			[c = move(cont)]() { return CreateEnumeratorFor<ForcedResult>(c); }
		};
	}



	/// Taking a created pair of iterators.
	/// @remarks
	///		Not preferred for containers, since
	///		 * increases capture size
	///		   (~ the chances of malloc by std::function if converted to interfaced Enumerable<T>)
	///		 * iterators can invalidate on a simple Push/Add/Delete...
	///		 * a container itself is an iterator-factory
	template <class ForcedResult = void, class TBegin, class TEnd>
	auto Enumerate(TBegin begin, TEnd end)
	{
		return AutoEnumerable {
			[b = move(begin), e = move(end)]() { return IteratorEnumerator<TBegin, TEnd, ForcedResult> { b, e }; }
		};
	}

	#pragma endregion




	#pragma region Initializer support

	// Helper: provides the  R* -> R&  "capture-syntax" on request.
	// Could work publicly IF enumerated type was always provided explicitly.
	template <class R, class A, class I>
	auto InitEnumerable(std::initializer_list<I>&& init, const A& alloc)
	{
		static_assert (!is_reference<R>() || is_pointer<I>(), "Supply pointers to output references.");
		
		// NOTE: List-init support is assumed only here for ListType! Is it expectable?
		if constexpr (is_reference<R>())
			return Enumerate<remove_reference_t<R>*>(BracedInitWithOptionalAlloc(init, alloc)).Dereference();
		else
			return Enumerate<R>(BracedInitWithOptionalAlloc(init, alloc));
	}



	// Implicit type deduction is achieved by 4 additional, public wrappers.
	//	Explicit Result type => braced-initializer elems can be converted
	//							Note subcase: init-list type already deduced but needing conversion should work!
	//										  Important for indirect usage, like Concat.
	//	void Result type	 => initializer must be deducable -> pointer usage defaults to "capture-syntax"

	template <class ForcedResult>
	using IfInitRefs   = enable_if_t<is_reference_v<ForcedResult>, int>;
	template <class ForcedResult>
	using IfInitValues = enable_if_t<!is_reference_v<ForcedResult> && !is_void_v<ForcedResult>, int>;

	template <class ForcedResult, class I>
	using IfInitDeducedRefs   = enable_if_t< std::is_pointer_v<I> &&
											(	is_void_v<ForcedResult>
											||	is_reference_v<ForcedResult> &&
												is_convertible_v<remove_pointer_t<I>&, ForcedResult>),
											int >;
	template <class ForcedResult, class I>
	using IfInitDeducedValues = enable_if_t< !std::is_pointer_v<I> &&
											 (is_void_v<ForcedResult> || is_convertible_v<I, ForcedResult>),
											 int >;


	/// Take explicitly typed values from braced initializer. (Explicit type allows conversions.)
	template <class ForcedResult, class CustomAllocator = None,
			  IfInitValues<ForcedResult> = 0>
	auto Enumerate(std::initializer_list<NoDeduce<ForcedResult>>&& init, const CustomAllocator& alloc = {})
	{
		return InitEnumerable<ForcedResult>(move(init), alloc);
	}

	/// Take explicitly typed references from braced initializer. Use pointers as "capture-syntax".
	/// (Explicit type allows conversions, thus usage of interfaces.)
	template <class ForcedResult, class CustomAllocator = None,
			  IfInitRefs<ForcedResult> = 0>
	auto Enumerate(std::initializer_list<remove_reference_t<ForcedResult>*>&& init, const CustomAllocator& alloc = {})
	{
		return InitEnumerable<ForcedResult>(move(init), alloc);
	}


	/// Take implicitly typed values (pointers excluded) from braced initializer.
	template <class ForcedResult = void, class CustomAllocator = None,
			  class T, IfInitDeducedValues<ForcedResult, T> = 0>
	auto Enumerate(std::initializer_list<T>&& init, const CustomAllocator& alloc = {})
	{
		return InitEnumerable<OverrideT<ForcedResult, T>>(move(init), alloc);
	}

	/// Take implicitly typed references from braced initializer, using pointers as "capture-syntax".
	template <class ForcedResult = void, class CustomAllocator = None,
			  class T, IfInitDeducedRefs<ForcedResult, T*> = 0>
	auto Enumerate(std::initializer_list<T*>&& init, const CustomAllocator& alloc = {})
	{
		return InitEnumerable<OverrideT<ForcedResult, T&>>(move(init), alloc);
	}


	// For lvalue initializer lists (often used in template tricks), does not copy elements!
	template <class ForcedResult = void, class T>
	auto Enumerate(std::initializer_list<T>& cont)
	{
		using It = typename std::initializer_list<T>::iterator;
		return AutoEnumerable {
			[&cont]() { return IteratorEnumerator<It, It, ForcedResult> { cont.begin(), cont.end() }; }
		};
	}

	#pragma endregion

#pragma endregion




#pragma region Shorthands + Free functions

	/// Two sequences have equal elements (according to operator ==).
	template <class A, class B>
	bool AreEqual(A&& lhs, B&& rhs);

	template <class I, class B>
	bool AreEqual(std::initializer_list<I>&& lhs, B&& rhs);

	template <class I, class A>
	bool AreEqual(A&& lhs, std::initializer_list<I>&& rhs);



	#pragma region Concat

	template <class TForced = void, class TCommon0 = TForced, class C1, class C2, class... CMore>
	auto ConcatInternal(C1&&, C2&&, CMore&&...);

	template <class TForced = void, class TCommon0 = TForced, class C1, class C2>
	auto ConcatInternal(C1&&, C2&&);


	template <class TForced>
	using DefaultInitT = conditional_t<is_reference_v<TForced>, remove_reference_t<TForced>*, TForced>;



	// NOTE: Inline braced-init-list syntax can be handy to append/prepend some fixed elements - but requires explicit overloads.
	//		 Those are kindly provided for up to 3(+) parameters. Otherwise Concat(Enumerate({...}), ...), or named (and moved)
	//		 "auto" variables can be used. (Unfortunately parameter packs can only be deduced at the end of parameter list.)

	template <class TForced = void, class... Containers>
	auto Concat(Containers&&... conts)
	{
		return ConcatInternal<TForced>(forward<Containers>(conts)...);
	}

	// NOTE: Concatenating init lists might be debatable - might be useful for template code.
	template <class TForced = void, class... Is>
	auto Concat(std::initializer_list<Is>&&... inits)
	{
		return ConcatInternal<TForced>(move(inits)...);
	}


	template <class TForced = void, class I1, class... Containers, class = IfVoid<TForced>>
	auto Concat(std::initializer_list<I1>&& iList1, Containers&&... tail)
	{
		return ConcatInternal<TForced>(move(iList1), forward<Containers>(tail)...);
	}
	template <class TForced, class... Containers>
	auto Concat(std::initializer_list<DefaultInitT<TForced>>&& iList1, Containers&&... tail)
	{
		return ConcatInternal<TForced>(move(iList1), forward<Containers>(tail)...);
	}


	template <class TForced = void, class C1, class I2, class... Containers, class = IfVoid<TForced>>
	auto Concat(C1&& cont1, std::initializer_list<I2>&& iList2, Containers&&... tail)
	{
		return ConcatInternal<TForced>(forward<C1>(cont1), move(iList2), forward<Containers>(tail)...);
	}
	template <class TForced, class C1, class... Containers>
	auto Concat(C1&& cont1, std::initializer_list<DefaultInitT<TForced>>&& iList2, Containers&&... tail)
	{
		return ConcatInternal<TForced>(forward<C1>(cont1), move(iList2), forward<Containers>(tail)...);
	}



	template <class TForced = void, class C1, class C2, class I3, class... Containers, class = IfVoid<TForced>>
	auto Concat(C1&& cont1, C2&& cont2, std::initializer_list<I3>&& ilist3, Containers&&... tail)
	{
		return ConcatInternal<TForced>(forward<C1>(cont1), forward<C2>(cont2), move(ilist3), forward<Containers>(tail)...);
	}
	template <class TForced, class C1, class C2, class... Containers>
	auto Concat(C1&& cont1, C2&& cont2, std::initializer_list<DefaultInitT<TForced>>&& ilist3, Containers&&... tail)
	{
		return ConcatInternal<TForced>(forward<C1>(cont1), forward<C2>(cont2), move(ilist3), forward<Containers>(tail)...);
	}


	template <class TForced = void, class I1, class I2, class... Containers, class = IfVoid<TForced>>
	auto Concat(std::initializer_list<I1>&& iList1, std::initializer_list<I2>&& iList2, Containers&&... tail)
	{
		return ConcatInternal<TForced>(move(iList1), move(iList2), forward<Containers>(tail)...);
	}
	template <class TForced, class... Containers>
	auto Concat(std::initializer_list<DefaultInitT<TForced>>&& iList1, std::initializer_list<DefaultInitT<TForced>>&& iList2, Containers&&... tail)
	{
		return ConcatInternal<TForced>(move(iList1), move(iList2), forward<Containers>(tail)...);
	}

	template <class TForced = void, class C1, class I2, class I3, class... Containers, class = IfVoid<TForced>>
	auto Concat(C1&& cont1, std::initializer_list<I2>&& ilist2, std::initializer_list<I3>&& ilist3, Containers&&... tail)
	{
		return ConcatInternal<TForced>(forward<C1>(cont1), move(ilist2), move(ilist3), forward<Containers>(tail)...);
	}
	template <class TForced, class C1, class... Containers>
	auto Concat(C1&& cont1, std::initializer_list<DefaultInitT<TForced>>&& ilist2, std::initializer_list<DefaultInitT<TForced>>&& ilist3, Containers&&... tail)
	{
		return ConcatInternal<TForced>(forward<C1>(cont1), move(ilist2), move(ilist3), forward<Containers>(tail)...);
	}


	template <class TForced = void, class I1, class C2, class I3, class... Containers, class = IfVoid<TForced>>
	auto Concat(std::initializer_list<I1>&& iList1, C2&& cont2, std::initializer_list<I3>&& iList3, Containers&&... tail)
	{
		return ConcatInternal<TForced>(move(iList1), forward<C2>(cont2), move(iList3), forward<Containers>(tail)...);
	}
	template <class TForced, class C2, class... Containers>
	auto Concat(std::initializer_list<DefaultInitT<TForced>>&& iList1, C2&& cont2, std::initializer_list<DefaultInitT<TForced>>&& iList3, Containers&&... tail)
	{
		return ConcatInternal<TForced>(move(iList1), forward<C2>(cont2), move(iList3), forward<Containers>(tail)...);
	}

	#pragma endregion



	#pragma region Wrap+Query shorthands

	template <class C, class Pred = FreePredicatePtr<decay_t<IterableT<C>>>>
	bool AnyOf(C&& cont, Pred&& p)
	{
		return Enumerate(cont).Any(forward<Pred>(p));
	}

	template <class C, class Pred = FreePredicatePtr<decay_t<IterableT<C>>>>
	bool AllOf(C&& cont, Pred&& p)
	{
		return Enumerate(cont).All(forward<Pred>(p));
	}



	template <class C, class Pred = FreePredicatePtr<decay_t<IterableT<C>>>>
	decltype(auto)	FirstFrom(C&& cont, Pred&& p)
	{
		return Enumerate(cont).First(forward<Pred>(p));
	}

	template <class C, class Pred = FreePredicatePtr<decay_t<IterableT<C>>>>
	decltype(auto)	SingleFrom(C&& cont, Pred&& p)
	{
		return Enumerate(cont).Single(forward<Pred>(p));
	}

	template <class C, class Pred = FreePredicatePtr<decay_t<IterableT<C>>>>
	auto			SingleOrNoneFrom(C&& cont, Pred&& p)
	{
		return Enumerate(cont).SingleOrNone(forward<Pred>(p));
	}



	template <class C, class Pred = FreePredicatePtr<decay_t<IterableT<C>>>>
	auto Filter(C&& cont, Pred&& p)
	{
		return Enumerate(cont).Where(forward<Pred>(p));
	}

	template <class C, class Mapper>
	auto Map(C&& cont, Mapper&& m)
	{
		return Enumerate(cont).Map(forward<Mapper>(m));
	}

	template <class Result, class C, class Mapper>
	auto MapTo(C&& cont, Mapper&& m)
	{
		return Enumerate(cont).template MapTo<Result>(forward<Mapper>(m));
	}

	#pragma endregion

#pragma endregion


}	// namespace Enumerables::Def




namespace Enumerables {			

	// Published to client code:

	// -- interfaced enumerable --
	using Def::Enumerable;
	using Def::IEnumerator;
	using Def::InterfacedEnumerator;


	// -- source constructors --
	// wrap collection
	using Def::Enumerate;

	// generators
	using Def::Empty;
	using Def::Once;
	using Def::Repeat;
	using Def::IndexRange;
	using Def::IndexRangeReversed;
	using Def::Range;
	using Def::RangeDown;
	using Def::RangeBetween;
	using Def::RangeDownBetween;
	using Def::Sequence;

	// -- free helpers --
	using Def::AreEqual;

	// -- shorthands --
	using Def::AllOf;
	using Def::AnyOf;
	using Def::Concat;
	using Def::Filter;
	using Def::Map;
	using Def::MapTo;
	using Def::FirstFrom;
	using Def::SingleFrom;
	using Def::SingleOrNoneFrom;

}		// namespace Enumerables

#endif	// ENUMERABLES_INTERFACE_HPP
