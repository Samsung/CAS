## Dereference information

Function/Type database (DBJSON) provides a possibility to gather information regarding dereference information used in the code of a given function. It is implemented as a `derefs` entry in the function array of DBJSON. It is a list of dereference information records and has the following format:

```
"derefs": [ 
            {
                "kind":         dereference kind; this is "unary", "array", "member", "function", "assign", "init", "offsetof", "return", "parm", "cond" or "logic"
                "offset":       computed constant offset value in the dereference expression [not present for the "member" and "return" kind]
                                for the "function" kind this is an index into the 'callrefs'+'refcallrefs' concatenated array of call information for this function
                                for the "assign" kind this is a value that describes the kind of this assignment operator
                                for the "parm" kind this is an index of the function call argument this entry pertains to
                                for the "init" kind this field indicates how many elements (in case of structure type initializer list) have been actually initialized
                                for the "offsetof" kind this is the computed offset value from a given member expression (or -1 if the value cannot be computed statically)
                                for the "cond" kind this is the associated compound statement id in 'csamp'
                                for the "logic" kind this is a value that describes the kind of this operator
                "basecnt":      number of variables referenced in the base array expression (most of the times the value is 1) [only present for the "array" kind]
                                number of variables referenced on the left-hand-side expression [only present for the "logic" kind]
                "offsetrefs" :  variables referenced in the dereference offset expression
                                  for the "array" kind the first 'basecnt' elements describe the base variable of the array
                                  for the "init" kind the first element describes the variable being initialized
                                  for the "assign" kind the first element describes the variable expression being assigned to
                "expr": full dereference expression written in plain text combined with the location of the expression in the source (i.e. "[<loc>]: <expr>")
                "ord": indicates order of expressions within function (not present in the examples below)
                "csid": compound statement id where this dereference expression was taken
            // The following entries are present only for the "member" and "offsetof" kinds
                "member" : list of member references for the variable being dereferenced for "member" kind
                           list of field references (or -1 for array offset expressions) for the "offsetof" kind
                "type":  type of the corresponding member reference (through outermost cast or field type)
            // The following entries are present only for the "member" kind
                "access": member expression kind, i.e.
                            variable object access (.): 0
                            variable pointer access (->): 1
                "shift": computed constant offset value for the corresponding member reference
                "mcall": index into the 'callrefs'+'refcallrefs' concatenated array of call information for this function for corresponding function call through member reference
                         or -1 if there is no function call for a given member [this field is only present when there is at least one call for some of its members]
            },
(...)
]
```

where the `offsetrefs` single entry has the following format:
```
"offsetrefs": {
      "kind": scope of the referenced variable, i.e.:
                "global", "local", "parm", // ordinary variable id
                "integer", "float", "string", "address", // literal values
                "callref", "refcallref", "addrcallref", // function call information
                "unary", "array", "member", "assign", "function" or "logic" // variable-like expressions
              "unary", "array", "member", "assign" and "logic" kind variables are actually indexes into parent array with corresponding dereference entry
      "id":   unique id of the referenced variable;
                for "address" or "integer" type this is the integer value extracted from original expression
                for "callref", "refcallref" or "addrcallref" type this is an index into the 'callrefs'+'refcallrefs' concatenated array of call information for this function
                for "unary", "array", "member" or "assign" kinds this is an index into 'derefs' array for this function that points to the appropriate entry of this kind
                for "function" this is the function id for the function reference
      "mi":   index of the member reference in the member expression chain (or offsetof array expression) this particular variable contributes to [only present for the "member" or "offsetof" kind of the parent entry]
      "di":   index into parent array with corresponding dereference entry [only present for "refcallref" kind] or constant address used as a base for the function call [only present for the "addrcallref" kind]
      "cast": type of the cast used directly on the expression for the referenced variable
              for the first variable expression referenced for the parent "assign" entry this is the type of the variable expression
}
```

The different kind values for logic operator are:
```
{
    '<=>': 9,
    '<': 10,
    '>': 11,
    '<=': 12,
    '>=': 13,
    '==': 14,
    '!=': 15,
    '&': 16,
    '^': 17,
    '|': 18,
    '&&': 19,
    '||': 20
}
```

The different kind values for assignment operator are:
```
{
    '=': 21,
    '*=': 22,
    '/=': 23,
    '%=': 24,
    '+=': 25,
    '-=': 26,
    '<<=': 27,
    '>>=': 28,
    '&=': 29,
    '^=': 30,
    '|=': 31
}
```

Let's see how it works through extensive examples. Consider the following code:

```c
struct A;
struct B;
struct C;

struct B* getB(char c, float f) {
  return 0;
}

typedef struct B* (*pfun_t)(char c, float f);
typedef int (*pfi_t)(void);
typedef void* (*pfv_t)(void);

struct A {
  int i;
  void* p;
  struct B* pB;
  pfun_t pF;
};

struct B {
  int i;
  char T[10];
  void* p;
  struct A a;
  struct A Ta[4][4];
  struct C* pC;
};

struct C {
  float f;
  void* p;
  unsigned long* pul;
  struct B b;
  struct A* pA;
  union {
    void *arg;
    int* B;
  };
  union {
    void *arg;
    int* B;
  } N;
};

struct A gA;
unsigned long gi;

int getN(void) {
  return 0;
}

void* getV(void) {
  return 0;
}

struct B* (*pfun)(char c, float f);
int (*pfi)(void);
void* (*pfv)(void);

void f(int* px, char b) {

  int i = 2;
  char T[10] = {};
  int** ppx = &px;
  struct A oA;
  struct B* pB = 0;
  struct B** ppB = &pB;
  void* q = pB;
  void** pq = &q;
  pfun = getB;
  pfi = getN;
  pfv = getV;
  pfun_t F[2] = { pfun, pfun };

  (void) getB('s',6.);

    (void) *px;                                                                // (1)
    (void) *(px+3*2);                                                          // (2)
    (void) *((4+1)+3-(1+2)+px);                                                // (3)
    (void) *(px+2+2*gi+b-i);                                                   // (4)
    (void) *(px+((void*)&q-(void*)pB));                                        // (5a)
    (void) *(px+((void*)400-(void*)300)+100);                                  // (5b)
    (void) *((int*)400);                                                       // (6)
    (void) *((int*)0+getN()+pB->i);                                            // (7)
    (void) *(px+({ do {} while(0); 4;}));                                      // (8)
    (void) *(px+({ do {} while(0); 4+gi*getN()-pB->i;}));                      // (9)
    (void) **ppx;                                                              // (10)
    (void) *(*ppx+4);                                                          // (11)
    (void) T[4];                                                               // (12)
    (void) ({do {} while(0); (struct A*)0+gi;})[4+i];                          // (13)
    (void) 4[T];                                                               // (14)
    (void) T[+(4+1)+3+(1+2)];                                                  // (15)
    (void) T[-3];                                                              // (16)
    (void) T[gi+2+1];                                                          // (17)
    (void) T[getN()+pB->i];                                                    // (18)
    (void) ( *(*ppx+4+T[2]-pB->i+(2*3&0xFF)-1*0)+((pB->i)) );                  // (19)
    (void) T[getN()+pB->i*({ do {} while(0); 4+gi*getN()-pB->i;})];            // (20)
    (void) oA.p;                                                               // (21)
    (void) (&oA)->i;                                                           // (22)
    (void) ((struct B*)q)->i;                                                  // (23)
    (void) (pB->pC+4+gi)->f;                                                   // (24)
    (void) ((struct C*)((pB+4+gi)->p)+gi+2)->f;                                // (25)
    (void) ((struct C*)((4+2+pB)->p)+gi+2)->f;                                 // (26)
    (void) ((struct C*)(((struct B*)(12+4+16))->p)+gi+2)->f;                   // (27)
    (void) ((struct A*)((struct C*)((struct A*)pB->p)->p)->p+2+gi)->i;         // (28)
    (void) ((struct A*)oA.pB->pC->pA->pB->pC->pA->pB->pC->p)->i;               // (29)
    (void) *((int*)oA.p);                                                      // (30)
    (void) *(pB->pC->pul);                                                     // (31)
    (void) *( (*(pB->pC)).arg );                                               // (32)
    (void) *( (*(pB->pC)).B );                                                 // (33)
    (void) (*(pB->pC)).N.arg;                                                  // (34)
    (void) (*(pB->pC)).N.B;                                                    // (35)
    (void) pB->T[4];                                                           // (36)
    (void) pB->T[4+(2+gi)];                                                    // (37)
    (void) *(pB->pC->pul+2+(3+1)+((( 
      ((struct B*)((int*)(&oA)+sizeof(int)+sizeof(void*)))->T[4] ))));         // (38)
    (void) T[i+1+2+*px-
      ((struct A*)(void*)(struct A*)(((struct B*)(pB->pC->p))->p))->i];        // (39)
    (void) getB('x',3.0)->a.i;                                                 // (40)
    (void) (*pfun)('x',3.0)->a.i;                                              // (41)
    (void) (*getB)('x',3.0)->a.i;                                              // (42)
    (void) pfun('x',3.0)->a.i;                                                 // (43)
    (void) (*pfun)('x',3.0);                                                   // (44)
    (void) pfun('x',3.0);                                                      // (45)
    (void) (*getB)('s',5.);                                                    // (46)
    (void) (*({do {} while(0); (struct A*)0+gi; pfun;}))('x',3.0)->a.i;        // (47)
    (void) (*({do {} while(0); (struct A*)0+gi; getB;}))('x',3.0)->a.i;        // (48)
    (void) (*({do {} while(0); (struct A*)0+gi; pfun;}))('x',3.0);             // (49)
    (void) (*({do {} while(0); (struct A*)0+gi; getB;}))('x',3.0);             // (50)
    (void) (*F[1])('x',3.0)->a.i;                                              // (51)
    (void) (*F[1])('x',3.0);                                                   // (52)
    (void) F[1]('x',3.0)->a.i;                                                 // (53)
    (void) F[1]('x',3.0);                                                      // (54)
    (void) ((pfun_t)(3333+1))('x',3.0)->a.i;                                   // (55)
    (void) ((pfun_t)(3333+1))('x',3.0);                                        // (56)
    (void) oA.pF('x',3.0);                                                     // (57)
    (void) ((struct A*)((struct C*)oA.p)->p)->pF('@',1.5);                     // (58)
    (void) oA.pF('x',3.0)->i;                                                  // (59)
    (void) ((struct A*)oA.pF('x',3.0)->p)->pF('u',999.1)->i;                   // (60)
    (void) ((pfun_t)oA.pB->p)('x',16.5)->i;                                    // (61)
    (void) (0 ? (struct B *)0 : (pB))->p;                                      // (62)
    (void) ((1+313) ? (struct B *)0 : (pB))->p;                                // (63)
    (void) (*px ? (struct B *)0 : (pB))->p;                                    // (64)
    (void) ((struct B*)0 ? : (pB))->p;                                         // (65)
    (void) ((struct B*)(1+313) ? : (pB))->p;                                   // (66)
    (void) ((struct B*)*px ? : (pB))->p;                                       // (67)
    (void) ((struct B *)30)->a;                                                // (68)
    (void) ((struct A){.i=3,.pB=oA.pB+gi}).pB->a;                              // (69a)
    (void) ((struct A){.i=(long)(3+1),.pB=(void*)oA.pB+(short)gi}).pB->a;      // (69b)
    (void) ({do {} while(0); (struct A*)0+gi;})->i;                            // (70)
    (void) (((&((&oA)->pB+4)->a)+gi+pB->i)->pB->pC+10*T[9])->f;                // (71)
    (void) (&((&oA)->pB+({do {} while(0); (int)10+gi; }))->a)->pB->p;          // (72)
    (void) ({ ((void)(sizeof ((long)(0 && getN())))); getB(0,0); })->p;        // (73)
    (void) (*((struct B**)q))->i;                                              // (74)
    (void) *((unsigned char*)px + (long)gi - (unsigned long)2 +
      (signed int)pB->i + (int)T[4] + ((unsigned)*px+1) + (long long)getV());  // (75)
    (void) ((struct A*)(oA.pB) ? : ((struct A*)pB))->p;                        // (76)
    (void) ((struct A*)(oA.pB) ? (&oA) : ((struct A*)pB))->p;                  // (77)
    (void) ( *( ((struct A*)(oA.pB) ? (&oA) : ((struct A*)pB)) ) ).i;          // (78)
    (void) ((struct A*)(oA.pB) ? 
      (&oA) : (i?(((struct A*)pB)):((struct A*)pfv())))->p;                    // (79)
    (void) ( (struct A*)
      (((struct A*)((struct C*)((struct A*)pB->p)->p)->p+2+gi)->i ?
      (struct B *)0 : (unsigned long)(*(px+((void*)&q-(void*)pB)) +
        (int)T[getN()+(long)pB->i]) ? (pB) : ((void*)ppB)))->p;                // (80)
    int vi0 = 0;                                                               // (81)
    int vi1 = (int)1.0;                                                        // (82)
    unsigned vu0 = 2;                                                          // (83)
    void* vq0 = q;                                                             // (84)
    void* vq1 = (struct A*)pB;                                                 // (85)
    void* vq2 = pB;                                                            // (86)
    void* vq3 = getB('a',3.);                                                  // (87)
    int vi2 = (*getN)();                                                       // (88)
    unsigned long vul0 = (long)pfi();                                          // (89)
    unsigned long vul1 = (*pfi)();                                             // (90)
    int* vpi0 = "ABRAKADABRA";                                                 // (91)
    unsigned vu1 = i+*((int*)pB->p)-(long)oA.i;                                // (92)
    struct A vA0 = ((struct A){.i=(long)3,.pB=oA.pB->pC+gi});                  // (93)
    double vd0; vd0 = 4UL;                                                     // (94)
    pB->pC->p = pB;                                                            // (95)
    long vl0 = 0xff; vl0&=0x3f;                                                // (96)
    unsigned long vul3 = (long)*((int*)pB->p)-(short)T[2]+(int)(gi+=3);        // (97)
    unsigned long ul = 4+__builtin_offsetof(struct C,b.Ta[4][oA.i].p);         // (98)
    if(pfi) pfi();                                                             // (99)
    while(i<10) i++;                                                           // (100)
}
```
The first kind of dereference is ordinary pointer dereference (through so called unary expression). 

