<h2>BCS</h2>

<ul>
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
   <li><a href="#expressions">Expressions</a></li>
   <li><a href="#miscellaneous">Miscellaneous</a></li>
</ul>

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

The BCS preprocessor replicates much of the behavior of the C99 preprocessor. The preprocessor is case-sensitive. To be compatible with ACS, the preprocessor-based `#define` and `#include` only get executed if they appear in an `#if/#ifdef/#ifndef` block.

```
#if 1
   #define MAKE_STR( arg ) #arg
#endif

script MAKE_STR( 1 2 3 ) open {
   Print( s: MAKE_STR( Hello World! ) );
}
```

--

When multiple strings appear next to each other, they are combined into one. This can be used to break up a long string into smaller parts, making it easier to see the whole string.

```
script 1 open {
   Print( s: "Hello, " "World" "!" ); // Output: Hello, World!
}
```

<h3>Namespaces</h3>

Namespaces in BCS work similar to namespaces in other languages like C++ and C#.

```
namespace Test {
   str message = "Hello, World!";
   void Print( str message ) {
      upmost.Print( s: message );
   }
}

script 1 open {
   Test.Print( Test.message );
}
```

<h4><code>using</code></h4>

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

For multidimensional arrays, the nested initializer with the most values will determine the length of the dimension:

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

<h4>Block Scoping</h4>
In bcc, names of objects follow scoping rules.

A scope is a set of names, each name referring to some object&#8212;a variable say. The same name can be used to refer to some other object&#8212;such as a constant&#8212;as long as it's not in the same scope. Anywhere in your code, some scope is active. When you create a variable, say, the name of the variable is placed in the active scope.

A scope is commonly created by a code block, the statement delimited by braces ({}). At the start of a code block, a new scope is created. This new scope is now the active scope. The previous scope is still there, but is now the parent of the new scope. At the end of the code block, the active scope is destroyed, and the parent scope becomes the active scope again.

When using a name, the compiler will first search the active scope. If the name is not found, the parent scope is then searched. This continues until the top scope is reached. (The __top scope__ is the first scope made and the last scope searched. It is the scope containing objects like scripts and functions.) Through this process, the first instance of the name found is the instance that will be used to retrieve an object.

<h6>Code:</h6>
```
// In scope #0 (top scope)
int a = 0;

script 1 open {
   // In scope #1
   print( i: a );
   int a = 1;
   {
      // In scope #2
      int a = 2;
      print( i: a );
   }
   print( i: a );
}
```

<h6>Output:</h6>
<pre>
0
2
1
</pre>

<h4>Type Deduction</h4>

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

<h4><code>typedef</code></h4>

<h3>Enumerations</h3>

An <code>enum</code> is used to create a group of constants.

```
enum {
   CONSTANT_A, // 0
   CONSTANT_B, // 1
   CONSTANT_C  // 2
};
```

The first constant has a value of 0, the next constant has a value of 1, and so on. The value increases by 1.

```
enum {
   CONSTANT_A,      // 0
   CONSTANT_B = 10, // 10
   CONSTANT_C       // 11
};
```

The value of a constant can be changed. The next value will increase starting from the new value.

```
enum CONSTANT_LONESOME = 123;
```

A single constant can also be created. This is similar to using <code>#define</code>.

<h5>Other Details</h5>

The last constant can have a comma after it. This is a convenience feature.

```
enum {
   CONSTANT_A,
   CONSTANT_B,
   CONSTANT_C, // Comma here is allowed.
};
```

An <code>enum</code> can appear in a script. The constants are only visible inside the script. An <code>enum</code> can also appear in a function, and in other block statements.

```
script 1 open {
   enum { CONSTANT_A, CONSTANT_B, CONSTANT_C };
   enum CONSTANT_LONESOME = 123;
   // Constants can be used here...
}

// ...but not here.
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
// These are all the same.
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

<h4>Optional Parameters</h4>

```
void print_string( str string = "Hello, World!" ) {
   Print( s: string );
}

