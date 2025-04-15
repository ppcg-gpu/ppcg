cmake_minimum_required(VERSION 3.15)

project(ppcg LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(GMP REQUIRED)

# Use our custom module to find LLVM and Clang
# There are two ways to use the module:
# 1. Using find_package (preferred CMake way)
find_package(LLVMClang REQUIRED)

# Set up include directories and definitions now that LLVM and Clang are found
include_directories(${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

include(ExternalProject)

ExternalProject_Add(isl
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/isl
    CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/isl/configure --prefix=<INSTALL_DIR>
    BUILD_COMMAND make
    INSTALL_COMMAND make install
)

ExternalProject_Add(pet
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/pet
    CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/pet/configure --prefix=<INSTALL_DIR>
    BUILD_COMMAND make
    INSTALL_COMMAND make install
)

ExternalProject_Get_Property(isl install_dir)
include_directories(${install_dir}/include)
ExternalProject_Get_Property(pet install_dir)
include_directories(${install_dir}/include)

# Define the executable
add_executable(${PROJECT_NAME}
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
    print.c
    util.c
    version.c
    dummy.cpp
)

# Link libraries
target_link_libraries(${PROJECT_NAME} ${install_dir}/lib/libpet.a ${install_dir}/lib/libisl.a libclang clang-cpp LLVM GMP::gmp)

# Find necessary components
find_package(OpenCL QUIET)
find_package(OpenMP QUIET)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Rest of your CMakeLists.txt content continues here...
# Enable testing
enable_testing()

# Copy the Python comparison script to the build directory
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/compare_outputs.py
    ${CMAKE_CURRENT_BINARY_DIR}/compare_outputs.py
    COPYONLY
)
file(CHMOD ${CMAKE_CURRENT_BINARY_DIR}/compare_outputs.py
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

# Create output directories
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/examples")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/polybench")

#--------------------------------------------------------------------
# OpenCL Test Framework
#--------------------------------------------------------------------

# OpenCL test utility function
function(add_opencl_test test_name source_file ppcg_options)
    # Get base name without extension
    get_filename_component(base_name ${source_file} NAME_WE)

    # Output filenames
    set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/tests/${test_name}")
    set(output_c "${output_dir}/${base_name}.ppcg.c")
    set(output_exe "${output_dir}/${base_name}.ppcg${CMAKE_EXECUTABLE_SUFFIX}")

    # Create output directory
    file(MAKE_DIRECTORY ${output_dir})

    # Check for optional OpenCL functions file
    set(functions_file "${CMAKE_CURRENT_SOURCE_DIR}/tests/${base_name}_opencl_functions.cl")
    if(EXISTS ${functions_file})
        # Copy the functions file to the output directory
        configure_file(${functions_file} ${output_dir}/${base_name}_opencl_functions.cl COPYONLY)
        set(additional_options "--opencl-include-file=${output_dir}/${base_name}_opencl_functions.cl")
        set(compiler_options "-I${output_dir}")
    else()
        set(additional_options "")
        set(compiler_options "")
    endif()

    # Target name for this specific test
    set(target_name opencl_test_${test_name}_${base_name})

    # Add custom command to generate OpenCL code
    add_custom_command(
        OUTPUT ${output_c}
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}
            --target=opencl
            --opencl-no-use-gpu
            ${additional_options}
            --opencl-compiler-options=${compiler_options}
            ${ppcg_options}
            ${source_file}
            -o ${output_c}
        DEPENDS ${PROJECT_NAME} ${source_file}
        COMMENT "Generating OpenCL code for ${base_name} with options '${test_name}'"
        VERBATIM
    )

    # Add custom command to compile the generated code
    add_custom_command(
        OUTPUT ${output_exe}
        COMMAND ${CMAKE_C_COMPILER}
            --std=gnu99
            -I "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/ocl_utilities.c"
            ${compiler_options}
            ${output_c}
            -o ${output_exe}
            -lOpenCL
        DEPENDS ${output_c}
        COMMENT "Compiling OpenCL test ${base_name} with options '${test_name}'"
        VERBATIM
    )

    # Add custom target to run the test
    add_custom_target(${target_name}
        COMMAND ${output_exe}
        DEPENDS ${output_exe}
        COMMENT "Running OpenCL test ${base_name} with options '${test_name}'"
        VERBATIM
    )

    # Add to test suite
    add_test(
        NAME OpenCLTest_${test_name}_${base_name}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${target_name}
    )
endfunction()

# Example test function
function(add_opencl_example_test example_file)
    # Get base name without extension
    get_filename_component(base_name ${example_file} NAME_WE)

    # Output directory
    set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/examples")

    # Output filenames
    set(exe_ref "${output_dir}/${base_name}.ref${CMAKE_EXECUTABLE_SUFFIX}")
    set(gen_ocl "${output_dir}/${base_name}.ppcg.c")
    set(exe_ocl "${output_dir}/${base_name}.ppcg${CMAKE_EXECUTABLE_SUFFIX}")
    set(output_ref "${output_dir}/${base_name}.ref.out")
    set(output_ocl "${output_dir}/${base_name}.ppcg.out")

    # Target name for this specific example test
    set(target_name opencl_example_${base_name})

    # Add custom command to compile reference
    add_custom_command(
        OUTPUT ${exe_ref}
        COMMAND ${CMAKE_C_COMPILER} --std=gnu99 ${example_file} -o ${exe_ref}
        DEPENDS ${example_file}
        COMMENT "Compiling reference example ${base_name}"
        VERBATIM
    )

    # Add custom command to generate reference output
    add_custom_command(
        OUTPUT ${output_ref}
        COMMAND ${exe_ref} > ${output_ref}
        DEPENDS ${exe_ref}
        COMMENT "Generating reference output for example ${base_name}"
        VERBATIM
    )

    # Add custom command to generate OpenCL code
    add_custom_command(
        OUTPUT ${gen_ocl}
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}
            --target=opencl
            --opencl-no-use-gpu
            ${example_file}
            -o ${gen_ocl}
        DEPENDS ${PROJECT_NAME} ${example_file}
        COMMENT "Generating OpenCL code for example ${base_name}"
        VERBATIM
    )

    # Add custom command to compile generated code
    add_custom_command(
        OUTPUT ${exe_ocl}
        COMMAND ${CMAKE_C_COMPILER}
            --std=gnu99
            -I "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/ocl_utilities.c"
            ${gen_ocl}
            -o ${exe_ocl}
            -lOpenCL
        DEPENDS ${gen_ocl}
        COMMENT "Compiling OpenCL example ${base_name}"
        VERBATIM
    )

    # Add custom command to generate OpenCL output
    add_custom_command(
        OUTPUT ${output_ocl}
        COMMAND ${exe_ocl} > ${output_ocl}
        DEPENDS ${exe_ocl}
        COMMENT "Generating OpenCL output for example ${base_name}"
        VERBATIM
    )

    # Add custom target to compare outputs
    add_custom_target(${target_name}
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_BINARY_DIR}/compare_outputs.py
            ${output_ref}
            ${output_ocl}
            $<STREQUAL:${base_name},chemv>
        DEPENDS ${output_ref} ${output_ocl}
        COMMENT "Comparing outputs for example ${base_name}"
        VERBATIM
    )

    # Add to test suite
    add_test(
        NAME OpenCLExampleTest_${base_name}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${target_name}
    )
