# Enumerables for C++

This header-only library aims to bring the features of .Net's Linq library to C++14 (and newer),
to enable a more declarative / functional coding style.

> &#9888; *The **master** branch preserves **compatibility with C++14.** \
> &emsp;&ensp;&thinsp;Unless being constrained by an old compiler, the recommended branch is clean17!*


The library follows a pragmatic approach with the core objectives of:
 * provide as concise and **fluently readable** syntax as possible
 * by automatically deducing **convenient defaults** (for type parameters) wherever possible
 * still, have the ability to override those manually
 * **prioritize convenience**, but try to minimize overhead too (often by providing a way to opt out)
 * depend solely on STL &ndash; but **allow configuration** to use custom container types in algorithms

The pragmatism should take form in *sensible limitations* of the scope, not in a low level of attention to details.
Particularly:
 * **volatile** elements are currently **not supported** \
   (forming queries over them is quite questionable; much of the internal machinery lacks volatile specializations)
 * behaviour with explicit **&& (r-value reference)** elements is **unspecified**
	* very basic operations might work as expected, possibly with unnecessary materialization (move/copy) of items
	* but in general, **expiring** elements should be represented as **pr-values**\
	  (which might not even lead to unnecessary moves in those simple cases due to RVO)
 * behaviour with **const pr-value** elements is **unspecified**\
   (however, it is probably similar to using immutable types)
	* *no sense* to qualify freshly created objects directly (the receiver can *const the target variable* anytime)
 * **immutable types** should work properly
	* except with multipass operations manipulating containers\
	  (unless the container itself handles them specially)
		> This could be improved in the future.
 * automatic deductions and optimizations are often best-effort only

> &#9989; In general, the considered element types are:\
> non-exotic **object types** (exclude arrays and member-pointers), and possibly const-qualified **l-value references** to them.

On top of this, operations may pose additional requirements only as conceptually necessary to carry them out. (Starting with C++17 even unmovable types are a thing.)

> &#8505; Known limitation: some complex operations might require copy-constructability in cases when *move* should be sufficient &ndash; there are plans to avoid this.
 



## Key Features

### Type-erased sequences &ndash; Enumerable\<T\>

The foundation in .Net is the *IEnumerable\<T\>* interface, implemented by every collection. In place of that,
our *Enumerable\<T\>* is a wrapper, able to hide anything iterable. Automatic conversions are provided:

```cpp
class Registry {
	std::list<Person>          persons;
public:
	Enumerable<const Person&>  Persons() const  { return persons; }
//...
```

Just like in .Net, the *Enumerable* basically represents a query. In the simple case above it stores a reference
to the *persons* list &ndash; meaning that it can be repeatedly evaluated as long as the *Registry* object lives on.
(Modifications will be reflected.)

Querying the **size** is implemented in an **optional-feature** pattern. In such simple cases it is provided in
constant time &ndash; but for a filtered sequence it requires an exhaustive iteration.

```cpp
	Registry  reg;
	/* ... */
	size_t population = reg.Persons().Count();   // O(1)
```

### Fully templated sequences 

The convenience of *Enumerable\<T\>* comes with an overhead, so it is opt-in. Algorithms that stay within function scope can (and should)
stay in "template-land", working with implicitly typed sequences (some *AutoEnumerable\<Factory\>* type, staying hidden behind *"auto"*).
For this the main **entry-point** is the ***Enumerate*** function:
```cpp
	std::vector<int>  vec;
	auto numbers   = Enumerate(vec);             // or:
	auto constNums = Enumerate<const int&>(vec);

	for (int& x : numbers) { /*...*/ }

	for (const int& c : constNums) { /*...*/ }
```


### Chainable operations

