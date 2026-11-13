find_package(PAM REQUIRED)

if(WITH_QT6)
	find_package(Qt6 COMPONENTS DBus REQUIRED)
else()
	find_package(Qt5DBus REQUIRED)
endif()

add_executable(ikeback-auth-helper ${CMAKE_CURRENT_SOURCE_DIR}/IkebackAuthHelper.cpp)

set_default_target_properties(ikeback-auth-helper)

target_include_directories(ikeback-auth-helper PRIVATE ${PAM_INCLUDE_DIR})
target_link_libraries(ikeback-auth-helper PRIVATE ikeback-core Qt${QT_MAJOR_VERSION}::DBus ${PAM_LIBRARY})

install(TARGETS ikeback-auth-helper RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ikeback-auth-helper.service.in
			   ${CMAKE_CURRENT_BINARY_DIR}/ikeback-auth-helper.service @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/io.ikeback.AuthHelper.service.in
			   ${CMAKE_CURRENT_BINARY_DIR}/io.ikeback.AuthHelper.service @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ikeback-auth-helper.service
		DESTINATION ${SYSTEMD_SERVICE_INSTALL_DIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.ikeback.AuthHelper.service
		DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/system-services)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/io.ikeback.AuthHelper.conf
		DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/system.d)
