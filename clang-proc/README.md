# clang-proc

## Introduction

This is a program for finding all functions among given set of source files (which belong to the same linked module) and recognize all of its argument types (and return types). Types are derived recursively and JSON database with all function and types is generated.

Consider the following function with corresponding types:

```c
struct Y {
    float f;
};

struct Z {
    void (*pf)(struct Y*);
};

struct X {
    unsigned long z;
    struct X* _X;
    struct Y* _Y;
    struct Z _Z;
}

int foo(struct X* pX) { ... }
```

In this case one function is recognized together with all 10 distinct types (`int`,`struct X`,`struct X*`,`unsigned long`,`struct Y`,`struct Y*`,`struct Z`,`float`,`void` and `void (*)(struct Y*)`). Information with size for each type is preserved.

How it can be used?

The JSON file virtually contains all the information required to properly recreate all the type information (header files) for specific compilation of given source files. The first thing that comes to mind where this can be useful is generation of fuzzing wrappers for specific functions from kernel or libraries (when writing simple fuzzing application for a given function it is always required to include relevant header files with all type definitions; this disables the possibility of generation such fuzzing wrappers (as deciding which header files to include is non-trivial in the mildest sense of this word)).

## JSON database format

JSON database is composed of the following main entries:
1. "funcs" : list of all functions extracted from source files
2. "sources" : list of all source files that were used to create the database
3. "types" : list of all recognized types

### Functions

Exemplary entry for a function looks like follows:
```
  {
    "hash": "R2kfJrOE1wuDWMPw0f9jJb9gHzMAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAsCA==",
    "name": "wmi_print_event_log_buffer",
    "calls": 
      [ 67347, 10702, 10894, 10604 ],
    "nargs": 4,
    "refcount": 1,
    "linkage": "internal",
    "location": "/src/android/android_linux/kernel/wahoo-kernel/drivers/staging/qca-wifi-host-cmn/wmi/src/wmi_unified.c:598:1",
    "fid": 155,
    "variadic": false,
    "attributes": [],
    "declhash": "ZSZvvmQsurdSb+ldS08TqiEcSw86NjQ7OjY0O1A6OkI6OnZvaWQ6MDs6NjQ7gAAAAAAAAAAAAAAAAAAAAAAFaA==",
    "id": 12423,
    "types": 
      [ 8, 20886, 3, 14416, 11 ]
  },
```
`hash` represents the hash of function body (this is one of the factors how functions are distinguished between each other). Hash is computed from the AST using the clang `printPretty` function of the root function compound statement:
```c++
const FunctionDecl* D = /* Acquire function definition */;
Stmt* body = D->getBody();
body->printPretty(...);
```
This has the effect of indenting the body source code (as you would do correct indentation with any decent IDE) so semantically the same functions that differ only by the whitespaces and comments will be considered the same.

`name` is a function name.

`calls` lists all the functions (through function IDs) that are called from this function (if a function is called multiple times it is referenced only once in the `calls` entry). The order of function calls is not preserved, it merely lists the the occurence of function invocation within the body of this function.

`nargs` specified number of arguments of this function.

`refcount` is number of translation units this function appeared in.

`linkage` specifies the linkage of this function. The possible values are `internal` (static function) or `external` (normal function).

`location` specifies where exactly in the source code this function was defined (or declared).

`fid` is the id of the source file among all the files that this linked module is composed of. The mapping between source file paths and corresponding ids is located in the `sources` main JSON database entry:
```
"sources": 
    [
      { "/src/android_kernel_exynos9810/drivers/gpu/arm/tHEx/r10p0/platform/exynos/gpu_notifier.c" : 0 },
      { "/src/android_kernel_exynos9810/drivers/media/platform/exynos/fimc-is2/ischain/fimc-is-v6_0_0/fimc-is-subdev-sensor_vc2.c" : 1 },
      { "/src/android_kernel_exynos9810/drivers/video/fbdev/exynos/panel/dimming.c" : 2 },
      { "/src/android_kernel_exynos9810/drivers/video/fbdev/exynos/dpu_9810/secdp_unit_test.c" : 3 },
      ...
    ]
```

`variadic` specifies whether the function is variadic, i.e. the function type arguments end with "...".

