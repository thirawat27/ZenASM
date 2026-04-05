if(NOT DEFINED ZENASM_EXE OR NOT DEFINED SOURCE_FILE OR NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "ZENASM_EXE, SOURCE_FILE and OUTPUT_DIR are required")
endif()

execute_process(
    COMMAND "${ZENASM_EXE}" package "${SOURCE_FILE}" --output-dir "${OUTPUT_DIR}" --opt 3
    RESULT_VARIABLE package_result
    OUTPUT_VARIABLE package_stdout
    ERROR_VARIABLE package_stderr
)

if(NOT package_result EQUAL 0)
    message(FATAL_ERROR "package command failed:\n${package_stdout}\n${package_stderr}")
endif()

set(MANIFEST_FILE "${OUTPUT_DIR}/manifest.json")
set(ASM_FILE "${OUTPUT_DIR}/production.asm")
set(OBJ_FILE "${OUTPUT_DIR}/production.obj")
set(EXE_FILE "${OUTPUT_DIR}/production.exe")
set(SOURCE_COPY "${OUTPUT_DIR}/production.zen")

foreach(required_file IN ITEMS "${MANIFEST_FILE}" "${ASM_FILE}" "${OBJ_FILE}" "${EXE_FILE}" "${SOURCE_COPY}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Expected package artifact missing: ${required_file}")
    endif()
endforeach()

file(READ "${MANIFEST_FILE}" MANIFEST_TEXT)

if(NOT MANIFEST_TEXT MATCHES "\"exe\"")
    message(FATAL_ERROR "Manifest did not include exe artifact:\n${MANIFEST_TEXT}")
endif()

if(NOT MANIFEST_TEXT MATCHES "\"source\"")
    message(FATAL_ERROR "Manifest did not include source artifact:\n${MANIFEST_TEXT}")
endif()
