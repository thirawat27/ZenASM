if(NOT DEFINED ZENASM_EXE OR NOT DEFINED SOURCE_FILE OR NOT DEFINED ASM_FILE OR NOT DEFINED OBJ_FILE)
    message(FATAL_ERROR "ZENASM_EXE, SOURCE_FILE, ASM_FILE and OBJ_FILE are required")
endif()

execute_process(
    COMMAND "${ZENASM_EXE}" build "${SOURCE_FILE}" -o "${ASM_FILE}" --target sysv64 --emit-obj "${OBJ_FILE}" --opt 3
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_stdout
    ERROR_VARIABLE build_stderr
)

if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "sysv object build failed:\n${build_stdout}\n${build_stderr}")
endif()

if(NOT EXISTS "${ASM_FILE}")
    message(FATAL_ERROR "Expected sysv asm output was not produced: ${ASM_FILE}")
endif()

if(NOT EXISTS "${OBJ_FILE}")
    message(FATAL_ERROR "Expected sysv object output was not produced: ${OBJ_FILE}")
endif()
