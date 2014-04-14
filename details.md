__Note:__ A feature described below might have influence from another language, but the characteristics of the feature, and the terminology used here, is not meant to follow the other language.

<h3>enums</h3>

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

<h3>structs</h3>

Creating a structure is like creating a structure in C. A structure has a name and a list of members.

```acs
// This is a structure. The name of the structure is "player", and the members
// are "number" and "name".
struct player {
   int number;
   str name;
};
```

```
// Create variable of a structure type.
struct player player;

script 1 enter {
   // Use structure.
   player.number = playerNumber();
   player.kills = 0;
   player.deaths = 0;
}

script 2 death {
   player.deaths = player.deaths + 1;
}
```

<h3>regions</h3>

A region is a group of functions, scripts, variables, constants, and other code. Regions are similar to namespaces, found in other programming languages.

```
script 1 open {
   my_region.v = my_region.c;
   my_region.f();
}

// ==========================================================================
region my_region;
// ==========================================================================

int v = 0;
enum c = 123;
void f() {}
```

Here we have a region called \`my_region\`. It contains a variable, a constant, and a function.

A region is created by using the <code>region</code> keyword, followed by the name of the region. Any code that follows will be part of the region.

To use an item of a region, you specify the region name, followed by the item you want to use. In script 1, we first select the constant from the region, then assign it to the variable found in the same region. Finally, we call the function.

====

You can have as many regions as you want.

Items of one region don't conflict with the items of another region. This means you can have a function called \`f()\` in one region and a function called \`f()\` in another region. They are different functions with the same name, but can exist because they are in different regions.

```
script 1 open {
   my_region.f();
   my_other_region.f();
}

// ==========================================================================
region my_region;
// ==========================================================================

void f() {}

// ==========================================================================
region my_other_region;
// ==========================================================================

void f() {}
```

====

A region can contain other regions. The region that contains the other region is called the parent region, and the region being contained is called the child region, or a nested region.

```
script 1 open {
   parent.f();
   parent.child.f();
}

// ==========================================================================
region parent;
// ==========================================================================

void f() {}

// ==========================================================================
region parent.child;
// ==========================================================================

void f() {}
```

You access the item of a child region like any other item. In the above example, from the \`parent\` region you select the \`child\`, then from the \`child\` region you select the \`f()\` function.

====

Inside a region, only the items of the region are visible. To use an item from elsewhere, there are multiple ways of getting a hold of the item.

<h3>Proper Scoping</h3>
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
   print( i : a );
   int a = 1;
   {
      // In scope #2
      int a = 2;
      print( i : a );
   }
   print( i : a );
}
```

<h6>Output:</h6>
<pre>
0
2
1
</pre>

<h3>Logical AND and OR Operators</h3>
In bcc, the logical AND (__&&__) and OR (__||__) operators exhibit short-circuit evaluation.

When using the logical AND operator, the left side is evaluated first. If the result is 0, the right side is __SKIPPED__, and the result of the operation is 0. Otherwise, the right side is then evaluated, and, like the left side, if the result is 0, the result of the operation is 0. Otherwise, the result of the operation is 1.

When using the logical OR operator, the left side is evaluated first. If the result is __NOT__ 0, the right side is __SKIPPED__, and the result of the operation is 1. Otherwise, the right side is evaluated, and if the result is not 0, the result of the operation is 1. Otherwise, the result of the operation is 0.

