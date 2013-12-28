__Note:__ A feature described below might have influence from another language, but the characteristics of the feature, and the terminology used here, is not meant to follow the other language.

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
When calling functions like print(), a format block gives you more precision in the formatting of the message. A format block is like a normal code block, but also allows you to mix format items with other statements. (Format items are those colon-separated arguments passed to print() and the like.) Inside a format block, a format item uses a triple left bracket in the middle, not the colon.

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

A format block can contain function calls that themselves use a format block.

You cannot move into a format block with a goto statement, and you cannot move out of the format block with a break, continue, or goto statement. You must naturally enter and leave the format block.
