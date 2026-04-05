if(NOT DEFINED ASM_FILE OR NOT DEFINED EXE_FILE OR NOT DEFINED COMPILER_PATH)
    message(FATAL_ERROR "ASM_FILE, EXE_FILE and COMPILER_PATH are required")
endif()

if(NOT EXISTS "${ASM_FILE}")
    message(FATAL_ERROR "Assembly input not found: ${ASM_FILE}")
endif()

execute_process(
    COMMAND "${COMPILER_PATH}" -x assembler "${ASM_FILE}" -o "${EXE_FILE}"
    RESULT_VARIABLE compile_result
    OUTPUT_VARIABLE compile_stdout
    ERROR_VARIABLE compile_stderr
)

if(NOT compile_result EQUAL 0)
    message(FATAL_ERROR "Failed to assemble and link generated output:\n${compile_stdout}\n${compile_stderr}")
endif()

execute_process(
    COMMAND "${EXE_FILE}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_stdout
    ERROR_VARIABLE run_stderr
)

if(NOT run_stdout MATCHES "40")
    message(FATAL_ERROR "Expected program output to contain 40, got:\n${run_stdout}\n${run_stderr}")
endif()