Like in .Net, transformation steps can be layered on top of sequences in a builder style (utilizing move-semantics wherever possible).\
For that to be readable it is fortunate to have a concise lambda syntax (like C#'s `x => f(x)`), but C++ lambdas do not excel in that.
The closest thing I could come up with is a pair of macros abbreviating the most generic lambdas available in C++:
 * one for value-, one for ref-capture (***FUNV*** and ***FUN*** respectively)
 * forwarding-reference parameters
 * decltype(auto) result

These are controversial for many C++ devs, and optional to use, but I found them much more handy and readable than their expanded form.
```cpp
std::vector<Person> persons = /*...*/;

Enumerable<std::string&> adultNames = Enumerate(persons).Where (FUN(p,  p.GetAge() >= 18))
                                                        .Select(FUN(p,  p.GetName()));
// alternatively:
Enumerable<std::string&> adultNames2 = Enumerate(persons).Where ([](const Person& p) { return p.GetAge() >= 18; })
                                                         .Select(&Person::GetName);
```

As shown above, member-pointers in C++ can also give some remedy against long lambdas (imagine the trailing return type for refs!), but
having FUN can provide a uniform look and logic-focused code even when some steps require more complex expressions.

Member-pointers play nice in simple tasks:
```cpp
struct Measurement {
	unsigned    sensorId;
	short       value;
	bool        isOfficial;
	long long   time;
};

std::vector<Measurement> measurements = /*...*/;

std::vector<Measurement*> recordsByTime = Enumerate(measurements)
                                            .Where     (&Measurement::isOfficial)
                                            .Addresses ()
                                            .MaximumsBy(&Measurement::value)
                                            .OrderBy   (&Measurement::time)
                                            .ToList    ();
```

### Cached-result optimization

The previous example also demonstrates an optimization feature: since *MinimumsBy* and *OrderBy* need a result cache (a vector) to work with,
of the exact same type as what's requested by ToList in the end, that cache can be passed down as a whole to ultimately become the
requested result &ndash; which means 0 memory overhead compared to a handwritten algorithm!

This is the fortunate case though. To exploit this feature, one should carefully order the chained operations and adjust the element type to fit the result.
However, it's easy to conceptualize what happens: whenever operations that represent multipass algorithms are followed by each other (or a container materialization, like *ToList*),
elements can pass down the pipeline not only one-by-one, but also at once, as the whole cache container.
(Of course, any subchain of operations can share a cache this way, this is not specific to closing with *ToList*.)


### Semi-automatic lifetime protection

During transformation steps the goal is to use references whenever possible (to avoid copies + preserve object identity). The *FUN* macros are defined in that mindset:
accessing a field in their body makes the compiler deduce a reference return type. Member-pointers behave the same way by standard. In most cases this is desired &ndash; except when the parameter is an expiring object,
and its subobject is to be selected to continue down the pipeline. This case a pr-value must be materialized.


This can be automated only partially: the parameter's value-category is well known, as well as the result's ref-ness &ndash; but how could the compiler know whether the resulting reference points to something
tied to the input object, or living somewhere outside of it? (Even a getter can return a reference to something in a parent object, or even in a static table!)

For this, the fundamental transformation has 2 forms. The programmer should express intent by choosing:
 * ***.Select*** to expand a subobject
 * ***.Map*** / ***.MapTo*** to apply a general transformation with freely deduced / specified result type

 ```cpp
class Pet {
public:
	const Person&       Owner() const;
	const std::string&  Name()  const;
// ...
};

Enumerable<Pet&>  pets   = /*...*/;
Enumerable<Pet>   copies = pets.Copy();

// Declaring Enumerable<T>-s on lhs only to visualize deduced element types - could use just "auto"!

Enumerable<const std::string&>  names1 = pets.Select(&Pet::Name);     // subobject addressable
Enumerable<std::string>         names2 = copies.Select(&Pet::Name);   // must materialize!

Enumerable<const std::string&>  names3 = copies.Map(&Pet::Name);      // compiles, but dangling if executed!
Enumerable<const Person&>       owners = copies.Map(&Pet::Owner);     // OK, desired!
```


An explicit result type can be specified in both cases &ndash; which acts as a return conversion, and can be used to force materialization as well.
 ```cpp
 Enumerable<std::string>  names4 = pets.Select<std::string>(&Pet::Name);       // OK, requested pr-values
 Enumerable<std::string>  names5 = pets.MapTo<std::string>(&Pet::Name);
 Enumerable<std::string>  dangling = copies.Select<std::string&>(&Pet::Name);  // Compile Error
```

Fortunately, the intention of lambdas in more complex operations tends to be more obvious, so this duality hasn't arisen elsewhere yet.


### Further info

The implemented operations are found on the public interface of the *AutoEnumerable* class,\
located in [Enumerables_Interface.hpp](/Enumerables/Enumerables_Interface.hpp)

To have a detailed overview of the library's features, look at:
 * [Introduction_Fundamentals.cpp](/Tests/Introduction_Fundamentals.cpp)
 * [Introduction_Operations.cpp](/Tests/Introduction_Operations.cpp)
 * [Introduction_Examples.cpp](/Tests/Introduction_Examples.cpp)

> &#128712;&ensp;Some complex operations, like Join, have not been implemented yet.\
> &emsp;&thinsp;&thinsp;Scan/Aggregate is in an experimental-ish state.



## Performance

Utilizing *Enumerables* to code in a very C# style (i.e. to return filtered or transformed views of member collections via *Enumerable\<T\>*, or accept
parameters as *Enumerable\<T\>*), while functionally provides great flexibility, can affect performance in various ways.\
It can save a lot of copies and (typically) 1 larger allocation to return the results or to convert the argument into the desired form &ndash;
at the cost of virtual calls during iteration, and 0..2 smaller allocations for the *Enumerable\<T\>* itself, and its interfaced *Enumerator* (both can get inlined depending on their size).
The actual cost of allocations highly depends on the real load on the allocator.

