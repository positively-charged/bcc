<h2>BCS</h2>

<h3>Table of Contents</h3>

<ol>
   <li><a href="#incompatibilities-with-acs">Incompatibilities with ACS</a></li>
   <li><a href="#libraries">Libraries</a></li>
   <li><a href="#preprocessor">Preprocessor</a></li>
   <li><a href="#namespaces">Namespaces</a></li>
   <li><a href="#declarations">Declarations</a></li>
   <li><a href="#enumerations">Enumerations</a></li>
   <li><a href="#structures">Structures</a></li>
   <li><a href="#functions">Functions</a></li>
   <li><a href="#statements">Statements</a></li>
   <li><a href="#references">References</a></li>
   <li><a href="#strong-types">Strong Types</a></li>
   <li><a href="#expressions">Expressions</a></li>
   <li><a href="#miscellaneous">Miscellaneous</a></li>
</ol>

<h3>Incompatibilities with ACS</h3>

* Logical-AND (`&&`) and Logical-OR (`||`) use short-circuit evaluation
* In radix constants, an `r` is used to separate the base from the number

<h3>Libraries</h3>

The library specified as an argument to the compiler is called the _main library_. First, the main library is parsed and preprocessed. Then, each library imported by the main library is parsed and preprocessed. The preprocessor starts afresh for every library, so macros defined in one library will not be available in another library.

--

In an imported library, the predefined macro `__IMPORTED__` is available. It's defined as `1`. This macro is useful for reporting an error when the user imports a library using `#include` instead of `#import`:

```
#library "somelib"

#ifndef __IMPORTED__
   #error you must #import this file
#endif
```

<h4>Private visibility</h4>

A library can have private variables. Only the library can see its private variables. A library that imports another library will not be able to see the private variables of the imported library. Functions and unnamed enumerations can also be private.

<h6>File: <i>lib1.bcs</i></h6>
```
#library "lib1"

int a;

// The following variable, function, and unnamed enumeration are only visible
// inside "lib1":
private int b;
private void F() {}
private enum { C = 123 };
```

<h6>File: <i>lib2.bcs</i></h6>
```
#library "lib2"

#import "lib1.bcs"

// This `b` is only visible inside "lib2".
private int b;

script "Main" open {
   ++a; // Will increment `a` in "lib1".
   ++b; // Will increment `b` in "lib2".
   F(); // Error: `F` not found. 
}
```

<h4>External declarations</h4>

Variables can be declared `extern`. An external variable declaration is not actually a real variable. All it does is tell the compiler that such a variable exists in some library. The compiler will tell the game to import the variable when the game runs. External function declarations are also supported.

<h6>File: <i>lib1.bcs</i></h6>
```
#library "lib1"

int v;
void F() {}
```

<h6>File: <i>lib2.bcs</i></h6>
```
#library "lib2"

extern int v;
extern void F();

script "Main" open {
   ++v;
   F();
}
```

The game cannot use an external variable unless it knows which library has the variable. To instruct the game to look in some library so it can find the external variable, use `#linklibrary`:

<h6>File: <i>lib2.bcs (Fixed)</i></h6>
```
#library "lib2"

// The game will now load the "lib1" library. It will also look in the "lib1"
// library when it tries to find the `v` variable.
#linklibrary "lib1"

extern int v;

script "Main" open {
   ++v;
}
```

External declarations and the `#linklibrary` directive are probably not all that useful unless you plan to use header files.

<h4>Header files</h4>

The header file development style of C/C++ is supported by BCS. To avoid conflicts with C/C++ header files, it is suggested you give your header files a `.h.bcs` file extension, although it is not mandatory to do so.

<h6>Header file: <i>example.h.bcs</i></h6>
```
#ifndef EXAMPLE_H_BCS
#define EXAMPLE_H_BCS

#linklibrary "example"

extern int v;
extern void F();

#endif
```

<h6>Source (library) file: <i>example.bcs</i></h6>
```
#library "example"

#include "example.h.bcs"

int v = 123;
void F() {}
```

<h6>File: <i>main.bcs</i></h6>
```
#include "zcommon.h.bcs"
#include "example.h.bcs"

script "Main" open {
   v = INT_MAX;
   F();
}
```

--

If the name of your header file ends with `.h.bcs`, the `.bcs` extension does not need to be specified in an `#include` directive:

<h6>File: <i>main.bcs (Now with shorter include paths)</i></h6>
```
#include "zcommon.h"
#include "example.h"

script "Main" open {
   v = INT_MAX;
   F();
}
```

<h3>Preprocessor</h3>

The BCS preprocessor replicates much of the behavior of the C (C99) preprocessor. The preprocessor is case-sensitive. To be compatible with ACS, the preprocessor-based `#define` and `#include` only get executed if they appear in an `#if/#ifdef/#ifndef` block:

