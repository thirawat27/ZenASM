if(NOT DEFINED ASM_FILE)
    message(FATAL_ERROR "ASM_FILE was not provided")
endif()

if(NOT EXISTS "${ASM_FILE}")
    message(FATAL_ERROR "Assembly output not found: ${ASM_FILE}")
endif()

file(READ "${ASM_FILE}" ASM_TEXT)

if(NOT ASM_TEXT MATCHES "\\.globl main")
    message(FATAL_ERROR "Expected generated assembly to export main")
endif()

if(NOT ASM_TEXT MATCHES "call printf")
    message(FATAL_ERROR "Expected generated assembly to contain a print call")
endif()
