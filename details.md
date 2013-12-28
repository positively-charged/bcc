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
> bottom  
> top
