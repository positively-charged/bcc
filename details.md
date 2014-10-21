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

<h3>regions</h3>

A region is a group of functions, scripts, variables, constants, and other code. Regions are similar to namespaces, found in other programming languages.

```
script 1 open {
   stuff::v = stuff::c;
   stuff::f();
}

region stuff {
   int v = 0;
   enum c = 123;
   void f() {}
}
```

A region is created by using the <code>region</code> keyword, followed by the name and body of the region. The body is delimited by braces and contains code like functions, scripts, and variables. In the above example, we have a region called <code>stuff</code>. It contains a variable, a constant, and a function.

To use an item of a region, we specify the region name, followed by the item we want to use. In script 1, we first select the constant from the region, then assign it to the variable found in the same region. Finally, we call the function.

---

You can have as many regions as you want.

Items of one region don't conflict with the items of another region. This means you can have a function called <code>f()</code> in one region and a function called <code>f()</code> in another region. They are different functions with the same name, but can exist because they are in different regions.

```
script 1 open {
   stuff::f();
   other_stuff::f();
}

region stuff {
   void f() {}
}

region other_stuff {
   void f() {}
}
```

---

A region can contain other regions. The region that contains the other region is called the parent region, and the region being contained is called the child region, or a nested region.

```
script 1 open {
   parent::f();
   parent::child::f();
}

region parent {
   void f() {}
   region child {
      void f() {}
   }
}
```

We access the item of a child region like any other item. In the above example, from the <code>parent</code> region we select the <code>child</code>, then from the <code>child</code> region we select the <code>f()</code> function.

---

Code found outside a region you create is part of the __upmost__ region. The upmost region is an implicitly-created region that contains all other regions, and other code.

```
// HERE: In upmost region.

region a {
   // HERE: In region `a`
}

region b {
   // HERE: In region `b`
}

// HERE: In upmost region.
```

The upmost region doesn't have a name. You can refer to the upmost region using the `upmost` keyword.

```
int a = 123;

script 1 open {
   int a = 321;
   Print( i: a );         // Output: 321
   Print( i: upmost::a ); // Output: 123
}
```

In the above example, the inner `a` variable hides the outer `a` variable. To access the outer variable, we first select the upmost region, then from the upmost region, we select the variable.

---

Inside a region, only the items of the region are visible. To use an item found outside the region, there are multiple ways.

```
region a {
   void f() {}
}

region b {
   script 1 open {
      upmost::a::f();
   }
}
```

In the above example, we access `f()` by first going into the upmost region, then into the `a` region where we find the function. Accessing items through the upmost region can get cumbersome. Instead, we can import items from a region.

---

```
region b {
   import upmost: a;
   script 1 open {
      a::f();
   }
}
```

When making an `import`, we first select what region we want to import from. Then we select which items we want to import. In the above example, we select the upmost region, and from the upmost region, we select the `a` region to be imported.

---

```
region b {
   import upmost: a_alias = a;
   script 1 open {
      a_alias::f();
   }
}
```

We can create an alias to an imported item.

---

```
region b {
   import upmost: top = region;
   script 1 open {
      top::a::f();  
   }
}
```

We can also create an alias to the selected region. In the above example, we select the upmost region, then use the `region` keyword to refer to the selected region as the item to import. At the same time, we create an alias to it.

---

```
region b {
   import upmost: region = a;
   script 1 open {
      f();
   }
}
```

We can establish a relationship with another region. This is like importing every item from a region. The item selected during the `import` must be a region for this to work.

---

You can import multiple items by separating them with a comma.

```
region a {
   import std: Print, Delay;
   script 1 open {
      Delay( 35 * 3 );
      Print( s: "Hello, World!" );
   }
}
```

_Sidenote:_ The standard functions `Print()` and `Delay()` are found in the `std` region. The `std` region is found in the upmost region. The `std.acs` file creates the `std` region.

Note how we don't need to specify the upmost region in the `import`; using just `std` is enough. It is assumed that the first region specified is found in the upmost region.

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

