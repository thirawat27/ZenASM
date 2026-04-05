if(NOT DEFINED ZENASM_EXE OR NOT DEFINED SOURCE_FILE OR NOT DEFINED ASM_FILE OR NOT DEFINED EXE_FILE)
    message(FATAL_ERROR "ZENASM_EXE, SOURCE_FILE, ASM_FILE and EXE_FILE are required")
endif()

execute_process(
    COMMAND "${ZENASM_EXE}" run "${SOURCE_FILE}" -o "${ASM_FILE}" --emit-exe "${EXE_FILE}" --opt 3
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_stdout
    ERROR_VARIABLE run_stderr
)

if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "zenasm run failed:\n${run_stdout}\n${run_stderr}")
endif()

if(NOT EXISTS "${ASM_FILE}")
    message(FATAL_ERROR "Expected asm output was not produced: ${ASM_FILE}")
endif()

if(NOT EXISTS "${EXE_FILE}")
    message(FATAL_ERROR "Expected executable output was not produced: ${EXE_FILE}")
endif()

if(NOT run_stdout MATCHES "62")
    message(FATAL_ERROR "Expected runtime output to contain 62, got:\n${run_stdout}\n${run_stderr}")
endif()

if(NOT run_stdout MATCHES "Program exited with code 62")
    message(FATAL_ERROR "Expected run command to report exit code 62, got:\n${run_stdout}\n${run_stderr}")
endif()
