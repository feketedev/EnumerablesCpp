#ifndef ENUMERABLES_CHAINTOOL_HPP
#define ENUMERABLES_CHAINTOOL_HPP

	/*  -----------------------------------------------------------------------------------------------  *
	 *  Part of Enumerables for C++.																	 *
	 *																									 *
	 *  This file contains some template-utilities to enable the most concise syntax					 *
	 *  achievable for chaining algorithms inline in AutoEnumerable methods.							 *
	 *	[Reason of being complicated is that more automated approaches ran into language limitations.]	 *
	 *																									 *
	 *  -----------------------------------------------------------------------------------------------  *
	 *	Usage requires the following conventions on the order of parameters:							 *
	 * 																									 *
	 * 	Chained Enumerators should take the form:														 *
	 * 		XYEnumerator<Source, Args..., PureTypeArgs...>												 *
	 * 																									 *
	 * 	with a constructor:																				 *
	 * 		XYEnumerator(SourceFactory& fact, const Args&... args, const SteadyArgs&... sargs)			 *
	 * 																									 *
	 * 		Where "args..." are custom templated arguments for XY's algorithm, stored in its factory,	 *
	 * 		and  "sargs..." are template-independent (fixed type) arguments - typically enums/numbers.	 *
	 * 		PureTypeArgs... are manually specified additional template type arguments.					 *
	 * 																									 *
	 * 		Source == decltype(fact()) is the enumerator supplying the input elements for XY.			 *
	 * 																									 *
	 * 		On this receiving side, PureTypeArgs and SteadyArgs are last to be able to have defaults.	 *
	 * 																									 *
	 *  -----------------------------------------------------------------------------------------------  *
	 * 	The intended way of chaining is via AutoEnumerable helper method:								 *
	 * 		this->Chain<XYEnumerator, PureTypeArgs...>(forward(args)...);								 *
	 * 		this->Chain<XYEnumerator, PureTypeArgs...>(SteadyParams(sargs...), forward(args)...);		 *
	 * 																									 *
	 * 	Which forwards the call to this file's entry point, passing its own factory as source:			 *
	 * 		ChainFactory<XYEnumerator, PureTypeArgs...>(sourceFactory, args...)							 *
	 * 		ChainFactory<XYEnumerator, PureTypeArgs...>(sourceFactory, SteadyParams(sargs...), args...)	 *
	 * 																									 *
	 *	[Call-side SteadyParams (i.e. sargs wrapped) must preceed args... due to language limitations.]  *
	 * 																									 *
	 * 	This results in a self-contained factory storing (sourceFactory, args, sargs) decayed.			 *
	 * 	Enumerators can store const&-s, since their life is tied to the creator AutoEnumerable.			 *
	 * 	Template type arguments for args... are injected implicitly to XYEnumerator.					 *
	 *  -----------------------------------------------------------------------------------------------  */


#include <tuple>
#include "Enumerables_GenericStorage.hpp"		// for StorableT -> StoreAllowingRef


namespace Enumerables {
namespace Def {


#pragma region Tuple-tools (internal)

	template<class... Elems>
	constexpr typename std::conjunction<std::is_reference<Elems>...>::type	IsReferenceOnlyTrait(const std::tuple<Elems...>&)
	{
		return {};
	}

	template<class... Elems>
	constexpr typename std::disjunction<std::is_reference<Elems>...>::type	HasReferenceTrait(const std::tuple<Elems...>&)
	{
		return {};
	}

	template<class Tuple>
	constexpr bool	IsReferenceOnlyTuple = decltype(IsReferenceOnlyTrait(std::declval<Tuple>()))::value;

	template<class Tuple>
	constexpr bool	IsGroundTuple		 = !decltype(HasReferenceTrait(std::declval<Tuple>()))::value;




	template<class... Elems>
	constexpr std::tuple<const Elems&...>		 ToConstRefTuple(const std::tuple<Elems...>& src)
	{
		return src;
	}

	template<class... Elems>
	constexpr std::tuple<std::decay_t<Elems>...>   ToGroundTuple(const std::tuple<Elems...>& src)
	{
		return src;
	}




