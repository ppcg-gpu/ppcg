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

    tests/whole-program/verify.py \
        --ppcg-build <a build of this repository> \
        --clang <the build of patched LLVM it was built against> \
        --llama <a checkout of llama-dspark>

One call: it emits the corpus, links the units the list names, generates
one module, builds one library with `-fopenmp`, counts what that library
holds against the four the ordinary build makes, and prints the numbers.
It exits non-zero if any of them differs from what is expected, and the
expectations are flags -- `--expect-refused`, `--expect-records` and the
rest -- so a change in them is written down rather than argued about.

Two things have to exist first.  The patched clang, which
`ThirdParty/pet/llvm-patches/README.md` describes and which this
repository has to be built against; and llama-dspark configured the way
its ordinary build is, since the flags the corpus is read with come from
that build's compilation database and the four libraries it makes are
what the one library is measured against:

    cmake -S . -B build-noavx -DCMAKE_BUILD_TYPE=Release -DGGML_NATIVE=OFF \
      -DGGML_AVX=OFF -DGGML_AVX2=OFF -DGGML_AVX512=OFF -DGGML_FMA=OFF \
      -DGGML_F16C=OFF -DGGML_SSE42=OFF -DGGML_BMI2=OFF \
      -DCMAKE_C_FLAGS=-ffp-contract=off -DCMAKE_CXX_FLAGS=-ffp-contract=off \
      -DLLAMA_MODEL_DIMS=dims/deepseek4.h -DLLAMA_DRAFT_DIMS=dims/dflash.h \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    cmake --build build-noavx -j

`llama-dspark-one-record-shape.patch` has to be applied to it as well:
ggml-common.h otherwise describes one record two ways and the link
refuses it, which is the thing that patch is about.

A run looks like this:

    ppcg build                  /dev/shm/p218
    clang                       /dev/shm/llvm-218-build/bin/clang
    llama-dspark                /dev/shm/llama-frozen
    corpus                      /dev/shm/verify-run/corpus
    corpus emitted              52 units in 6s
    units listed                51
    link                        units 51 refused 0 records 277 rc 0
    module                      73.0 MB in 87s
    library                     3.7 MB
    symbols                     1965 in the four, 2836 in the one, 0 missing

    verify: the whole-program path holds

`--reuse-corpus` reads a corpus that is already there instead of writing
it again, which saves the six seconds and is where the one real trap
lives: a serialised AST can be read only by the build of clang that
wrote it, and handed to another all it says is "unable to load
precompiled file".  The corpus therefore carries a stamp of what wrote
it, and the stamp is checked before a single unit is read:

    verify: the corpus was written by another build and cannot be read
    back by this one.
      written by: /dev/shm/p218/ThirdParty/pet/pet_emit_ast
          against: /dev/shm/llvm-218-build/lib/libclang-cpp.so.22.1
      reading with: /dev/shm/cleanbuild/ThirdParty/pet/pet_emit_ast
           against: /dev/shm/llvm-dbg/lib/libclang-cpp.so.22.1

## To run the project's own tests against the library

Apply `llama-dspark-whole-program-lib.patch` and configure a second
build with the same options as the first, plus the library that
verify.py made:

    cmake -S . -B build-whole <the options above> \
      -DLLAMA_WHOLE_PROGRAM_LIB=/dev/shm/whole-program-verify/libwhole.so
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

