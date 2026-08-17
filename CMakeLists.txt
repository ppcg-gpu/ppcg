cmake_minimum_required(VERSION 3.21)

project(ppcg VERSION 0.09.2 LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Without a build type CMake passes no optimisation flags at all, which
# silently produces a compiler that is an order of magnitude slower than the
# -O2 -g the autotools build used to default to.  Multi-config generators
# pick the configuration at build time, so leave those alone.
get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()
if(NOT IS_MULTI_CONFIG)
    message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
endif()

include(GNUInstallDirs)

# Establish BUILD_TESTING before the subprojects, so that turning testing
# off reaches all of them.
include(CTest)

# Checked here rather than next to the tests that use it: configuring the
# subprojects takes minutes, and a mistyped path should be reported now.
# Only when the tests it feeds are going to be registered at all.
set(POLYBENCH_DIR "" CACHE PATH "Path to Polybench directory")
if(BUILD_TESTING AND POLYBENCH_DIR AND
        NOT EXISTS "${POLYBENCH_DIR}/utilities/benchmark_list")
    message(FATAL_ERROR
        "POLYBENCH_DIR is set to ${POLYBENCH_DIR}, but it does not contain "
        "utilities/benchmark_list")
endif()

# Keep the installed program pointing at the libraries in the same prefix
# instead of whatever isl the system happens to provide.
set(CMAKE_INSTALL_RPATH "$ORIGIN;$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")

# pet is built as part of this project and brings in isl.  Pass
# -DPET_FORCE_LLVM_VERSION=<n> to pin a particular LLVM.
add_subdirectory(ThirdParty/pet)

# gitversion.h, reported by ppcg --version
execute_process(
    COMMAND git describe --always --dirty
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HEAD_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE GIT_DESCRIBE_RESULT
)
if(NOT GIT_DESCRIBE_RESULT EQUAL 0 OR NOT GIT_HEAD_VERSION)
    set(GIT_HEAD_VERSION "ppcg-${PROJECT_VERSION}")
endif()
file(GENERATE
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gitversion.h
    CONTENT "#define GIT_HEAD_ID \"${GIT_HEAD_VERSION}\"\n"
)

add_executable(ppcg
    cpu.c
    cuda.c
    opencl.c
    cuda_common.c
    gpu.c
    gpu_array_tile.c
    gpu_group.c
    gpu_hybrid.c
    gpu_print.c
    gpu_tree.c
    grouping.c
    hybrid.c
    schedule.c
    ppcg_options.c
    ppcg.c
    reduction.c
    print.c
    util.c
    version.c
)
# The only C++ in the program comes from the libraries it links against.
set_target_properties(ppcg PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(ppcg PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(ppcg PRIVATE pet isl)

install(TARGETS ppcg RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# The generated OpenCL host code calls into ocl_utilities.c, which the user
# has to compile alongside it, so it has to be available after installing.
install(FILES ocl_utilities.c ocl_utilities.h
    DESTINATION ${CMAKE_INSTALL_DATADIR}/ppcg)

#--------------------------------------------------------------------
# Tests
#--------------------------------------------------------------------

find_package(OpenCL)
find_package(OpenMP)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Everything below is registered only when testing is on.
if(BUILD_TESTING)

set(RUN_AND_COMPARE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunAndCompare.cmake)
set(COMPARE_OUTPUTS ${CMAKE_CURRENT_SOURCE_DIR}/compare_outputs.py)

# Translate a source file with ppcg and build the result.
#
#   target       name of the program to create
#   source       input C file
#   output_dir   where to place the generated code and the program
#   ppcg_options extra options for ppcg
#
# The remaining arguments are passed to the compiler of the generated code.
function(add_ppcg_program target source output_dir ppcg_options)
    get_filename_component(base_name ${source} NAME_WE)
    set(generated "${output_dir}/${base_name}.${target}.c")

    file(MAKE_DIRECTORY ${output_dir})

    add_custom_command(
        OUTPUT ${generated}
        COMMAND ppcg ${ppcg_options} ${source} -o ${generated}
        DEPENDS ppcg ${source}
        COMMENT "Generating ${base_name} with '${ppcg_options}'"
        VERBATIM
    )

    add_executable(${target} ${generated} ${ARGN})
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR} ${output_dir})
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${output_dir}
        C_STANDARD 99
        C_EXTENSIONS ON
    )
endfunction()

#--------------------------------------------------------------------
# Reduction detection
#
# An accumulation is what stands between a loop that reads like a
# reduction and a loop that can be run in parallel, so which statements
# are recognised as one is checked on its own, before anything is done
# with them.
#--------------------------------------------------------------------

file(GLOB REDUCTION_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/reduction/*.c")
foreach(source ${REDUCTION_FILES})
    get_filename_component(base_name ${source} NAME_WE)
    add_test(NAME Reduction_${base_name}
        COMMAND ${CMAKE_COMMAND}
            -DPPCG=$<TARGET_FILE:ppcg>
            -DSOURCE=${source}
            -DBINDIR=${CMAKE_CURRENT_BINARY_DIR}/tests/reduction/${base_name}
            -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/tests/reduction/${base_name}.reductions
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunReductionTest.cmake
    )
endforeach()

#--------------------------------------------------------------------
# Reduction parallelisation
#
# Recognising an accumulation is only half of it.  The other half is that
# the loop around it is then handed to OpenMP with a reduction clause, and
# that the answer survives being computed by several threads at once.
#
# Both halves are checked, because either one on its own can be satisfied
# by a compiler that does nothing: a sequential loop gives the right answer
# too, and a clause that is emitted for a loop that is never run in
# parallel proves nothing about the result.
#--------------------------------------------------------------------

set(REDUCTION_RUN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/reduction/run")

# Check the code generated for "base_name" against the pragmas it has to
# contain, and check that running it gives the same answer as running the
# program it was generated from.
#
# The corpus is a pair of files: base_name.c holds the scop and is also
# the reference implementation, base_name_main.c drives it and prints
# what it computed.
#
# "tolerant" says whether the answer may differ in its last digits, which
# it does when the partial results of a floating point accumulation are
# put back together in a different order.
function(add_reduction_run_test base_name tolerant)
    set(source ${REDUCTION_RUN_DIR}/${base_name}.c)
    set(main ${REDUCTION_RUN_DIR}/${base_name}_main.c)
    set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/tests/reduction/run/${base_name}")
    file(MAKE_DIRECTORY ${output_dir})

    add_test(NAME Reduction_codegen_${base_name}
        COMMAND ${CMAKE_COMMAND}
            -DPPCG=$<TARGET_FILE:ppcg>
            -DSOURCE=${source}
            -DBINDIR=${output_dir}/codegen
            -DEXPECTED=${REDUCTION_RUN_DIR}/${base_name}.omp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunReductionCodegenTest.cmake
    )

    if(NOT OpenMP_C_FOUND)
        return()
    endif()

    set(reference reduction_${base_name}_ref)
    add_executable(${reference} ${source} ${main})
    set_target_properties(${reference} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${output_dir}
        C_STANDARD 99
        C_EXTENSIONS ON
    )

    set(candidate reduction_${base_name}_omp)
    add_ppcg_program(${candidate} ${source} ${output_dir}
        "--target=c;--openmp" ${main})
    target_link_libraries(${candidate} PRIVATE OpenMP::OpenMP_C)

    # Once with a thread count that divides the trip count evenly and
    # once with one that does not, since an accumulator that is combined
    # wrongly tends to show up at the boundaries rather than in the
    # middle.
    #
    # A tolerant comparison is one that allows the last digits to differ:
    # the partial results are put back together in a different order than
    # the sequential program computes them in, and floating point
    # addition is not associative.  That is the trade every compiler
    # makes for a reduction; what would not be tolerable, and what this
    # test is looking for, is a lost update, which is off by whole terms.
    foreach(threads 8 3)
        # A directory of its own for each thread count, because the two
        # tests may be run at the same time and the driver names the
        # captured output after the program rather than after the test.
        set(threads_dir ${output_dir}/threads_${threads})
        file(MAKE_DIRECTORY ${threads_dir})
        add_test(NAME Reduction_run_${base_name}_${threads}
            COMMAND ${CMAKE_COMMAND}
                -DREFERENCE=$<TARGET_FILE:${reference}>
                -DCANDIDATE=$<TARGET_FILE:${candidate}>
                -DOUTPUT_DIR=${threads_dir}
                "-DCOMPARE=${Python3_EXECUTABLE}|${COMPARE_OUTPUTS}"
                -DCOMPARE_ARGS=${tolerant}
                -P ${RUN_AND_COMPARE}
        )
        set_tests_properties(Reduction_run_${base_name}_${threads} PROPERTIES
            ENVIRONMENT "OMP_NUM_THREADS=${threads}")
    endforeach()
endfunction()

# What the accumulations do to the code generated without --openmp:
# nothing.  The dependences they carry are only left out of the test that
# asks whether a loop can be run in parallel, and that test is only made
# when the loops are going to be marked.
add_test(NAME Reduction_codegen_sum_plain
    COMMAND ${CMAKE_COMMAND}
        -DPPCG=$<TARGET_FILE:ppcg>
        -DSOURCE=${REDUCTION_RUN_DIR}/sum.c
        -DBINDIR=${CMAKE_CURRENT_BINARY_DIR}/tests/reduction/run/sum/plain
        -DEXPECTED=${REDUCTION_RUN_DIR}/sum.plain
        -DOPTIONS=--target=c
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunReductionCodegenTest.cmake
)

add_reduction_run_test(sum TRUE)
add_reduction_run_test(chained FALSE)
add_reduction_run_test(conditional TRUE)
add_reduction_run_test(histogram FALSE)
add_reduction_run_test(operators FALSE)
add_reduction_run_test(matvec TRUE)
add_reduction_run_test(scatter TRUE)
add_reduction_run_test(sections FALSE)
add_reduction_run_test(slice TRUE)
add_reduction_run_test(parametric FALSE)
add_reduction_run_test(pointers FALSE)
add_reduction_run_test(narrowing FALSE)
add_reduction_run_test(fused FALSE)
add_reduction_run_test(aliased TRUE)

#--------------------------------------------------------------------
# C target tests
#
# These need no accelerator runtime, so they are the tests that always
# run and they are what keeps a bare "ctest" honest about ppcg itself.
#
# CONFIGURE_DEPENDS so that a newly added program becomes a test on the next
# build instead of only after a manual reconfigure.  The "*.c" pattern also
# keeps the tests/*_opencl_functions.cl kernel fragments out of the corpus:
# they are inputs to a translation, not programs.
#--------------------------------------------------------------------

file(GLOB EXAMPLE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.c")
file(GLOB TEST_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.c")

# Translate an example to plain C and check that it still computes
# the same thing as the untranslated program.
function(add_c_example_test variant source ppcg_options)
    get_filename_component(base_name ${source} NAME_WE)
    set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/examples/${variant}")
    file(MAKE_DIRECTORY ${output_dir})

    set(reference ${base_name}_ref_${variant})
    add_executable(${reference} ${source})
    set_target_properties(${reference} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${output_dir}
        C_STANDARD 99
        C_EXTENSIONS ON
    )
    target_link_libraries(${reference} PRIVATE m)

    add_ppcg_program(${base_name}_${variant} ${source} ${output_dir}
        "${ppcg_options}")
    target_link_libraries(${base_name}_${variant} PRIVATE m)
    if(variant STREQUAL "openmp")
        target_link_libraries(${base_name}_${variant} PRIVATE OpenMP::OpenMP_C)
    endif()

    # chemv accumulates its floating point results in a different order
    # once the loops have been restructured.
    if(base_name STREQUAL "chemv")
        set(tolerant TRUE)
    else()
        set(tolerant FALSE)
    endif()

    add_test(NAME CExample_${variant}_${base_name}
        COMMAND ${CMAKE_COMMAND}
            -DREFERENCE=$<TARGET_FILE:${reference}>
            -DCANDIDATE=$<TARGET_FILE:${base_name}_${variant}>
            -DOUTPUT_DIR=${output_dir}
            "-DCOMPARE=${Python3_EXECUTABLE}|${COMPARE_OUTPUTS}"
            -DCOMPARE_ARGS=${tolerant}
            -P ${RUN_AND_COMPARE}
    )
endfunction()

foreach(example_file ${EXAMPLE_FILES})
    add_c_example_test(tile ${example_file} "--target=c;--tile")
    add_c_example_test(live ${example_file}
        "--target=c;--no-live-range-reordering;--tile")
    if(OpenMP_C_FOUND)
        add_c_example_test(openmp ${example_file} "--target=c;--openmp")
    endif()
endforeach()

if(NOT OpenMP_C_FOUND)
    message(STATUS "OpenMP not found. OpenMP tests will be skipped.")
endif()

# Translate a regression test to plain C, build it and check that it runs.
# This is the assertion the autotools opencl_test.sh made about the same
# corpus: nothing is claimed about the output, because each program verifies
# its own result and returns EXIT_FAILURE when the translation went wrong.
function(add_c_test source)
    get_filename_component(base_name ${source} NAME_WE)
    set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/tests/c")
    set(target c_${base_name})

    add_ppcg_program(${target} ${source} ${output_dir} "--target=c;--tile")

    # Some tests call a function that is only defined as an OpenCL kernel
    # helper.  Its body is plain C apart from the address space qualifier,
    # so the same file provides the host definition the linker needs.
    set(functions_file
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/${base_name}_opencl_functions.cl")
    if(EXISTS ${functions_file})
        set(host_functions "${output_dir}/${base_name}_functions.c")
        configure_file(${functions_file} ${host_functions} COPYONLY)
        target_sources(${target} PRIVATE ${host_functions})
        target_compile_definitions(${target} PRIVATE __global=)
    endif()

    add_test(NAME CTest_${base_name} COMMAND $<TARGET_FILE:${target}>)
endfunction()

foreach(test_file ${TEST_FILES})
    add_c_test(${test_file})
endforeach()

#--------------------------------------------------------------------
# OpenCL tests
#--------------------------------------------------------------------

if(OpenCL_FOUND)
    # Translate a test to OpenCL, build it and check that it runs.
    function(add_opencl_test variant source extra_ppcg_options)
        get_filename_component(base_name ${source} NAME_WE)
        set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/tests/${variant}")
        set(target opencl_${variant}_${base_name})

        set(ppcg_options --target=opencl --opencl-no-use-gpu)

        # Tests may come with OpenCL helper functions to embed in the kernel.
        set(functions_file
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/${base_name}_opencl_functions.cl")
        if(EXISTS ${functions_file})
            file(MAKE_DIRECTORY ${output_dir})
            configure_file(${functions_file}
                ${output_dir}/${base_name}_opencl_functions.cl COPYONLY)
            list(APPEND ppcg_options
                --opencl-include-file=${output_dir}/${base_name}_opencl_functions.cl
                --opencl-compiler-options=-I${output_dir})
        endif()

        list(APPEND ppcg_options ${extra_ppcg_options})

        add_ppcg_program(${target} ${source} ${output_dir} "${ppcg_options}"
            ${CMAKE_CURRENT_SOURCE_DIR}/ocl_utilities.c)
        target_link_libraries(${target} PRIVATE OpenCL::OpenCL)

        add_test(NAME OpenCL_${variant}_${base_name}
            COMMAND $<TARGET_FILE:${target}>
            WORKING_DIRECTORY ${output_dir}
        )
    endfunction()

    foreach(test_file ${TEST_FILES})
        add_opencl_test(default ${test_file} "")
        add_opencl_test(embed ${test_file} --opencl-embed-kernel-code)
    endforeach()

    # The examples are checked by comparing their output against a plain
    # compilation of the same source.
    function(add_opencl_example_test source)
        get_filename_component(base_name ${source} NAME_WE)
        set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/examples")
        file(MAKE_DIRECTORY ${output_dir})

        add_executable(${base_name}_ref ${source})
        set_target_properties(${base_name}_ref PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${output_dir}
            C_STANDARD 99
            C_EXTENSIONS ON
        )

        add_ppcg_program(${base_name}_ppcg ${source} ${output_dir}
            "--target=opencl;--opencl-no-use-gpu"
            ${CMAKE_CURRENT_SOURCE_DIR}/ocl_utilities.c)
        target_link_libraries(${base_name}_ppcg PRIVATE OpenCL::OpenCL)

        # chemv accumulates floating point results in a different order, so
        # its output is compared with a tolerance.
        if(base_name STREQUAL "chemv")
            set(tolerant TRUE)
        else()
            set(tolerant FALSE)
        endif()

        add_test(NAME OpenCLExample_${base_name}
            COMMAND ${CMAKE_COMMAND}
                -DREFERENCE=$<TARGET_FILE:${base_name}_ref>
                -DCANDIDATE=$<TARGET_FILE:${base_name}_ppcg>
                -DOUTPUT_DIR=${output_dir}
                "-DCOMPARE=${Python3_EXECUTABLE}|${COMPARE_OUTPUTS}"
                -DCOMPARE_ARGS=${tolerant}
                -P ${RUN_AND_COMPARE}
        )
    endfunction()

    foreach(example_file ${EXAMPLE_FILES})
        add_opencl_example_test(${example_file})
    endforeach()
else()
    message(STATUS "OpenCL not found. OpenCL tests will be skipped.")
endif()

#--------------------------------------------------------------------
# Polybench tests
#--------------------------------------------------------------------

if(POLYBENCH_DIR AND EXISTS "${POLYBENCH_DIR}/utilities/benchmark_list")
    message(STATUS "Polybench directory found at ${POLYBENCH_DIR}. Polybench tests are enabled.")

    file(STRINGS "${POLYBENCH_DIR}/utilities/benchmark_list" BENCHMARK_LIST)

    set(POLYBENCH_DEFINITIONS
        POLYBENCH_USE_C99_PROTO
        POLYBENCH_DUMP_ARRAYS
        MINI_DATASET
    )

    # Translate every benchmark with the given ppcg options and check that
    # the result still produces the same arrays as the original.
    function(add_polybench_test variant ppcg_options)
        set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/polybench/${variant}")
        file(MAKE_DIRECTORY ${output_dir})

        foreach(bench_path ${BENCHMARK_LIST})
            get_filename_component(bench_name ${bench_path} NAME_WE)
            get_filename_component(bench_dir ${bench_path} DIRECTORY)
            set(bench_source "${POLYBENCH_DIR}/${bench_path}")

            set(orig ${bench_name}_orig_${variant})
            add_executable(${orig}
                ${bench_source} ${POLYBENCH_DIR}/utilities/polybench.c)
            target_include_directories(${orig} PRIVATE
                ${POLYBENCH_DIR}/${bench_dir} ${POLYBENCH_DIR}/utilities)
            target_compile_definitions(${orig} PRIVATE ${POLYBENCH_DEFINITIONS})
            target_link_libraries(${orig} PRIVATE m)
            set_target_properties(${orig} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY ${output_dir}
                C_STANDARD 99
                C_EXTENSIONS ON
            )

            set(opt ${bench_name}_${variant})
            add_ppcg_program(${opt} ${bench_source} ${output_dir}
                "-I;${POLYBENCH_DIR}/${bench_dir};-I;${POLYBENCH_DIR}/utilities;-DPOLYBENCH_USE_C99_PROTO;-DPOLYBENCH_DUMP_ARRAYS;-DMINI_DATASET;${ppcg_options}"
                ${POLYBENCH_DIR}/utilities/polybench.c)
            target_include_directories(${opt} PRIVATE
                ${POLYBENCH_DIR}/${bench_dir} ${POLYBENCH_DIR}/utilities)
            target_compile_definitions(${opt} PRIVATE ${POLYBENCH_DEFINITIONS})
            target_link_libraries(${opt} PRIVATE m)

            if(variant STREQUAL "omp")
                target_link_libraries(${opt} PRIVATE OpenMP::OpenMP_C)
            elseif(variant STREQUAL "opencl")
                target_sources(${opt} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ocl_utilities.c)
                target_link_libraries(${opt} PRIVATE OpenCL::OpenCL)
            endif()

            # The benchmarks dump their arrays to stderr.
            add_test(NAME Polybench_${variant}_${bench_name}
                COMMAND ${CMAKE_COMMAND}
                    -DREFERENCE=$<TARGET_FILE:${orig}>
                    -DCANDIDATE=$<TARGET_FILE:${opt}>
                    -DOUTPUT_DIR=${output_dir}
                    -DSTREAM=stderr
                    "-DCOMPARE=${CMAKE_COMMAND}|-E|compare_files"
                    -P ${RUN_AND_COMPARE}
            )
        endforeach()
    endfunction()

    add_polybench_test(c "--target=c;--tile")
    add_polybench_test(live "--target=c;--no-live-range-reordering;--tile")

    if(OpenMP_C_FOUND)
        add_polybench_test(omp "--target=c;--openmp")
    else()
        message(STATUS "OpenMP not found. Polybench OpenMP tests will be skipped.")
    endif()

    if(OpenCL_FOUND)
        add_polybench_test(opencl "--target=opencl;--opencl-no-use-gpu")
    endif()
else()
    message(STATUS "POLYBENCH_DIR is not set. Polybench tests will be skipped.")
endif()

endif()
