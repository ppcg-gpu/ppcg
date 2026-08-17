# Check what the OpenMP backend emits for a loop that accumulates.
#
# Running the generated program only shows that the answer is right, which
# it also is when the loop was left sequential.  What says that the
# accumulation was actually turned into a parallel loop is the pragma, so
# that is checked separately, here.
#
#   PPCG      the ppcg program
#   SOURCE    the input
#   BINDIR    directory to place the generated code in
#   EXPECTED  file holding the lines the generated code has to contain

foreach(required PPCG SOURCE BINDIR EXPECTED)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "RunReductionCodegenTest: ${required} is required")
    endif()
endforeach()

file(MAKE_DIRECTORY "${BINDIR}")

get_filename_component(base_name "${SOURCE}" NAME)
set(generated "${BINDIR}/${base_name}")

execute_process(
    COMMAND "${PPCG}" --target=c --openmp "${SOURCE}" -o "${generated}"
    ERROR_FILE "${BINDIR}/diagnostics.txt"
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    file(READ "${BINDIR}/diagnostics.txt" diagnostics)
    message(FATAL_ERROR "ppcg failed with ${result}\n${diagnostics}")
endif()

# Whole lines are matched, but with the leading and trailing whitespace of
# both sides removed: what is being checked is the pragma, not how deeply
# the code around it happens to be indented.
#
# The lines are read with file(STRINGS) rather than split out of the file
# contents by hand, because a C statement ends in a semicolon and splitting
# a string on newlines would leave those to be taken as list separators.
file(STRINGS "${EXPECTED}" wanted_lines)
file(STRINGS "${generated}" code_lines)
foreach(line ${code_lines})
    string(STRIP "${line}" line)
    list(APPEND stripped_code_lines "${line}")
endforeach()

foreach(wanted ${wanted_lines})
    string(STRIP "${wanted}" wanted)
    if(NOT "${wanted}" IN_LIST stripped_code_lines)
        file(READ "${generated}" code)
        message(FATAL_ERROR
            "the generated code does not contain\n  ${wanted}\n"
            "--- generated ---\n${code}")
    endif()
endforeach()