endfunction()

# Get all test files
file(GLOB TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.c")

# Add default tests
foreach(test_file ${TEST_FILES})
    add_opencl_test("default" ${test_file} "")
endforeach()

# Add embed tests
foreach(test_file ${TEST_FILES})
    add_opencl_test("embed" ${test_file} "--opencl-embed-kernel-code")
endforeach()

# Get all example files
file(GLOB EXAMPLE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.c")

# Add example tests
foreach(example_file ${EXAMPLE_FILES})
    add_opencl_example_test(${example_file})
endforeach()

# Add convenience targets to run all OpenCL tests
add_custom_target(opencl_test_all)

# Add dependencies to the main target
foreach(test_file ${TEST_FILES})
    get_filename_component(base_name ${test_file} NAME_WE)
    add_dependencies(opencl_test_all opencl_test_default_${base_name} opencl_test_embed_${base_name})
endforeach()

# Add convenience target for all example tests
add_custom_target(opencl_example_test_all)

foreach(example_file ${EXAMPLE_FILES})
    get_filename_component(base_name ${example_file} NAME_WE)
    add_dependencies(opencl_example_test_all opencl_example_${base_name})
endforeach()

# Add a master target to run all OpenCL tests
add_custom_target(opencl_all_tests DEPENDS opencl_test_all opencl_example_test_all)

# Main OpenCL test target
add_test(
    NAME OpenCLTests
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target opencl_all_tests
)

#--------------------------------------------------------------------
# Polybench Test Framework
#--------------------------------------------------------------------

# Function to check if a variable is defined
function(is_defined var)
    if(DEFINED "${var}" AND NOT "${${var}}" STREQUAL "")
        set(${var}_DEFINED TRUE PARENT_SCOPE)
    else()
        set(${var}_DEFINED FALSE PARENT_SCOPE)
    endif()
endfunction()

# Check for POLYBENCH_DIR
is_defined(POLYBENCH_DIR)
if(NOT POLYBENCH_DIR_DEFINED)
    set(POLYBENCH_DIR "" CACHE PATH "Path to Polybench directory")
endif()

if(POLYBENCH_DIR AND EXISTS "${POLYBENCH_DIR}")
    message(STATUS "Polybench directory found at ${POLYBENCH_DIR}. Polybench tests are enabled.")

    # Function to add a Polybench test
    function(add_polybench_test test_name ppcg_options cc_options)
        # Create output directory
        set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/polybench/${test_name}")
        file(MAKE_DIRECTORY ${output_dir})

        # Read benchmark list
        file(STRINGS "${POLYBENCH_DIR}/utilities/benchmark_list" BENCHMARK_LIST)

        # Common flags
        set(CPPFLAGS "-DPOLYBENCH_USE_C99_PROTO -DPOLYBENCH_DUMP_ARRAYS -DMINI_DATASET -I${POLYBENCH_DIR}/utilities")
        set(CFLAGS "-lm --std=gnu99")

        # Target name for this specific test
        set(target_name polybench_test_${test_name})
        add_custom_target(${target_name})

        foreach(bench_path ${BENCHMARK_LIST})
            # Get the benchmark name
            get_filename_component(bench_name ${bench_path} NAME_WE)
            get_filename_component(bench_dir ${bench_path} DIRECTORY)

            # Full path to the benchmark source
            set(bench_source "${POLYBENCH_DIR}/${bench_path}")

            # Output files
            set(source_opt "${output_dir}/${bench_name}.${test_name}.c")
            set(prog_orig "${output_dir}/${bench_name}.orig${CMAKE_EXECUTABLE_SUFFIX}")
            set(prog_opt "${output_dir}/${bench_name}.${test_name}${CMAKE_EXECUTABLE_SUFFIX}")
            set(output_orig "${output_dir}/${bench_name}.orig.out")
            set(output_opt "${output_dir}/${bench_name}.${test_name}.out")

            # Generate optimized source with PPCG
            add_custom_command(
                OUTPUT ${source_opt}
                COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}
                    -I ${POLYBENCH_DIR}/${bench_dir}
                    ${bench_source}
                    ${CPPFLAGS}
                    -o ${source_opt}
                    ${ppcg_options}
                DEPENDS ${PROJECT_NAME} ${bench_source}
                COMMENT "Generating optimized code for ${bench_name} with options '${test_name}'"
                VERBATIM
            )

            # Compile original benchmark
            add_custom_command(
                OUTPUT ${prog_orig}
                COMMAND ${CMAKE_C_COMPILER}
                    -I ${POLYBENCH_DIR}/${bench_dir}
                    ${CPPFLAGS}
                    ${bench_source}
                    -o ${prog_orig}
                    ${POLYBENCH_DIR}/utilities/polybench.c
                    ${CFLAGS}
                DEPENDS ${bench_source}
                COMMENT "Compiling original benchmark ${bench_name}"
                VERBATIM
            )

            # Generate original output
            add_custom_command(
                OUTPUT ${output_orig}
                COMMAND ${prog_orig} 2> ${output_orig}
                DEPENDS ${prog_orig}
                COMMENT "Running original benchmark ${bench_name}"
                VERBATIM
            )

            # Compile optimized benchmark
            add_custom_command(
                OUTPUT ${prog_opt}
                COMMAND ${CMAKE_C_COMPILER}
                    -I ${POLYBENCH_DIR}/${bench_dir}
                    ${CPPFLAGS}
                    ${source_opt}
                    -o ${prog_opt}
                    ${POLYBENCH_DIR}/utilities/polybench.c
                    ${CFLAGS}
                    ${cc_options}
                DEPENDS ${source_opt}
                COMMENT "Compiling optimized benchmark ${bench_name} with options '${test_name}'"
                VERBATIM
            )

            # Generate optimized output
            add_custom_command(
                OUTPUT ${output_opt}
                COMMAND ${prog_opt} 2> ${output_opt}
                DEPENDS ${prog_opt}
                COMMENT "Running optimized benchmark ${bench_name} with options '${test_name}'"
                VERBATIM
            )

            # Individual benchmark test target
            set(bench_target polybench_${test_name}_${bench_name})
            add_custom_target(${bench_target}
                COMMAND ${CMAKE_COMMAND} -E compare_files ${output_orig} ${output_opt}
                DEPENDS ${output_orig} ${output_opt}
                COMMENT "Comparing outputs for benchmark ${bench_name} with options '${test_name}'"
                VERBATIM
            )

            # Add to the main test target
            add_dependencies(${target_name} ${bench_target})

            # Add to test suite
            add_test(
                NAME PolybenchTest_${test_name}_${bench_name}
                COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${bench_target}
            )
        endforeach()

        # Return the target name to the parent scope
        set(${test_name}_TARGET ${target_name} PARENT_SCOPE)
    endfunction()

    # Add Polybench tests with different options
    add_polybench_test("ppcg" "--target=c --tile" "")
    add_polybench_test("ppcg_live" "--target=c --no-live-range-reordering --tile" "")

    # Add OpenMP test if supported
    if(OpenMP_C_FOUND)
        add_polybench_test("ppcg_omp" "--target=c --openmp" "${OpenMP_C_FLAGS}")
        message(STATUS "OpenMP found. Polybench OpenMP tests are enabled.")
    else()
        message(STATUS "OpenMP not found. Polybench OpenMP tests will be skipped.")
    endif()

    # Always add OpenCL test - no need for if(OpenCL_FOUND) check now
    add_polybench_test("ppcg_opencl" "--target=opencl --opencl-no-use-gpu" "-I${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/ocl_utilities.c -lOpenCL")

    # Create a target for all Polybench tests
    add_custom_target(polybench_all_tests)
    add_dependencies(polybench_all_tests ${ppcg_TARGET} ${ppcg_live_TARGET})

    if(OpenMP_C_FOUND)
        add_dependencies(polybench_all_tests ${ppcg_omp_TARGET})
    endif()

    # Always add OpenCL dependency - no need for if check
    add_dependencies(polybench_all_tests ${ppcg_opencl_TARGET})

    # Main Polybench test
    add_test(
        NAME PolybenchTests
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target polybench_all_tests
    )
else()
    message(STATUS "Polybench directory not found. Polybench tests will be skipped.")
    message(STATUS "Set POLYBENCH_DIR to enable Polybench tests.")
endif()

# Create a target for all tests
add_custom_target(all_tests)

# Always add OpenCL dependency to all_tests - no need for if check
add_dependencies(all_tests opencl_all_tests)

if(POLYBENCH_DIR AND EXISTS "${POLYBENCH_DIR}")
    add_dependencies(all_tests polybench_all_tests)
endif()

# Test all target
add_test(
    NAME AllTests
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target all_tests
)
