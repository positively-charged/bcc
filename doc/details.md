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
script 1 ( void ) {}
script 1 () {}
script 1 {}
```

--

World and global variables can be created inside a script, a function, or any other block statement.

```
void add_kill() {
   global int 1:kills;
   ++kills;
}
```

You can specify the size of a dimension of world and global arrays. World and global arrays can be multidimensional.

```
script 1 open {
   global int 1:array[ 10 ];
   global int 2:multi_array[ 10 ][ 20 ];
   array[ 0 ] = 123;
   multi_array[ 0 ][ 1 ] = 321;
}
```

--

When creating an array, the size of the _first dimension_ can be omitted. The size will be determined based on the number of values found in the initialization part. So if the array is initialized with 5 values, the size of the dimension will be 5.

```
// The size of this array is 5, because it is initialized with 5 strings.
str names[] = {
   "Positron",
   "Hypnotoad",
   "AC3",
   "Frank",
   ""
};
```

--

The location of a region item doesn't matter. In the example below, a variable, a constant, and a function are used before they appear.

```
script 1 open {
   v = c;
   f(); // Output: 123
}

int v = 0;
enum c = 123;
void f() { Print( i: v ); }
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

<h4><code>auto</code></h4>

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