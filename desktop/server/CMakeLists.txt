include(BuildIkebackApplication)

build_ikeback_application(ikeback-server
	NAME Server
	SOURCES
	src/ComputerControlClient.cpp
	src/ComputerControlClient.h
	src/ComputerControlServer.cpp
	src/ComputerControlServer.h
	src/main.cpp
	src/ServerAccessControlManager.cpp
	src/ServerAccessControlManager.h
	src/ServerAuthenticationManager.cpp
	src/ServerAuthenticationManager.h
	src/IkebackServerProtocol.cpp
	src/IkebackServerProtocol.h
	src/VncProxyConnection.cpp
	src/VncProxyConnectionFactory.h
	src/VncProxyConnection.h
	src/VncProxyServer.cpp
	src/VncProxyServer.h
	src/VncServer.cpp
	src/VncServer.h)

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/data/io.ikeback.ikeback-server.desktop.in"
	"${CMAKE_CURRENT_BINARY_DIR}/data/io.ikeback.ikeback-server.desktop"
	@ONLY
)
configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/data/io.ikeback.ikeback-server.service.in"
	"${CMAKE_CURRENT_BINARY_DIR}/data/io.ikeback.ikeback-server.service"
	@ONLY
)

install(
	FILES "${CMAKE_CURRENT_BINARY_DIR}/data/io.ikeback.ikeback-server.desktop"
	DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
	COMPONENT pipewire-vnc-server
)

install(
	FILES "${CMAKE_CURRENT_BINARY_DIR}/data/io.ikeback.ikeback-server.service"
	DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/services"
	COMPONENT pipewire-vnc-server
)
