include(BuildIkebackApplication)

file(GLOB master_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.*)

set(master_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources/master.qrc)
if(IKEBACK_DEBUG)
	set(master_RESOURCES ${master_RESOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/resources/examples.qrc)
endif()

build_ikeback_application(ikeback-master
	NAME Master
	WINDOWS_ICON data/ikeback-master.ico
	SOURCES
	${master_SOURCES}
	${master_RESOURCES})

if(IKEBACK_BUILD_LINUX)
	xdg_install(${CMAKE_CURRENT_BINARY_DIR}/data/ikeback-master.desktop ${CMAKE_CURRENT_SOURCE_DIR}/data/ikeback-master.xpm ${CMAKE_CURRENT_SOURCE_DIR}/data/ikeback-master.png ${CMAKE_CURRENT_SOURCE_DIR}/data/ikeback-master.svg)
endif()
