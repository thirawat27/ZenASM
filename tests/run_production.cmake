if(NOT DEFINED ZENASM_EXE OR NOT DEFINED SOURCE_FILE OR NOT DEFINED ASM_FILE OR NOT DEFINED EXE_FILE)
    message(FATAL_ERROR "ZENASM_EXE, SOURCE_FILE, ASM_FILE and EXE_FILE are required")
endif()

execute_process(
    COMMAND "${ZENASM_EXE}" run "${SOURCE_FILE}" -o "${ASM_FILE}" --emit-exe "${EXE_FILE}" --emit-ir "${ASM_FILE}.ir.txt" --emit-ast "${ASM_FILE}.ast.txt" --opt 3
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_stdout
    ERROR_VARIABLE run_stderr
)

if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "production run failed:\n${run_stdout}\n${run_stderr}")
endif()

if(NOT EXISTS "${ASM_FILE}")
    message(FATAL_ERROR "Expected production asm output was not produced: ${ASM_FILE}")
endif()

if(NOT EXISTS "${EXE_FILE}")
    message(FATAL_ERROR "Expected production exe output was not produced: ${EXE_FILE}")
endif()

if(NOT run_stdout MATCHES "ZenAsm package ready")
    message(FATAL_ERROR "Expected production output to contain banner text, got:\n${run_stdout}\n${run_stderr}")
endif()

if(NOT run_stdout MATCHES "26")
    message(FATAL_ERROR "Expected production output to contain 26, got:\n${run_stdout}\n${run_stderr}")
endif()

if(NOT run_stdout MATCHES "Program exited with code 26")
    message(FATAL_ERROR "Expected production run to report exit code 26, got:\n${run_stdout}\n${run_stderr}")
endif()
