# Raccoon Compiler

## Downloading

Run `git clone --recursive https://github.com/wdhg/racc` or `git clone --recursive git@github.com:wdhg/racc.git`

## Building the Compiler

Run `make` in the top directory to build the compiler binary `bin/racc`
  - Run `make debug` to build the compiler with debugging info (`bin/racc_debug`)
  - Run `make test` to build the unit test binary (`bin/racc_test`)

## Compiling Raccoon Code

Use the compiler to convert Raccoon code into C code:

```
$ bin/racc main.rc output.c
```

Then use your local C compiler to compile the output. You must link to the `base.o` and `arena.o` library objects and include their headers:

```
$ cc -o main output.c lib/base/obj/lib/base.o lib/arena/obj/lib/arena.o -std=c99 -Ilib/base/obj/include -Ilib/arena/include
```

See examples in `exm`.
