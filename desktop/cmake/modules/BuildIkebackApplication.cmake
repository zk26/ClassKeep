# BuildVeyonApplication.cmake - Copyright (c) 2019-2026 Tobias Junghans
#
# description: build Ikeback application
# usage: build_ikeback_application(<NAME> <SOURCES>)
include (WindowsBuildHelpers)

function(build_ikeback_application TARGET)
	cmake_parse_arguments(PARSE_ARGV 1 arg
		""
		""
		"SOURCES")

	# ikeback 2.0: every shipped process is named ikeback-* so that Task
	# Manager / service console / firewall rules are unambiguous to teachers.
	# The CMake target keeps its upstream `ikeback-*` name (CI --target lists and
	# plugin build graphs still refer to it); only the OUTPUT_NAME changes.
	string(REPLACE "ikeback-" "ikeback-" IKEBACK_OUTPUT_NAME "${TARGET}")
	if(IKEBACK_BUILD_ANDROID)
		add_library(${TARGET} SHARED ${arg_SOURCES})
	else()
		add_executable(${TARGET} ${arg_SOURCES})
		set_target_properties(${TARGET} PROPERTIES OUTPUT_NAME "${IKEBACK_OUTPUT_NAME}")
		install(TARGETS ${TARGET} RUNTIME DESTINATION bin)
	endif()
	target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
	set_target_properties(${TARGET} PROPERTIES COMPILE_OPTIONS "${CMAKE_COMPILE_OPTIONS_PIE}")
	set_target_properties(${TARGET} PROPERTIES LINK_OPTIONS "${CMAKE_LINK_OPTIONS_PIE}")
	target_link_libraries(${TARGET} PRIVATE ikeback-core)
	set_default_target_properties(${TARGET})
	if(WITH_PCH)
		target_precompile_headers(${TARGET} REUSE_FROM ikeback-application-pch)
	endif()
	if (IKEBACK_BUILD_WINDOWS)
		add_windows_resources(${TARGET} ${ARGN})
	endif()
endfunction()