In the (1) case we have mere indirection with offset 0. We are referencing a local variable (function parameter in this case) with id=0 in the locals entry of the given function.

The underlying JSON entry might have the following format:
```json
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "parm",
      "id" : 0
    }   
  ],
  "expr" : "*px"
}
```

In the (2) case the indirection offset is computed to 6.
The offset expression can be more complicated as in (3). The `offset` field will be populated with the sum of the offset expressions for which it is possible to compute the value at compile time (i.e. integer constant expression). In the (3) case we are able to compute the offset value 5.
Offset expression might have many variables as in (4). In that case references to all variables can be found in `offsetrefs` list:
```json
{
  "kind" : "unary",
  "offset" : 2,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 2
    },
    {
      "kind" : "parm",
      "id" : 1
    },
    {
      "kind" : "global",
      "id" : 1
    },
    {
      "kind" : "parm",
      "id" : 0
    }   
  ],
  "expr" : "*(px + 2 + 2 * gi + b - i)"
}
```
Here we are referencing some global variable, automatic (local) variable created inside function and two function parameters. 

Please note that in general it is difficult to resolve which variable is the actual pointer variable in offset expression. In most cases it should be a variable of pointer type. There might be cases however (as in (5a)) where multiple pointer variables are involved:
```json
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "parm",
      "id" : 0
    },
    {
      "kind" : "local",
      "cast" : 3,
      "id" : 8
    },
    {
      "kind" : "local",
      "cast" : 3,
      "id" : 6
    }
  ],
  "expr" : "*(px + ((void *)&q - (void *)pB))"
}
```
Here we cannot know (without inspecting the dereference expression) whether the base pointer variable is `px` or `pB`. Both expressions that involve variables `q` and `pB` are casted to `void*` hence the value 3 in the `cast` field.

Whenever the value of the expression used as a part of the sum that constitute the dereference address can be precomputed (as in (5b)) it becomes a part of the offset:
```json
{
  "kind" : "unary",
  "offset" : 200,
  "offsetrefs" : [
    {
      "kind" : "parm",
      "id" : 0
    }
  ],
  "expr" : "*(px + ((void *)400 - (void *)300) + 100)"
}
```
No cast information is available at this point.

In some low-level routines it is useful to use pointer dereference on direct address value like in (6). It is implemented as an `address` variable kind:
```json
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "address",
      "cast" : 12,
      "id" : 400
    }
  ],
  "expr" : "*((int *)400)"
}
```
The cast information for the pointer type for direct address value is available as well.

It is also possible to use a function call or a member expression inside the dereference offset expression as in (7).
```json
{
  "member" : [ 0 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }   
  ],
  "expr" : "pB->i"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "address",
      "cast" : 12,
      "id" : 0
    },
    {
      "kind" : "callref",
      "id" : 0
    },
    {
      "kind" : "member",
      "id" : 0
    }
  ],
  "expr" : "*((int *)0 + getN() + pB->i)"
}

```
Here we have two additional types of variables inside the `offsetrefs` list. For function call we have `callref` kind which points to the `calls` array of the JSON information for this function. For member expression we have `member` kind which points to the appropriate `member` kind entry in the dereference information list (more on that later).

Original GNU extension to the C language allows to use very peculiar form of expression, called statement expression. It defines a series of statements embedded into `({})` and returns the value of the last expression. This is very widely used in the Linux kernel source, especially in safe macro handling, i.e. evaluating its macro operands only once. When statement expression is used inside the dereference offset expression all variables used in the value yielding expression are reported through the `offsetrefs` list. For example for (8) we have:
```json
{
  "kind" : "unary",
  "offset" : 4,
  "offsetrefs" : [
    {
      "kind" : "parm",
      "id" : 0
    }   
  ],
  "expr" : "*(px + ({\n    do {\n    } while (0);\n    4;\n}))"
}
```

For (9) it could be:
```json
{
  "member" : [ 0 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }   
  ],
  "expr" : "pB->i"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "global",
      "id" : 1
    },
    {
      "kind" : "callref",
      "id" : 0
    },
    {
      "kind" : "member",
      "id" : 0
    },
    {
      "kind" : "parm",
      "id" : 0
    }   
  ],
  "expr" : "*(px + ({\n    do {\n    } while (0);\n    4 + gi * getN() - pB->i;\n}))"
}
```

For nested pointer dereference (10) there is a `unary` kind for the referenced variable which points to the relevant pointer dereference information for this function:
```json
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 4
    }   
  ],
  "expr" : "*ppx"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "unary",
      "id" : 0
    }   
  ],
  "expr" : "**ppx"
}
```

Constant expressions in offset expression (11) will populate the `offset` field as described in previous examples:
```json
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 4
    }   
  ],
  "expr" : "*ppx"
},
{
  "kind" : "unary",
  "offset" : 4,
  "offsetrefs" : [
    {
      "kind" : "unary",
      "id" : 0
    }   
  ],
  "expr" : "*(*ppx + 4)"
}
```

The second kind of dereference is an array access (through so called array subscript expression). This is handled in similar fashion to ordinary pointer dereference with tiny modifications. The variable that describes the array itself is placed at the beginning of `offsetrefs` list as in (12):
```json
{
  "kind" : "array",
  "offset" : 4,
  "basecnt" : 1,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 3
    }   
  ],
  "expr" : "T[4]"
}
```

More variables can constitute the array (which can be found in statement exression) as in (13):
```json
{
  "kind" : "array",
  "offset" : 4,
  "basecnt" : 2,
  "offsetrefs" : [
    {
      "kind" : "global",
      "id" : 1
    },
    {
      "kind" : "address",
      "cast" : 11,
      "id" : 0
    },
    {
      "kind" : "local",
      "id" : 2
    }
  ],
  "expr" : "({\n    do {\n    } while (0);\n    (struct A *)0 + gi;\n})[4 + i]"
}

```
The `basecnt` variable in the main dereference entry suggests how many variables from the `offsetrefs` list constitute the array (which are placed at the beginning of the list).