script 1 open {
   print_string( "Hello, Fine Fella!" ); // Output: Hello, Fine Fella!
   print_string();                       // Output: Hello, World!
}
```

If you call `print_string()` with an argument, your argument will be used. If you don't provide an argument, a default argument will be used. In the above example, the default argument is the `"Hello, World!"` string.

A script cannot have optional parameters.

---

You can have multiple optional parameters.

```
void print_numbers( int a = 111, int b = 999 ) {
   Print( s: "a ", i: a, s: ", b ", i: b );
}

script 1 open {
   print_numbers( 5, 35 ); // Output: a 5, b 35
   print_numbers( 5 );     // Output: a 5, b 999
   print_numbers();        // Output: a 111, b 999
}
```

---

You can have required parameters and optional parameters in the same function. The optional parameters must follow the required parameters. So the required parameters appear first, then come the optional parameters.

```
// Correct.
void printf( str format, int arg1 = 0, int arg2 = 0, int arg3 = 0 ) {}

// Incorrect.
void printf( int arg1 = 0, int arg2 = 0, int arg3 = 0, str format ) {}
```

---

Optional parameters having a string as a default argument, now work with builtin functions. This means you can use the `MorphActor()` function with one argument, as allowed by the [wiki page](http://zdoom.org/wiki/MorphActor):

```
script 1 enter {
   // These are the same.
   MorphActor( 0 );
   MorphActor( 0, "", "", 0, 0, "", "" );
}
```

<h4>Nested Functions</h4>

<h4>Message-building Functions</h4>

When calling functions like `Print()`, a format block gives you more precision in the composition of the message. A format block is like a normal block of code, but it can also contain format items. (Format items are those colon-separated arguments you pass to `Print()` and the like.) Inside a format block, a format item uses `:=` in the middle, not `:`. Everytime a format item is encountered, it is added as a part of the message. By mixing format items with other statements, you can create a different message based on how those other statements execute.

```
script 1 open {
   bool alive = false;
   Print( {} ) := {
      s:= "The monster is: ";
      if ( alive ) {
         s:= "Alive and doing well";
      }
      else {
         s:= "Dead";
      }
   }
   // Output: The monster is: Dead
}
```

In the example above, we call the `Print()` function. In the argument list, we use `{}`. This indicates we want to use a format block. Following the function call, we add `:=`. This is just a syntactic requirement. Then we have a block of code. In the block, we have a basic `if` statement and a few format items. The first format item is added to the message. If the `if` statement is true, the second format item is added to the message; otherwise, the third.

```
script 1 open {
   HudMessage( {}; HUDMSG_PLAIN, 0, 0, 1.5, 0.5, 5.0 ) := {
      for ( int i = 0; i < 10; ++i ) {
         c:= '*';
      }
   }
   // Output: **********
}
```

In this example, we print a border consisting of 10 `*` characters. Change the 10 to some other number to print a border of a different length.

---

<h5>Other Details</h5>

Functions like `HudMessage()` take multiple arguments. When calling such a function, the format block is executed first, then the rest of the arguments.

A format block can contain a function call that itself uses a format block.

You cannot move into a format block with a `goto` statement, and you cannot move out of a format block with a `break`, `continue`, or `goto` statement. You must naturally enter and leave the format block.

Inside a format block, calling a waiting function like `Delay()` is not allowed.

<h3>Statements</h3>

<h4><code>foreach</code></h4>

<pre>
foreach ( <i>value</i> ; <i>collection</i> ) {}
foreach ( <i>key</i>, <i>value</i> ; <i>collection</i> ) {}
</pre>

<h4><code>goto</code></h4>

A <code>goto</code> statement is used to move to some location within a script or a function. A location is identified with a label. A label consists of a name, followed by a colon character. There must be no duplicate labels inside the same script or function.

```
script 1 open {
   goto bottom;
   top:
   Print( s: "top" );
   goto end;
   bottom:
   Print( s: "bottom" );
   goto top;
   end:
   // Output:
   // bottom
   // top
}
```

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

<h3>Expressions</h3>

The assignment operation now produces a result. The result is the value being assigned. This way, you can chain together multiple assignments or use an assignment in a condition.

```
script 1 open {
   int a, b, c;
   a = b = c = 123; // a, b, and c now have the value 123.
   // First a random number is generated. Then the random number is assigned to
   // variable `a`. The result of the assignment, which is the random number,
   // is then checked if it's not 3.
   while ( ( a = random( 0, 10 ) ) != 3 ) {
      Print( s: "Bad number: ", i: a );
   }
}
```

--

There are two functions that are associated with the `str` type: `at()` and `length()`. These functions can only be called on a value or a variable of `str` type. `at()` returns the character found at the specified index, and `length()` returns the length of the string.

```
script 1 open {
   Print( c: "Hello, World!".at( 7 ) );  // Output: W
   Print( i: "Hello, World!".length() ); // Output: 13
}
```

<h4>Logical-AND and Logical-OR</h4>
In bcc, the logical AND (__&&__) and OR (__||__) operators exhibit short-circuit evaluation.

When using the logical AND operator, the left side is evaluated first. If the result is 0, the right side is __SKIPPED__, and the result of the operation is 0. Otherwise, the right side is then evaluated, and, like the left side, if the result is 0, the result of the operation is 0. Otherwise, the result of the operation is 1.

When using the logical OR operator, the left side is evaluated first. If the result is __NOT__ 0, the right side is __SKIPPED__, and the result of the operation is 1. Otherwise, the right side is evaluated, and if the result is not 0, the result of the operation is 1. Otherwise, the result of the operation is 0.

<h6>Code:</h6>
```
function int get_0( void ) {
   print( s: "called get_0()" );
   return 0;
}