<h3>Logical AND and OR Operators</h3>
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

<h3>goto statement</h3>

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

<h3>Format Blocks</h3>

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

<h3>Optional Parameters</h3>

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

<h3>Printing Section of an Array</h3>

<h5>Syntax</h5>
<pre>
Print( a:( array<b>[</b>, start<b>[</b>, length<b>]]</b> ) );
</pre>

If `start` is specified, printing will begin from this index. If `length` is also specified, then `length` amount of characters will be printed.

It is important to provide correct arguments. `start` must not be negative and must not be greater than or equal to the array size. `length` must be between zero and the array size, inclusive. If these requirements are not met, bad things will happen!

<h5>Example</h5>
<pre>
#include "zcommon.acs"

<b>int</b> array[] = { 'a', 'b', 'c', 'd' };

<b>script</b> 1 <b>open</b> {
   Print( a:( array ) );       // Output: abcd
   Print( a:( array, 1 ) );    // Output: bcd
   Print( a:( array, 1, 1 ) ); // Output: c
}
</pre>

<h5>Technical</h5>

This feature uses a special instruction. At this time, Zandronum does not support this instruction so existing instructions need to used to emulate this feature. This means your object file will be bigger and the number of instructions executed will be larger.

<h3>Miscellaneous</h3>

There are new keywords: `enum`, `false`, `fixed`, `region`, `struct`, `true`, and `upmost`. `fixed` is currently reserved but is not used. In acc, the `goto` keyword is reserved but is not used; in bcc, it is used to represent the goto statement.

The following keywords can be used as names for your objects and are no longer reserved: `define`, `include`, `print`, `printbold`, `log`, `hudmessage`, `hudmessagebold`, `nocompact`, `wadauthor`, `nowadauthor`, `acs_executewait`, `encryptstrings`, `library`, `libdefine`, `strparam`, and `strcpy`.

---

It is not necessary to `#include "zcommon.acs"` in order to use the boolean literals. The boolean literals `true` and `false` are now keywords. `true` is the value 1, and `false` is the value 0.

---

The following directives are not supported: `#wadauthor` and `#nowadauthor`

---

When a script has no parameters, the `void` keyword is not necessary. The parentheses are not required either.

```
// These are all the same.
script 1 ( void ) {}
script 1 () {}
script 1 {}
```

---

When creating a function, the `function` keyword is not necessary. If the function has no parameters, the `void` keyword is not necessary.

```
// These are all the same.
function void f( void ) {}
function void f() {}
void f() {}
```

---

When a function returns a value, it is not necessary to have a return statement at the end of the function. (In fact, as of this time, it is possible to skip the return statement entirely. It's possible, but __don't__ do this.)

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

---

The name of a function or script parameter is not required. You still need to pass an argument, but you won't be able to use such a parameter. This can be used to indicate a parameter is no longer used.

```
int sum( int used1, int, int used2 ) {
   return used1 + used2;
}

script 1 open {
   Print( i: sum( 100, 0, 200 ) ); // Output: 300
}
```

---

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

---

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

---

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

---

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

---

There are two functions that are associated with the `str` type: `at()` and `length()`. These functions can only be called on a value or a variable of `str` type. `at()` returns the character found at the specified index, and `length()` returns the length of the string.

```
script 1 open {
   Print( c: "Hello, World!".at( 7 ) );  // Output: W
   Print( i: "Hello, World!".length() ); // Output: 13
}
```

---

When multiple strings appear next to each other, they are combined into one. This can be used to break up a long string into smaller parts, making it easier to see the whole string.

```
script 1 open {
   Print( s: "Hello, " "World" "!" ); // Output: Hello, World!
}
```

---

A line of code can be broken up into multiple lines. To break up a line, position your cursor somewhere in the line, type in a backslash character, then press Enter. Make sure no other characters follow the backslash.

```
script 1 open {
   str reallyniceintro = "Hello, World!";
   Print( s: really\
nice\
intro );
}
```