Weird and obscure expressions for array usage (14) are handled properly as well as constant expression computation (15),(16):
```json
{
  "kind" : "array",
  "offset" : 11,
  "basecnt" : 1,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 3
    }   
  ],
  "expr" : "T[+(4 + 1) + 3 + (1 + 2)]"
}
{
  "kind" : "array",
  "offset" : -3,
  "basecnt" : 1,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 3
    }   
  ],
  "expr" : "T[-3]"
}
```
For (17) it could be:
```json
{
  "kind" : "array",
  "offset" : 3,
  "basecnt" : 1,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 3
    },
    {
      "kind" : "global",
      "id" : 1
    }   
  ],
  "expr" : "T[gi + 2 + 1]"
}
```

For (18) it could be:
```json
{
  "member" : [ 0 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }   
  ],
  "expr" : "pB->i"
},
{
  "kind" : "array",
  "offset" : 0,
  "basecnt" : 1,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 3
    },
    {
      "kind" : "member",
      "id" : 0
    },
    {
      "kind" : "callref",
      "id" : 0
    }   
  ],
  "expr" : "T[getN() + pB->i]"
}
```

To save space whenever some dereference expression is encountered and at the same time the same expression (semantically) was already processed it is not added to the derefernce list unless it is referenced by some other dereference entry. For example for (19):
```json
{
  "member" : [ 0 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }   
  ],
  "expr" : "pB->i"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 4
    }   
  ],
  "expr" : "*ppx"
},
{
  "kind" : "unary",
  "offset" : 10,
  "offsetrefs" : [
    {
      "kind" : "member",
      "id" : 0
    },
    {
      "kind" : "array",
      "id" : 4
    },
    {
      "kind" : "unary",
      "id" : 2
    }   
  ],
  "expr" : "*(*ppx + 4 + T[2] - pB->i + (2 * 3 & 255) - 1 * 0)"
},
{
  "kind" : "array",
  "offset" : 2,
  "basecnt" : 1,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 3
    }   
  ],
  "expr" : "T[2]"
}
```
The use of the `((pB->i))` expression is semantically equivalent to the `pB->i` encountered earlier in the (19) therefore it is not added to the dereference entries. However in (20):
```json
{
  "member" : [ 0 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }   
  ],
  "expr" : "pB->i"
},
{
  "member" : [ 0 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }   
  ],
  "expr" : "pB->i"
},
{
  "kind" : "array",
  "offset" : 0,
  "basecnt" : 1,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 3
    },
    {
      "kind" : "global",
      "id" : 1
    },
    {
      "kind" : "callref",
      "id" : 0
    },
    {
      "kind" : "member",
      "id" : 0
    },
    {
      "kind" : "member",
      "id" : 1
    },
    {
      "kind" : "callref",
      "id" : 1
    }   
  ],
  "expr" : "T[getN() + pB->i * ({\n    do {\n    } while (0);\n    4 + gi * getN() - pB->i;\n})]"
}
```
The second use of the `pB->i` expression is added to the dereference entries (even though it is semantically equivalent to its first encounter) because it is referenced by the main array subscript expression for `T`.

The third kind of dereference is a member expression. In other words it is getting the value of a structure member (possibly dereferencing memory when `->` operator is involved). To keep the information in order the object like expression (using `.` operator) is reported as well. The simplest form of member expression is shown in (21) which translates to:
```json
{
  "member" : [ 1 ],
  "type" : [ 0 ],
  "access" : [ 0 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 5,
      "mi" : 0
    }   
  ],
  "expr" : "oA.p"
}
```
Here we are using the member `p` with index 1 in the `struct A` structure (`member` entry list). The type id of the `struct A` structure is 0 which is stored in the `type` entry list. We are using object access (`.`) which translates to 0 inside the `access` entry list. The member is accessed directly without any memory offset (hence the 0 in the `shift` entry list). The base for the member expression is a local variable with id 5 (`oA`).

In another example (22) we have slight modification to use the pointer access (`->`). The type for the base structure changes as well as the `access` specifier.

```json
{
  "member" : [ 0 ],
  "type" : [ 11 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 5,
      "mi" : 0
    }   
  ],
  "expr" : "(&oA)->i"
}
```

Sometimes there is cast required to properly designate the type of member expression. For example in (23) the `q` variable has type `void*`. In order to make a member expression we need to cast it to proper type beforehand. The ultimate type will be placed in `type` entry list as below:
```json
{
  "member" : [ 0 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "cast" : 16,
      "id" : 8,
      "mi" : 0
    }
  ],
  "expr" : "((struct B *)q)->i"
}
```

We can also apply some offset value to the pointer in the structure member (24):
```json
{
  "member" : [ 4,0 ],
  "type" : [ 16,15 ],
  "access" : [ 1,1 ],
  "shift" : [ 0,4 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "global",
      "id" : 1,
      "mi" : 1
    },
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }   
  ],
  "expr" : "(pB->pC + 4 + gi)->f"
}
```

In the second member expression in the chain (`pC + 4 + gi`) we have constant offset 4 which is placed in the `shift` entry list at index 1. Furthermore global variable `gi` is also used in the offset expression. We can find it by looking at the `offsetrefs` array and finding variables with `mi` value 1. All these variables takes part in the offset computation for the second member expression in the chain (one global variable in our case).

More examples can be found in (25-29):

```json
{
  "member" : [ 2,0 ],
  "type" : [ 16,15 ],
  "access" : [ 1,1 ],
  "shift" : [ 4,2 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "global",
      "id" : 1,
      "mi" : 1
    },
    {
      "kind" : "global",
      "id" : 1,
      "mi" : 0
    },
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }   
  ],
  "expr" : "((struct C *)((pB + 4 + gi)->p) + gi + 2)->f"
}
```
All member expressions are done through the pointer (`->`) hence access values are set to 1. In the first member expression in the chain we have offset 4 (index 0 in `shift` entry list) and two variables with `mi` set to 0. The same global variable (`mi` set to 1) is used in the second member expression in the chain with additional offset 2 (index 1 in `shift` entry list).

```json
{
  "member" : [ 2,0 ],
  "type" : [ 16,15 ],
  "access" : [ 1,1 ],
  "shift" : [ 6,2 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "global",
      "id" : 1,
      "mi" : 1
    },
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }   
  ],
  "expr" : "((struct C *)((4 + 2 + pB)->p) + gi + 2)->f"
}
```
Similar as above two offset values are computed and placed in the `shift` entry list. The type for first member expression in the chain is set to `struct B*` as original variable `pB` designates (first `type` entry set to 16), the second member expression type is `struct C*` that originates from the cast used (second `type` entry set to 15).


```json
{
  "member" : [ 2,0 ],
  "type" : [ 16,15 ],
  "access" : [ 1,1 ],
  "shift" : [ 0,2 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "global",
      "id" : 1,
      "mi" : 1
    },
    {
      "kind" : "address",
      "cast" : 16,
      "id" : 32,
      "mi" : 0
    }
  ],
  "expr" : "((struct C *)(((struct B *)(12 + 4 + 16))->p) + gi + 2)->f"
}
```
Here the first member expression is based on the integer values hence the `address` kind for `offsetrefs` variable with `mi` set to 0.


```json
{
  "member" : [ 2,1,1,0 ],
  "type" : [ 16,11,15,11 ],
  "access" : [ 1,1,1,1 ],
  "shift" : [ 0,0,0,2 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "global",
      "id" : 1,
      "mi" : 3
    },
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }   
  ],
  "expr" : "((struct A *)((struct C *)((struct A *)pB->p)->p)->p + 2 + gi)->i"
}
```
Here we have global variable (and integer value) used in the offset for the last member expression in the chain (`mi` set to 3, last `shift` value set to 2). Local variable in `offsetrefs` with `mi` set to 0 designates the original varialbe `pB` which is the base for the member expression.


```json
{
  "member" : [ 2,4,4,2,4,4,2,4,1,0 ],
  "type" : [ 0,16,15,11,16,15,11,16,15,11 ],
  "access" : [ 0,1,1,1,1,1,1,1,1,1 ],
  "shift" : [ 0,0,0,0,0,0,0,0,0,0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 5,
      "mi" : 0
    }   
  ],
  "expr" : "((struct A *)oA.pB->pC->pA->pB->pC->pA->pB->pC->p)->i"
}
```
In this case only the first member expression in the chain is not done through the pointer (hence the 0 value in the `access` entry list).

When the member expression is used inside the unary dereference it is referred through the `member` kind of `offsetrefs` variable as in (30) and (31):
```json
{
  "member" : [ 1 ],
  "type" : [ 0 ],
  "access" : [ 0 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 5,
      "mi" : 0
    }   
  ],
  "expr" : "oA.p"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "member",
      "cast" : 12,
      "id" : 0
    }   
  ],
  "expr" : "*((int *)oA.p)"
}
```
```json
{
  "member" : [ 4,2 ],
  "type" : [ 16,15 ],
  "access" : [ 1,1 ],
  "shift" : [ 0,0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }   
  ],
  "expr" : "pB->pC->pul"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "member",
      "id" : 0
    }   
  ],
  "expr" : "*(pB->pC->pul)"
}
```
In both the cases above the referred member expression is first in the dereference list for this function hence the id (which represents index in the main dereference list) set to 0.

When member expression refers to a member of an union (as in (32) and (33)) there are two entries in the `member` entry list (and all remaining relevant entries as well):
```json
{
  "member" : [ 4 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }   
  ],
  "expr" : "pB->pC"
},
{
  "member" : [ 5,0 ],
  "type" : [ 7,13 ],
  "access" : [ 0,0 ],
  "shift" : [ 0,0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "unary",
      "id" : 2,
      "mi" : 0
    }   
  ],
  "expr" : "(*(pB->pC)).arg"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "member",
      "id" : 0
    }   
  ],
  "expr" : "*(pB->pC)"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "member",
      "id" : 1
    }   
  ],
  "expr" : "*((*(pB->pC)).arg)"
}
```
First entry points to the type definition for the union in the enclosing type (index 5 in the type id 7 which is `struct C`). Second entry points to the actual member of the union (index 0 in the type id 13 which is the anonymous union definition).


