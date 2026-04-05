# CMake generated Testfile for 
# Source directory: D:/zenasm
# Build directory: D:/zenasm/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[smoke_build_hello]=] "D:/zenasm/build/zenasm.exe" "build" "D:/zenasm/examples/hello.zen" "-o" "D:/zenasm/build/hello.asm" "--target" "win64" "--opt" "3")
set_tests_properties([=[smoke_build_hello]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/zenasm/CMakeLists.txt;45;add_test;D:/zenasm/CMakeLists.txt;0;")
add_test([=[verify_hello_output]=] "C:/Program Files/CMake/bin/cmake.exe" "-DASM_FILE=D:/zenasm/build/hello.asm" "-P" "D:/zenasm/tests/verify_asm.cmake")
set_tests_properties([=[verify_hello_output]=] PROPERTIES  DEPENDS "smoke_build_hello" _BACKTRACE_TRIPLES "D:/zenasm/CMakeLists.txt;57;add_test;D:/zenasm/CMakeLists.txt;0;")
add_test([=[assemble_and_run_hello]=] "C:/Program Files/CMake/bin/cmake.exe" "-DASM_FILE=D:/zenasm/build/hello.asm" "-DEXE_FILE=D:/zenasm/build/hello_smoke.exe" "-DCOMPILER_PATH=C:/Program Files/LLVM/bin/clang++.exe" "-P" "D:/zenasm/tests/assemble_and_run.cmake")
set_tests_properties([=[assemble_and_run_hello]=] PROPERTIES  DEPENDS "smoke_build_hello" _BACKTRACE_TRIPLES "D:/zenasm/CMakeLists.txt;66;add_test;D:/zenasm/CMakeLists.txt;0;")
add_test([=[run_advanced_program]=] "C:/Program Files/CMake/bin/cmake.exe" "-DZENASM_EXE=D:/zenasm/build/zenasm.exe" "-DSOURCE_FILE=D:/zenasm/examples/advanced.zen" "-DASM_FILE=D:/zenasm/build/advanced.asm" "-DEXE_FILE=D:/zenasm/build/advanced.exe" "-P" "D:/zenasm/tests/run_and_verify.cmake")
set_tests_properties([=[run_advanced_program]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/zenasm/CMakeLists.txt;77;add_test;D:/zenasm/CMakeLists.txt;0;")
add_test([=[build_sysv_object]=] "C:/Program Files/CMake/bin/cmake.exe" "-DZENASM_EXE=D:/zenasm/build/zenasm.exe" "-DSOURCE_FILE=D:/zenasm/examples/advanced.zen" "-DASM_FILE=D:/zenasm/build/advanced_sysv.asm" "-DOBJ_FILE=D:/zenasm/build/advanced_sysv.o" "-P" "D:/zenasm/tests/build_sysv_object.cmake")
set_tests_properties([=[build_sysv_object]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/zenasm/CMakeLists.txt;88;add_test;D:/zenasm/CMakeLists.txt;0;")
add_test([=[run_production_program]=] "C:/Program Files/CMake/bin/cmake.exe" "-DZENASM_EXE=D:/zenasm/build/zenasm.exe" "-DSOURCE_FILE=D:/zenasm/examples/production.zen" "-DASM_FILE=D:/zenasm/build/production.asm" "-DEXE_FILE=D:/zenasm/build/production.exe" "-P" "D:/zenasm/tests/run_production.cmake")
set_tests_properties([=[run_production_program]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/zenasm/CMakeLists.txt;99;add_test;D:/zenasm/CMakeLists.txt;0;")
add_test([=[package_production_program]=] "C:/Program Files/CMake/bin/cmake.exe" "-DZENASM_EXE=D:/zenasm/build/zenasm.exe" "-DSOURCE_FILE=D:/zenasm/examples/production.zen" "-DOUTPUT_DIR=D:/zenasm/build/package-production" "-P" "D:/zenasm/tests/package_verify.cmake")
set_tests_properties([=[package_production_program]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/zenasm/CMakeLists.txt;110;add_test;D:/zenasm/CMakeLists.txt;0;")