`attributes` lists all the special attributes assigned to the function. Possible attributes might be `always_inline`, `no_instrument_function`, `unused` etc.

`declhash` represents the hash of function declaration. This is used to link this specific function definition with declarations in other translation units.

`id` uniquely identifies this function. Function IDs starts from 0 and are consecutive in the database.

`types` is the list of referenced types, i.e. first value is an id of the return type, the rest are the types of all declared parameters. Type information can be extracted from the `types` main JSON database entry.

### Types

Exemplary entry for a type looks like follows:
```
  {
    "class": "builtin",
    "hash": "B::void:0;",
    "str": "void",
    "refs": [],
    "refcount": 2673,
    "id": 0,
    "fid": 0,
    "qualifiers": "",
    "size": 0
  },
```

`class` is a family representation of types. The following class values are allowed:
- `builtin`: this is a builtin type (like `float` or `char`)
- `pointer`: this is a pointer type; it references other type it points to
- `record`: this is a structure/union/bitfield type; it might reference a number of other types of its members
- `record_forward`: this is a forward declaration of a structure/union/bitfield; it does not reference any other type (it's there just for its name)
- `const_array`: this is an array with known size; it references other type, the type of array elements
- `incomplete_array`: this is an incomplete array (without specified type); it references other type, the type of array elements
- `enum`: this is an enumerator type; it references other type, the underlying integer type for enum representation
- `enum_forward`: this is a forward declaration of an enum; it does not reference any other type (it's there just for its name)
- `decayed_pointer`: this is a pointer that was made from the array which collapsed after passing to function; in all other aspects it behaves like a pointer
- `function`: this is a function type; the first reference (which is always present) is the return type of the function, other references are the types of function arguments

`hash` is the unique identifier for a given type. This is necessary as there might be semantically the same types across many translation units. For example for the following type:

```c
struct X {
    unsigned long z;
    struct X* _X;
    struct Y* _Y;
    struct Z _Z;
};
```

the identifier would be: `R::X:256;` (assuming the size of the structure is 256 bytes). If the same types appear across many translation units, only one type entry is generated for it.

`str` is a string representation of a type. For builtin types `str` is a detailed type name (like `void` or `unsigned int`). For record or enum types its a tag name of a type. For other types it has only symbolic meaning.

`refs` represents other types which are referenced by this type (as explained at the class description section).

`refcount` is number of translation units this type appeared in.

`id` is a unique identifier of this type.

`fid` is an identifier for a file this type first appeared in (all other equivalent types in other translation units were based upon this specific translation unit appearance).

`qualifiers` are specific attributes of a type; the following values are currently allowed: `c` for `const`, `v` for `volatile` and `r` for `restrict`.

`size` is a total size of a type in bits (if size is 0 it doesn't take any storage)

Some types have additional fields with special properties. For the `record` type the fields are:
- `union`: this specifies whether a record is a union (`true` value) or structure (`false` value)
- `decls`: this specifies which references are internal type declarations for a record
- `bitfields`: this is a dictionary that maps index of a referenced type (which is a base type for a bitfield) with corresponding bitfield size

For example for a given type:
```c
struct A {
    struct B {
        int x;
    };
    float y;
    enum C {
        u
    };
    long z : 5;
};
```

the type entry for `struct A` might look as follows:
```
{
  "hash": "R::A:64;",
  "fid": 0,
  "union": false,
  "refs": 
    [ 2, 3, 5, 6 ],
  "refcount": 1,
  "id": 1,
  "str": "A",
  "qualifiers": "",
  "decls": 
    [ 0, 2 ],
  "bitfields": { "3" : 5 },
  "class": "record",
  "size": 64
},
```

From the above we might conclude that the type with id 2 and 5 are internal references of a record `A` (index 0 and 2 in the `refs` array). Moreover this record contains a bitfield with underlying type with id 6 (index 3 in the `refs` array) with size 5.

There is one additional field for the `enum` type: `values`. It contains all the integer values of a given enumeration.

Finally there is one specific field for the 'function' type: 'variadic'. This specifies whether the function type is variadic, i.e. the function type arguments end with "..." in the type declaration.