```json
{
  "member" : [ 4 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }   
  ],
  "expr" : "pB->pC"
},
{
  "member" : [ 5,1 ],
  "type" : [ 7,13 ],
  "access" : [ 0,0 ],
  "shift" : [ 0,0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "unary",
      "id" : 2,
      "mi" : 0
    }   
  ],
  "expr" : "(*(pB->pC)).B"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "member",
      "id" : 0
    }   
  ],
  "expr" : "*(pB->pC)"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "member",
      "id" : 1
    }   
  ],
  "expr" : "*((*(pB->pC)).B)"
}
```
In similar fashion second member of the union is accessed here.

Even though the members of the anonymous union are accessed as internal parts of the anonymous type itself they still takes part in the index computation for the members of the enclosig type. For example in (34) and (35):

```json
{
  "member" : [ 4 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }   
  ],
  "expr" : "pB->pC"
},
{
  "member" : [ 7,0 ],
  "type" : [ 7,14 ],
  "access" : [ 0,0 ],
  "shift" : [ 0,0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "unary",
      "id" : 2,
      "mi" : 0
    }   
  ],
  "expr" : "(*(pB->pC)).N.arg"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "member",
      "id" : 0
    }   
  ],
  "expr" : "*(pB->pC)"
}
```

The index value for the member `N` of the `struct C` type is 7 (the union members leak into the enclosing parent type). Similar situation happens below:
```json
{
  "member" : [ 4 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }   
  ],
  "expr" : "pB->pC"
},
{
  "member" : [ 7,1 ],
  "type" : [ 7,14 ],
  "access" : [ 0,0 ],
  "shift" : [ 0,0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "unary",
      "id" : 2,
      "mi" : 0
    }   
  ],
  "expr" : "(*(pB->pC)).N.B"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "member",
      "id" : 0
    }   
  ],
  "expr" : "*(pB->pC)"
}
```

The member expression can resolve to array as in (36-39). In such cases the member expression is referred in the enclosing array dereference entry.

```json
{
  "member" : [ 1 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }   
  ],
  "expr" : "pB->T"
},
{
  "kind" : "array",
  "offset" : 4,
  "basecnt" : 1,
  "offsetrefs" : [
    {
      "kind" : "member",
      "id" : 0
    }   
  ],
  "expr" : "pB->T[4]"
}
```
Here the array entry with offset 4 refers to underlying member expression in its first `offsetrefs` entry.

```json
{
  "member" : [ 1 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }   
  ],
  "expr" : "pB->T"
},
{
  "kind" : "array",
  "offset" : 4,
  "basecnt" : 1,
  "offsetrefs" : [
    {
      "kind" : "member",
      "id" : 0
    },
    {
      "kind" : "global",
      "id" : 1
    }   
  ],
  "expr" : "pB->T[4 + (2 + gi)]"
}
```
Here'a a bit more complicated expression with computed constant offset 6 and additional global variable referenced therein. The base variable for the array is also a reference to a member expression.

```json
{
  "member" : [ 1 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 12 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 5,
      "mi" : 0
    }   
  ],
  "expr" : "((struct B *)((int *)(&oA) + sizeof(int) + sizeof(void *)))->T"
},
{
  "member" : [ 4,2 ],
  "type" : [ 16,15 ],
  "access" : [ 1,1 ],
  "shift" : [ 0,0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }   
  ],
  "expr" : "pB->pC->pul"
},
{
  "kind" : "unary",
  "offset" : 6,
  "offsetrefs" : [
    {
      "kind" : "member",
      "id" : 1
    },
    {
      "kind" : "array",
      "id" : 3
    }   
  ],
  "expr" : "*(pB->pC->pul + 2 + (3 + 1) + (((((struct B *)((int *)(&oA) + sizeof(int) + sizeof(void *)))->T[4]))))"
},
{
  "kind" : "array",
  "offset" : 4,
  "basecnt" : 1,
  "offsetrefs" : [
    {
      "kind" : "member",
      "id" : 0
    }   
  ],
  "expr" : "((struct B *)((int *)(&oA) + sizeof(int) + sizeof(void *)))->T[4]"
}
```
In this example the array dereference is a part of much more complicated unary dereference. The base member expression contains computed offset (`shift` entry list) using the `sizeof` operator.

```json
{
  "member" : [ 4,1,2,0 ],
  "type" : [ 16,15,16,11 ],
  "access" : [ 1,1,1,1 ],
  "shift" : [ 0,0,0,0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }   
  ],
  "expr" : "((struct A *)(void *)(struct A *)(((struct B *)(pB->pC->p))->p))->i"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "parm",
      "id" : 0
    }   
  ],
  "expr" : "*px"
},
{
  "kind" : "array",
  "offset" : 3,
  "basecnt" : 1,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 3
    },
    {
      "kind" : "local",
      "id" : 2
    },
    {
      "kind" : "unary",
      "id" : 1
    },
    {
      "kind" : "member",
      "id" : 0
    }   
  ],
  "expr" : "T[i + 1 + 2 + *px - ((struct A *)(void *)(struct A *)(((struct B *)(pB->pC->p))->p))->i]"
}
```
When several casts are used in the base for the member expression the outermost cast is used to extract the base type for the member expression.

The base type for the member expression can also originate from a function call as in (40):
```json
{
  "member" : [ 3,0 ],
  "type" : [ 16,0 ],
  "access" : [ 1,0 ],
  "shift" : [ 0,0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "callref",
      "id" : 0,
      "mi" : 0
    }   
  ],
  "expr" : "getB('x', 3.)->a.i"
}
```
In this case in the `offsetrefs` list we will have a `callref` kind variable. The `id` field will point to the appropriate entry in the `callrefs`+`refcallrefs` concatenated array of call information for this function which describes relevant function call information.

It is a little bit different when at the base of offset expression is function call through pointer as in (41):
```json
{
  "kind" : "function",
  "offset" : 1,
  "offsetrefs" : [
    {
      "kind" : "unary",
      "id" : 2
    }   
  ],
  "expr" : "(*pfun)('x', 3.)"
},
{
  "member" : [ 3,0 ],
  "type" : [ 16,0 ],
  "access" : [ 1,0 ],
  "shift" : [ 0,0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "refcallref",
      "id" : 1,
      "mi" : 0,
      "di" : 2
    }   
  ],
  "expr" : "(*pfun)('x', 3.)->a.i"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "global",
      "id" : 2
    }   
  ],
  "expr" : "*pfun"
}
```

In this case in the `offsetrefs` list we will have a `refcallref` kind variable. The `id` field will point to the appropriate entry in the `callrefs`+`refcallrefs` concatenated array of call information for this function which describes relevant function call information. As `refcallrefs` array doesn't have information about the pointer to function variable used to make the actual call the `di` entry for the `refcallref` kind variable in the `offsetrefs` list will point to the appropriate dereference entry that clearly indicates the variable used.
It is worth notice that pointer through function call is a separate kind of dereference apart form mere indirection for a plain variable which has its own entry in the deference information array as the `function` kind.

Strangely enough it is also possible to call normal function using the `*` operator as in (42):
```json
{
  "kind" : "function",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "unary",
      "id" : 2
    }   
  ],
  "expr" : "(*getB)('x', 3.)"
},
{
  "member" : [ 3,0 ],
  "type" : [ 16,0 ],
  "access" : [ 1,0 ],
  "shift" : [ 0,0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "callref",
      "id" : 0,
      "mi" : 0
    }   
  ],
  "expr" : "(*getB)('x', 3.)->a.i"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [    
  ],
  "expr" : "*getB"
}
```
In this case the call is detected as a normal call and appropriate `callref` kind entry is added to `offsetrefs`.

C language also allows to make a function call without the `*` operator as in (43):
```json
{
  "kind" : "function",
  "offset" : 1,
  "offsetrefs" : [
    {
      "kind" : "global",
      "id" : 2
    }   
  ],
  "expr" : "pfun('x', 3.)"
},
{
  "member" : [ 3,0 ],
  "type" : [ 16,0 ],
  "access" : [ 1,0 ],
  "shift" : [ 0,0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "refcallref",
      "id" : 1,
      "mi" : 0,
      "di" : 0
    }   
  ],
  "expr" : "pfun('x', 3.)->a.i"
}
```
This is where the additional `function` kind dereference information starts to play a role. The `di` entry in the `offsetrefs` list will point to this record which is the only way to indicate the underlying pointer to function variable used to make the actual call.

  When function call through pointer is not a base of member expression but just normal call in the source, the `function` kind dereference information is the place to look for details about the actual call as in (44) and (45):

```json
{
  "kind" : "function",
  "offset" : 1,
  "offsetrefs" : [
    {
      "kind" : "unary",
      "id" : 1
    }   
  ],
  "expr" : "(*pfun)('x', 3.)"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "global",
      "id" : 2
    }   
  ],
  "expr" : "*pfun"
}
```
```json
{
  "kind" : "function",
  "offset" : 1,
  "offsetrefs" : [
    {
      "kind" : "global",
      "id" : 2
    }   
  ],
  "expr" : "pfun('x', 3.)"
}
```
It will point to the variable used to make the call through the `offsetrefs` list. Furthermore the `offset` field in the `function` kind dereference entry will point to the appropriate entry in the `callrefs`+`refcallrefs` concatenated array of call information for this function which describes relevant function call information (which was also placed in the `di` entry of member expression references which is now absent).

Additional function dereference on function name is handled as well (46):
```json
{
  "kind" : "function",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "unary",
      "id" : 1
    }   
  ],
  "expr" : "(*getB)('s', 5.)"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [    
  ],
  "expr" : "*getB"
}
```

