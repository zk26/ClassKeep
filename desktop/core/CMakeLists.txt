include(WindowsBuildHelpers)

file(GLOB ikebackcore_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.ui
	${CMAKE_CURRENT_SOURCE_DIR}/src/Configuration/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Configuration/*.h
	)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/builddata.qrc.in ${CMAKE_CURRENT_BINARY_DIR}/builddata.qrc)
set(core_RESOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/resources/core.qrc
	${CMAKE_CURRENT_BINARY_DIR}/builddata.qrc
	)

add_library(ikeback-core SHARED ${ikebackcore_SOURCES} ${core_RESOURCES} ${libvncclient_SOURCES})

target_compile_definitions(ikeback-core PRIVATE XK_KOREAN)

set_default_target_properties(ikeback-core)

target_compile_options(ikeback-core PRIVATE -Wno-parentheses)

target_include_directories(ikeback-core PUBLIC
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}/src
	)

target_link_libraries(ikeback-core PUBLIC
	Qt${QT_MAJOR_VERSION}::Concurrent
	Qt${QT_MAJOR_VERSION}::Gui
	Qt${QT_MAJOR_VERSION}::Network
	Qt${QT_MAJOR_VERSION}::Widgets
	qca-qt${QT_MAJOR_VERSION}
	)

if(WITH_QT6)
	# required by qca-qt6
	target_link_libraries(ikeback-core PUBLIC Qt6::Core5Compat)
endif()

if(WITH_TESTS)
	target_link_libraries(ikeback-core PUBLIC Qt${QT_MAJOR_VERSION}::Test)
endif()

target_link_libraries(ikeback-core PUBLIC OpenSSL::SSL)

if(WITH_BUNDLED_LIBVNC)
	target_link_libraries(ikeback-core PUBLIC vncclient)
else()
	target_link_libraries(ikeback-core PUBLIC LibVNC::LibVNCClient)
endif()

if(IKEBACK_BUILD_WINDOWS)
	# add Windows Socket library required by libvncclient
	target_link_libraries(ikeback-core PRIVATE -lws2_32)
	set_target_properties(ikeback-core PROPERTIES PREFIX "")
	add_windows_resources(ikeback-core
		NAME IkebackCore
		DESCRIPTION "Ikeback Core Library")
	if(NOT WITH_CORE_ONLY)
		install(TARGETS ikeback-core RUNTIME DESTINATION ${IKEBACK_LIB_DIR})
	endif()
else()
	if(NOT WITH_CORE_ONLY)
		install(TARGETS ikeback-core LIBRARY DESTINATION ${IKEBACK_LIB_DIR})
	endif()
endif()

if(IKEBACK_BUILD_WINDOWS)
	target_compile_definitions(ikeback-core PUBLIC _WIN32_WINNT=0x0602)
endif()

if(WITH_PCH)
	set(PCH ${CMAKE_CURRENT_SOURCE_DIR}/src/PrecompiledHeader.h)
	target_precompile_headers(ikeback-core PRIVATE ${PCH})

	add_pch_target(ikeback-application-pch ${PCH})
	set_target_properties(ikeback-application-pch PROPERTIES COMPILE_OPTIONS "${CMAKE_COMPILE_OPTIONS_PIE}")
	set_target_properties(ikeback-application-pch PROPERTIES LINK_OPTIONS "${CMAKE_LINK_OPTIONS_PIE}")

	add_pch_target(ikeback-library-pch ${PCH})
	set_target_properties(ikeback-library-pch PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
endif()