	template <class... T>
	struct ArgStorage {
		std::tuple<T...> tuple;

		// NOTE: Idea is that lambdas on their own should be stateless - thus, storing them always by value into the factory
		//		 seems the most relyable solution. Enumerators can simply reference them, while the user is being able to
		//		 precisely regulate what to reference vs. copy into the factory explicitly by the standard capture block.
		//		 -- This is just a benign check though, can be eased on need. 
		//		 -> The first example needing to hack it around is Scan - see StoreAllowingRef.
		static_assert (IsGroundTuple<decltype(tuple)>, "Chained Factories need to store own copies!");
	};

#pragma endregion





#pragma region Enumerator construction (internal)

	template <template <class...> class Et,	class... TypeOnlyArgs,
			  class... SourceFactories,		size_t... FIndices,
			  class... ParamdArgs,			size_t... PIndices,
			  class... SteadyArgs,			size_t... SIndices	 >
	auto ConstructFromStorageImpl(const std::tuple<const SourceFactories&...>&	sourceFacts,	std::index_sequence<FIndices...>,
								  const ArgStorage<ParamdArgs...>&				pargs,			std::index_sequence<PIndices...>,
								  const ArgStorage<SteadyArgs...>&				sargs,			std::index_sequence<SIndices...>)
	{
		return Et<decltype(std::get<FIndices>(sourceFacts)())..., ParamdArgs..., TypeOnlyArgs...> {
			std::get<FIndices>(sourceFacts)...,
			std::get<PIndices>(pargs.tuple)...,
			std::get<SIndices>(sargs.tuple)...
		};
	}


	// Create:		Et<invokeResult<SourceFactories>..., decay_t<ParamdArgs>..., TypeOnlyArgs...>
	// by invoking	Et { SourceFactories..., ParamdArgs..., SteadyArgs... }
	template <template <class...> class Et,
			  class... TypeOnlyArgs,
			  class... SourceFactories,
			  class... ParamdArgs,
			  class... SteadyArgs		  >
	auto ConstructFromStorage(std::tuple<const SourceFactories&...> sourceFacts,
							  const ArgStorage<ParamdArgs...>&		pargs,
							  const ArgStorage<SteadyArgs...>&		sargs	   )
	{
		return ConstructFromStorageImpl<Et, TypeOnlyArgs...>(sourceFacts, std::index_sequence_for<SourceFactories...> {},
															 pargs,		  std::index_sequence_for<ParamdArgs...> {},
															 sargs,		  std::index_sequence_for<SteadyArgs...> {}		);
	}




	/// The heart of chained AutoEnumerables - the factory itself. Recursively contains the factories of upstream Enumerators.
	/// @tparam ChainedEnumerator:	to be constructed
	/// @tparam TArgsStorage:		ArgStorage for templated ctor arguments of ChainedEnumerator
	/// @tparam SteadyStorage:		ArgStorage for fixed (non-templated) ctor arguments of ChainedEnumerator
	/// @tparam PureTypeArgs:		optional type args for ChainedEnumerator to be passed independently from ctor params
	template <template <class...> class ChainedEnumerator, class SourceFactory, class TArgsStorage, class SteadyStorage, class... PureTypeArgs>
	struct ChainedFactory {

		/* stay movable! */
		/*const*/ SourceFactory		sourceFactory;				// - perf-critical to get copy-elision!
		/*const*/ TArgsStorage		parametrizedCtorArgs;
		/*const*/ SteadyStorage		steadyCtorArgs;

		static_assert (!std::is_reference<SourceFactory>(), "Factory of source Enumerator must be stored here.");

		auto operator ()() const
		{
			std::tuple<const SourceFactory&> factList { sourceFactory };
			return ConstructFromStorage<ChainedEnumerator, PureTypeArgs...>(factList, parametrizedCtorArgs, steadyCtorArgs);
		}
	};



	/// Same as ChainedFactory, but generalized to have multiple source-Enumerators (for: Zip, Concat etc.)
	/// @remarks
	///		Supersedes the simple ChainedFactory, yet it's kept in place for a tiny bit more readability in compile errors.
	template <template <class...> class ChainedEnumerator, class SourceFactoryStorage, class TArgsStorage, class SteadyStorage, class... PureTypeArgs>
	struct JoinerChainedFactory {

