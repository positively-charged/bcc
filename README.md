<kbd>bcc</kbd> is a BCS, ACS, and ACS95 compiler.

## Supported Scripting Languages

<kbd>bcc</kbd> can compile source code written in the following languages.

### BCS

```
strict namespace SampleCode {
   script "Main" open {
      static str basket[] = { "apples", "oranges", "pears" };
      foreach ( auto fruit; basket ) {
         Print( s: "I love ", s: fruit, s: ( fruit == "oranges" ) ?
            " very much" : "" );
      }
   }
}
```

BCS is an extension of ACS. BCS is mostly compatible with ACS and provides many interesting and useful features, including the following:

* Structures
* Enumerations
* Namespaces
* Preprocessor
* Strong types
* Block scoping
* Optional function parameters
* `&&` and `||` operators are short-circuited
* `foreach` loop
* `?:` operator

See the [wiki](https://github.com/wormt/bcc/wiki) for an overview of the features.

### ACS/ACS95
<kbd>bcc</kbd> can also compile ACS and ACS95 code. ACS95 is the ACS scripting language that was used for scripting Hexen. The name, ACS95, is invented by <kbd>bcc</kbd> to distinguish between the two languages.
