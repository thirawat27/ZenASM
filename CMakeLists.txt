cmake_minimum_required(VERSION 3.25)

project(ZenAsm VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ZENASM_ENABLE_TESTS "Build ZenAsm tests" ON)

add_library(
    zenasm_lib
    src/source.cpp
    src/diagnostics.cpp
    src/lexer.cpp
    src/parser.cpp
    src/semantic.cpp
    src/ir.cpp
    src/lowering.cpp
    src/optimizer.cpp
    src/regalloc.cpp
    src/emitter.cpp
    src/compiler.cpp
)

target_include_directories(zenasm_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(MSVC)
    target_compile_options(zenasm_lib PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(zenasm_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(zenasm src/main.cpp)
target_link_libraries(zenasm PRIVATE zenasm_lib)
target_compile_definitions(zenasm PRIVATE ZENASM_DEFAULT_TOOLCHAIN_PATH="${CMAKE_CXX_COMPILER}")

install(TARGETS zenasm RUNTIME DESTINATION bin)
install(FILES README.md DESTINATION .)
install(DIRECTORY examples DESTINATION share/zenasm FILES_MATCHING PATTERN "*.zen")

include(CTest)

if(BUILD_TESTING AND ZENASM_ENABLE_TESTS)
    add_test(
        NAME smoke_build_hello
        COMMAND zenasm build
                ${CMAKE_CURRENT_SOURCE_DIR}/examples/hello.zen
                -o
                ${CMAKE_CURRENT_BINARY_DIR}/hello.asm
                --target
                win64
                --opt
                3
    )

    add_test(
        NAME verify_hello_output
        COMMAND ${CMAKE_COMMAND}
                -DASM_FILE=${CMAKE_CURRENT_BINARY_DIR}/hello.asm
                -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_asm.cmake
    )
    set_tests_properties(verify_hello_output PROPERTIES DEPENDS smoke_build_hello)

    add_test(
        NAME assemble_and_run_hello
        COMMAND ${CMAKE_COMMAND}
                -DASM_FILE=${CMAKE_CURRENT_BINARY_DIR}/hello.asm
                -DEXE_FILE=${CMAKE_CURRENT_BINARY_DIR}/hello_smoke.exe
                -DCOMPILER_PATH=${CMAKE_CXX_COMPILER}
                -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/assemble_and_run.cmake
    )
    set_tests_properties(assemble_and_run_hello PROPERTIES DEPENDS smoke_build_hello)

    add_test(
        NAME run_advanced_program
        COMMAND ${CMAKE_COMMAND}
                -DZENASM_EXE=$<TARGET_FILE:zenasm>
                -DSOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/advanced.zen
                -DASM_FILE=${CMAKE_CURRENT_BINARY_DIR}/advanced.asm
                -DEXE_FILE=${CMAKE_CURRENT_BINARY_DIR}/advanced.exe
                -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_and_verify.cmake
    )

    add_test(
        NAME build_sysv_object
        COMMAND ${CMAKE_COMMAND}
                -DZENASM_EXE=$<TARGET_FILE:zenasm>
                -DSOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/advanced.zen
                -DASM_FILE=${CMAKE_CURRENT_BINARY_DIR}/advanced_sysv.asm
                -DOBJ_FILE=${CMAKE_CURRENT_BINARY_DIR}/advanced_sysv.o
                -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/build_sysv_object.cmake
    )

    add_test(
        NAME run_production_program
        COMMAND ${CMAKE_COMMAND}
                -DZENASM_EXE=$<TARGET_FILE:zenasm>
                -DSOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/production.zen
                -DASM_FILE=${CMAKE_CURRENT_BINARY_DIR}/production.asm
                -DEXE_FILE=${CMAKE_CURRENT_BINARY_DIR}/production.exe
                -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_production.cmake
    )

    add_test(
        NAME package_production_program
        COMMAND ${CMAKE_COMMAND}
                -DZENASM_EXE=$<TARGET_FILE:zenasm>
                -DSOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/production.zen
                -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/package-production
                -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/package_verify.cmake
    )
endif()
