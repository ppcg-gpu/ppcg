# Translate one source file with ppcg, from inside the test.
#
# The ordinary C target tests run ppcg as a build step, so a ppcg that
# fails to terminate would hang the build and no test timeout would ever
# fire.  This driver runs ppcg as part of the test instead, so that
# set_tests_properties(... TIMEOUT) bounds it.
#
#   PPCG         the ppcg executable
#   SOURCE       the C file to translate
#   OUTPUT       where to place the generated code
#   TIMEOUT_S    seconds ppcg itself may run before it is killed
#   OPTIONS      optional extra ppcg options, '|' separated
#   EXPECT_FILE  optional file of regular expressions, one per line, each
#                of which the generated code has to match
#
# The translation has to finish within the timeout and has to leave a
# non-empty file behind.  What the generated code computes is checked by
# the program that includes it, not here.
#
# EXPECT_FILE is for the claims a running program cannot make.  A write
# that ppcg wrongly eliminates because two arrays share storage leaves
# the same bytes behind either way -- the array it was dropped from is
# covered by the other one, so a reference and a candidate agree while
# the caller has lost an output.  What changed is the translation, so
# that is what is read.

foreach(required PPCG SOURCE OUTPUT TIMEOUT_S)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "RunTranslate: ${required} is required")
  endif()
endforeach()

get_filename_component(output_dir "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${output_dir}")

set(command "${PPCG}" --target=c --pet-autodetect)
if(DEFINED OPTIONS)
  string(REPLACE "|" ";" options "${OPTIONS}")
  list(APPEND command ${options})
endif()
list(APPEND command "${SOURCE}" -o "${OUTPUT}")

# A cell that claims something about the dependences has to ask for them.
# PPCG_DEBUG_DEPS makes ppcg print reads, writes, dep_flow and dep_false
# before it schedules; without it those lines are not there to match.
set(environment)
if(DEFINED EXPECT_DEPS_FILE AND NOT "${EXPECT_DEPS_FILE}" STREQUAL "")
  set(environment ${CMAKE_COMMAND} -E env PPCG_DEBUG_DEPS=1)
endif()

execute_process(
  COMMAND ${environment} ${command}
  RESULT_VARIABLE result
  TIMEOUT ${TIMEOUT_S}
  OUTPUT_VARIABLE stdout
  ERROR_VARIABLE stderr
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR
    "ppcg did not translate ${SOURCE} within ${TIMEOUT_S}s "
    "(exit ${result}):\n${stdout}${stderr}")
endif()

if(NOT EXISTS "${OUTPUT}")
  message(FATAL_ERROR "ppcg produced no output file ${OUTPUT}")
endif()
file(SIZE "${OUTPUT}" size)
if(size EQUAL 0)
  message(FATAL_ERROR "ppcg produced an empty ${OUTPUT}")
endif()

if(DEFINED EXPECT_FILE AND NOT "${EXPECT_FILE}" STREQUAL "")
  if(NOT EXISTS "${EXPECT_FILE}")
    message(FATAL_ERROR "RunTranslate: no such EXPECT_FILE ${EXPECT_FILE}")
  endif()
  file(READ "${OUTPUT}" generated)
  file(READ "${SOURCE}" original)
  file(STRINGS "${EXPECT_FILE}" expectations)
  foreach(expectation IN LISTS expectations)
    if("${expectation}" STREQUAL "" OR "${expectation}" MATCHES "^#")
      continue()
    endif()
    if(original MATCHES "${expectation}")
      message(FATAL_ERROR
        "'${expectation}' is already in ${SOURCE}, so it says nothing "
        "about what ppcg did with it.  ppcg copies a source comment into "
        "the file it writes, and an expectation the commentary can "
        "satisfy passes whatever the translation turned out to be.")
    endif()
    if(NOT generated MATCHES "${expectation}")
      message(FATAL_ERROR
        "the translation of ${SOURCE} has nothing matching "
        "'${expectation}'\n${stdout}${stderr}")
    endif()
  endforeach()
endif()

# What ppcg SAID about the translation, rather than what it wrote.
#
# Some claims are not in the generated code at all: which array a lost
# write is charged to is a name on stderr, and the whole point of
# charging it correctly is that a reader can act on the name.  Those
# expectations are read from ppcg's own output.
if(DEFINED EXPECT_SAYS_FILE AND NOT "${EXPECT_SAYS_FILE}" STREQUAL "")
  if(NOT EXISTS "${EXPECT_SAYS_FILE}")
    message(FATAL_ERROR
      "RunTranslate: no such EXPECT_SAYS_FILE ${EXPECT_SAYS_FILE}")
  endif()
  file(STRINGS "${EXPECT_SAYS_FILE}" said)
  foreach(expectation IN LISTS said)
    if("${expectation}" STREQUAL "" OR "${expectation}" MATCHES "^#")
      continue()
    endif()
    if(NOT "${stdout}${stderr}" MATCHES "${expectation}")
      message(FATAL_ERROR
        "translating ${SOURCE}, ppcg said nothing matching "
        "'${expectation}'\n${stdout}${stderr}")
    endif()
  endforeach()
endif()

# What the DEPENDENCES came out as.
#
# A relation is neither in the generated code nor in an ordinary report:
# an anti-dependence that ppcg failed to build leaves no trace except an
# empty dep_false and, later, a fusion nobody forbade.  The claims here
# are matched against the dump PPCG_DEBUG_DEPS asks for, which the run
# above turned on for exactly these cells.
if(DEFINED EXPECT_DEPS_FILE AND NOT "${EXPECT_DEPS_FILE}" STREQUAL "")
  if(NOT EXISTS "${EXPECT_DEPS_FILE}")
    message(FATAL_ERROR
      "RunTranslate: no such EXPECT_DEPS_FILE ${EXPECT_DEPS_FILE}")
  endif()
  file(READ "${SOURCE}" original)
  file(STRINGS "${EXPECT_DEPS_FILE}" claimed)
  foreach(expectation IN LISTS claimed)
    if("${expectation}" STREQUAL "" OR "${expectation}" MATCHES "^#")
      continue()
    endif()
    if(original MATCHES "${expectation}")
      message(FATAL_ERROR
        "'${expectation}' is already in ${SOURCE}, so it says nothing "
        "about the dependences ppcg built from it.")
    endif()
    if(NOT "${stdout}${stderr}" MATCHES "${expectation}")
      message(FATAL_ERROR
        "in the dependences of ${SOURCE}, nothing matches "
        "'${expectation}'\n${stdout}${stderr}")
    endif()
  endforeach()
endif()