As if C was not simple enough, function call dereference for all the above cases can be also done through statement expression (47-50):
```json
{
  "kind" : "function",
  "offset" : 1,
  "offsetrefs" : [
    {
      "kind" : "unary",
      "id" : 2
    }   
  ],
  "expr" : "(*({\n    do {\n    } while (0);\n    (struct A *)0 + gi;\n    pfun;\n}))('x', 3.)"
},
{
  "member" : [ 3,0 ],
  "type" : [ 16,0 ],
  "access" : [ 1,0 ],
  "shift" : [ 0,0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "refcallref",
      "id" : 1,
      "mi" : 0,
      "di" : 2
    }   
  ],
  "expr" : "(*({\n    do {\n    } while (0);\n    (struct A *)0 + gi;\n    pfun;\n}))('x', 3.)->a.i"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "global",
      "id" : 2
    }   
  ],
  "expr" : "*({\n    do {\n    } while (0);\n    (struct A *)0 + gi;\n    pfun;\n})"
}
```
```json
{
  "kind" : "function",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "unary",
      "id" : 2
    }   
  ],
  "expr" : "(*({\n    do {\n    } while (0);\n    (struct A *)0 + gi;\n    getB;\n}))('x', 3.)"
},
{
  "member" : [ 3,0 ],
  "type" : [ 16,0 ],
  "access" : [ 1,0 ],
  "shift" : [ 0,0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "callref",
      "id" : 0,
      "mi" : 0
    }   
  ],
  "expr" : "(*({\n    do {\n    } while (0);\n    (struct A *)0 + gi;\n    getB;\n}))('x', 3.)->a.i"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [    
  ],
  "expr" : "*({\n    do {\n    } while (0);\n    (struct A *)0 + gi;\n    getB;\n})"
}
```
```json
{
  "kind" : "function",
  "offset" : 1,
  "offsetrefs" : [
    {
      "kind" : "unary",
      "id" : 1
    }   
  ],
  "expr" : "(*({\n    do {\n    } while (0);\n    (struct A *)0 + gi;\n    pfun;\n}))('x', 3.)"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "global",
      "id" : 2
    }   
  ],
  "expr" : "*({\n    do {\n    } while (0);\n    (struct A *)0 + gi;\n    pfun;\n})"
}
```
```json
{
  "kind" : "function",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "unary",
      "id" : 1
    }   
  ],
  "expr" : "(*({\n    do {\n    } while (0);\n    (struct A *)0 + gi;\n    getB;\n}))('x', 3.)"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [    
  ],
  "expr" : "*({\n    do {\n    } while (0);\n    (struct A *)0 + gi;\n    getB;\n})"
}
```

Function call can be done using function pointer taken from array (51-52):
```json
{
  "kind" : "function",
  "offset" : 1,
  "offsetrefs" : [
    {
      "kind" : "unary",
      "id" : 2
    }   
  ],
  "expr" : "(*F[1])('x', 3.)"
},
{
  "member" : [ 3,0 ],
  "type" : [ 16,0 ],
  "access" : [ 1,0 ],
  "shift" : [ 0,0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "refcallref",
      "id" : 1,
      "mi" : 0,
      "di" : 2
    }   
  ],
  "expr" : "(*F[1])('x', 3.)->a.i"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "array",
      "id" : 3
    }   
  ],
  "expr" : "*F[1]"
},
{
  "kind" : "array",
  "offset" : 1,
  "basecnt" : 1,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 8
    }   
  ],
  "expr" : "F[1]"
}
```
```json
{
  "kind" : "function",
  "offset" : 1,
  "offsetrefs" : [
    {
      "kind" : "unary",
      "id" : 1
    }   
  ],
  "expr" : "(*F[1])('x', 3.)"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "array",
      "id" : 2
    }   
  ],
  "expr" : "*F[1]"
},
{
  "kind" : "array",
  "offset" : 1,
  "basecnt" : 1,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 8
    }   
  ],
  "expr" : "F[1]"
}
```

It can be also a direct function call without the `*` operator (53-54):
```json
{
  "kind" : "function",
  "offset" : 1,
  "offsetrefs" : [
    {
      "kind" : "array",
      "id" : 2
    }   
  ],
  "expr" : "F[1]('x', 3.)"
},
{
  "member" : [ 3,0 ],
  "type" : [ 16,0 ],
  "access" : [ 1,0 ],
  "shift" : [ 0,0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "refcallref",
      "id" : 1,
      "mi" : 0,
      "di" : 2
    }   
  ],
  "expr" : "F[1]('x', 3.)->a.i"
},
{
  "kind" : "array",
  "offset" : 1,
  "basecnt" : 1,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 8
    }   
  ],
  "expr" : "F[1]"
}
```
```json
{
  "kind" : "function",
  "offset" : 1,
  "offsetrefs" : [
    {
      "kind" : "array",
      "id" : 1
    }   
  ],
  "expr" : "F[1]('x', 3.)"
},
{
  "kind" : "array",
  "offset" : 1,
  "basecnt" : 1,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 8
    }   
  ],
  "expr" : "F[1]"
}
```

Finally function can be called using direct address as in (55).
```json
{
  "kind" : "function",
  "offset" : 1,
  "offsetrefs" : [
    {
      "kind" : "address",
      "id" : 3334
    }   
  ],
  "expr" : "((pfun_t)(3333 + 1))('x', 3.)"
},
{
  "member" : [ 3,0 ],
  "type" : [ 16,0 ],
  "access" : [ 1,0 ],
  "shift" : [ 0,0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "addrcallref",
      "cast" : 19,
      "id" : 1,
      "mi" : 0,
      "di" : 3334
    }   
  ],
  "expr" : "((pfun_t)(3333 + 1))('x', 3.)->a.i"
}
```
When this happens as a base of member expression special `addrcallref` kind of variable in the `offsetrefs` list is used to indicate that. As usual the `id` field points to the appropriate entry in the `callrefs`+`refcallrefs` concatenated array of call information for this function which describes relevant function call information. Furthermore the `di` field contains the address value at which the call was made.

When address based call happens without the member expression as in (56) the address value is stored inside the `address` kind variable in the `offsetrefs` list.
```json
{
  "kind" : "function",
  "offset" : 1,
  "offsetrefs" : [
    {
      "kind" : "address",
      "id" : 3334
    }   
  ],
  "expr" : "((pfun_t)(3333 + 1))('x', 3.)"
}
```

Function call can also happen in the middle of the chain inside the member expression (57-60):
```json
{
  "member" : [ 3 ],
  "type" : [ 0 ],
  "access" : [ 0 ],
  "shift" : [ 0 ],
  "mcall" : [ 1 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 5,
      "mi" : 0
    }   
  ],
  "expr" : "oA.pF('x', 3.)"
}
```
To handle that properly additional `mcall` field is introduced in the `member` kind of dereference information entry. When at least one function call is made inside the member expression chain past the base member expression this value will point to the appropriate entry in the `callrefs`+`refcallrefs` concatenated array of call information for this function which describes relevant function call information.

The concatenated array of call information might look like follows (when only case (57) is considered):
```json
"callrefs": [
  [
    {
      "type" : "char_literal",
      "id" : 115
    },
    {
      "type" : "float_literal",
      "id" : 6
    }
  ]
],
"refcallrefs": [
  [
    {
      "type" : "char_literal",
      "id" : 120
    },
    {
      "type" : "float_literal",
      "id" : 3
    }
  ]
]
```
The `mcall` index value 1 points to two function parameters, char with value 120 `'x'` and float value `3.0`.

```json
{
  "member" : [ 1,1,3 ],
  "type" : [ 0,15,11 ],
  "access" : [ 0,1,1 ],
  "shift" : [ 0,0,0 ],
  "mcall" : [ -1,-1,1 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 5,
      "mi" : 0
    }   
  ],
  "expr" : "((struct A *)((struct C *)oA.p)->p)->pF('@', 1.5)"
}
```
`mcall` information equals to -1 for member expressions without the function call.

```json
{
  "member" : [ 3,0 ],
  "type" : [ 0,16 ],
  "access" : [ 0,1 ],
  "shift" : [ 0,0 ],
  "mcall" : [ 1,-1 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 5,
      "mi" : 0
    }   
  ],
  "expr" : "oA.pF('x', 3.)->i"
}
```
Function calls in the middle of member exression chain holds as well.

```json
{
  "member" : [ 3,2,3,0 ],
  "type" : [ 0,16,11,16 ],
  "access" : [ 0,1,1,1 ],
  "shift" : [ 0,0,0,0 ],
  "mcall" : [ 1,-1,2,-1 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 5,
      "mi" : 0
    }   
  ],
  "expr" : "((struct A *)oA.pF('x', 3.)->p)->pF('u', 999.10000000000002)->i"
}
```
There might be more than one function call in the member expression chain. The corresponding call referencen information below:
```json
"callrefs": [
  [
    {
      "type" : "char_literal",
      "id" : 115
    },
    {
      "type" : "float_literal",
      "id" : 6
    }
  ]
],
"refcallrefs": [
  [
    {
      "type" : "char_literal",
      "id" : 120
    },
    {
      "type" : "float_literal",
      "id" : 3
    }
  ],
  [
    {
      "type" : "char_literal",
      "id" : 117
    },
    {
      "type" : "float_literal",
      "id" : 999.1
    }
  ]
]
```

This also applies to functions called through the generic members as in (61):
```json
{
  "member" : [ 2,2,0 ],
  "type" : [ 0,16,16 ],
  "access" : [ 0,1,1 ],
  "shift" : [ 0,0,0 ],
  "mcall" : [ -1,1,-1 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 5,
      "mi" : 0
    }   
  ],
  "expr" : "((pfun_t)oA.pB->p)('x', 16.5)->i"
}
```

There might be conditional operator at the member expression base. Whenever the condition can be computed at compile time only control flow paths are considered for referenced variables. For example in (62):
```json
{
  "member" : [ 2 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }   
  ],
  "expr" : "(0 ? (struct B *)0 : (pB))->p"
}
```
It is known that the condition is false therefore only `pB` variable is listed as a base variable for the member expression.

Similarly for (63):
```json
{
  "member" : [ 2 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "address",
      "cast" : 16,
      "id" : 0,
      "mi" : 0
    }   
  ],
  "expr" : "((1 + 313) ? (struct B *)0 : (pB))->p"
}
```
It is known that then condition is true therefore variable with `address` kind and value 0 is listed as a base variable for the member expression.

In case condition cannot be computed at compile time we need to consider both paths (64):
```json
{
  "member" : [ 2 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    },
    {
      "kind" : "address",
      "cast" : 16,
      "id" : 0,
      "mi" : 0
    }   
  ],
  "expr" : "(*px ? (struct B *)0 : (pB))->p"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "parm",
      "id" : 0
    }   
  ],
  "expr" : "*px"
}
```
Here we have both `pB` and `address` 0 as variables at the base for the member expression.

Binary conditional operator (GNU extension) behaves in the same way (65-67):
```json
{
  "member" : [ 2 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }   
  ],
  "expr" : "((struct B *)0 ?: (pB))->p"
}
```
When condition resolves to false the result of the expression is the second argument of the binary conditional operator (variable `pB` in this case).

