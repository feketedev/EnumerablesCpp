# Enumerables for C++

This header-only library aims to bring the features of .Net's Linq library to C++14 (or newer),
to enable a more declarative / functional coding style.

> &#9989; *This branch (**clean17**) contains a cleaned-up, slightly modernized version requiring **C++17**.<br/>
> &emsp;&ensp;&thinsp;This is the **currently recommended** branch unless you are limited to using an older compiler!*


The library follows a pragmatic approach with the core objectives of:
 * provide as concise and fluently readable syntax as possible
 * by automatically deducing convenient defaults (for type parameters) wherever possible
 * still, have the ability to override those manually
 * prioritize convenience, but try to minimize overhead too (often by providing a way to opt out)
 * depend solely on STL (but allow configuration of algorithms to use custom container types)

The pragmatism should take form in sensible **limitations** of the scope, not in a low level of attention to details.
Particularly:
 * **volatile** elements are **not** supported
 * behaviour with **&& (r-value reference)** elements is **unspecified**
	* very basic cases might work, possibly with unnecessary materialization (move/copy) of items
	* but in general, expiring elements should be represented as pr-values<br/>(which might not even use more moves in those simple cases due to RVO)
 * automatic deductions and optimizations are often best-effort only
 * [In general, the considered element types are non-exotic *object types* (arrays and member-pointers excluded) and l-value references to them]



## Key Features

### Type-erased sequences - Enumerable\<T\>

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
to the *persons* list - meaning that it can be repeatedly evaluated as long as the *Registry* object lives on.
(Modifications will be reflected.)

[Querying the **size** is implemented in an **optional-feature** pattern. In such simple cases it is provided in
constant time - but for a filtered sequence it requires an exhaustive iteration.]

This convenience comes with an overhead, so it is opt-in. Algorithms that stay within function scope can (and should)
stay in "template-land", working with implicitly typed sequences (some *AutoEnumerable\<Factory\>* type staying hidden behind *"auto"*).
For that the main **entry-point** is the *Enumerate* function:
```cpp
	std::vector<int>  vec;
	auto numbers   = Enumerate(vec);             // or:
	auto constNums = Enumerate<const int&>(vec);

	for (int& x : numbers) { /*...*/ }
```


### Chainable operations

Like in .Net, transformation steps can be layered on top of sequences in a builder style (utilizing move-semantics wherever possible).<br/>
For that to be readable it is fortunate to have a concise lambda syntax (like C#'s `x => f(x)`), but C++ lambdas do not excel in that.
The closest thing I could come up with is a pair of macros for the most generic lambdas available in C++ (one for value-, one for ref-capture).
These are controversial, and optional to use, but I found them much more handy and readable than the expanded form.
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
This example also demonstrates some optimization effort: since MinimumsBy and OrderBy need a result cache (a vector) to work with,
of the exact same type as what's requested by ToList in the end, that cache can be passed down as a whole to ultimately become the
requested result. (This is the fortunate case though.)
 
> &#128712;&ensp;More complex operations, like Join, are not implemented yet.<br/>
> &emsp;&thinsp;&thinsp;Scan/Aggregate is in an experimental-ish state.

The implemented operations are found on the public interface of the *AutoEnumerable* class, located in [Enumerables_Interface.hpp](/Enumerables/Enumerables_Interface.hpp)

To have a detailed overview of the library's features, look at:
 * [Introduction_Fundamentals.cpp](/Tests/Introduction_Fundamentals.cpp)
 * [Introduction_Operations.cpp](/Tests/Introduction_Operations.cpp)
 * [Introduction_Examples.cpp](/Tests/Introduction_Examples.cpp)


## Setup

* **Copy** the *Enumerables* folder as a whole to your project
* Based on [Enumerables.hpp](/Tests/Enumerables.hpp) create **your own config file** (of the same name),<br/>
  which will ultimately include *Enumerables_Implementation.hpp* to instantiate the library
	* Usage of non-standard container types can be configured there via some macros and simple binding classes<br/>
      [affects outputs (like *.ToList()*) and internal algorithm implementations - anything iterable can always serve as input]
	* Find available options in [Enumerables_ConfigDefaults.hpp](/Enumerables/Enumerables_ConfigDefaults.hpp)
* **Include** that *Enumerables.hpp* in client code
>* As an optimization extension-point *GetSize(cont)* function overloads for custom containers can be added to the *Enumerables* namespace.
>
>&#128712;&ensp;From C++17: Unnecessary if the container supports std::size / size(container)
* Loading [Enumerables.natvis](/Enumerables/Enumerables.natvis) to Visual Studio can help debugging.

### Extensibility

The builder-style composition has the drawback that it requires a monolith interface. Since C++ doesn't have extension methods, nor an accepted UFCS proposal
(like N4474), adding extra operations currently requires adding them directly to the *AutoEnumerable* class (while the algorithms themselves can be defined
freely anywhere, in the form of *IEnumerator\<T\>* descendants).

The theoretical benefit is that a good intellisense can provide method completion after each period hit, because deduction "flows" simply downwards
(in contrast to a piping syntax). The real one does struggle though - but hopefully, C++20 concepts will help on that, if utilized.

--

As mentioned, the library can use any container for its List / Set types through a few macros and user-defined binding classes that consist of simple static methods for the necessary basic operations.
This means that - while the library is not tied to STL -, in each module (.exe or .dll) only 1 such binding-set can exist, due to ODR.<br/>

For a small demo, the tests under [TestsAltBinding](/TestsAltBinding) employ an alternative configuration set up in [EnumerablesAlt.hpp](/TestsAltBinding/EnumerablesAlt.hpp),
which for example binds C++17's std::optional as the result type of concerned terminal operations in place of the default Enumerables::OptResult.
(These tests must be instantiated in separated DLLs to avoid theoretical link-time collision with the default *Enumerable* types laid out for the base tests.)

The alternative could be to use additional template parameters for those binding (or strategy) classes, but *Enumerable*s of the various bindings would still be incompatible, so I didn't see enough value in that added complexity so far.


## Maturity status

The original (let's say 1.0, but rather 0.9) version was utilized in an old project (from which the C++14 requirement originated), where it was part of production code. Therefore, the core concepts are battle-proven.

However, that version relied heavily on the usage of *std\:\:function*, which really added overhead for simplifying the code. I collected many half-baked improvement ideas during that time (including fully templating the code to avoid *std\:\:function*), which were not polished enough to apply them in real code.
Lately, I finished and organized these to see them working together, and gave them a mostly consistent interface - hence this is version 2.0 (with one sole usage of *std\:\:function*, for type-erasure).

This version has much more complexity, yet is not actively used at the moment, the only coverage is what's provided by the uploaded tests!<br/>
(Fortunately, most errors manifest in compilation errors, so large surprises should be unexpected. \:\) )


## Copyrights

This code is released under MIT license. See [LICENSE.txt](LICENSE.txt).  
Copyright 2024 Norbert Fekete.
