# Translate one source file with ppcg, from inside the test.
#
# The ordinary C target tests run ppcg as a build step, so a ppcg that
# fails to terminate would hang the build and no test timeout would ever
# fire.  This driver runs ppcg as part of the test instead, so that
# set_tests_properties(... TIMEOUT) bounds it.
#
#   PPCG       the ppcg executable
#   SOURCE     the C file to translate
#   OUTPUT     where to place the generated code
#   TIMEOUT_S  seconds ppcg itself may run before it is killed
#   OPTIONS    optional extra ppcg options, '|' separated
#
# The translation has to finish within the timeout and has to leave a
# non-empty file behind.  What the generated code computes is checked by
# the program that includes it, not here.

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

execute_process(
  COMMAND ${command}
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
