# Building llama-dspark out of one whole-program library

`llama-dspark-whole-program-lib.patch` adds `LLAMA_WHOLE_PROGRAM_LIB` to
llama-dspark.  Given a shared library holding the whole engine, the four
targets a build usually makes -- `llama`, `ggml`, `ggml-base`,
`ggml-cpu` -- are declared as that one library and nothing is compiled
for them.  The drafter's library is still built from source: it is the
same engine again with another model and a prefix on its symbols, so a
whole-program build of it is a second library and a second question.

The patch is kept here rather than in llama-dspark because it exists for
this work.  It applies to the tree it was made from and has not been
carried upstream.

## What it is for

A link can report that it refused nothing, resolved every call, and
holds every record, and still have lost something -- because the report
is made from the same reading that produced the module.  The project's
own tests do not share that reading.  They were written against the
engine and do not know where it came from, so what they catch is what
was actually lost.

It caught this: three of ggml's lookup tables, `ggml_table_f32_f16` and
the two beside it, declared in the module and defined nowhere.  Out of
28600 functions and 2833 exported symbols, they were the only things
missing, and nothing in the link's own account of itself mentioned them.

## How to run it

Make the corpus and the library:

    ppcg/build/ThirdParty/pet/pet_ast_link $(cat llama-dspark.units)
    ppcg/build/ThirdParty/pet/pet_linked_ir whole.ll $(cat llama-dspark.units)
    clang -c -O2 -o whole.o whole.ll
    clang -shared -fopenmp -o libwhole.so whole.o -lm -lstdc++

`-fopenmp` is not optional and is easy to leave out: ggml is built with
OpenMP, so the module calls `__kmpc_*`, and a library made without it
links but leaves thirteen undefined symbols for whoever uses it to
discover.

Then configure llama-dspark with the same options its ordinary build
uses, plus:

    -DLLAMA_WHOLE_PROGRAM_LIB=/path/to/libwhole.so

and build and run its tests.  `llama-dspark.units` names the units and
the order they are linked in; both matter, and why is written there.
