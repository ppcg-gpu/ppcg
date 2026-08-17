# Compile through ccache.  Not optionally.
#
# Every compilation this project runs goes through ccache: the sources,
# the generated code the tests build, the probes that ask Clang what it
# provides, and the little programs CMake compiles to find out about the
# compiler.  A second configure of a fresh build directory then costs
# almost nothing, which matters here because the Clang feature probes
# parse headers that take ten seconds apiece.
#
# It is not a switch.  There is no option to turn it off, the cache
# entries are set with FORCE so that a launcher given on the command
# line does not quietly replace it, and ccache is REQUIRED, so a machine
# without it fails while configuring rather than silently spending the
# time.  Anyone who really means to compile without it can point
# CCACHE_DISABLE at ccache itself, which is a decision made out loud.
#
# CCACHE_BASEDIR rewrites the absolute paths under the build directory
# into relative ones and CCACHE_NOHASHDIR leaves the working directory
# out of the hash, so that two build directories of the same source
# share the cache instead of filling it twice.  It names the build
# directory rather than the source one because the source paths are
# already the same in both, while everything generated, and every -I
# that points at it, is what tells them apart.

include_guard(GLOBAL)

find_program(CCACHE_PROGRAM ccache REQUIRED
    DOC "ccache, which every compilation goes through")

# The variables have to reach the compiler at build time, not only while
# configuring, so they are carried by the launcher rather than set here.
set(CCACHE_LAUNCHER
    ${CMAKE_COMMAND} -E env
        "CCACHE_BASEDIR=${CMAKE_BINARY_DIR}"
        "CCACHE_NOHASHDIR=1"
        ${CCACHE_PROGRAM}
    CACHE STRING "How every compiler is launched" FORCE)

foreach(lang C CXX)
    set(CMAKE_${lang}_COMPILER_LAUNCHER ${CCACHE_LAUNCHER}
        CACHE STRING "Launcher for the ${lang} compiler" FORCE)
endforeach()

# try_compile builds in a project of its own, which only inherits what
# it is told to.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
    CMAKE_C_COMPILER_LAUNCHER CMAKE_CXX_COMPILER_LAUNCHER)

message(STATUS "Compiling through ${CCACHE_PROGRAM}")