		/* stay movable! */
		/*const*/ SourceFactoryStorage	sourceFactories;		// - perf-critical to get copy-elision!
		/*const*/ TArgsStorage			parametrizedCtorArgs;
		/*const*/ SteadyStorage			steadyCtorParams;

		static_assert (!std::is_reference<SourceFactoryStorage>() && IsGroundTuple<SourceFactoryStorage>, "Factories must be stored here.");

		auto operator ()() const
		{
			// NOTE: could just use const tuple<Factories...>& if wouldn't keep simple ChainedFactory
			return ConstructFromStorage<ChainedEnumerator, PureTypeArgs...>(ToConstRefTuple(sourceFactories), parametrizedCtorArgs, steadyCtorParams);
		}
	};

#pragma endregion




#pragma region Factory construction

	/// Wrapper to separate fix-typed ctor arguments of a template Enumerator from those received by templated parameters.
	template <class... T>
	struct SteadyParamPack {
		std::tuple<T...> tuple;

		// Call-time helper - only references, either l- or r-value
		static_assert (IsReferenceOnlyTuple<decltype(tuple)>, "Need no copy at this stage!");
	};


	template <class T> struct IsSteadyParamPack;

	template <class... Types> struct IsSteadyParamPack<SteadyParamPack<Types...>> {
		constexpr static bool value = true;
	};

	template <class T> struct IsSteadyParamPack {
		constexpr static bool value = false;
	};


	inline constexpr SteadyParamPack<> NoSteadyParams()  { return { std::tuple<> {} }; }




	/// Creates a new NextEnumerator-factory chained over the supplied factory of previous-level (Source) enumerator.
	/// @tparam NextEnumerator:	algorithm template to be constructed - bound as NextEnumerator<Source, Args..., PureTypeArgs...>.
	/// @tparam PureTypeArgs:	Optional type-only arguments passed to NextEnumerator as last.
	/// @param  sourceFact:		upstream Enumerator factory - producing "Source".
	/// @param  args:			data to be stored into this factory -
	///							NextEnumerator gets them both as type arguments and as const& data during construction.
	/// @param  sargs:			SteadyParams(...) - further arguments likewise passed to ctor, but aren't template type arguments.
	/// @remarks
	///		All args are forwarded to the factory which stores them decayed.
	///		The factory is movable, but NextEnumerator's ctor must receive all data as const& for the invokation to be repeatable.
	///		Ultimately the factory returns NextEnumerator<Source, Args..., PureTypeArgs...> { args..., sargs... }.
	///
	template <template <class...> class NextEnumerator, class... PureTypeArgs,
			   class... Args, class... SteadyArgs,class SourceFact>
	auto ChainFactory(SourceFact&& sourceFact, SteadyParamPack<SteadyArgs...>&& sargs, Args&&... args)
	{
		// just against cryptic errors...
		static_assert (!std::is_lvalue_reference<SourceFact>() || std::is_copy_constructible<std::remove_reference_t<SourceFact>>(),
					   "Can't chain by copy: the source factory contains uncopiable elements!"		);
		static_assert (std::is_move_constructible<std::remove_reference_t<SourceFact>>(),
					   "Can't chain: the source factory contains unmovable and uncopiable elements!");

		using SF				  = typename std::remove_reference_t<SourceFact>;
		using ParametrizedStorage = ArgStorage<std::decay_t<Args>...>;
		using SteadyStorage		  = ArgStorage<std::decay_t<SteadyArgs>...>;

		return ChainedFactory<NextEnumerator, SF, ParametrizedStorage, SteadyStorage, PureTypeArgs...> {
			std::forward<SourceFact>(sourceFact),
			ParametrizedStorage		{ { std::forward<Args>(args)... } },
			SteadyStorage			{ std::move(sargs.tuple) }			// forwards by element
		};
	}


