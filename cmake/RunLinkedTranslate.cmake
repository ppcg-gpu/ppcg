# Translate a set of units together with ppcg_linked, from inside a test.
#
# ppcg_linked reads serialised ASTs rather than source, and writes one
# file per unit into the directory it is run in, named after the unit.
# So the units are emitted first and the driver is run with the output
# directory as its working directory.
#
#   EMITTER    the pet_emit_ast program
#   DRIVER     the ppcg_linked program
#   SRCDIR     directory holding the corpus
#   BINDIR     directory to place the ASTs and the generated code in
#   UNITS      '|' separated base names, in the order they are linked
#   TIMEOUT_S  seconds the translation may run before it is killed
#
# Like the dead code tests, this runs as a test of its own as well as a
# build step, so that a translation that never returns is recorded as a
# failing test rather than a build that never finishes.

foreach(required EMITTER DRIVER SRCDIR BINDIR UNITS TIMEOUT_S)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "RunLinkedTranslate: ${required} is required")
  endif()
endforeach()

file(MAKE_DIRECTORY "${BINDIR}")
string(REPLACE "|" ";" units "${UNITS}")

set(asts "")
foreach(unit ${units})
  execute_process(
    COMMAND "${EMITTER}" -I "${SRCDIR}"
            "${SRCDIR}/${unit}.c" "${BINDIR}/${unit}.ast"
    RESULT_VARIABLE result
    ERROR_VARIABLE stderr
  )
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "serialising ${unit}.c failed with ${result}:\n${stderr}")
  endif()
  list(APPEND asts "${BINDIR}/${unit}.ast")
endforeach()

execute_process(
  COMMAND "${DRIVER}" ${asts}
  WORKING_DIRECTORY "${BINDIR}"
  RESULT_VARIABLE result
  TIMEOUT ${TIMEOUT_S}
  OUTPUT_VARIABLE stdout
  ERROR_VARIABLE stderr
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR
    "ppcg_linked did not translate ${UNITS} within ${TIMEOUT_S}s "
    "(exit ${result}):\n${stdout}${stderr}")
endif()

foreach(unit ${units})
  set(generated "${BINDIR}/${unit}.ppcg.c")
  if(NOT EXISTS "${generated}")
    message(FATAL_ERROR "ppcg_linked wrote no ${unit}.ppcg.c")
  endif()
  file(SIZE "${generated}" size)
  if(size EQUAL 0)
    message(FATAL_ERROR "ppcg_linked wrote an empty ${unit}.ppcg.c")
  endif()
endforeach()