```json
{
  "member" : [ 2 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "address",
      "cast" : 16,
      "id" : 314,
      "mi" : 0
    }   
  ],
  "expr" : "((struct B *)(1 + 313) ?: (pB))->p"
}
```
When condition resolves to true the result of the expression is the condition itself (which in this case is the `address` value 314).

```json
{
  "member" : [ 2 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    },
    {
      "kind" : "unary",
      "cast" : 16,
      "id" : 1,
      "mi" : 0
    }   
  ],
  "expr" : "((struct B *)*px ?: (pB))->p"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "parm",
      "id" : 0
    }   
  ],
  "expr" : "*px"
}
```
When condition cannot be computed at compile time we need to consider both paths as usual.

To finalize let's take a look at some various examples (68-73).

```json
{
  "member" : [ 3 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "address",
      "cast" : 16,
      "id" : 30,
      "mi" : 0
    }   
  ],
  "expr" : "((struct B *)30)->a"
}
```
Here we have member expression that is based on some `address` value.


```json
{
  "member" : [ 2 ],
  "type" : [ 0 ],
  "access" : [ 0 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 5,
      "mi" : 0
    }   
  ],
  "expr" : "oA.pB"
},
{
  "member" : [ 2,3 ],
  "type" : [ 0,16 ],
  "access" : [ 0,1 ],
  "shift" : [ 0,0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "global",
      "id" : 1,
      "mi" : 0
    },
    {
      "kind" : "address",
      "id" : 3,
      "mi" : 0
    },
    {
      "kind" : "member",
      "id" : 0,
      "mi" : 0
    }   
  ],
  "expr" : "((struct A){.i = 3, .pB = oA.pB + gi}).pB->a"
}
```
Here at the base of member expression we have so called compound literal. The `struct A` type is initialized as a temporary and then it serves as a base for member expression. All variables that take part in the initializer are listed in the `offsetrefs` list.

```json
{
  "member" : [ 2 ],
  "type" : [ 0 ],
  "access" : [ 0 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 5,
      "mi" : 0
    }   
  ],
  "expr" : "oA.pB"
},
{
  "member" : [ 2,3 ],
  "type" : [ 0,16 ],
  "access" : [ 0,1 ],
  "shift" : [ 0,0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "global",
      "cast" : 33,
      "id" : 1,
      "mi" : 0
    },
    {
      "kind" : "address",
      "cast" : 32,
      "id" : 3,
      "mi" : 0
    },
    {
      "kind" : "member",
      "cast" : 3,
      "id" : 0,
      "mi" : 0
    }   
  ],
  "expr" : "((struct A){.i = (long)(3 + 1), .pB = (void *)oA.pB + (short)gi}).pB->a"
}
```
All the casts used in the initializers are detected and placed into proper `cast` fields in `offsetrefs`.

```json
{
  "member" : [ 0 ],
  "type" : [ 11 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "global",
      "id" : 1,
      "mi" : 0
    },
    {
      "kind" : "address",
      "cast" : 11,
      "id" : 0,
      "mi" : 0
    }   
  ],
  "expr" : "({\n    do {\n    } while (0);\n    (struct A *)0 + gi;\n})->i"
}
```
Yet another member expression with statement expression at its base.

```json
{
  "member" : [ 0 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }   
  ],
  "expr" : "pB->i"
},
{
  "member" : [ 2,3,2,4,0 ],
  "type" : [ 11,16,11,16,15 ],
  "access" : [ 1,1,1,1,1 ],
  "shift" : [ 0,4,0,0,0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "global",
      "id" : 1,
      "mi" : 2
    },
    {
      "kind" : "local",
      "id" : 5,
      "mi" : 0
    },
    {
      "kind" : "member",
      "id" : 0,
      "mi" : 2
    },
    {
      "kind" : "array",
      "id" : 2,
      "mi" : 4
    }   
  ],
  "expr" : "(((&((&oA)->pB + 4)->a) + gi + pB->i)->pB->pC + 10 * T[9])->f"
},
{
  "kind" : "array",
  "offset" : 9,
  "basecnt" : 1,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 3
    }   
  ],
  "expr" : "T[9]"
}
```
Here we have local variable with `id` set to 5 as the base of member expression. Second member expression in the chain have offset 4 (`shift` entry list). Third and fifth member expression in the chain use variables and other expression to facilitate offset computation.


```json
{
  "member" : [ 2,3,2,2 ],
  "type" : [ 11,16,11,16 ],
  "access" : [ 1,1,1,1 ],
  "shift" : [ 0,0,0,0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "global",
      "id" : 1,
      "mi" : 1
    },
    {
      "kind" : "local",
      "id" : 5,
      "mi" : 0
    },
    {
      "kind" : "address",
      "cast" : 1,
      "id" : 10,
      "mi" : 1
    }   
  ],
  "expr" : "(&((&oA)->pB + ({\n    do {\n    } while (0);\n    (int)10 + gi;\n}))->a)->pB->p"
}
```
Here we have statement expression in the offset computation for the second member expression in the chain (referencing one global variable and one integer address value).

```json
{
  "member" : [ 2 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "callref",
      "id" : 0,
      "mi" : 0
    }   
  ],
  "expr" : "({\n    ((void)(sizeof ((long)(0 && getN()))));\n    getB(0, 0);\n})->p"
}
```
We end up with statement expression that facilitates member expression through call to `getB` function at its value yielding expression.

```json
{
  "member" : [ 0 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "unary",
      "id" : 11,
      "mi" : 0
    }
  ],
  "expr" : "(*((struct B **)q))->i"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "local",
      "cast" : 27,
      "id" : 8
    }
  ],
  "expr" : "*((struct B **)q)"
}
```
In the example above variable at the base of dereference operator is being casted to proper type which results in filling the `cast` field.


```json
{
  "kind" : "unary",
  "offset" : -2,
  "offsetrefs" : [
    {
      "kind" : "parm",
      "cast" : 33,
      "id" : 0
    },
    {
      "kind" : "global",
      "cast" : 34,
      "id" : 1
    },
    {
      "kind" : "member",
      "cast" : 1,
      "id" : 1
    },
    {
      "kind" : "array",
      "cast" : 1,
      "id" : 2
    },
    {
      "kind" : "unary",
      "cast" : 35,
      "id" : 3
    },
    {
      "kind" : "callref",
      "cast" : 36,
      "id" : 0
    }
  ],
  "expr" : "*((unsigned char*)px + (long)gi - (unsigned long)2 + (int)pB->i + (int)T[4] + ((unsigned int)*px + 1) + (long long)getV())"
},
{
  "member" : [ 0 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }
  ],
  "expr" : "pB->i"
},
{
  "kind" : "array",
  "offset" : 4,
  "basecnt" : 1,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 3
    }
  ],
  "expr" : "T[4]"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "parm",
      "id" : 0
    }
  ],
  "expr" : "*px"
}
```
In the example above each component of the dereference expression has its own cast information through the `cast` field. The offset is computed to the value -2. The `offset` field is a sum of each part of the binary operator (+/-) from the original dereference expression which is a proper integer constant expression (i.e. can be computed at compile time). The are 6 parts in the dereference binary operator and only one can be computed at compile time, i.e. `(unsigned long)2`. The expression `((unsigned int)*px + 1)` which is another part of the dereference binary operator cannot.

```json
{
  "member" : [ 2 ],
  "type" : [ 0 ],
  "access" : [ 0 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 5,
      "mi" : 0
    }
  ],
  "expr" : "oA.pB"
},
{
  "member" : [ 1 ],
  "type" : [ 11 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "cast" : 11,
      "id" : 6,
      "mi" : 0
    },
    {
      "kind" : "member",
      "cast" : 11,
      "id" : 0,
      "mi" : 0
    }
  ],
  "expr" : "((struct A *)(oA.pB) ?: ((struct A *)pB))->p"
}
```
In the example above we have binary conditional operator at the base of member expression. The condition cannot be precomputed hence we have variables in both paths that can be a base for member expression. Both paths have a cast expression associated with it which is reflected in the `cast` field in appropriate `offsetrefs` entries.

```json
{
  "member" : [ 2 ],
  "type" : [ 0 ],
  "access" : [ 0 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 5,
      "mi" : 0
    }
  ],
  "expr" : "oA.pB"
},
{
  "member" : [ 1 ],
  "type" : [ 11 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 5,
      "mi" : 0
    },
    {
      "kind" : "local",
      "cast" : 11,
      "id" : 6,
      "mi" : 0
    }
  ],
  "expr" : "((struct A *)(oA.pB) ? (&oA) : ((struct A *)pB))->p"
}
```
Same things happen for standard conditional operator with a cast in the false path of the operator.

```json
{
  "member" : [ 2 ],
  "type" : [ 0 ],
  "access" : [ 0 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 5,
      "mi" : 0
    }
  ],
  "expr" : "oA.pB"
},
{
  "member" : [ 0 ],
  "type" : [ 0 ],
  "access" : [ 0 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "unary",
      "id" : 2,
      "mi" : 0
    }
  ],
  "expr" : "(*(((struct A *)(oA.pB) ? (&oA) : ((struct A *)pB)))).i"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 5
    },
    {
      "kind" : "local",
      "cast" : 11,
      "id" : 6
    }
  ],
  "expr" : "*(((struct A *)(oA.pB) ? (&oA) : ((struct A *)pB)))"
}
```
Conditional operator can be also a base for dereference operator with the same results.

```json
{
  "member" : [ 2 ],
  "type" : [ 0 ],
  "access" : [ 0 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 5,
      "mi" : 0
    }
  ],
  "expr" : "oA.pB"
},
{
  "member" : [ 1 ],
  "type" : [ 11 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 5,
      "mi" : 0
    },
    {
      "kind" : "local",
      "cast" : 11,
      "id" : 6,
      "mi" : 0
    },
    {
      "kind" : "refcallref",
      "cast" : 11,
      "id" : 1,
      "mi" : 0,
      "di" : 10
    }
  ],
  "expr" : "((struct A *)(oA.pB) ? (&oA) : (i ? (((struct A *)pB)) : ((struct A *)pfv())))->p"
}
```
In the case of double conditional operator all variables from all viable paths are included in `offsetrefs`.