```
#if 1
   #define MAKE_STR( arg ) #arg
#endif

script MAKE_STR( 1 2 3 ) open {
   Print( s: MAKE_STR( Hello World! ) );
}
```

--

The compiler performs string literal concatentation: adjacent string literals are combined into a single string literal. This can be used to break up a long string literal into smaller parts, making it easier to see the whole string:

```
script "Main" open {
   Print( s:
      "This is a very long string. It will require a lot of horizontal "
      "scrolling to see it all. Horizontal scrolling is not fun. Thankfully, "
      "we can break this string into smaller strings, which will then be "
      "combined into a single string by the compiler. Awesome!" );
}
```

<h3>Namespaces</h3>

Namespaces in BCS work similar to namespaces in other languages like C++ and C#. The `.` operator is used to access the objects of a namespace:

```
namespace Test {
   int v = 123;
   void F() { Print( d: v ); }
   enum { C = 321 };
}

script "Main" open {
   Test.v = Test.C;
   Test.F(); // Output: 321
}
```

Nested namespaces can be declared in one go:

```
// These are the same:
namespace A { namespace B { namespace C {} } }
namespace A.B.C {}
```

To avoid confusion with global variables, the global namespace is called the _upmost namespace_. The `upmost` keyword refers to the upmost namespace:

```
int a = 123;
namespace Test {
   int a = 321;
   script "Main" open {
      Print( d: a ); // Output: 321
      Print( d: upmost.a ); // Output: 123
   }
}
```

