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
#   EXPECTED  file holding the lines the generated code has to contain,
#             or, written with a leading '!', the text it may not
#   OPTIONS   '|' separated options for ppcg, "--target=c|--openmp" when
#             not given

foreach(required PPCG SOURCE BINDIR EXPECTED)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "RunReductionCodegenTest: ${required} is required")
    endif()
endforeach()

if(NOT DEFINED OPTIONS OR "${OPTIONS}" STREQUAL "")
    set(OPTIONS "--target=c|--openmp")
endif()
string(REPLACE "|" ";" ppcg_options "${OPTIONS}")

file(MAKE_DIRECTORY "${BINDIR}")

get_filename_component(base_name "${SOURCE}" NAME)
set(generated "${BINDIR}/${base_name}")

execute_process(
    COMMAND "${PPCG}" ${ppcg_options} "${SOURCE}" -o "${generated}"
    ERROR_FILE "${BINDIR}/diagnostics.txt"
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    file(READ "${BINDIR}/diagnostics.txt" diagnostics)
    message(FATAL_ERROR "ppcg failed with ${result}\n${diagnostics}")
endif()

# ppcg copies whatever surrounds the scop through unchanged, comments and
# all, so only what it generated is looked at.  A corpus could otherwise
# satisfy an expected pragma by mentioning it in a comment.
set(marker "/* ppcg generated CPU code */")
set(in_generated FALSE)
set(code "")
set(n_code_lines 0)
file(STRINGS "${generated}" lines)
foreach(line ${lines})
    if(in_generated)
        # A separate variable per line rather than a list, because
        # list(APPEND) splits a value on its semicolons and every C
        # statement ends in one.
        string(STRIP "${line}" stripped)
        set(code_line_${n_code_lines} "${stripped}")
        math(EXPR n_code_lines "${n_code_lines} + 1")
        string(APPEND code "${line}\n")
    elseif(line MATCHES "ppcg generated CPU code")
        set(in_generated TRUE)
    endif()
endforeach()
if(NOT in_generated)
    message(FATAL_ERROR
        "no '${marker}' in ${generated}, so there is nothing to check")
endif()

file(STRINGS "${EXPECTED}" wanted_lines)
foreach(wanted ${wanted_lines})
    string(STRIP "${wanted}" wanted)

    # A leading '!' marks text the generated code may not contain, which
    # is how a corpus says that its loop has to stay sequential.
    #
    # Text that is wanted is matched as a whole line, so that a clause
    # naming more than it should does not pass for the one that was asked
    # for.  Text that is forbidden is matched anywhere, since whatever
    # follows the beginning of a pragma would otherwise let it through.
    if(wanted MATCHES "^!")
        string(SUBSTRING "${wanted}" 1 -1 wanted)
        string(FIND "${code}" "${wanted}" position)
        if(NOT position EQUAL -1)
            message(FATAL_ERROR
                "the generated code contains\n  ${wanted}\n"
                "--- generated ---\n${code}")
        endif()
    else()
        set(found FALSE)
        math(EXPR last "${n_code_lines} - 1")
        foreach(i RANGE 0 ${last})
            if("${code_line_${i}}" STREQUAL "${wanted}")
                set(found TRUE)
                break()
            endif()
        endforeach()
        if(NOT found)
            message(FATAL_ERROR
                "the generated code does not contain\n  ${wanted}\n"
                "--- generated ---\n${code}")
        endif()
    endif()
endforeach()