```json
{
  "member" : [ 0 ],
  "type" : [ 16 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }
  ],
  "expr" : "pB->i"
},
{
  "kind" : "array",
  "offset" : 0,
  "basecnt" : 1,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 3
    },
    {
      "kind" : "callref",
      "id" : 0
    },
    {
      "kind" : "member",
      "cast" : 32,
      "id" : 0
    }
  ],
  "expr" : "T[getN() + (long)pB->i]"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "parm",
      "id" : 0
    },
    {
      "kind" : "local",
      "cast" : 3,
      "id" : 6
    },
    {
      "kind" : "local",
      "cast" : 3,
      "id" : 8
    }
  ],
  "expr" : "*(px + ((void *)&q - (void *)pB))"
},
{
  "member" : [ 2,1,1,0 ],
  "type" : [ 16,11,15,11 ],
  "access" : [ 1,1,1,1 ],
  "shift" : [ 0,0,0,2 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "global",
      "id" : 1,
      "mi" : 3
    },
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    }
  ],
  "expr" : "((struct A *)((struct C *)((struct A *)pB->p)->p)->p + 2 + gi)->i"
},
{
  "member" : [ 1 ],
  "type" : [ 11 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 6,
      "mi" : 0
    },
    {
      "kind" : "local",
      "cast" : 3,
      "id" : 7,
      "mi" : 0
    },
    {
      "kind" : "address",
      "cast" : 16,
      "id" : 0,
      "mi" : 0
    }
  ],
  "expr" : "((struct A *)(((struct A *)((struct C *)((struct A *)pB->p)->p)->p + 2 + gi)->i ? (struct B *)0 : (unsigned long)(*(px + ((void *)&q - (void *)pB)) + (int)T[getN() + (long)pB->i]) ? (pB) : ((void *)ppB)))->p"
}

```
We need to remember that condition expression of conditional operator which is a base for member expression is not included in the `offsetrefs` field of this member expression therefore we have only 3 viable paths in the complicated expression above (which are listed in the `offsetrefs` field with appropriate casts).

There are two more types of expressions that can be found in the dereference information, i.e. `init` and `assign` kinds. First expression is a variable definition with appropriate initializer (only definitions with initializer are included). Second one is an ordinary assignment of value to existing variable.

Let's have a look for the simplest case of defining and initializing an `int` variable as in (81).
```json
{
  "kind" : "init",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 13
    },
    {
      "kind" : "integer",
      "id" : 0
    }
  ],
  "expr" : "int vi0 = 0"
}
```
The first entry describes the variable being defined and initialized. The following entries are all variables extracted from the initializer (in the case above we have only integer value `0`).

If there is a cast at the initializer expression (as in (82)) the `cast` field is filled with appropriate value.
```json
{
  "kind" : "init",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 13
    },
    {
      "kind" : "float",
      "cast" : 1,
      "id" : 1
    }
  ],
  "expr" : "int vi1 = (int)1."
}
```

When variable is initialized or assignment operator is used the cast can also be implicit (as in (83)).
```json
{
  "kind" : "init",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 13
    },
    {
      "kind" : "integer",
      "cast" : 32,
      "id" : 2
    }
  ],
  "expr" : "unsigned int vu0 = 2"
}
```
Here the initializer has originally type `int` and is implicitly casted to `unsigned int` based on the variable type. It is reflected in the `cast` field for the initializer variable.

Same principles apply for `void*` variables (84,85,86) with one exception.
```json
{
  "kind" : "init",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 13
    },
    {
      "kind" : "local",
      "id" : 8
    }
  ],
  "expr" : "void *vq0 = q"
}
```
Here the `q` variable has type `void*` therefore no implicit or explicit cast is performed.

```json
{
  "kind" : "init",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 13
    },
    {
      "kind" : "local",
      "cast" : 11,
      "id" : 6
    }
  ],
  "expr" : "void *vq1 = (struct A *)pB"
}
```
Here we make an explicit cast to `struct A*` which is reflected in the `cast` field.

```json
{
  "kind" : "init",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 13
    },
    {
      "kind" : "local",
      "cast" : 16,
      "id" : 6
    }
  ],
  "expr" : "void *vq2 = pB"
}
```
Here we have a special case handling for `void*` type. When a value is assigned to `void*` variable and there is an implicit cast from the initializer the `cast` field will contain the type of the initializer and not the variable type (i.e. `void*` in this case) as normally. For example in (87) we have implicit cast from `struct B*` to `void*` but the `cast` field contains the `struct B*` type and not the `void*`. This can be used to properly track real types of variables passed around in generic `void*` pointers.

The expression in the initializer can be a function call (87,88):
```json
{
  "kind" : "init",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 13
    },
    {
      "kind" : "callref",
      "cast" : 16,
      "id" : 0
    }
  ],
  "expr" : "void *vq3 = getB('a', 3.)"
}
```
```json
{
  "kind" : "init",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 13
    },
    {
      "kind" : "callref",
      "id" : 0
    }
  ],
  "expr" : "int vi2 = (*getN)()"
}
```

It can also be a function call through pointer variable (89,90):
```json
{
  "kind" : "init",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 13
    },
    {
      "kind" : "refcallref",
      "cast" : 32,
      "id" : 1,
      "di" : 11
    }
  ],
  "expr" : "unsigned long vul0 = (long)pfi()"
}
```
```json
{
  "kind" : "init",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 13
    },
    {
      "kind" : "refcallref",
      "cast" : 9,
      "id" : 1,
      "di" : 12
    }
  ],
  "expr" : "unsigned long vul1 = (*pfi)()"
}
```

When a string literal is used as an initializer there is no implicit cast information in the `cast` field (even though there is implicit cast from `char*` to `int*` as in (91)).
```json
{
  "kind" : "init",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 13
    },
    {
      "kind" : "string",
      "id" : "ABRAKADABRA"
    }
  ],
  "expr" : "int *vpi0 = \"ABRAKADABRA\""
}
```

Initializer can have many variables and other expressions involved as in (92):
```json
{
  "kind" : "init",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 13
    },
    {
      "kind" : "local",
      "id" : 2
    },
    {
      "kind" : "member",
      "cast" : 33,
      "id" : 12
    },
    {
      "kind" : "unary",
      "id" : 13
    }
  ],
  "expr" : "unsigned int vu1 = i + *((int *)pB->p) - (long)oA.i"
}
```

Initializer can also be a compound literal as in (93):
```json
{
  "kind" : "init",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 13
    },
    {
      "kind" : "global",
      "id" : 1
    },
    {
      "kind" : "address",
      "cast" : 32,
      "id" : 3
    },
    {
      "kind" : "member",
      "cast" : 33,
      "id" : 11
    }
  ],
  "expr" : "struct A obA = ((struct A){.i = (long)3, .pB = (long long)oA.pB->pC + gi})"
}
```

Assignment expressions are very similar to variable initialization in all aspects. The first entry in `offsetrefs` is the variable being assigned to. Further entries are variables or expressions from the right hand side of the assignment. One difference is the `offset` field which is populated by specific id number of the assignment kind (and there are 11 kinds of assignment) whereas it was always 0 for variable initialization. For example in (94) we have implicit convertion of `unsigned long` value 4 to `long` which is reflected by the typeid value in `cast` field. In (95) we're assigning to the result of member expression (which has type `void*` and therefore cast to the right hand side type is extracted). In both below cases the assignment kind is 21 which is the ordinary `=` assignment.
```json
{
  "kind" : "assign",
  "offset" : 21,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 13
    },
    {
      "kind" : "integer",
      "cast" : 32,
      "id" : 4
    }
  ],
  "expr" : "vd0 = 4UL"
}
```
```json
{
  "kind" : "assign",
  "offset" : 21,
  "offsetrefs" : [
    {
      "kind" : "member",
      "id" : 10
    },
    {
      "kind" : "local",
      "cast" : 16,
      "id" : 6
    }
  ],
  "expr" : "pB->pC->p = pB"
}
```

In (96) we take the value from variable `vl0`, apply binary and operator to it with the value 63 and store the result back in `vl0`. This is achieved throught the `&=` assignment operator which kind is 29 as shown below:
```json
{
  "kind" : "init",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 13
    },
    {
      "kind" : "integer",
      "cast" : 32,
      "id" : 255
    }
  ],
  "expr" : "long vl0 = 255"
},
{
  "kind" : "assign",
  "offset" : 29,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 13
    },
    {
      "kind" : "integer",
      "cast" : 32,
      "id" : 63
    }
  ],
  "expr" : "vl0 &= 63"
}
```

Last missing piece of information is presented in the example below. Assignment expression can be found in various value yielding expressions like in (97). In this case there is `assign` kind entry in the `offsetrefs` list which points to the appropriate assignment in the `derefs` array (assignment kind 25 which is the `+=` assignment operator).

```json
{
  "kind" : "init",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 13
    },
    {
      "kind" : "unary",
      "cast" : 32,
      "id" : 12
    },
    {
      "kind" : "array",
      "cast" : 33,
      "id" : 13
    },
    {
      "kind" : "assign",
      "cast" : 1,
      "id" : 1
    }
  ],
  "expr" : "unsigned long vul3 = (long)*((int *)pB->p) - (short)T[2] + (int)(gi += 3)"
},
{
  "kind" : "assign",
  "offset" : 25,
  "offsetrefs" : [
    {
      "kind" : "global",
      "id" : 1
    },
    {
      "kind" : "integer",
      "cast" : 9,
      "id" : 3
    }
  ],
  "expr" : "gi += 3"
}
```

There are facilities in the C/C++ languages to extract offset of a specified member inside the structure. This is exactly what `offsetof` macro does. This is supported in the dereference information as a "offsetof" kind entry. Consider the example as in (98):
```json
{
  "kind" : "init",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 13
    },
    {
      "kind" : "offsetof",
      "id" : 1
    }
  ],
  "expr" : "unsigned long ul = 4 + __builtin_offsetof(struct C, b.Ta[4][oA.i].p)"
},
{
  "member" : [ 3,4,-1,-1,1 ],
  "type" : [ 9,4,4,4,0 ],
  "kind" : "offsetof",
  "offsetrefs" : [
    {
      "kind" : "integer",
      "id" : 4,
      "mi" : 1
    },
    {
      "kind" : "member",
      "id" : 11,
      "mi" : 1
    }
  ],
  "expr" : "__builtin_offsetof(struct C, b.Ta[4][oA.i].p)"
}
```

