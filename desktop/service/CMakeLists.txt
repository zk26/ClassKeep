include(BuildIkebackApplication)

build_ikeback_application(ikeback-service
	NAME Service
	SOURCES
	src/main.cpp)

if(IKEBACK_BUILD_LINUX)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ikeback.service.in ${CMAKE_CURRENT_BINARY_DIR}/ikeback.service @ONLY)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ikeback.service DESTINATION ${SYSTEMD_SERVICE_INSTALL_DIR})
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/io.ikeback.ikeback-service.conf DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/system.d")
endif()
