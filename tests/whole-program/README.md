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

The corpus is the units of llama-dspark as pet reads them, and it has to
be made before anything else.  The build it is read from decides what
the units say, so it is configured the way `build-noavx` is -- the whole
SIMD group off, production dims -- and the flags are taken from its
compilation database rather than guessed:

    cmake -S . -B build-noavx -DCMAKE_BUILD_TYPE=Release -DGGML_NATIVE=OFF \
      -DGGML_AVX=OFF -DGGML_AVX2=OFF -DGGML_AVX512=OFF -DGGML_FMA=OFF \
      -DGGML_F16C=OFF -DGGML_SSE42=OFF -DGGML_BMI2=OFF \
      -DCMAKE_C_FLAGS=-ffp-contract=off -DCMAKE_CXX_FLAGS=-ffp-contract=off \
      -DLLAMA_MODEL_DIMS=dims/deepseek4.h -DLLAMA_DRAFT_DIMS=dims/dflash.h \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

    pet_emit_ast --compile-commands build-noavx/compile_commands.json \
        <source> <unit>.ast          # once per unit; see emit_corpus.py

Then link them, generate one module and make one library.  The units and
the order are in `llama-dspark.units`, and both matter:

    pet_ast_link $(units)                    # has to say: refused 0
    pet_linked_ir whole.ll $(units)
    clang -c -O2 -o whole.o whole.ll
    clang -shared -fopenmp -o libwhole.so whole.o -lm -lstdc++

`-fopenmp` is not optional and is easy to leave out: ggml is built with
OpenMP, so the module calls `__kmpc_*`, and a library made without it
links but leaves thirteen undefined symbols for whoever uses it to
discover.

Then apply the patch to llama-dspark and configure a second build with
the same options as the first, plus the library:

    git apply .../llama-dspark-whole-program-lib.patch
    cmake -S . -B build-whole <the options above> \
      -DLLAMA_WHOLE_PROGRAM_LIB=/path/to/libwhole.so
    cmake --build build-whole -j
    ctest --test-dir build-whole

The ordinary `build-noavx` is needed either way: the drafter's library
and llama-common are compiled from source in both builds.

## What it came to

31 of 32.  The one that fails is `test-generate-models`, which writes
model fixtures and skips them because this build has production dims --
"deepseek4 model (MoE) is 128 wide and this build is 4096" -- and it
fails the same way on `build-noavx`, so it is not about where the engine
came from.