The `ul` variable initializer uses offsetof expression to compute the initialization value. This is marked in the `offsetrefs` array as a `offsetof` kind. The `member` array of the `offsetof` kind entry specifies which member was used for offset extraction. Usually it is only one member but the expression can get more complicated as in this example. First used member was `b` which has offset 3 in the `struct C` definition. The type of `struct C` is places at first position in the `type` array. Second member in the chain was `Ta` which has index 4 in the `struct B` definition (and the type id is 4). Next we have two array index expressions which are marked in the `member` array as an `-1` index value. Array expression type in the `type` arary is the same type as the array member (type index 4 in this case). References to components of the array index expressions can be found in the `offsetrefs` array where `mi` field indicates which member in the chain uses the array index expression. Finally the `p` member (with index 1) of the `struct A` type (with type index 0) is used for offset computation as a final component.

There is additional information that can be found in the dereference information that allows to track data information flow between functions, i.e. return expression information and information about passed function arguments. First is implemented as `return` kind entry. Consider the following code:
```c
int main(void) {

    long x = 3;
    if (x<0) {
        return x;
    }
    else if (x==0) {
        return (char)100;
    }
    else {
        return (int)x + (int*)4;
    }
    return 3;
}
```
The dereference information for this simple program could look as follows:
```json
(...)
{
  "kind" : "return",
  "offsetrefs" : [
    {
      "kind" : "local",
      "cast" : 1,
      "id" : 0
    }   
  ],
  "ord" : [ 1 ],
  "expr" : "return x;\n"
},
{
  "kind" : "return",
  "offsetrefs" : [
    {
      "kind" : "integer",
      "cast" : 2,
      "id" : 100
    }   
  ],
  "ord" : [ 2 ],
  "expr" : "return (char)100;\n"
},
{
  "kind" : "return",
  "offsetrefs" : [
    {
      "kind" : "local",
      "cast" : 0,
      "id" : 0
    },
    {
      "kind" : "address",
      "cast" : 3,
      "id" : 4
    }   
  ],
  "ord" : [ 3 ],
  "expr" : "return (int)x + (int *)4;\n"
},
{
  "kind" : "return",
  "offsetrefs" : [
    {
      "kind" : "integer",
      "id" : 3
    }   
  ],
  "ord" : [ 4 ],
  "expr" : "return 3;\n"
}
```
We have special entry of the `return` kind which contains references to variables/expressions used in the return expression in the `offsetrefs` list. For example in the first `if` clause (`if (x<0)`) we have local variable `x` of type `long`. There is implicit cast from `long` to the returned value of type `int` which is reflected in the `cast` field. The `cast` field for the variable/expression used in the return statement will contain type id of a type of this variable/expression whenever this type is different from the return type or explicit cast is used on the variable/expression. For example in the second `if` clause (`if (x==0)`) there is explicit cast to `char` therefore the `cast` field has the value 2 (which points to the `char` type). On the other hand in the final return statement (`return 3`) the value returned has type `int`, exactly the same as the returned type of this function therefore no `cast` field is propagated. Finally in the `return (int)x + (int*)4` expression we have more complicated case where the return expression is composed of two other expressions (one variable and one literal value). Each of the components have their casts in place but in such complex expressions the `cast` attribute of the return expression is not directly accessible.

The second one is implemented as `parm` kind entry. Consider the following code:
```c
int foo(int a, const char* b) {
    return 0;
}

struct A {
    void* p;
    const char* s;
    int (*pf)(int x, const char* q);
};

typedef int (*pfun_t)(int a, const char* b);

int main(void) {

    struct A* pA = 0;
    struct A a = {};
    char T[10];
    pfun_t f = foo;

    foo(*((int*)pA->p),a.s);
    foo(10,a.s);
    foo(10,0);
    (*f)(20,"roll!");
    pA->pf(20,T);

    return 0;
}
```

The dereference information could look as follows:
```json
(...)
{
  "member" : [ 0 ],
  "type" : [ 9 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 0,
      "mi" : 0
    }   
  ],
  "ord" : [ 8 ],
  "expr" : "pA->p"
},
{
  "member" : [ 1 ],
  "type" : [ 3 ],
  "access" : [ 0 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 1,
      "mi" : 0
    }   
  ],
  "ord" : [ 9 ],
  "expr" : "a.s"
},
{
  "member" : [ 1 ],
  "type" : [ 3 ],
  "access" : [ 0 ],
  "shift" : [ 0 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 1,
      "mi" : 0
    }   
  ],
  "ord" : [ 13 ],
  "expr" : "a.s"
},
{
  "member" : [ 2 ],
  "type" : [ 9 ],
  "access" : [ 1 ],
  "shift" : [ 0 ],
  "mcall" : [ 3 ],
  "kind" : "member",
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 0,
      "mi" : 0
    }   
  ],
  "ord" : [ 24 ],
  "expr" : "pA->pf(20, T)"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "member",
      "cast" : 12,
      "id" : 0
    }   
  ],
  "ord" : [ 7 ],
  "expr" : "*((int *)pA->p)"
},
{
  "kind" : "unary",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 3
    }   
  ],
  "ord" : [ 20 ],
  "expr" : "*f"
},
{
  "kind" : "return",
  "offsetrefs" : [
    {
      "kind" : "integer",
      "id" : 0
    }   
  ],
  "ord" : [ 25 ],
  "expr" : "return 0;\n"
},
{
  "kind" : "parm",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "unary",
      "cast" : 0,
      "id" : 4
    }   
  ],
  "ord" : [ 5 ],
  "expr" : "*((int *)pA->p)"
},
{
  "kind" : "parm",
  "offset" : 1,
  "offsetrefs" : [
    {
      "kind" : "member",
      "cast" : 2,
      "id" : 1
    }   
  ],
  "ord" : [ 6 ],
  "expr" : "a.s"
},
{
  "kind" : "parm",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "integer",
      "id" : 10
    }   
  ],
  "ord" : [ 11,15 ],
  "expr" : "10"
},
{
  "kind" : "parm",
  "offset" : 1,
  "offsetrefs" : [
    {
      "kind" : "member",
      "cast" : 2,
      "id" : 2
    }   
  ],
  "ord" : [ 12 ],
  "expr" : "a.s"
},
{
  "kind" : "parm",
  "offset" : 1,
  "offsetrefs" : [
    {
      "kind" : "address",
      "cast" : 0,
      "id" : 0
    }   
  ],
  "ord" : [ 16 ],
  "expr" : "0"
},
{
  "kind" : "parm",
  "offset" : 0,
  "offsetrefs" : [
    {
      "kind" : "integer",
      "id" : 20
    }   
  ],
  "ord" : [ 18,22 ],
  "expr" : "20"
},
{
  "kind" : "parm",
  "offset" : 1,
  "offsetrefs" : [
    {
      "kind" : "string",
      "cast" : 13,
      "id" : "roll!"
    }   
  ],
  "ord" : [ 19 ],
  "expr" : "\"roll!\""
},
{
  "kind" : "parm",
  "offset" : 1,
  "offsetrefs" : [
    {
      "kind" : "local",
      "cast" : 13,
      "id" : 2
    }   
  ],
  "ord" : [ 23 ],
  "expr" : "T"
}
```

Here we have 5 function calls, each with 2 arguments. This gives 8 `parm` entries in the `derefs` array (2 entries with integer value arguments are deduplicated). Each entry describes the argument expression. The `call_info` and `refcall_info` looks like follows:
```json
"call_info": [ 
  {
    "start":"2249:5",
    "end":"2249:13",
    "ord":14,
    "args": [ 9,11 ],
    "expr": "foo(10, 0)"
  }, 
  {
    "start":"2248:5",
    "end":"2248:15",
    "ord":10,
    "args": [ 9,10 ],
    "expr": "foo(10, a.s)"
  }, 
  {
    "start":"2247:5",
    "end":"2247:27",
    "ord":4,
    "args": [ 7,8 ],
    "expr": "foo(*((int *)pA->p), a.s)"
  }
],
"refcall_info": [
  {
    "start":"2251:5",
    "end":"2251:16",
    "ord":26,
    "args": [ 12,14 ],
    "expr": "pA->pf(20, T)"
  },
  {
    "start":"2250:5",
    "end":"2250:20",
    "ord":27,
    "args": [ 12,13 ],
    "expr": "(*f)(20, \"roll!\")"
  }
],
```
The values in the `args` array of each call information entry points to the appropriate `parm` entry in the `derefs` array.

In (99) and (100) we have examples of `cond` and `logic` derefs. First `cond` refers to global checked in `if(pfi)`.
Second `cond` refers to `logic` deref which represents comparison `i < 10`. In `logic` deref `offset` vaule 10 indicates use of `<` operator and `basecnt` 1 informs us that only the first of `offsetrefs` is on the left-hand-side of the operator.
Value of `offset` in `cond` is the id in `csmap` of (implicit) compound statement containing conditional expressions.
```json
{
  "kind" : "cond",
  "offset" : 22,
  "offsetrefs" : [
    {
      "kind" : "global",
      "cast" : 23,
      "id" : 3
    }		
  ],
  "ord" : [ 272 ],
  "expr" : "[/home/m.manko/sec-tools/clang-proc/build-6443078/test.c:294:8]: pfi",
  "csid" : 0
},
{
  "kind" : "cond",
  "offset" : 23,
  "offsetrefs" : [
    {
      "kind" : "logic",
      "id" : 206
    }		
  ],
  "ord" : [ 274 ],
  "expr" : "[/home/m.manko/sec-tools/clang-proc/build-6443078/test.c:295:11]: i < 10",
  "csid" : 0
},
{
  "kind" : "logic",
  "offset" : 10,
  "basecnt" : 1,
  "offsetrefs" : [
    {
      "kind" : "local",
      "id" : 2
    },
    {
      "kind" : "integer",
      "id" : 10
    }		
  ],
  "ord" : [ 275 ],
  "expr" : "[/home/m.manko/sec-tools/clang-proc/build-6443078/test.c:295:11]: i < 10",
  "csid" : 0
}
```
