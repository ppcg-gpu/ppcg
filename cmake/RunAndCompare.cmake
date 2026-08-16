# Run two programs and compare what they produce.
#
# Used as a ctest driver so that the programs under test are executed by the
# test, not by the build.  Redirection cannot be expressed in add_test(), and
# the polybench programs write their results to stderr, so the stream to
# capture is selectable.
#
#   REFERENCE   program producing the expected output
#   CANDIDATE   program producing the output to check
#   STREAM      "stdout" or "stderr"
#   OUTPUT_DIR  directory to place the captured output in
#   COMPARE     '|' separated command used to compare the two files
#   COMPARE_ARGS
#               '|' separated arguments appended after the two file names
#
# The programs are run in OUTPUT_DIR, because the generated OpenCL programs
# look for their kernel source next to the working directory.

# ${required} holds the name of the variable to check, so it has to be
# dereferenced exactly once.  if(NOT ${required}) dereferences twice and
# would accept a value such as 0 or NOTFOUND as "not given".
foreach(required REFERENCE CANDIDATE OUTPUT_DIR COMPARE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "RunAndCompare: ${required} is required")
  endif()
endforeach()

if(NOT STREAM)
  set(STREAM stdout)
endif()

get_filename_component(reference_name "${REFERENCE}" NAME)
get_filename_component(candidate_name "${CANDIDATE}" NAME)
set(reference_output "${OUTPUT_DIR}/${reference_name}.out")
set(candidate_output "${OUTPUT_DIR}/${candidate_name}.out")

function(run_program program output)
  if(STREAM STREQUAL "stderr")
    execute_process(
      COMMAND "${program}"
      WORKING_DIRECTORY "${OUTPUT_DIR}"
      ERROR_FILE "${output}"
      RESULT_VARIABLE result
    )
  else()
    execute_process(
      COMMAND "${program}"
      WORKING_DIRECTORY "${OUTPUT_DIR}"
      OUTPUT_FILE "${output}"
      RESULT_VARIABLE result
    )
  endif()
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "${program} failed with ${result}")
  endif()
endfunction()

run_program("${REFERENCE}" "${reference_output}")
run_program("${CANDIDATE}" "${candidate_output}")

string(REPLACE "|" ";" compare_command "${COMPARE}")
# COMPARE_ARGS is optional, and callers legitimately pass false-looking
# values such as FALSE, so test whether it was given rather than whether it
# is true.
set(compare_args "")
if(DEFINED COMPARE_ARGS)
  string(REPLACE "|" ";" compare_args "${COMPARE_ARGS}")
endif()
execute_process(
  COMMAND ${compare_command} "${reference_output}" "${candidate_output}" ${compare_args}
  RESULT_VARIABLE compare_result
)
if(NOT compare_result EQUAL 0)
  message(FATAL_ERROR
    "${reference_output} and ${candidate_output} differ")
endif()