<h6>Code:</h6>
```
function int get_0( void ) {
   print( s : "called get_0()" );
   return 0;
}

function int get_1( void ) {
   print( s : "called get_1()" );
   return 1;
}

script 1 open {
   print( s : "get_0() && get_1() == ", i : get_0() && get_1() );
   print( s : "get_1() && get_0() == ", i : get_1() && get_0() );
   print( s : "get_0() || get_1() == ", i : get_0() || get_1() );
   print( s : "get_1() || get_0() == ", i : get_1() || get_0() );
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

<h3>goto statement</h3>
A goto statement is used to move to some location within a script or a function. A location is identified with a label. A label consists of a name, followed by a colon character. There must be no duplicate labels inside the same script or function.

<h6>Code:</h6>
````
script 1 open {
   goto bottom;
   top:
   print( s : "top" );
   goto end;
   bottom:
   print( s : "bottom" );
   goto top;
   end:
}
````

<h6>Output:</h6>
<pre>
bottom  
top
</pre>

<h3>Format Blocks</h3>
When calling functions like print(), a format block gives you more precision in the formatting of the message. A format block is like a normal code block, but also allows you to mix format items with other statements. (Format items are those colon-separated arguments passed to print() and the like.) Inside a format block, a format item uses a sequence of three left-angled brackets in the middle, not the colon.

<h6>Code:</h6>
```
script 1 open {
   bool alive = false;
   print( {
      s <<< "The monster is: ";
      if ( alive ) {
         s <<< "Alive and doing well";
      }
      else {
         s <<< "Dead";
      }
   } );
}
```

<h6>Output:</h6>
<pre>The monster is: Dead</pre>

A format block can contain function calls that themselves contain a format block.

You cannot move into a format block with a goto statement, and you cannot move out of the format block with a break, continue, or goto statement. You must naturally enter and leave the format block.

<h3>Optional Parameters</h3>

```
void print_string( str string = "Hello, World!" ) {
   Print( s : string );
}

script 1 open {
   print_string( "Hello, Fine Fella!" ); // Output: Hello, Fine Fella!
   print_string();                       // Output: Hello, World!
}
```

If you call <code>print_string()</code> with an argument, your argument will be used. If you don't provide an argument, a default argument will be used. In the above example, the default argument is the <code>"Hello, World!"</code> string.

A script cannot have optional parameters.

====

You can have multiple optional parameters.

```
void print_numbers( int a = 111, int b = 999 ) {
   Print( s : "a ", i : a, s : ", b ", i : b );
}

script 1 open {
   print_numbers( 5, 35 ); // Output: a 5, b 35
   print_numbers( 5 );     // Output: a 5, b 999
   print_numbers();        // Output: a 111, b 999
}
```

====

You can have required parameters and optional parameters in the same function. The optional parameters must follow the required parameters. So the required parameters appear first, then come the optional parameters.

```
// Correct.
void printf( str format, int arg1 = 0, int arg2 = 0, int arg3 = 0 ) {}

// Incorrect.
void printf( int arg1 = 0, int arg2 = 0, int arg3 = 0, str format ) {}
```

====

Optional parameters having a string as a default argument, now work with builtin functions. This means you can use the <code>MorphActor()</code> function with one argument, as allowed by the [wiki page](http://zdoom.org/wiki/MorphActor):

```
script 1 enter {
   // These are the same.
   MorphActor( 0 );
   MorphActor( 0, "", "", 0, 0, "", "" );
}
```

<h3>Loading Files</h3>

<code>#include</code> and <code>#import</code> perform the same action. These directives tell the compiler to load a file. Each file is loaded once; a request to load a file again will be ignored. Every <code>#include</code> and <code>#import</code> is processed, even in libraries.

If a file starts with a <code>#library</code> directive, the file is a library. The <code>#library</code> directive must appear at the very top, before any other code except for comments:

```
// File: nice_library.acs
#library "lumpname"
#include "zcommon.acs"