	/// Same as above overload but with no SteadyParams specified.
	template <template <class...> class NextEnumerator, class... PureTypeArgs,
			  class... Args, class SourceFact,
			  class = std::enable_if_t<sizeof...(Args) == 0 || !IsSteadyParamPack<std::tuple_element_t<0, std::tuple<Args..., void>>>::value>>
	auto ChainFactory(SourceFact&& sourceFact, Args&&... args)
	{
		return ChainFactory<NextEnumerator, PureTypeArgs...>(std::forward<SourceFact>(sourceFact), NoSteadyParams(), std::forward<Args>(args)...);
	}




	/// Same as ChainFactory but for connecting multiple source-Enumerators.
	/// @param sourceFactories:	 forwarding tuple of source-Enumerators to be moved/copied into next factory (i.e. AutoEnumerable)
	template <template <class...> class NextEnumerator, class... PureTypeArgs,
			  class... FactoryRefs, class... SteadyArgs, class... Args>
	auto JoinFactories(const std::tuple<FactoryRefs...>& sourceFactories, SteadyParamPack<SteadyArgs...>&& sargs, Args&&... args)
	{
		static_assert (sizeof...(FactoryRefs) >= 2, "Currently not used for simple chaining. Bad call or Use ChainFactory instead.");
		static_assert (IsReferenceOnlyTuple<decltype(sourceFactories)>, "Do not copy factories here!");

		using FactoryStorage	  = decltype(ToGroundTuple(sourceFactories));
		using ParametrizedStorage = ArgStorage<std::decay_t<Args>...>;
		using SteadyStorage		  = ArgStorage<std::decay_t<SteadyArgs>...>;

		return JoinerChainedFactory<NextEnumerator, FactoryStorage, ParametrizedStorage, SteadyStorage, PureTypeArgs...> {
			sourceFactories,
			ParametrizedStorage	{ { std::forward<Args>(args)... } },
			SteadyStorage		{ std::move(sargs.tuple) }			// forwards by element
		};
	}


	/// Same as above overload but with no SteadyParams specified.
	template <template <class...> class NextEnumerator, class... PureTypeArgs, class FactoryRefsTuple, class... Args,
			  class = typename std::enable_if_t<!IsSteadyParamPack<std::tuple_element_t<0, std::tuple<Args..., void>>>::value>>
	auto JoinFactories(const FactoryRefsTuple& sourceFactories, Args&&... pargs)
	{
		return JoinFactories<NextEnumerator, PureTypeArgs...>(sourceFactories, NoSteadyParams(), std::forward<Args>(pargs)...);
	}

#pragma endregion




#pragma region Interface Helpers

	/// Use this wrapper to separates fix-typed ctor arguments from those received by templated parameters - enabling 
	/// AutoEnumerable::Chain / ChainFactory to determine the exact type arguments for the Enumerator to instantiate.
	/// Must be Chain's 1st argument if present.
	template <class... T>
	SteadyParamPack<T&&...>   SteadyParams(T&&... args)
	{
		return { std::tuple<T&&...> { std::forward<T>(args)... } };
	}


	/// Initialize a stored parameter in the chained factory by a universal reference source,
	/// allowing & storage or type conversion only if specified explicitly.
	/// @remarks
	///		Using StorableT for & arguments is an opaque workaround for ArgStorage / ChainFactory decays all input expecting lambdas.
	///		Constructed Enumerator must accept StorableT. Current usage is scarce.
	template <class Src, class ToInit>
	auto StoreAllowingRef(Src& x) -> std::conditional_t< std::is_same<std::decay_t<Src>, std::decay_t<ToInit>>::value
														 && !std::is_reference<ToInit>::value,
														 Src&&,
														 TypeHelpers::StorableT<ToInit> >
	{
		static_assert (!std::is_lvalue_reference<ToInit>() || std::is_lvalue_reference<Src>(),
					   "Requested lvalue reference to an rvalue.");
		static_assert (!std::is_reference<ToInit>() || TypeHelpers::IsRefCompatible<ToInit, Src>,
					   "The specified types are not reference-compatible.");

		return std::forward<Src>(x);
	}

#pragma endregion


}		// namespace Def
}		// namespace Enumerables

#endif	// ENUMERABLES_CHAINTOOL_HPP