The C++20/23 STL basically offers coroutines to use on such boundaries (assuming non-header-defined functions), which incurs other costs.

Using *Enumerables* in the *"Fully templated"* manner, simply to transform data within function scope, the overhead encountered is much more definite. It can be traced back to one of the following:
* suboptimal implementation of operations
* some structural cost of abstraction (e.g. more passes, inability to reuse resources)
* the compiler's struggling to inline/optimize the chained operations

### Measurements

The primary goal of the [PerfTests](./Tests/PerfTests.cpp) unit is to measure the latter, more "raw" kind of overhead in some basic problems, compared against the handwritten (or the STL *Ranges*) solution.
(Although it can measure the cost of virtual calls too.)

To further "homologate" the testcases, each algorithm variant must pass its result simply in a materialized std::vector (except in aggregation tests, where it is a scalar)
&ndash; thus, the cost of creating a vector is always included, but we won't compare apples to oranges.\
Testcases are run for **short** (\~10 elements) and **long** (10000+ elements) sequences separately.

The following charts show these results measured with [v2.0.3-PerfCheck](https://github.com/feketedev/EnumerablesCpp/releases/tag/v2.0.3-PerfCheck) for testcases having a *ranges* variant, compiled for **x64** in **C++23** mode.

Some *ranges* implementations have some caveats though, for not having fully equivalent operations in every case:
 * Minimum/Maximum place searches:\
	The canonical way would have to loop over iterators, repeatedly calling *min_element* &ndash; which I considered too close to the handwritten style, so I used a 2-pass method,
	using *min* and *views::filter*, as a way to avoid manual loops. \
	(Favors *Ranges* for short, *Enumerables* for long sequences)
 * Integer summation\
	Uses fold, whereas *Enumerables*' *.Sum* has a loop.\
	(Favors *Enumerables*) \
	[*"Naive-sum doubles"* is valid, as the *Enumerables* version also uses *.Aggregate*!]
 * Sorted results\
	*ranges::sort* modifies a *vector* copy, whereas *.Order / .OrderBy* provides a lazy, updatable view \
	(Favors *Ranges*)

*It might be worth mentioning too that the whole solution is Visual Studio based, so the Clang version compiles with MS STL as well.*

The whole test-suit ran 10 times on an i7-14700K, with all Turbo, SpeedStep and SpeedShift functions disabled, clocked at 3.4 GHz. The diagrams present the median runtimes.


<details open>
<summary>Clang Runtimes</summary>

  ![Clang Long runtimes](./doc/perfCharts/Clang23_longs.svg) \
  ![Clang Short runtimes](./doc/perfCharts/Clang23_shorts.svg)

</details>

<details>
<summary>Clang Overhead comparison</summary>

  ![Clang Long overheads](./doc/perfCharts/Clang23_longs_overhead.svg)\
  **-- Note that extreme cases are off the chart to visualize the rest. --**

  ![Clang Short overheads](./doc/perfCharts/Clang23_shorts_overhead.svg)

</details>

<details>
<summary>MSVC Runtimes</summary>

  ![MSVC Long runtimes](./doc/perfCharts/MS23_longs.svg)\
  ![MSVC Short runtimes](./doc/perfCharts/MS23_shorts.svg)

</details>

<details>
<summary>MSVC Overhead comparison</summary>

  ![MSVC Long overheads](./doc/perfCharts/MS23_longs_overhead.svg)\
  **-- Note that extreme cases are off the chart to visualize the rest. --**
  ![MSVC Short overheads](./doc/perfCharts/MS23_shorts_overhead.svg)

</details>

### Conclusions

If we exclude the problematic testcases (minimum searches, integer summation), the rest shows an overhead pretty close to that of *Ranges*.
In fact, the cumulated overhead (for running all testcases 1 time) of *Enumerables* against going handwritten is around **13%** on *MSVC*, **26%** on *Clang*.
(Personally, I can argue that - especially the former - is an acceptable price for the abstraction.)

With the problematic or unsupported testcases excluded, *Ranges* performs undeniably better, by around 15% on each compiler.

Some observations:
 * *"Subrange of filtered int"* always performs worst in handwritten version.\
	This is because I missed the early-exit opportunity for having 1 branch only.\
	(At least both libraries do it right.)
 * *"Naive-sum doubles"* aka. *fold* vs. *Aggregate* is interesting, the compilers optimize it differently with drastically opposite results.
 * *"Hashmap iteration"* shows no problem under MSVC, yet significant handicap under Clang.




## Setup

* **Copy** the *Enumerables* folder as a whole to your project
* Based on [Enumerables.hpp](/Tests/Enumerables.hpp) create **your own config file** (of the same name),\
  which will ultimately include *Enumerables_Implementation.hpp* to instantiate the library
	* Internal usage of non-standard container types can be configured there via some macros and simple binding classes
	* Find available options in [Enumerables_ConfigDefaults.hpp](/Enumerables/Enumerables_ConfigDefaults.hpp)
* **Include** that *Enumerables.hpp* in client code
* As an optimization extension-point ***GetSize(container)*** function overloads for arbitrary (input) containers can be added to the *Enumerables* namespace.
  [Mandatory for custom internal containers.]
* Loading [Enumerables.natvis](/Enumerables/Enumerables.natvis) to Visual Studio can help debugging.

## Extensibility

The builder-style composition has the drawback that it requires a monolith interface. Since C++ doesn't have extension methods, nor an accepted UFCS proposal
(like N4474), adding extra operations currently requires adding them directly to the *AutoEnumerable* class (while the algorithms themselves can be defined
freely anywhere, in the form of *IEnumerator\<T\>* descendants).

The theoretical benefit is that a good intellisense can provide method completion after each period hit, because deduction "flows" simply downwards
(in contrast to a piping syntax). The real one does struggle though &ndash; hopefully, C++20 concepts will help on that, once utilized.

> It should be possible to define extended descendants with some CRTP support &ndash; this possibility is unexplored.


### Input Containers

Naturally, any range-iterable type can serve as the source of a sequence.\
Querying there size however does not have a standard way before C++17.

The client is allowed to overload 2 functions in the library's namespace:
* size_t *Enumerables::GetSize*(const Container&)
	* To achieve the best possible performance, in C++14 it is advised to overload for all encountered containers!
* bool *Enumerables::HasValue(const Optional&)*	
	* Required for a custom optional type if set by binding
	* Enables convenience methods *(.ValuesOnly)* over other optional-like types


### Created Containers

As mentioned in *Setup*, it is also possible to configure custom container types to be used throughout the library: both by the internal implementation of algorithms, and as the output of some terminal functions *(ToList / ToSet / ToDictionary)*.

This is done through a few macros, and user-defined binding classes that consist of simple static methods for the basic operations.
For example, to use *std\::set* instead of the default *std::unordered_set*, one could use the following config in his *Enumerables.hpp*:

 ```cpp
#include <set>

#define ENUMERABLES_SET_BINDING  MyConfig::TreeSetOperations

namespace MyConfig {
	struct TreeSetOperations {

		template <class V, class... Options>
		using Container = std::set<V, Options...>;

		static constexpr unsigned AllocatorOptionIdx = 1;


		template <class TContainer, class... Opts>
		static TContainer	Init(size_t /*capacity*/, const Opts&... options)
		{
			return TContainer (options...);    // no option to .reserve(capacity)
		}


		template <class V, class... Opts>
		static bool	Contains(const Container<V, Opts...>& s, const V& elem)
		{ 
			return s.find(elem) != s.end();
		}

		template <class V, class... Opts, class Vin>
		static void	Add(Container<V, Opts...>& s, Vin&& elem)
		{
			s.insert(std::forward<Vin>(elem));
		}
	};
}
// GetSize can be overloaded for any input containers as well, to enable O(1) size hints!
namespace Enumerables {
	template <class... Args>
	size_t GetSize(const std::set<Args...>& s) { return s.size(); }
}

// -- Instantiate the library after all config. --
#include "Enumerables_Implementation.hpp"
```

Of course, in this case, any set-using operations (e.g. *.Except / .Intersect*) will expect a Comparator, instead of the Hasher and EqualityComparer of the default *std::unordered_set*.


> &#8505; Currently this is a config-level customization only. Due to ODR this means that &ndash; while the library can be tailored for usage with any project's ecosystem &ndash;, each module (.exe or .dll) can only have 1 such configuration in effect!

> The alternative could be to use additional template parameters for those binding (or strategy) classes.
> This idea is yet unexplored.



## Maturity status

The original (let's say 1.0, but rather 0.9) version was utilized in an old project (from which the C++14 requirement originated), where it was part of production code. Therefore, the core concepts are battle-proven.

However, that version relied heavily on the usage of *std\:\:function*, which really added overhead in exchange for simplifying the code. I collected many improvement ideas during that time (including fully templating the code to avoid *std\:\:function*).\
Lately, I had time to polish them, add new ones, and reorganize the interface to make it mostly consistent.

Hence this is version 2.0 (with one sole usage of *std\:\:function*, for type-erasure).
Keep in mind that this version has much greater complexity, yet is not actively used at the moment, the only coverage is provided by the uploaded tests &ndash; many of them became quite comprehensive though!\
(Fortunately, most errors manifest in compilation errors, so large surprises should be unexpected. \:\) )


## Copyrights

This code is released under MIT license. See [LICENSE.txt](LICENSE.txt).\
Copyright 2024-2025 Norbert Fekete.
