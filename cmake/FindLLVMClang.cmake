# FindLLVMClang.cmake
# Finds LLVM and Clang installations intelligently
#
# Input variables:
#   LLVM_VERSION - Specific LLVM version to use (e.g. "17")
#
# Output variables:
#   LLVM_FOUND - True if LLVM was found
#   LLVM_INCLUDE_DIRS - LLVM include directories
#   LLVM_DEFINITIONS - LLVM compiler definitions
#   LLVM_PACKAGE_VERSION - LLVM version
#   LLVM_DIR - Location of LLVM CMake modules
#   Clang_FOUND - True if Clang was found  
#   CLANG_INCLUDE_DIRS - Clang include directories
#   CLANG_VERSION - Clang version

# Add an option for manually specifying LLVM version if not already defined
if(NOT DEFINED LLVM_VERSION)
  set(LLVM_VERSION "" CACHE STRING "Specific LLVM version to use (e.g. 17)")
endif()

# Function to find highest available LLVM version
function(find_highest_llvm_version out_version)
    # Common locations for llvm-config
    set(search_paths 
        "/usr/bin"
        "/usr/local/bin"
        "/opt/local/bin"
        "/opt/homebrew/bin"
        "/usr/lib/llvm/bin"
    )
    
    # Try to find plain llvm-config first
    find_program(LLVM_CONFIG_EXEC llvm-config PATHS ${search_paths})
    
    if(LLVM_CONFIG_EXEC)
        # Get version from llvm-config
        execute_process(
            COMMAND ${LLVM_CONFIG_EXEC} --version
            OUTPUT_VARIABLE LLVM_FOUND_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        # Extract major version number
        string(REGEX MATCH "^([0-9]+)" LLVM_MAJOR_VERSION ${LLVM_FOUND_VERSION})
        set(${out_version} ${LLVM_MAJOR_VERSION} PARENT_SCOPE)
        return()
    endif()
    
    # If llvm-config not found, look for versioned variants
    set(highest_version 0)
    
    # Check common version range
    foreach(version RANGE 11 21)
        find_program(LLVM_CONFIG_${version} llvm-config-${version} PATHS ${search_paths})
        if(LLVM_CONFIG_${version})
            if(version GREATER highest_version)
                set(highest_version ${version})
            endif()
        endif()
        unset(LLVM_CONFIG_${version} CACHE)
    endforeach()
    
    if(highest_version GREATER 0)
        set(${out_version} ${highest_version} PARENT_SCOPE)
    else()
        set(${out_version} "" PARENT_SCOPE)
    endif()
endfunction()

# Main LLVM/Clang detection logic
if(LLVM_VERSION)
    message(STATUS "Using specified LLVM version: ${LLVM_VERSION}")
    set(DETECTED_LLVM_VERSION ${LLVM_VERSION})
else()
    # Auto-detect highest available version
    find_highest_llvm_version(DETECTED_LLVM_VERSION)
    if(DETECTED_LLVM_VERSION)
        message(STATUS "Detected LLVM version: ${DETECTED_LLVM_VERSION}")
    else()
        message(STATUS "No specific LLVM version detected, will try default paths")
    endif()
endif()

# Set up potential LLVM paths based on detected or specified version
set(POTENTIAL_LLVM_PATHS "")
if(DETECTED_LLVM_VERSION)
    # Common installation paths for specific LLVM versions
    list(APPEND POTENTIAL_LLVM_PATHS 
        "/usr/lib/llvm-${DETECTED_LLVM_VERSION}/lib/cmake"
        "/usr/lib/llvm-${DETECTED_LLVM_VERSION}/cmake"
        "/usr/local/opt/llvm@${DETECTED_LLVM_VERSION}/lib/cmake"
        "/opt/homebrew/opt/llvm@${DETECTED_LLVM_VERSION}/lib/cmake"
        "/usr/lib/cmake/llvm-${DETECTED_LLVM_VERSION}"
        "/usr/local/lib/cmake/llvm-${DETECTED_LLVM_VERSION}"
    )
endif()

# First try with specific paths if available
if(POTENTIAL_LLVM_PATHS)
    find_package(LLVM CONFIG PATHS ${POTENTIAL_LLVM_PATHS} NO_DEFAULT_PATH)
    if(NOT LLVM_FOUND)
        message(STATUS "LLVM not found in version-specific paths, trying default locations")
    endif()
endif()

# If not found with specific paths, try default paths
if(NOT LLVM_FOUND)
    find_package(LLVM CONFIG)
endif()

# Handle LLVM detection result
if(LLVM_FOUND)
    message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} at ${LLVM_DIR}")
    
    # Now find Clang using same paths as LLVM
    get_filename_component(LLVM_CMAKE_DIR "${LLVM_DIR}" DIRECTORY)
    find_package(Clang CONFIG REQUIRED HINTS "${LLVM_CMAKE_DIR}")
    
    if(Clang_FOUND)
        message(STATUS "Found Clang ${CLANG_VERSION} at ${Clang_DIR}")
    else()
        message(FATAL_ERROR "Found LLVM but could not find matching Clang")
    endif()
else()
    message(FATAL_ERROR "Could not find LLVM. Please install LLVM and Clang or specify LLVM_VERSION.")
endif()

# For CMake's find_package system, set the standard PACKAGE_FOUND variable
# No need to export variables to parent scope when using include() or find_package()
# Variables set in this file will be visible to the calling scope
