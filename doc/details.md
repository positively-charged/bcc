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
   <li><a href="#scripts">Scripts</a></li>
   <li><a href="#statements">Statements</a></li>
   <li><a href="#references">References</a></li>
   <li><a href="#strong-types">Strong Types</a></li>
   <li><a href="#expressions">Expressions</a></li>
   <li><a href="#miscellaneous">Miscellaneous</a></li>
</ol>

<h3>Incompatibilities with ACS</h3>

* Logical-AND (`&&`) and Logical-OR (`||`) do [short-circuit evaluation](#short-circuit-evaluation)
* In [radix constants](#radix-constants), an `r` is used to separate the base from the number
* Some previously usable identifiers are now [keywords](#keywords)

<h3>Libraries</h3>

The library specified as an argument to the compiler is called _the main library_. First, the main library is preprocessed and parsed. Then, each library imported by the main library is preprocessed and parsed. The preprocessor starts afresh for every library, so macros defined in one library will not be available in another library. Macros defined on the command-line are only available during the preprocessing of the main library.

--

In an imported library, the predefined macro, `__IMPORTED__`, is available. It is defined as `1`. This macro is useful for identifying the error where the user imports a library using `#include` instead of `#import`:

```
#library "somelib"

#ifndef __IMPORTED__
   #error you must #import this file
#endif
```

<h4>Private visibility</h4>

A library can have private objects, which can be variables, functions, and unnamed enumerations. Only the library can see its private objects. A library that imports another library will not be able to see the private objects of the imported library:

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

// This `b` is only visible inside "lib2":
private int b;

script "Main" open {
   ++a; // Will increment `a` in "lib1".
   ++b; // Will increment `b` in "lib2".
   F(); // Error: `F` not found. 
}
```

You can have as many private variables in your library as you like. When the maximum variable limit is about to be reached, the compiler will combine the remaining private variables into a single array.

<h4>External declarations</h4>

Variables can be declared `extern`. An external variable declaration is not actually a real variable. All it does is tell the compiler that such a variable exists in some library. The compiler will tell the game to import the variable when the game runs. Along with external variable declarations, external function declarations are also supported:

<h6>File: <i>lib1.bcs</i></h6>
```
#library "lib1"

int v = 123;
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
// library when it tries to find the `v` variable and the `F` function.
#linklibrary "lib1"

extern int v;
extern void F();

script "Main" open {
   ++v;
   F();
}
```

External declarations and the `#linklibrary` directive are probably not all that useful unless you plan to use header files.

<h4>Header files</h4>

The header file organization style of C/C++ is supported by BCS. To avoid conflicts with C/C++ header files, it is suggested you give your header files a `.h.bcs` file extension, although it is not mandatory to do so:

<h6>Header file: <i>lib1.h.bcs</i></h6>
```
#ifndef LIB1_H_BCS
#define LIB1_H_BCS

#linklibrary "lib1"

extern int v;
extern void F();

#endif
```

<h6>Source (library) file: <i>lib1.bcs</i></h6>
```
#library "lib1"

#include "lib1.h.bcs"

int v = 123;
void F() {}
```

<h6>File: <i>main.bcs</i></h6>
```
#include "zcommon.h.bcs"
#include "lib1.h.bcs"

script "Main" open {
   v = INT_MAX;
   F();
}
```

If the name of your header file ends with `.h.bcs`, the `.bcs` extension does not need to be specified in an `#include` directive:

<h6>File: <i>main.bcs (Now with shorter include paths)</i></h6>
```
#include "zcommon.h"
#include "lib1.h"

script "Main" open {
   v = INT_MAX;
   F();
}
```

<h3>Preprocessor</h3>

The BCS preprocessor attempts to replicate much of the behavior of the C (C99) preprocessor. The preprocessor is case-sensitive. To be compatible with ACS, the preprocessor-based `#define` and `#include` directives only get executed if they appear in an `#if/#ifdef/#ifndef` block:

```
#if 1
   #define MAKE_STR( arg ) #arg
#endif

script MAKE_STR( 1 2 3 ) open {
   Print( s: MAKE_STR( Hello World! ) );
}
```

<h4>String literal concatenation</h4>

The compiler performs _string literal concatenation_: adjacent string literals are combined to form a single string literal. This can be used to break up a long string literal into smaller parts, making it easier to see the whole string:

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

--

A nested namespace can be declared in one go:

```
// These are the same:
namespace A { namespace B { namespace C {} } }
namespace A.B.C {}
```

--

To avoid confusion with `global` variables, the global namespace is called _the upmost namespace_. The `upmost` keyword refers to the upmost namespace:

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

<h4>Qualifiers</h4>

There are two namespace qualifiers: `typeaware` and `blockscoping`. You specify these when declaring a namespace block. The qualifiers affect only the namespace block that they are attached to; other blocks which are part of the same namespace are not affected. The order of the qualifiers does not matter.

The `typeaware` qualifier enables [strong types](#strong-types). It tells the compiler not to implicitly cast values of a primitive type to [`raw`](#raw-type):

```
typeaware namespace Test {
   script "Main" open {
      int value1 = "abc"; // Error: initializer of wrong type.
      str value2 = "abc"; // All good.
   }
}
```

The `blockscoping` qualifier enables [block scoping](#block-scoping). For every local declaration, the `let` keyword is implied, so you don't need to specify it:

```
blockscoping namespace Test {
   script "Main" open {
      int var = 123;
      {
         int var = 321;
      }
   }
}
```

If you don't want to use namespaces but still want strong types and block scoping, you can wrap your code in a _nameless_ namespace block and qualify it with the desired qualifiers. The nameless namespace block will have the name of the parent namespace block; if there is no parent block, the nameless namespace block will be a part of the upmost namespace: 

```
// This namespace block is part of the upmost namespace.
typeaware blockscoping namespace {
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

The `using` directive is used to import objects from a namespace. You can either import a whole namespace or import specific objects from a namespace.

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
using <i>namespace</i> : [enum|struct] [<i>alias</i> =] <i>object</i> [, ...] ;
</pre>

If you need only a few objects from a namespace, you can import only those objects you want. You can give the imported object a different name if you like:

```
namespace Test {
   int v = 123;
   void F() { Print( d: v ); }
   enum { C = 321 };
}

// Import only `v` and `C` from the `Test` namespace. `C` is referred to as
// `CONSTANT`.
using Test: v, CONSTANT = C;

script "Main" open {
   v = CONSTANT;
   Test.F(); // Output: 321
}
```

<h3>Declarations</h3>

A namespace object does not need to be declared before it can be used. In the following example, a variable, a constant, and a function are used before they are declared:

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

World and global variables can be declared inside a script (or a function). It is the same thing as declaring the variable outside the script, except that the name of the variable will not be visible to code outside the script.

```
script "DeathCounter" death {
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

In a namespace block qualified with `blockscoping`, the `let` keyword is implied, so you get block scoping by default:

```
// In a namespace block, the `let` keyword is not necessary.
blockscoping namespace {

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
typeaware namespace {

script "Main" open {
   auto number = 123;    // Same as: int number = 123;
   auto string = "abc";  // Same as: str string = "abc";
   static fixed array[] = { 1.0, 2.0, 3.0 };
   auto r = array;       // Same as: fixed[]& r = array;
   auto f = array[ 0 ];  // Same as: fixed d = array[ 0 ];
}

}
```

When an enumerator is the initializer, the base type of the enumeration will be selected as the variable type. If you want the enumeration itself to be the variable type, use `auto enum`:

```
typeaware namespace {

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

The name of a type alias is called a <em>type name</em>. Type names must end with a lowercase letter, an underscore, or nothing at all, followed by a capital T. Some valid type names: <code>Numbe<b>rT</b></code>, <code>Str10<b>_T</b></code>, <code><b>T</b></code>.

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

An enumeration has a base type. The value of every enumerator will be of the base type. By default, `int` is the base type, but you can change it. If `int` is not the base type, then you must explicitly set the value of every enumerator:

```
typeaware namespace {

enum : str {
   FRUIT_APPLE = "Apple",
   FRUIT_ORANGE = "Orange",
   FRUIT_PEAR = "Pear"
};

script "Main" open {
   // Output: AppleOrangePear
   Print( s: FRUIT_APPLE + FRUIT_ORANGE + FRUIT_PEAR );
}

}
```

--

Enumerations can be named and then used as a variable type. The only thing special about enumeration variables is that they must be initialized and updated with one of the enumerators:

```
enum Fruit {
   FRUIT_APPLE,
   FRUIT_ORANGE,
   FRUIT_PEAR
};

script "Main" open {
   enum Fruit f = FRUIT_PEAR; // Works fine. Initializer is an enumerator.
   f = 2; // Error: value assigned not an enumerator.
}
```

--

If you don't want to type the `enum` keyword when declaring an enumeration variable, you can name the enumeration with a <a href="#type-names">type name</a>. This will implicitly create a <a href="#type-aliases">type alias</a> that refers to the enumeration:

```
enum FruitT {
   FRUIT_APPLE,
   FRUIT_ORANGE,
   FRUIT_PEAR
};

FruitT f = FRUIT_PEAR; // Same as: enum FruitT f = FRUIT_PEAR;
```

--

The last enumerator can have a comma after it:

```
enum {
   FRUIT_APPLE,
   FRUIT_ORANGE,
   FRUIT_PEAR, // Comma here is allowed.
};
```

<h3>Structures</h3>

Structures work much like in C:

```
// Declare a structure.
struct Boss {
   int id;
   str name;
};

// Declare and initialize a structure variable.
struct Boss bigBoss = { 123, "Really Mean Boss" };

script "Main" open {
   // Output: Boss ID is 123
   Print( s: "Boss ID is ", d: bigBoss.id );
   // Output: Boss name is Really Mean Boss
   Print( s: "Boss name is ", s: bigBoss.name );
}
```

If you don't want to type the `struct` keyword when declaring a structure variable, you can name the structure with a <a href="#type-names">type name</a>. This will implicitly create a <a href="#type-aliases">type alias</a> that refers to the structure:

```
struct BossT {
   int id;
   str name;
};

BossT bigBoss;      // Same as: struct BossT bigBoss;
BossT bosses[ 10 ]; // Same as: struct BossT bosses[ 10 ];
```

<h3>Functions</h3>

The `function` keyword is optional. If the function has no parameters, the `void` keyword is optional:

```
// These are all the same:
function void F( void ) {}
function void F() {}
void F() {}
```

--

For a function that returns a value, it is not necessary to have a return statement at the end of the function. As long as the function ultimately returns a value, you can nest the return statements:

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

In a function, the magic identifier, `__FUNCTION__`, is a string literal that contains the name of the current function:

```
void SomeFunc() {
   Print( s: __FUNCTION__ ); // Output: somefunc
}
```

`__FUNCTION__` might look like a predefined macro, but it is not a macro. As a consequence, it cannot be used in [string literal concatenation](#string-literal-concatenation).

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
   Greet(); // Output: Hello, World!
}
```

--

Functions can be nested arbitrarily deep:

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
   msg = "Hello, World!";
   ShowMsg(); // Output: Hello, World!
   msg = "Goodbye, World!";
   ShowMsg(); // Output: Goodbye, World!
}
```

--

Nested functions can have `auto` as the return type. The compiler will deduce the return type from the returned value. If no value is returned, the return type will be `void`:

```
typeaware namespace {

script "Main" open {
   auto F1() {} // Same as: void F1() {}
   auto F2() { return; } // Same as: void F2() {}
   auto F3() { return "abc"; } // Same as: str F3() {}
}

}
```

--

Passing around a reference to a nested function is not allowed at this time.

<h5>Technical details</h5>

Be aware when calling a nested function that has a lot of local variables. The compiler generates code to save and restore local variables. This might be too much a penalty for performance critical code.

_Calling a nested function recursively too many times will eventually crash the game._

<h4>Anonymous functions</h4>

<pre>
( function msgbuild { [<i>statements</i>] } )
( function { [<i>statements</i>] } )
( { [<i>statements</i>] } )
</pre>

You can declare and call a nested function at the same time. Only the body can be specified; the nested function will be implicitly defined as having no name, no parameters, and `auto` as the return type. Such a nested function is called an <em>anonymous function</em>. An anonymous function must be placed between parentheses:

```
script "Main" open {
   Print( s: "Sum: ", d: ( {
      int sum = 0, i = 1;
      while ( i <= 10 ) {
         sum = sum + i;
         ++i;
      }
      return sum;
   } )() ); // Output: Sum: 55
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
   Print( s: "Sum: ", d: CalculateSum() ); // Output: Sum: 55
}
```

<h4>Message-building functions</h4>

When calling `Print()` and the like, sometimes you want more control of the message-building process: what parts will form the message, in what order, and in what quantity. Message-building functions give you control of these characteristics.

A message-building function is qualified with `msgbuild`, must not have any parameters, and must have a `void` return type. In the body of a message-building function, a special function called `append()` becomes visible. You use `append()` to output the parts of the message. `append()` supports all of the cast types that are supported by `Print()`.

To utilize a message-building function, pass it with the `msgbuild:` cast:

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

Anonymous functions can be used as message-building functions:

```
void Welcome( int borderLength ) {
   // Create and print message.
   Print( msgbuild: ( function msgbuild {
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
   } ) );
}
```

<h3>Scripts</h3>

When a script has no parameters, the `void` keyword is not necessary. The parentheses are not required either:

```
// These are all the same:
script "Main" ( void ) {}
script "Main" () {}
script "Main" {}
```

--

In a script, the magic identifier, `__SCRIPT__`, is a string literal that contains either the name or number of the current script:

```
script 1 open {
   Print( s: __SCRIPT__ ); // Output: 1
}

script "abc" open {
   Print( s: __SCRIPT__ ); // Output: abc
}
```

<h3>Statements</h3>

For the `if`, `switch`, `while`, `until`, and `for` statements, you can declare and use a variable as the condition at the same time. The variable is first initialized, and the value of the variable is then used as the condition:

```
script "Main" open {
   // This if-statement will only be executed if `value` is nonzero.
   if ( let int value = Random( 0, 5 ) ) {
      Print( d: value );
   }
}
```

For the `if` and `switch` statements, the variable can be followed by an expression. The variable must be used in the expression, and the expression becomes the condition:

```
script "Main" open {
   // This if-statement will only be executed if `value` is greater than 3.
   if ( let int value = Random( 0, 5 ); value > 3 ) {
      Print( d: value );
   }
}
```

<h4><code>foreach</code></h4>

<pre>
foreach ( <i>value</i> ; <i>collection</i> ) {}
foreach ( <i>key</i>, <i>value</i> ; <i>collection</i> ) {}
</pre>

A `foreach` loop goes over every item in a collection. An array is a collection, and the elements of the array are the items. A string is also a collection, and the characters of the string are the items. `value` is a variable that will contain the current item being looked at:

```
script "Main" open {
   static int set[] = { 1, 2, 3 };
   foreach ( let int number; set ) {
      Print( d: number );
   }
}
```

You can declare another variable before the `value` variable. This variable is called the `key` variable. The `key` variable will contain the index of the array element or string character:

```
script "Main" open {
   static int set[] = { 1, 2, 3 };
   foreach ( let int index, number; set ) {
      Print( s: "set[", d: index, s: "] == ", d: number );
   }
}
```

If the `key` and `value` variables are of different type, you can declare two different variables by separating them with a semicolon:

```
typeaware blockscoping namespace {

script "Main" open {
   static fixed set[] = { 1.0, 2.0, 3.0 };
   foreach ( int index; fixed number; set ) {
      Print( s: "set[", d: index, s: "] == ", f: number );
   }
}

}
```

At this time, the `key` and `value` variables cannot be modified.

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

The `goto` statement is powerful and should be used with care.

<h4>Assertions</h4>

<pre>
assert ( <i>condition</i> [, str <i>description</i>] ) ;
static assert ( <i>condition</i> [, str <i>description</i>] ) ;
</pre>

An `assert` statement evaluates the specified condition. If the condition is `false`, an error message is logged into the game console and the current script is terminated. The error message will contain the full path to the source file that contains the failed assertion, along with the line and column positions. An optional description may be included in the error message.

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

A `static assert` statement is executed at compile time. If the condition is `false`, the compiler outputs an error message, and the compilation is aborted.

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

__NOTE:__ At this time, due to lack of support for non-function references in the game engine, references to arrays and structure variables have the following restrictions: 

* Only references to _private_ arrays and structure variables can be assigned to variables and passed to functions.
* References to arrays and structure variables cannot be shared between libraries. That means you cannot pass an array reference to another library, and you cannot use an array reference in, or returned from, another library.

<h4>Array references</h4>

<pre>
<i>element-type</i> [] &amp; <i>var</i> = <i>reference</i> ;
</pre>

In C, when you specify an array, it "decays" into a pointer to the first element of the array. In BCS, something similar happens: when you specify an array, what you actually get is a reference to the array. You can then save that reference into a reference variable, or pass it to a function:

```
#library "reftest"

// Only references to private variables can be passed around to functions.
private int a[] = { 1, 2, 3 };
private int b[] = { 6, 5, 4 };

script "Main" open {
   PrintArray( a );
   PrintArray( b );
}

void PrintArray( int[]& array ) {
   foreach ( let int element; array ) {
      Print( d: element );
   }
}
```

<h4>Structure variable references</h4>

<pre>
<i>structure</i> &amp; <i>var</i> = <i>reference</i> ;
</pre>

Similar to arrays, when you specify a structure variable, what you get is a reference to the structure variable:

```
#library "reftest"

struct NumberT {
   int value;
};

// Only references to private variables can be assigned to reference variables
// and passed around to functions.
private NumberT someNumber = { 123 };

script "Main" open {
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
<i>return-type</i> function( [<i>parameters</i>] ) [<i>qualifiers</i>] &amp; <i>var</i> = <i>reference</i> ;
</pre>

Similar to arrays and structure variables, when you specify a function, what you get is a reference to the function:

```
#library "reftest"

void F1() { Print( s: "F1() called" ); }
void F2() { Print( s: "F2() called" ); }

script "Main" open {
   void function()& f = F1;
   f(); // Output: F1() called
   f = F2;
   f(); // Output: F2() called
}
```

<h4>Nullable references</h4>

The _null reference_ is an invalid reference. A <em>nullable reference</em> is a reference that can either be a valid reference or the null reference. Nullable references can only be assigned to variables of nullable reference type. A nullable reference type is marked with a question mark (`?`) instead of an ampersand (`&`):

```
script "Main" open {
   int[]? r = null;
   struct S { int a; }? r2 = null;
   void function()? r3 = null;
}
```

<h5>Null check</h5>

The null reference cannot be dereferenced. Everytime you use a nullable reference, the game will check the reference. If it is the null reference, an error will be reported in the console and the current script will be terminated:

```
script "Main" open {
   int[]? r = null;
   r[ 0 ] = 123; // Prints error and terminates script.
}
```

<h5>From nullable to non-nullable</h5>

A nullable reference cannot be assigned to a non-nullable reference variable. The `!!` operator confirms that a nullable reference is not the null reference by performing the above null check and returns the nullable reference as a non-nullable reference:

```
script "Main" open {
   static int a[] = { 1, 2, 3 };
   int[]? r = a;
   // `r` refers to a valid reference, so will succeed.
   int[]& r2 = r!!;
   r = null;
   // `r` no longer refers to a valid reference, so an error will be printed
   // and the script will be terminated.
   r2 = r!!;
}
```

<h3>Strong Types</h3>

When assigning a value to a variable, the type of the value must match the type of the variable.

The primitive types are: `int`, `fixed`, `bool`, and `str`.

<h4><code>raw</code> type</h4>

To be compatible with ACS, the `raw` type is introduced. The `raw` type behaves like the `int` type, but has the following additional characteristics:

* A value of `raw` type can be assigned to a variable of any primitive type, and a variable of `raw` type can accept a value of any primitive type.
* In binary operations, when one operand is of `raw` type and the other is of primitive type, the operand of primitive type is implicitly casted to the `raw` type.

A value of primitive type gets implicitly casted to `raw`. In a namespace block qualified with `typeaware`, no such implicit casting occurs.

<h4>Casts</h4>

You can force the compiler to treat a value of one type as a value of another type:

```
typeaware namespace {

script "Main" open {
   Print( d: ( int ) "abc" );
   Print( s: ( str ) 0 );
}

}
```

<h4>Conversions</h4>

A value of one type can be converted to another type. A conversion looks like a function call: the type you want to convert to is the function name, and the value you want to convert is the argument.

<h5>Conversion to <code>int</code> type</h5>

<table>
  <tr>
    <th>Conversion</th>
    <th>Description</th>
  </tr>
  <tr>
    <td>int( int <i>value</i> )</td>
    <td>Returns <i>value</i>.</td>
  </tr>
  <tr>
    <td>int( fixed <i>value</i> )</td>
    <td>Returns the whole part of <i>value</i> as an integer.</td>
  </tr>
  <tr>
    <td>int( bool <i>value</i> )</td>
    <td>Returns 1 if <i>value</i> is <code>true</code>; otherwise, returns 0.</td>
  </tr>
  <tr>
    <td>int( str <i>value</i> )</td>
    <td><em>Not currently supported.</em></td>
  </tr>
</table>

<h5>Conversion to <code>fixed</code> type</h5>

<table>
  <tr>
    <th>Conversion</th>
    <th>Description</th>
  </tr>
  <tr>
    <td>fixed( int <i>value</i> )</td>
    <td>Returns a fixed-point number whose whole part is <i>value</i>.</td>
  </tr>
  <tr>
    <td>fixed( fixed <i>value</i> )</td>
    <td>Returns <i>value</i>.</td>
  </tr>
  <tr>
    <td>fixed( bool <i>value</i> )</td>
    <td>Returns 1.0 if <i>value</i> is <code>true</code>; otherwise, returns 0.0.</td>
  </tr>
  <tr>
    <td>fixed( str <i>value</i> )</td>
    <td><em>Not currently supported.</em></td>
  </tr>
</table>

<h5>Conversion to <code>bool</code> type</h5>

<table>
  <tr>
    <th>Conversion</th>
    <th>Description</th>
  </tr>
  <tr>
    <td>bool( int <i>value</i> )</td>
    <td>Returns <code>false</code> if <i>value</i> is 0.0; otherwise, returns <code>true</code>.</td>
  </tr>
  <tr>
    <td>bool( fixed <i>value</i> )</td>
    <td>Returns <code>false</code> if <i>value</i> is 0; otherwise, returns <code>true</code>.</td>
  </tr>
  <tr>
    <td>bool( bool <i>value</i> )</td>
    <td>Returns <i>value</i>.</td>
  </tr>
  <tr>
    <td>bool( str <i>value</i> )</td>
    <td>Returns <code>false</code> if <i>value</i> is the empty string (<code>""</code>); otherwise, returns <code>true</code>.</td>
  </tr>
  <tr>
    <td>bool( reference <i>value</i> )</td>
    <td>Returns <code>false</code> if <i>value</i> is the null reference; otherwise, returns <code>true</code>.</td>
  </tr>
</table>

<h5>Conversion to <code>str</code> type</h5>

<table>
  <tr>
    <th>Conversion</th>
    <th>Description</th>
  </tr>
  <tr>
    <td>str( int <i>value</i> )</td>
    <td>Same as: <code>StrParam( d: <i>value</i> )</code></td>
  </tr>
  <tr>
    <td>str( fixed <i>value</i> )</td>
    <td>Same as: <code>StrParam( f: <i>value</i> )</code></td>
  </tr>
  <tr>
    <td>str( bool <i>value</i> )</td>
    <td>Returns <code>""</code> if <i>value</i> is <code>true</code>; otherwise, returns <code>"1"</code>.</td>
  </tr>
  <tr>
    <td>str( str <i>value</i> )</td>
    <td>Returns <i>value</i>.</td>
  </tr>
</table>

<h4>Operations</h4>

In binary operations, the operands must be of the same type.

<h5>Operations of <code>fixed</code> type</h5>

<table>
   <tr>
   <th>Operation</th>
   <th>Result Type</th>
   <th>Description</th>
   </tr>
   <tr>
      <td><i>left</i> * <i>right</i></td>
      <td>fixed</td>
      <td>Same as: <code>FixedMul( <i>left</i>, <i>right</i> )</code></td>
   </tr>
   <tr>
      <td><i>left</i> / <i>right</i></td>
      <td>fixed</td>
      <td>Same as: <code>FixedDiv( <i>left</i>, <i>right</i> )</code></td>
   </tr>
</table>

<h5>Operations of <code>str</code> type</h5>

<table>
   <tr>
   <th>Operation</th>
   <th>Result Type</th>
   <th>Description</th>
   </tr>
   <tr>
     <td><i>left</i> == <i>right</i></td>
     <td>bool</td>
     <td>Same as: <code>( StrCmp( <i>left</i>, <i>right</i> ) == 0 )</code></td>
   </tr>
   <tr>
      <td><i>left</i> != <i>right</i></td>
      <td>bool</td>
      <td>Same as: <code>( StrCmp( <i>left</i>, <i>right</i> ) != 0 )</code></td>
   </tr>
   <tr>
      <td><i>left</i> &lt; <i>right</i></td>
      <td>bool</td>
      <td>Same as: <code>( StrCmp( <i>left</i>, <i>right</i> ) &lt; 0 )</code></td>
   </tr>
   <tr>
      <td><i>left</i> &lt;= <i>right</i></td>
      <td>bool</td>
      <td>Same as: <code>( StrCmp( <i>left</i>, <i>right</i> ) &lt;= 0 )</code></td>
   </tr>
   <tr>
      <td><i>left</i> &gt; <i>right</i></td>
      <td>bool</td>
      <td>Same as: <code>( StrCmp( <i>left</i>, <i>right</i> ) &gt; 0 )</code></td>
   </tr>
   <tr>
      <td><i>left</i> &gt;= <i>right</i></td>
      <td>bool</td>
      <td>Same as: <code>( StrCmp( <i>left</i>, <i>right</i> ) &gt;= 0 )</code></td>
   </tr>
   <tr>
      <td><i>left</i> + <i>right</i></td>
      <td>str</td>
      <td>Same as: <code>StrParam( s: <i>left</i>, s: <i>right</i> )</code></td>
   </tr>
   <tr>
      <td><i>left</i> += <i>right</i></td>
      <td>str</td>
      <td>Same as: <code>( <i>left</i> = StrParam( s: <i>left</i>, s: <i>right</i> ) )</code></td>
   </tr>
   <tr>
     <td><i>string</i>[ <i>index</i> ]</td>
     <td>int</td>
     <td>Same as: <code>GetChar( <i>string</i>, <i>index</i> )</code></td>
   </tr>
   <tr>
     <td>! <i>string</i></td>
     <td>bool</td>
     <td>Same as : <code>( StrCmp( <i>string</i>, "" ) == 0 )</code></td>
   </tr>
</table>

<h5>Operations of reference type</h5>

<table>
   <tr>
     <th>Operation</th>
     <th>Result Type</th>
     <th>Description</th>
   </tr>
   <tr>
     <td><i>left</i>&nbsp;==&nbsp;<i>right</i></td>
     <td>bool</td>
     <td>Returns <code>true</code> if <i>left</i> and <i>right</i> refer to the same object; otherwise, returns <code>false</code>.</td>
   </tr>
   <tr>
     <td><i>left</i> != <i>right</i></td>
     <td>bool</td>
     <td>Returns <code>true</code> if <i>left</i> and <i>right</i> refer to a different object; otherwise, returns <code>false</code>.</td>
   </tr>
   <tr>
      <td>! <i>operand</i></td>
      <td>bool</td>
      <td>Returns <code>true</code> if <i>operand</i> is the null reference; otherwise, returns <code>false</code>.</td>
   </tr>
   <tr>
      <td><i>operand</i> !!</td>
      <td>Reference</td>
      <td>See: <a href="#from-nullable-to-non-nullable">From nullable to non-nullable</a>.</td>
   </tr>
</table>

<h4>Functions</h4>

The `str` type has a function called `length()`. This function returns the length of the string:

```
typeaware namespace {

script "Main" open {
   // Same as: Print( d: StrLen( "Hello, World!" ) );
   Print( d: "Hello, World!".length() ); // Output: 13
}

}
```

--

An array has a function called `length()`. This function returns the length of the array dimension:

```
typeaware namespace {

int a[] = { 1, 2, 3 };

script "Main" open {
   Print( d: a.length() ); // Output: 3
}

}
```

<!--

<h4>Default initializers</h4>

When a variable does not have an initializer, it is initialized with a default value.

For `int` variables, the default initializer is 0.

For `fixed` variables, the default initializer is 0.0.

For `bool` variables, the default initializer is `false`.

For `str` variables, the default initializer is the string with index 0, which could be any string. Would be nice if the engine reserves index 0 for the empty string ("").

For mandatory reference variables, there is no default initializer; the variable must be initialized with a valid reference. For nullable reference variables, the default initializer is `null`.

For enumeration variables, the default initializer is the enumerator whose value is the default initializer of the base type. If no such enumerator exists, then there is no default initializer.

-->

<h3>Expressions</h3>

The assignment operation now produces a result. The result is the value being assigned. As a consequence, you can chain together multiple assignments or use an assignment in a condition.

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

For `strcpy()`, the `a:` is optional, unless you want to use the extra parameters:

```
script "Main" {
   static int v[ 10 ];
   strcpy( v, "abc" );
   strcpy( a: ( v, 0, 1 ), "abc" );
}
```

<h4>Short-circuit evaluation</h4>

In ACS, both of the operands to `&&` and `||` are evaluated at all times. In BCS, the right operand is only evaluated if necessary. For example, if the left operand to `&&` is false, there is no need to evaluate the right operand because the result of the operation will still be false. Similarly, if the left operand to `||` is true, there is no need to evaluate the right operand because the result of the operation will still be true. This is called _short-circuit evaluation_:

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

<h5>Radix constants</h5>

Like ACS, BCS supports radix constants, which allow you to specify the base of the number. The base can range from 2 to 36. The valid digits are, 0 to 9, and after that, `a` to `z`. The base and the number are separated by `r` (`_` in ACS):

```
script "Main" open {
   Print( d: 2r101 ); // Base-2 (Binary). Output: 5
   Print( d: 8r123 ); // Base-8 (Octal). Output: 83
   Print( d: 16rFF ); // Base-16 (Hexadecimal). Output: 255
   Print( d: 36rZZ ); // Base-36. Output: 1295
}
```

<h5>Digit separator</h5>

To improve readability of long numbers, an underscore can be used to separate digits into easily recognizable groups:

```
script "Main" open {
   Print( d: 2_000_000_000 ); // Output: 2000000000
   Print( d: 0b_1101_0111_0100_0110 ); // Output: 55110
}
```

<h4>Conditional operator (<code>?:</code>)</h4>

<pre>
<i>left</i> ? <i>middle</i> : <i>right</i>
<i>left</i> ? : <i>right</i>
</pre>

The `?:` operator is similar to the `if` statement. The left operand is evaluated first. If the result is `true`, then the middle operand is evaluated and returned; otherwise, the right operand is evaluated and returned:

```
script "Main" open {
   Print( d: 1 ? 123 : 321 ); // Output: 123
   Print( d: 0 ? 123 : 321 ); // Output: 321
}
```

The middle operand can be left out. In this case, the left operand is evaluated first. After an implicit conversion to `bool`, if the result is `true`, then the result before the implicit conversion is returned; otherwise, the right operand is evaluated and returned:

```
script "Main" open {
   Print( d: 123 ?: 321 ); // Output: 123
   Print( d: 0 ?: 321 );   // Output: 321
}
```

<h4>Copying arrays and structure variables</h4>

`memcpy()` is similar in syntax and functionality to [`strcpy()`](http://zdoom.org/wiki/StrCpy), but instead of copying strings, `memcpy()` copies arrays and structure variables.

<h5>Copying arrays</h5>

<pre>
bool memcpy ( a: ( <i>destination</i> [, int <i>offset</i> [, int <i>elementsToCopy</i>]] ) , <i>source</i> [, int <i>sourceOffset</i>] )
</pre>

```
int arrayA[] = { 1, 2, 3 };
int arrayB[] = { 4, 5, 6 };

script "Main" open {
   memcpy( arrayA, arrayB );
   // The elements of `arrayA` will now have the same value as the elements of
   // the `arrayB` array.
   Print( d: arrayA[ 1 ] ); // Output: 5
}
```

<h5>Copying structure variables</h5>

<pre>
bool memcpy ( <i>destination</i> , <i>source</i> )
</pre>

```
struct {
   int value;
} structA = { 123 }, structB = { 321 };

script "Main" open {
   memcpy( structA, structB );
   // The members of `structA` will now have the same value as the members of
   // the `structB` variable.
   Print( d: structA.value ); // Output: 321
}
```

<h5>Technical details</h5>

`memcpy()` is not a feature of the game engine. It is implemented by the compiler. The compiler will generate extra code behind the scenes to make it work. So be aware of that when writing performance critical code.

<h3>Miscellaneous</h3>

<h4>Keywords</h4>

New keywords in BCS: `assert`, `auto`, `blockscoping`, `enum`, `extern`, `false`, `fixed`, `foreach`, `let`, `memcpy`, `msgbuild`, `namespace`, `null`, `private`, `raw`, `struct`, `true`, `typeaware`, `typedef`, `upmost`, and `using`. In ACS, the `goto` keyword is reserved but is not used; in BCS, it is used to represent the [goto statement](#goto).

While the following identifiers still retain their usage, they can now also be used as function/variable names and are no longer reserved as keywords: `acs_executewait`, `acs_namedexecutewait`, `bluereturn`, `clientside`, `death`, `define`, `disconnect`, `encryptstrings`, `endregion`, `enter`, `event`, `hudmessage`, `hudmessagebold`, `import`, `include`, `kill`, `libdefine`, `library`, `lightning`, `log`, `net`, `nocompact`, `nowadauthor`, `open`, `pickup`, `redreturn`, `region`, `reopen`, `respawn`, `strparam`, `unloading`, `wadauthor`, and `whitereturn`.

<h2>bcc (Compiler)</h2>

<h3>Usage</h3>

<pre>
./bcc [<i>options</i>] <i>source-file</i> [<i>object-file</i>]
</pre>

<h4>Options</h4>

<table>
  <tr>
    <th>Option</th>
    <th>Description</th>
  </tr>
  <tr>
    <td>-acc-err</td>
    <td>On error, create an error file like one created by the acc compiler.</td>
  </tr>
  <tr>
    <td>-acc-stats</td>
    <td>Show compilation statistics like those shown by the acc compiler.</td>
  </tr>
  <tr>
    <td>-h</td>
    <td>Show help information.</td>
  </tr>
  <tr>
    <td>-i <i>directory</i></td>
    <td rowspan="2">Add a directory to search in for files.</td>
  </tr>
  <tr>
    <td>-I <i>directory</i></td>
  </tr>
  <tr>
    <td>-one-column</td>
    <td>Start column position at 1. Default is 0.</td>
  </tr>
  <tr>
    <td>-tab-size&nbsp;<i>size</i></code></td>
    <td>Specify the width of the tab character.</td>
  </tr>
  <tr>
    <td>-strip-asserts</td>
    <td>Do not include asserts in object file. (Asserts will not be executed at run-time.)</td>
  </tr>
  <tr>
    <td>-E</td>
    <td>Do preprocessing only.</td>
  </tr>
  <tr>
    <td>-D <i>name</i></td>
    <td>Create a macro with the specified name. The macro will be defined as <code>1</code>. The macro will only be available during the preprocessing of the main library.</td>
  </tr>
  <tr>
    <td>-x <i>language</i></td>
    <td>Specify the language of the source file. The language must be one of the following:
      <ul>
        <li><b>acs</b> (ACS as defined by the latest version of <a href="https://github.com/rheit/acc">acc</a>)</li>
        <li><b>acs95</b> (ACS as originally defined for Hexen)</li>
        <li><b>bcs</b> (See <a href="#bcs">BCS</a>)</li>
      </ul>
      If a language is not explicitly specified, the compiler will determine the language based on the file extension of the source file. If the file extension is <code>.bcs</code>, the language will be <b>bcs</b>; otherwise, <b>acs</b>.
    </td>
  </tr>
  <tr>
    <td>-l <i>library</i></td>
    <td>Create a link to the specified library. This is the same as: <code>#linklibrary "<i>library</i>"</code></td>
  </tr>
</table>

<h4>Cache options</h4>

<table>
  <tr>
    <th>Option</th>
    <th>Description</th>
  </tr>
  <tr>
    <td>-cache</td>
    <td>Enable caching of libraries. Only imported libraries are cached.</td>
  </tr>
  <tr>
    <td>-cache-dir <i>directory</i></td>
    <td>Store cache-related files in the specified directory. If one is not specified, a system-default temporary directory will be used.</td>
  </tr>
  <tr>
    <td>-cache-lifetime<br />&nbsp;&nbsp;&nbsp;<i>duration</i></td>
    <td>Keep a library cached for the specified duration. The duration is measured in hours.</td>
  </tr>
  <tr>
    <td>-cache-print</td>
    <td>Show the contents of the cache.</td>
  </tr>
  <tr>
    <td>-cache-clear</td>
    <td>Delete all cached library files.</td>
  </tr>
</table>

<h2>Credits</h2>

The following people have contributed to the __bcc__ project. The names are listed in alphabetical order. If I missed you, if you want me to use your personal name, or if a correction is needed, please let me know.

<table>
  <tr>
    <th>Name</th>
    <th>Contributions</th>
  </tr>
  <tr>
    <td><a href="https://github.com/caiosm1005">caiosm1005</a></td>
    <td>
      <ul>
        <li>Contributed code that checks for integer overflow in numeric literals</li>
      </ul>
    </td>
  </tr>
  <tr>
    <td><a href="https://github.com/csnxs">csnxs</a></td>
    <td>
      <ul>
        <li>Fixed compilation on systems that use glibc >= 2.19</li>
      </ul>
    </td>
  </tr>
  <tr>
    <td><a href="https://github.com/Hypnotoad90">Hypnotoad</a></td>
    <td>
      <ul>
        <li>Came up with the name, "bcc"</li>
        <li>For the upmost namespace, came up with the name, "upmost"</li>
      </ul>
    </td>
  </tr>
  <tr>
    <td><a href="https://github.com/MazterQyou">MazterQyou</a></td>
    <td>
      <ul>
        <li>Fixed compilation on certain operating systems</li>
      </ul>
    </td>
  </tr>
  <tr>
    <td><a href="https://github.com/Monsterovich">Monsterovich</a></td>
    <td>
      <ul>
        <li>Compiled and provided binaries to others</li>
      </ul>
    </td>
  </tr>
  <tr>
    <td><a href="https://github.com/wormt">Positron</a></td>
    <td>
      <ul>
        <li>Primary developer</li>
      </ul>
    </td>
  </tr>
</table>

<h2>Resources</h2>

* [ACC++ Manual](https://bitbucket.org/Blzut3/acc/src/25a415a3fcc6d16395cc832db85096fe93b66777/docs/manual.odt?at=default&fileviewer=file-view-default), by Blzut3. This document describes the ACS byte code.