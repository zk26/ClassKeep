# BuildVeyonFuzzer.cmake - Copyright (c) 2021-2026 Tobias Junghans
#
# description: build fuzzer test for Ikeback component
# usage: build_ikeback_fuzzer(<NAME> <SOURCES>)

macro(build_ikeback_fuzzer FUZZER_NAME)
	add_executable(${FUZZER_NAME} ${ARGN})
	set_default_target_properties(${FUZZER_NAME})
	target_compile_options(${FUZZER_NAME} PRIVATE "-g;-fsanitize=fuzzer")
	target_link_options(${FUZZER_NAME} PRIVATE "-g;-fsanitize=fuzzer")
	target_link_libraries(${FUZZER_NAME} ikeback-core)
	add_test(NAME ${FUZZER_NAME} COMMAND ${FUZZER_NAME} -max_total_time=60)
endmacro()

