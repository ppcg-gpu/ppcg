# Check which accumulations ppcg finds in a scop.
#
# The report says which location is accumulated into and with which
# operator, and how many of the statements were recognised, so it can be
# compared against a fixed expectation.
#
#   PPCG      the ppcg program
#   SOURCE    the input
#   BINDIR    directory to place the output in
#   EXPECTED  file holding the expected report

foreach(required PPCG SOURCE BINDIR EXPECTED)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "RunReductionTest: ${required} is required")
    endif()
endforeach()

file(MAKE_DIRECTORY "${BINDIR}")

execute_process(
    COMMAND "${PPCG}" --target=c --dump-reductions
            "${SOURCE}" -o "${BINDIR}/ignored.c"
    OUTPUT_FILE "${BINDIR}/report.txt"
    ERROR_FILE "${BINDIR}/diagnostics.txt"
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    file(READ "${BINDIR}/diagnostics.txt" diagnostics)
    message(FATAL_ERROR "ppcg failed with ${result}\n${diagnostics}")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files
            "${EXPECTED}" "${BINDIR}/report.txt"
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    file(READ "${BINDIR}/report.txt" got)
    file(READ "${EXPECTED}" want)
    message(FATAL_ERROR
        "the accumulations found are not the expected ones.\n"
        "--- expected ---\n${want}"
        "--- got ---\n${got}")
endif()