When in a namespace block, two special things happen: first, [strong types](#strong-types) are in effect; second, the `let` keyword is implied, so you get [block scoping](#block-scoping) by default.

If you don't want to use namespaces but still want strong types and default block scoping, you can put your code in a namespace block for the upmost namespace:

```
namespace upmost {
   // Value and variable must have the same type:
   int a = ( int ) "abc";
   script "Main" open {
      // Block scoping enabled by default. No need for the `let` keyword:
      for ( int i = 0; i < 10; ++i ) {}
      for ( int i = 0; i < 10; ++i ) {}
   }
}
```

<h4>Importing stuff</h4>

The `using` directive is used to import objects from other namespaces. You can either import a whole namespace or import specific objects from a namespace.

<h5>Importing namespaces</h5>

<pre>
using <i>namespace</i> ;
</pre>

You can import a whole namespace. This will make all of the objects in the specified namespace available for use:

```
namespace Test {
   int v = 123;
   void F() { Print( d: v ); }
   enum { C = 321 };
}

// All of the objects in the `Test` namespace will now be available.
using Test;

script "Main" open {
   v = C;
   F();
}
```

<h5>Importing specific objects</h5>

<pre>
using <i>namespace</i> : [<i>alias</i> =] <i>object</i> [, ...] ;
</pre>

If you need only a few objects from a namespace, you can import only those objects you want. You can give the imported object a different name if you like:

```
namespace Test {
   int v = 123;
   void F() { Print( d: v ); }
   enum { C = 321 };
}

// Import only `v` and `C` from the `Test` namespace. `C` is refered to as `CONSTANT`.
using Test: v, CONSTANT = C;

script "Main" open {
   v = CONSTANT;
   Test.F();
}
```

<h3>Declarations</h3>

When a script has no parameters, the `void` keyword is not necessary. The parentheses are not required either.

```
// These are all the same.
script "Main" ( void ) {}
script "Main" () {}
script "Main" {}
```

--

Objects outside of scripts and functions do not need to be declared first before they can be used. In the following example, a variable, a constant, and a function are used before they appear:

```
script "Main" open {
   v = C;
   F(); // Output: 123
}

int v;
enum { C = 123 };
void F() { Print( d: v ); }
```

--

When declaring an array, the length of _any_ dimension can be omitted. The length will be determined based on the number of values found in the brace initializer. So if the array is initialized with 5 values, the length of the dimension will be 5:

```
// This array is initialized with 5 strings, so the length of the array is 5.
str names[] = {
   "Positron",
   "Hypnotoad",
   "AC3",
   "Frank",
   ""
};
```

For multidimensional arrays, the nested brace initializer with the most values will determine the length of the dimension:

```
// The outmost brace initializer contains three values, the inner brace
// initializers, so the length of the first dimension will be 3. Of the three
// inner brace initializers, the second brace initializer contains the most
// values, so the length of the second dimension will be 4.
int years[][] = {
   { 1989 },
   { 1992, 1993, 1994, 1996 },
   { 2008, 2009 }
};
```

--

An array can be initialized with a string. Each character of the string, including the NUL character, initializes an element of the array. If the dimension length is omitted, the dimension length will be the length of the string, plus 1 for the NUL character. The array must be one-dimensional and have either `int` or `raw` element type:

```
int letters[] = "abc"; // Same as: int letters[] = { 'a', 'b', 'c', '\0' };
```

--

For the `if`, `switch`, `while`, `until`, and `for` statements, you can declare and use a variable as the condition at the same time. The variable is first initialized, and the value of the variable is then used as the condition:

```
script "Main" open {
   // This if-statement will only be executed if `value` is nonzero.
   if ( let int value = Random( 0, 5 ) ) {
      Print( d: value );
   }
}
```

--

World and global variables can be declared inside a script (or a function). It's the same thing as declaring the variable outside the script, except that the name of the variable will not be visible to code outside the script.

```
script "DeathCount" death {
   global int 1:deaths;
   ++deaths;
}
```

--

You can specify the length of world and global arrays. World and global arrays can be multidimensional.

```
script "Main" open {
   global int 1:array[ 10 ];
   global int 2:multiArray[ 10 ][ 20 ];
   array[ 0 ] = 123;
   multiArray[ 0 ][ 1 ] = 321;
}
```

--

Enumerations, structures, and type aliases can be nested inside scripts or functions.

<h4>Block scoping</h4>

A local scope is created by the block statement, statements that support a variable declaration as a condition, and the initialization part of the `for` and `foreach` loops. To put your objects in local scope, prefix the declarations with the `let` keyword: 

```
script "Main" open {
   let int var = 123;
   {
      // This `var` hides the outer `var` and will not be visible after the
      // closing brace of the block statement:
      let int var = 321;
      Print( d: var ); // Output: 321
   }
   Print( d: var ); // Output: 123
}
```

In a namespace block, the `let` keyword is implied, so you get block scoping by default:

```
// In a namespace block, the `let` keyword is not necessary.
namespace upmost {

script "Main" open {
   int var = 123;
   {
      // This `var` hides the outer `var` and will not be visible after the
      // closing brace of the block statement:
      int var = 321;
      Print( d: var ); // Output: 321
   }
   Print( d: var ); // Output: 123
}

}
```

<h4>Type deduction</h4>

When the variable type is `auto`, the compiler will deduce the type of the variable from its initializer. The variable type will be whatever the type of the initializer is. For example, if the variable is initialized with an integer such as 123, the type of the variable will be `int`; or if the variable is initialized with a string, the type of the variable will be `str`:

```
namespace upmost {

script "Main" open {
   auto number = 123;    // Same as: int number = 123;
   auto string = "abc";  // Same as: str string = "abc";
   static fixed array[] = { 1.0, 2.0, 3.0 };
   auto r = array;       // Same as: int[] r = array;
   auto f = array[ 0 ];  // Same as: fixed d = array[ 0 ];
}

}
```

When an enumerator is the initializer, the base type of the enumeration will be selected as the variable type. If you want the enumeration itself to be the variable type, use `auto enum`:

```
namespace upmost {

script "Main" open {
   enum FruitT { APPLE, ORANGE, PEAR };
   auto f1 = ORANGE;     // Same as: int f1 = ORAGE;
   auto enum f2 = PEAR;  // Same as: FruitT f2 = PEAR;
}

}
```

Type deduction is only supported for local variables.

<h4>Type aliases</h4>

A type alias is a name that refers to some type information. A type alias can be used as a type for variables, structure members, function parameters, or whatever else that can have a type. The variable, or whatever the object, will get all of the type information referenced by the type alias.

A type alias declaration starts with the `typedef` keyword and continues like a variable declaration. The variable name becomes the name of the type alias, and the type of the variable, including array dimensions, becomes the type information to be referenced by the type alias:

```
// Declare some type aliases.
typedef int NumberT;
typedef str Str10_T[ 10 ];

// Declare some variables and use the type aliases.
NumberT number; // Same as: int number;
Str10_T stringArray; // Same as: str stringArray[ 10 ];
```

A declaration for a function alias starts with the `typedef` keyword and continues like a function declaration, except that the function body is not specified. You cannot declare variables of a function type, but you can declare variables of reference-to-function type:

```
// Declare some function aliases.
typedef void PlainFuncT();
typedef str StrFuncIntIntT( IntT, IntT );

// Declare some variables and use the type aliases.
PlainFuncT? func; // Same as: void function()? func;
StrFuncIntIntT? func2; // Same as: str function( int, int )? func2;
```

<h5>Type names</h5>

The name of a type alias is called a <em>type name</em>. Type names must end with a lowercase letter, an underscore, or nothing at all, followed by a capital T. Some valid type names: `NumberT`, `Str10_T`, `T`.

<h3>Enumerations</h3>

An enumeration is a group of related constants, called enumerators. The first enumerator has a value of 0, the next constant has a value of 1, and so on. The value increases by 1:

```
enum {
   FRUIT_APPLE,  // 0
   FRUIT_ORANGE, // 1
   FRUIT_PEAR    // 2
};
```

--

The value of an enumerator can be set explicitly. The next value will increase starting from the new value:

```
enum {
   FRUIT_APPLE,       // 0
   FRUIT_ORANGE = 10, // 10
   FRUIT_PEAR         // 11
};
```

--

The type of an enumerator

--

Enumerations can be named and then used as a variable type. The only thing special about enumeration variables is that they must be initialized and updated with one of the enumerators instead of the value of an enumerator:

```
enum Fruit {
   FRUIT_APPLE,
   FRUIT_ORANGE,
   FRUIT_PEAR
};

script "Main" open {
   enum Fruit f = FRUIT_PEAR;
   f = 2; // Error: Not using an enumerator.
}
```

--

If you don't want to type the the `enum` keyword when declaring an enumeration variable, you can name the enumeration with a <a href="#type-names">type name</a>. This will implicitly create a <a href="#type-aliases">type alias</a> that refers to the enumeration:

```
enum FruitT {
   FRUIT_APPLE,
   FRUIT_ORANGE,
   FRUIT_PEAR
};

FruitT f = FRUIT_PEAR; // Same as: enum FruitT f = FRUIT_PEAR;
```

--

The last enumerator can have a comma after it. This is a convenience feature.

```
enum {
   FRUIT_APPLE,
   FRUIT_ORANGE,
   FRUIT_PEAR, // Comma here is allowed.
};
```

<h3>Structures</h3>

A structure is a group of data. It has a name and a list of members. The members are the actual data. In code, the <code>struct</code> keyword is used to represent a structure:

```
struct boss {
   int id;
   str name;
};
```

Here, the structure is named <code>boss</code>. It contains two members: an integer named <code>id</code> and a string named <code>name</code>.

---

A structure is used as a variable type. When a variable of a structure type is created, the variable will contain every member of the structure. If multiple variables of the same structure type are created, each variable will have its own copy of the members.

```
struct boss big_boss;
```

In the example above, we create a variable named <code>big\_boss</code>. The type of this variable is <code>struct boss</code>, the structure we made earlier. When specifying the type, notice we use the <code>struct</code> keyword plus the name of the structure we want to use.

---

The dot operator is used to access a member. A member can be modified like any other variable.

```
struct boss big_boss;

script 1 open {
   // Modify members:
   big_boss.id = 123;
   big_boss.name = "Really Mean Boss";
   // View members:
   Print( s: "Boss ID is ", i: big_boss.id );     // Output: Boss ID is 123
   Print( s: "Boss name is ", s: big_boss.name ); // Output: Boss name is Really Mean Boss
}
```

In the example above, we use the dot operator to access the <code>id</code> member and change its value to 123. We do the same for the <code>name</code> member, changing its value to <code>"Really Mean Boss"</code>. We then print the values of the members, using the dot operator to access each member.

---

<p style="background-color: #FCC; padding: 8px;">
<strong>Note:</strong> At this time, it is not possible to initialize the string members of a variable of a structure type. You will need to manually assign values to these members.
</p>

To initialize a variable of a structure type, the brace initializer is used. The first value in the initializer will be the starting value of the first member, the second value will be the starting value of the second member, and so on.

```
struct boss big_boss = { 123, "Really Mean Boss" };

script 1 open {
   // View members:
   Print( s: "Boss ID is ", i: big_boss.id );     // Output: Boss ID is 123
   Print( s: "Boss name is ", s: big_boss.name ); // Output: Boss name is Really Mean Boss
}
```

The example above and the example in the previous section are similar. The difference is how the members get their values. In the example above, we assign the values of the members when we create the variable. In the example in the previous section, we first create the variable, and later assign the values.

---

A member can be an array or a structure, or both.

```
struct boss_list {
   struct boss bosses[ 10 ];
   int count;
};

// `list` initialized with a single boss.
struct boss_list list = {
   { { 123, "Really Mean Boss" } },
   1
};

script 1 open {
   // Add second boss:
   list.bosses[ 1 ].id = 321;
   list.bosses[ 1 ].name = "Spooky Boss";
   ++list.count;
}
```

In the example above, we create a structure named <code>boss_list</code>. This structure has a member named <code>bosses</code> that is an array, and this array can hold 10 <code>boss</code> elements. The next member is an integer member named <code>count</code>, the number of bosses.

We create a variable named <code>list</code> using this new structure. The outermost braces initialize the <code>list</code> variable. Th middle braces initialize the <code>bosses</code> member, an array. The innermost braces initialize the first element of the array, a <code>boss</code> structure.

<h3>Functions</h3>

The `function` keyword is optional. If the function has no parameters, the `void` keyword is optional.

```
// These are all the same:
function void F( void ) {}
function void F() {}
void F() {}
```

--

For a function that returns a value, it is not necessary to have a return statement at the end of the function. (In fact, as of this time, it is possible to skip the return statement entirely. It's possible, but __don't__ do this.)

```
// Get absolute value of number.
int Abs( int number ) {
   if ( number < 0 ) {
      return number * -1;
   }
   else {
      return number;
   }
}
```

--

A parameter can lack a name. You won't be able to use such a parameter, but you still need to pass an argument for it. This can be used to indicate a parameter is no longer used. This works for script parameters as well.

```
int Sum( int used1, int, int used2 ) {
   return used1 + used2;
}

script "Main" open {
   Print( d: Sum( 100, 0, 200 ) ); // Output: 300
}
```

--

In a function, the magic identifier `__FUNCTION__` is a string literal that contains the name of the function:

```
void SomeFunc() {
   Print( s: __FUNCTION__ ); // Output: somefunc
}
```

`__FUNCTION__` might look like a predefined macro, but it is not a macro. Also, it cannot be used in string literal concatenation.

<h4>Optional parameters</h4>

A function parameter can have a default argument. If the user does not supply an argument for the parameter, the default argument will be used:

```
void Greet( str who = "Mate" ) {
   Print( s: "Hello, ", s: who, s: "!" );
}

script "Main" open {
   Greet( "Fine Fella" ); // Output: Hello, Fine Fella!
   Greet();               // Output: Hello, Mate!
}
```

--

Builtin functions now support default arguments that are strings. This means you can use `MorphActor()` with a single argument, as is allowed by the [wiki page](http://zdoom.org/wiki/MorphActor), and it will work as intended:

```
script "Main" enter {
   // These are the same:
   MorphActor( 0, "", "", 0, 0, "", "" );
   MorphActor( 0 );
}
```

<h4>Nested functions</h4>

A function can be declared inside a script or inside another function. Such a function is called a <em>nested function</em>:

```
script "Main" open {
   void Greet() {
      Print( s: "Hello, World!" );
   }
   Greet();
}
```

--

There is no limit on how deep functions can be nested:

```
script "Main" open {
   void F1() {
      Print( s: "F1()" );
      void F2() {
         Print( s: "F2()" );
         void F3() {
            Print( s: "F3()" );
         }
         F3();
      }
      F2();
   }
   F1();
}
```

--

A nested function can access the local variables located outside of it:

```
script "Main" open {
   str msg;
   void ShowMsg() {
      Print( s: msg );
   }
   msg = "Hello, World";
   ShowMsg();
   msg = "Goodbye, World!";
   ShowMsg();
}
```

--

Nested functions can have `auto` as the return type. The compiler will deduce the return type from the returned value. If no value is returned, the return type will be `void`:

```
namespace upmost {

script "Main" open {
   auto F1() {} // Same as: void F1() {}
   auto F2() { return; } // Same as: void F2() {}
   auto F3() { return "abc"; } // Same as: str F3() {}
}

}
```

--

Passing around a nested function as a reference is not allowed.

<h5>Technical details</h5>

Be aware when calling a nested function that has a lot of local variables. The compiler generates code to save and restore local variables. This might be too much a penalty for performance critical code.

Calling a nested function recursively too many times will eventually crash the game.

<h4>Anonymous functions</h4>

You can declare and call a nested function at the same time. Only the body can be specified; the nested function will be implicitly defined as having no name, no parameters, and `auto` as the return type. Such a nested function is called an <em>anonymous function</em>:

```
script "Main" open {
   Print( s: "Sum: ", d: {
      int sum = 0, i = 1;
      while ( i <= 10 ) {
         sum = sum + i;
         ++i;
      }
      return sum;
   }() );
}
```

The above code is equivalent to the following code:

```
script "Main" open {
   auto CalculateSum() {
      int sum = 0, i = 1;
      while ( i <= 10 ) {
         sum = sum + i;
         ++i;
      }
      return sum;
   }
   Print( s: "Sum: ", d: CalculateSum() );
}
```

<h4>Message-building functions</h4>

When calling `Print()` and the like, sometimes you want more control of the message-building process: what parts will form the message, in what order, and in what quantity. Message-building functions give you control of these characteristics.

A message-building function is qualified with `msgbuild`, must not have any parameters, and must have a `void` return type. In the body of a message-building function, a special function called `append()` becomes visible. You use `append()` to output the parts of the message. `append()` supports all of the cast types that are supported by `Print()`.

To create and print the message, pass the message-building function along with the `msgbuild:` cast type:

```
void Welcome( int borderLength ) {
   msgbuild void CreateMessage() {
      int i = 0;
      while ( i < borderLength ) {
         append( c: '*' );
         ++i;
      }
      append( s: "\n" );
      append( s: "Hello there!\n" );
      i = 0;
      while ( i < borderLength ) {
         append( c: '*' );
         ++i;
      }
   }
   // Create and print message.
   Print( msgbuild: CreateMessage );
}

script "Main" open {
   Welcome( 20 );
}
```

Message-building functions can call other message-building functions:

```
void Welcome( int borderLength ) {
   msgbuild void CreateMessage() {
      msgbuild void CreateBorder() {
         int i = 0;
         while ( i < borderLength ) {
            append( c: '*' );
            ++i;
         }
      }
      CreateBorder();
      append( s: "\n" );
      append( s: "Hello there!\n" );
      CreateBorder();
   }
   // Create and print message.
   Print( msgbuild: CreateMessage );
}
```

Anonymous functions can be used as message-building functions. The anonymous function is implicitly qualified with `msgbuild`:

```
void Welcome( int borderLength ) {
   // Create and print message.
   Print( msgbuild: {
      msgbuild void CreateBorder() {
         int i = 0;
         while ( i < borderLength ) {
            append( c: '*' );
            ++i;
         }
      }
      CreateBorder();
      append( s: "\n" );
      append( s: "Hello there!\n" );
      CreateBorder();
   } );
}
```

<h3>Statements</h3>

<h4><code>foreach</code></h4>

<pre>
foreach ( <i>value</i> ; <i>collection</i> ) {}
foreach ( <i>key</i>, <i>value</i> ; <i>collection</i> ) {}
</pre>

A `foreach` loop goes over every item in a collection. An array is a collection; the elements of the array are the items. A string is also a collection; the characters of the string are the items. `value` is a variable that will contain the item.

`value` will contain the current item being looked at:

```
script "Main" open {
   static int set[] = { 1, 2, 3 };
   foreach ( int number; set ) {
      Print( d: number );
   }
}
```

You can get the index of the element or character. Declare a variable before the value variable,

```
script "Main" open {
   static int set[] = { 1, 2, 3 };
   foreach ( int index, number; set ) {
      Print( s: "set[", d: index, s: "] == ", d: number );
   }
}
```

If the key and value are of different type, you can declare two different variables by separating them with a semicolon:

```
namespace upmost {

script "Main" open {
   static fixed set[] = { 1.0, 2.0, 3.0 };
   foreach ( int index; fixed number; set ) {
      Print( s: "set[", d: index, s: "] == ", f: number );
   }
}

}
```

<h4><code>goto</code></h4>

<pre>
goto <i>label</i> ;
<i>label</i> :
</pre>

A <code>goto</code> statement allows you to move anywhere within your script or function. A label is a destination of a `goto` statement:

```
script "Main" open {
   goto middle;
   top:
   Print( s: "top" );
   goto bottom;
   middle:
   Print( s: "middle" );
   goto top;
   bottom:
   Print( s: "bottom" );
   // Output:
   // middle
   // top
   // bottom
}
```

<h4>Assertions</h4>

<pre>
assert ( <i>condition</i> [, str <i>description</i>] ) ;
static assert ( <i>condition</i> [, str <i>description</i>] ) ;
</pre>

An `assert` statement evaluates the specified condition. If the condition is false, an error message is logged into the game console and the current script is terminated. The error message will contain the full path to the source file that contains the failed assertion, along with the line and column positions. An optional description may be included in the error message.

```
script "Main" open {
   enum { A, B, C } d = C;
   switch ( d ) {
   case A:
   case B:
      break;
   default:
      assert( 0, "missing case" );
   }
}
```

A `static assert` statement is executed at compile time. If the condition is false, the compiler outputs an error message, and the compilation is aborted.

```
script "Main" open {
   enum { A, B, C, TOTAL } d;
   static assert ( TOTAL == 2, "missing case" );
   switch ( d ) {
   case A:
   case B:
      break;
   }
}
```

<h3>References</h3>

References work similar to how class types are passed around 

A _reference_ is a value that identifies a particular variable or function. You can indirectly access a variable through a reference.

In languages such as Java, C#, and Python, you can create an object from a class and pass it around to other functions. This 

__NOTE:__ At this time, due to lack of support from the game engine, there are restrictions imposed on array and structure references, but not on function references: 

* Only private variables can be passed around as a reference.
* References cannot be shared between libraries. That means you cannot pass an array reference to another library, and you cannot use references in, or returned from, another library.

<h4>Array references</h4>

<pre>
<i>element-type</i> [] <i>var</i> = <i>reference</i> ;
</pre>

```
#library "reftest"

// Only references to private variables can be passed around to functions.
private int a[] = { 1, 2, 3 };
private int b[] = { 6, 5, 4 };

script "Main" open {
   PrintArray( a );
   PrintArray( b );
}

void PrintArray( int[] array ) {
   foreach ( let int element; array ) {
      Print( d: element );
   }
}
```

<h4>Structure references</h4>

<pre>
<i>structure</i> &amp; <i>var</i> = <i>reference</i> ;
</pre>

<pre>
struct PlayerT {
   str name;
};

PlayerT& player;
</pre>

Similar to arrays, when you use the name of a structure variable, the compiler implicitly creates a reference to the variable and uses that. So when you use a structure variable, it's like using a reference-to-structure value:

```
#library "reftest"

struct NumberT {
   int value;
};

// A reference can only accept private variables.
private NumberT someNumber = { 123 };

script "Main" open {
   // When using the `someNumber` variable, you will implicitly get a reference
   // value that refers to `someNumber`.
   NumberT& number = someNumber;
   Print( d: number.value ); // Output: 123
   ChangeNumber( number );
   Print( d: number.value ); // Output: 321
}

void ChangeNumber( NumberT& number ) {
   number.value = 321;
}
```

<h4>Function references</h4>
 
<pre>
<i>return-type</i> function( <i>parameters</i> ) <i>var</i> = <i>reference</i>  ;
<i>return-type</i> function( <i>parameters</i> ) <i>qualifiers</i> <i>var</i> = <i>reference</i> ;
</pre>

A variable of reference-to-function type looks like a header of a function, but the name is moved all the way to the right and the  

Unlike arrays and structure references, you can share function references between libraries.

```
#library "reftest"

void F1() { Print( s: "F1() called" ); }
void F2() { Print( s: "F2() called" ); }

script "Main" open {
   void function() f = F1;
   f(); // Output: F1() called
   f = F2;
   f(); // Output: F2() called
}
```

<h4>Null reference</h4>

The reference variables used above require y

__Be careful:__ if you dereference a null reference, the game will report an error and the current script will be terminated.

<h3>Strong Types</h3>

The primitive types are: `int`, `fixed`, `str`, and `bool`.

<h4>Raw type</h4>

To be compatible with ACS, the `raw` type is introduced. The `raw` type behaves like the `int` type, but has the following additional characteristics:

* A value of `raw` type can be assigned to a variable of primitive type, and a variable of `raw` type can accept any value of primitive type.
* When mixing a `raw` operand with a primitive operand, the primitive operand is implicitly casted to `raw`.
* A value of primitive type gets implicitly casted to `raw`. In a namespace block, no such implicit casting occurs.

<h4>Conversions and Casts</h4>

--

Operands in expressions must be of the same type.

The following subsections list the possible operations on the primitive types.

<h5>int</h5>
<h5>fixed</h5>
<h5>bool</h5>
<h5>str</h5>
<table>
   <tr>
   <th>Operation</th>
   <th>Result Type</th>
   <th>Description</th>
   </tr>
   <tr>
   <td><i>left</i> + <i>right</i></td>
   <td>str</td>
   <td>The left string is concatenated with the right string.</td>
   </tr>
   <tr>
   <td><i>left</i> == <i>right</i></td>
   <td>bool</td>
   <td>Returns <code>true</code> if the two strings are equal, <code>false</code> otherwise.</td>
   </tr>
   <tr>
   <td><i>string</i>[ <i>index</i> ]</td>
   <td>int</td>
   <td>
      Returns the character at the specified index. <br />
      This is the same as: <code>GetChar( <i>string</i>, <i>index</i> )</code></td>
   </tr>
   <tr>
   <td>! <i>string</i></td>
   <td>bool</td>
   <td>
      Returns <code>true</code> if the string is the empty string (<code>""</code>), <code>false</code> otherwise. <br />
      This is the same as : <code>( StrCmp( <i>string</i>, "" ) == 0 )</code>
      </td>
   </tr>
</table>

<h4>Methods</h4>

The `str` type has a function called `length()`. This function returns the length of the string:

```
namespace upmost {

script "Main" open {
   // Same as: Print( d: StrLen( "Hello, World!" ) );
   Print( d: "Hello, World!".length() ); // Output: 13
}

}
```

--

An array has a function called `length()`. This function returns the length of the array dimension:

```
namespace upmost {

int a[] = { 1, 2, 3 };

script "Main" open {
   Print( d: a.length() ); // Output: 3
}

}
```

<h4>Default initializers</h4>

<h3>Expressions</h3>

The assignment operation now produces a result. The result is the value being assigned. This way, you can chain together multiple assignments or use an assignment in a condition.

```
script "Main" open {
   int a, b, c;
   a = b = c = 123; // `a`, `b`, and `c` now have the value 123.

   // First a random number is generated. Then the random number is assigned to
   // variable `a`. The result of the assignment, which is the random number,
   // is then checked if it's not 3.
   while ( ( a = random( 0, 10 ) ) != 3 ) {
      Print( s: "Discarding number: ", d: a );
   }
}
```

--

For `strcpy()`, the `a:` is optional, unless you want to use the extra arguments:

```
script "Main" {
   static int v[ 10 ];
   strcpy( v, "abc" );
   strcpy( a: ( v, 0, 1 ), "abc" );
}
```

<h4>Short-circuit evaluation</h4>

In ACS, both of the operands to `&&` and `||` are evaluated at all times. In BCS, the right operand is only evaluated if necessary. For example, if the left operand to `&&` is false, there is no need to evaluate the right operand because the result of the operation will still be false. Similarly, if the left operand to `||` is true, there is no need to evaluate the right operand because the result of the operation will still be true. This is called short-circuit evaluation:

```
int Zero() {
   return 0;
}

int One() {
   return 1;
}

script "Main" open {
   // Zero() makes the && operation false, so calling One() is not necessary
   // because the result will still be false.
   Zero() && One();
   // One() makes the || operation true, so calling Zero() is not necessary
   // because the result will still be true.
   One() || Zero();
}
```

<h4>Numeric literals</h4>

Along with decimal and hexadecimal literals, there are now also octal and binary literals. Octal literals are base-8 numbers, use the digits, 0 to 7, and start with `0o`. Binary literals are base-2 numbers, use the digits, 0 and 1, and start with `0b`.

```
script "Main" open {
   Print( d: 0b101 ); // Output: 5
   Print( d: 0o123 ); // Output: 83
}
```

Like ACS, BCS supports radix constants, which allow you to specify the base of the number. The base can range from 2 to 36. The valid digits are, 0 to 9, and after that, `a` to `z`. The base and the number are separated by `r` (`_` in ACS):

```
script "Main" open {
   Print( d: 2r101 ); // Base-2 (Binary). Output: 5
   Print( d: 8r123 ); // Base-8 (Octal). Output: 83
   Print( d: 16rFF ); // Base-16 (Hexadecimal). Output: 255
   Print( d: 36rZZ ); // Base-36. Output: 1295
}
```

To improve readability of long numbers, an underscore can be used to separate digits into easily recognizable groups:

```
script "Main" open {
   Print( d: 2_000_000_000 );
   Print( d: 0b_1101_0111_0100_0110 );
}
```

<h4>Conditional operator (<code>?:</code>)</h4>

<pre>
<i>left</i> ? <i>middle</i> : <i>right</i>
<i>left</i> ? : <i>right</i>
</pre>

The `?:` operator is similar to the `if` statement. The left operand is evaluated first. If the result is true, then the middle operand is evaluated and returned; otherwise, the right operand is evaluated and returned:

```
script "Main" open {
   Print( d: 1 ? 123 : 321 ); // Output: 123
   Print( d: 0 ? 123 : 321 ); // Output: 321
}
```

The middle operand can be left out. In this case, the left operand is evaluated first. If the result is true, then it is returned; otherwise, the right operand is evaluated and returned:

```
script "Main" open {
   Print( d: 123 ?: 321 ); // Output: 123
   Print( d: 0 ?: 321 );   // Output: 321
}
```

<h4>Copy arrays and structures</h4>

<pre>
bool memcpy ( a: ( <i>destination</i> [, int <i>offset</i> [, int <i>elementsToCopy</i>]] ) , <i>source</i> [, int <i>sourceOffset</i>] )
bool memcpy ( <i>destination</i> , <i>source</i> ) // For structures.
</pre>

`memcpy()` is similar in syntax and functionality to [`strcpy()`](http://zdoom.org/wiki/StrCpy), but instead of copying strings, `memcpy()` copies arrays and structures:

```
int arrayA[] = { 1, 2, 3 };
int arrayB[] = { 4, 5, 6 };

script "CopyArrays" open {
   memcpy( arrayA, arrayB );
   // `arrayA` will now be the same as `arrayB`.
   Print( d: arrayA[ 1 ] ); // Output: 5
}

struct {
   int value;
} structA = { 123 }, structB = { 321 };

script "CopyStructs" open {
   memcpy( structA, structB );
   // `structA` will now be the same as `structB`.
   Print( d: structA.value ); // Output: 321
}
```

<h5>Technical details</h5>

`memcpy()` is not a feature of the game engine. It is implemented by the compiler. The compiler will generate extra code behind the scenes to make it work. So be aware of that when writing performance critical code.

<h3>Miscellaneous</h3>

New keywords in BCS: `assert`, `auto`, `enum`, `extern`, `false`, `fixed`, `foreach`, `let`, `memcpy`, `msgbuild`, `namespace`, `null`, `private`, `raw`, `struct`, `true`, `typedef`, `upmost`, and `using`. In ACS, the `goto` keyword is reserved but is not used; in BCS, it is used to represent the goto statement.

The following are keywords in ACS, but they are not keywords in BCS: `acs_executewait`, `acs_namedexecutewait`, `bluereturn`, `clientside`, `death`, `define`, `disconnect`, `encryptstrings`, `endregion`, `enter`, `event`, `hudmessage`, `hudmessagebold`, `import`, `include`, `kill`, `libdefine`, `library`, `lightning`, `log`, `net`, `nocompact`, `nowadauthor`, `open`, `pickup`, `redreturn`, `region`, `reopen`, `respawn`, `strparam`, `unloading`, `wadauthor`, and `whitereturn`. These identifiers can be used as names for your functions and variables.

--

It is not necessary to `#include "zcommon.acs"` in order to use the boolean literals. The boolean literals `true` and `false` are now keywords. `true` is the value 1, and `false` is the value 0.

<h2>bcc (Compiler)</h2>

<h3>Command-line Options</h3>

<h3>Cache</h3>