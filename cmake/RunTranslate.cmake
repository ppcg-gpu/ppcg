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

# Read a file of expectations, one per line.
#
# NOT file(STRINGS), which hands back a list: a semicolon inside a line
# is then a separator, and "for \(int c0 = 0; c0 <= 15; c0 \+= 1\)"
# arrives as three expectations that each match something harmless.  A
# cell written that way passes on the fragments and holds nothing.  The
# separator is escaped first and the split is on newlines only.
macro(read_expectations file out)
  file(READ "${file}" _content)
  string(REPLACE ";" "\\;" _content "${_content}")
  string(REPLACE "\n" ";" ${out} "${_content}")
endmacro()

# One expectation, against one body of text.
#
# A line beginning with "!" claims the OPPOSITE: that nothing matches.
# Some of what a fix buys is silence -- a report that stops naming an
# array the source subscribes in plain sight says nothing new, it stops
# saying something false -- and a harness that can only assert presence
# cannot hold that.
#
# The guards are the same either way.  An expectation the SOURCE already
# satisfies claims nothing about what ppcg did, and one that arrived
# merged with its neighbour matches nothing at all: file(STRINGS) keeps
# reading past a newline while a bracket is open, and the lines then
# reach here joined by the list separator.
macro(check_expectation haystack what where)
  set(_e "${expectation}")
  string(REPLACE "\\;" ";" _e "${_e}")
  set(_negated FALSE)
  if("${_e}" MATCHES "^!")
    set(_negated TRUE)
    string(SUBSTRING "${_e}" 1 -1 _e)
  endif()
  if(NOT _negated AND original MATCHES "${_e}")
    message(FATAL_ERROR
      "'${_e}' is already in ${SOURCE}, so it says nothing about what "
      "ppcg did with it.  ppcg copies a source comment into the file it "
      "writes, and an expectation the commentary can satisfy passes "
      "whatever the translation turned out to be.")
  endif()
  if(_negated AND "${haystack}" MATCHES "${_e}")
    message(FATAL_ERROR
      "${what} of ${SOURCE} still has something matching "
      "'${_e}'\n${stdout}${stderr}")
  endif()
  if(NOT _negated AND NOT "${haystack}" MATCHES "${_e}")
    message(FATAL_ERROR
      "${what} of ${SOURCE} has nothing matching "
      "'${_e}'\n${stdout}${stderr}")
  endif()
endmacro()

if(DEFINED EXPECT_FILE AND NOT "${EXPECT_FILE}" STREQUAL "")
  if(NOT EXISTS "${EXPECT_FILE}")
    message(FATAL_ERROR "RunTranslate: no such EXPECT_FILE ${EXPECT_FILE}")
  endif()
  file(READ "${OUTPUT}" generated)
  file(READ "${SOURCE}" original)
  read_expectations("${EXPECT_FILE}" expectations)
  foreach(expectation IN LISTS expectations)
    if("${expectation}" STREQUAL "" OR "${expectation}" MATCHES "^#")
      continue()
    endif()
    check_expectation("${generated}" "the translation" "${EXPECT_FILE}")
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
  file(READ "${SOURCE}" original)
  read_expectations("${EXPECT_SAYS_FILE}" said)
  foreach(expectation IN LISTS said)
    if("${expectation}" STREQUAL "" OR "${expectation}" MATCHES "^#")
      continue()
    endif()
    check_expectation("${stdout}${stderr}" "the report" "${EXPECT_SAYS_FILE}")
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
  read_expectations("${EXPECT_DEPS_FILE}" claimed)
  foreach(expectation IN LISTS claimed)
    if("${expectation}" STREQUAL "" OR "${expectation}" MATCHES "^#")
      continue()
    endif()
    check_expectation("${stdout}${stderr}" "the dependences"
                      "${EXPECT_DEPS_FILE}")
  endforeach()
endif()