function int get_1( void ) {
   print( s: "called get_1()" );
   return 1;
}

script 1 open {
   print( s: "get_0() && get_1() == ", i: get_0() && get_1() );
   print( s: "get_1() && get_0() == ", i: get_1() && get_0() );
   print( s: "get_0() || get_1() == ", i: get_0() || get_1() );
   print( s: "get_1() || get_0() == ", i: get_1() || get_0() );
}
```

<h6>Output:</h6>
<pre>
  called get_0()
get_0() && get_1() == 0
  called get_1()
  called get_0()
get_1() && get_0() == 0
  called get_0()
  called get_1()
get_0() || get_1() == 1
  called get_1()
get_1() || get_0() == 1
</pre>

Notice in the first expression, when the left side is 0, get_1() is not called. Similarly, in the final expression, when the left side of the expression is 1, get_0() is not called.

In simpler words: In the following discussion, _false_ is the value 0 and _true_ is any other value. When using the logical AND operator, you'll get 1 only if both sides are true. If the left side is false, the right side is skipped because the condition to get 1 won't be met. When using the logical OR operator, you'll get 1 as long as one of the sides is true. If the left side is true, there is no need to evaluate the right side, because the condition is already met.

<h3>memcpy()</h3>

<h3>Miscellaneous</h3>

New keywords in BCS: `assert`, `auto`, `enum`, `extern`, `false`, `fixed`, `foreach`, `let`, `memcpy`, `msgbuild`, `namespace`, `null`, `private`, `raw`, `struct`, `true`, `typedef`, `upmost`, and `using`. In ACS, the `goto` keyword is reserved but is not used; in BCS, it is used to represent the goto statement.

The following are keywords in ACS, but they are not keywords in BCS: `acs_executewait`, `acs_namedexecutewait`, `bluereturn`, `clientside`, `death`, `define`, `disconnect`, `encryptstrings`, `endregion`, `enter`, `event`, `hudmessage`, `hudmessagebold`, `import`, `include`, `kill`, `libdefine`, `library`, `lightning`, `log`, `net`, `nocompact`, `nowadauthor`, `open`, `pickup`, `redreturn`, `region`, `reopen`, `respawn`, `strparam`, `unloading`, `wadauthor`, and `whitereturn`. These identifiers can be used as names for your functions and variables.

--

It is not necessary to `#include "zcommon.acs"` in order to use the boolean literals. The boolean literals `true` and `false` are now keywords. `true` is the value 1, and `false` is the value 0.

<h2>bcc (Compiler)</h2>

<h3>Command-line Options</h3>

<h3>Cache</h3>