// File: not_so_nice_library.acs
#include "zcommon.acs"
#library "lumpname"
```

If multiple libraries have the same name, the libraries are merged into one. This allows you to break up a library into multiple files, then load each file separately, as needed.

If library-A loads library-B, and library-B loads library-C, library-A can use the objects of library-C. The compiler will try and figure out which libraries to load at run-time based on the objects you use. (It is not recommended you do this. If you want to use an object from a library, explicitly load the library with an <code>#include</code> or an <code>#import</code> directive.)

If a map loads your library and this library has an array, you can change the size of the array and recompile the library. The map will still be able to use this array, but now with a new size.

<h3>Miscellaneous</h3>

There are new keywords: <code>enum</code>, <code>false</code>, <code>fixed</code>, <code>region</code>, <code>struct</code>, <code>true</code>, and <code>upmost</code>. <code>fixed</code> is currently reserved but is not used. In acc, the <code>goto</code> keyword is reserved but is not used; in bcc, it is used to represent the goto statement.

In acc, there are keywords that are not used in bcc: <code>define</code>, <code>include</code>, <code>print</code>, <code>printbold</code>, <code>log</code>, <code>hudmessage</code>, <code>hudmessagebold</code>, <code>nocompact</code>, <code>wadauthor</code>, <code>nowadauthor</code>, <code>acs_executewait</code>, <code>encryptstrings</code>, <code>library</code>, <code>libdefine</code>, <code>strparam</code>, and <code>strcpy</code>. You can use these identifiers as names for your own objects.

====

It is not necessary to <code>#include "zcommon.acs"</code> in order to use the boolean literals. The boolean literals <code>true</code> and <code>false</code> are now keywords. <code>true</code> is the value 1, and <code>false</code> is the value 0.

====

The following directives are not supported: <code>#wadauthor</code>, <code>#nowadauthor</code>, and <code>#nocompact</code>.

====

Some limitations have been removed. You can now have more than 128 map variables. You can also have more than 256 functions.

====

When a script has no parameters, the <code>void</code> keyword is not necessary. The parentheses are not required either.

```
// These are all the same.
script 1 ( void ) {}
script 1 () {}
script 1 {}
```

====

When creating a function, the <code>function</code> keyword is not necessary. If the function has no parameters, the <code>void</code> keyword is not necessary.

```
// These are the same.
function void f( void ) {}
void f() {}
```

====

When a function returns a value, it is not necessary to have a return statement at the end of the function. (In fact, as of this time, it is possible to skip the return statement entirely. It's possible, but <strong>don't</strong> do this.)

```
// Get absolute value of number.
int abs( int number ) {
   if ( number < 0 ) {
      return number * -1;
   }
   else {
      return number;
   }
}
```

====

The name of a function or script parameter is not required. You still need to pass an argument, but you won't be able to use such a parameter. This can be used to indicate a parameter is no longer used.

```
int sum( int used1, int, int used2 ) {
   return used1 + used2;
}

script 1 open {
   Print( i : sum( 100, 0, 200 ) ); // Output: 300
}
```

====

World and global variables can be created inside a script, a function, or any other block statement.

```
void add_kill() {
   global int 1:kills;
   ++kills;
}
````

====

When creating an array, the size of the <em>first dimension</em> can be omitted. The size will be determined based on the number of values found in the initialization part. So if the array is initialized with 5 values, the size of the dimension will be 5.

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

====

The location of a region item doesn't matter. In the example below, a variable, a constant, and a function are used before they appear.

```
script 1 open {
   v = c;
   f(); // Output: 123
}

int v = 0;
enum c = 123;
void f() { Print( i : v ); }
```

====

The assignment operation now produces a result. The result is the value being assigned. This way, you can chain together multiple assignments or use an assignment in a condition.

```
script 1 open {
   int a, b, c;
   a = b = c = 123; // a, b, and c now have the value 123.
   // First a random number is generated. Then the random number is assigned to
   // variable `a`. The result of the assignment, which is the random number,
   // is then checked if it's not 3.
   while ( ( a = random( 0, 10 ) ) != 3 ) {
      Print( s : "Bad number: ", i : a );
   }
}
```

====

There are two functions that are associated with the <code>str</code> type: at() and length(). These functions can only be called on a value or a variable of <code>str</code> type. at() returns the character found at the specified index, and length() returns the length of the string.

```
script 1 open {
   Print( c : "Hello, World!".at( 7 ) );  // Output: W
   Print( i : "Hello, World!".length() ); // Output: 13
}
```

====

When multiple strings appear next to each other, they are combined into one. This can be used to break up a long string into smaller parts, making it easier to see the whole string.

```
script 1 open {
   Print( s : "Hello, " "World" "!" ); // Output: Hello, World!
}
```

====

A line of code can be broken up into multiple lines. To break up a line, position your cursor somewhere in the line, type in a backslash character, then press Enter. Make sure no other characters follow the backslash.

```
script 1 open {
   str reallyniceintro = "Hello, World!";
   Print( s : really\
nice\
intro );
}
```