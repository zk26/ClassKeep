include(BuildIkebackApplication)

build_ikeback_application(ikeback-configurator
	NAME Configurator
	WINDOWS_ICON "data/ikeback-configurator.ico"
	REQUIRE_ADMINISTRATOR_PRIVILEGES
	SOURCES
	src/AccessControlPage.cpp
	src/AccessControlPage.h
	src/AccessControlPage.ui
	src/AccessControlRuleEditDialog.cpp
	src/AccessControlRuleEditDialog.h
	src/AccessControlRuleEditDialog.ui
	src/AccessControlRuleListModel.cpp
	src/AccessControlRuleListModel.h
	src/AccessControlRulesTestDialog.cpp
	src/AccessControlRulesTestDialog.h
	src/AccessControlRulesTestDialog.ui
	src/GeneralConfigurationPage.cpp
	src/GeneralConfigurationPage.h
	src/GeneralConfigurationPage.ui
	src/main.cpp
	src/MainWindow.cpp
	src/MainWindow.h
	src/MainWindow.ui
	src/MasterConfigurationPage.cpp
	src/MasterConfigurationPage.h
	src/MasterConfigurationPage.ui
	src/ServiceConfigurationPage.cpp
	src/ServiceConfigurationPage.h
	src/ServiceConfigurationPage.ui
	resources/configurator.qrc)

if(IKEBACK_BUILD_LINUX)
	xdg_install(${CMAKE_CURRENT_BINARY_DIR}/data/ikeback-configurator.desktop ${CMAKE_CURRENT_SOURCE_DIR}/data/ikeback-configurator.xpm ${CMAKE_CURRENT_SOURCE_DIR}/data/ikeback-configurator.png ${CMAKE_CURRENT_SOURCE_DIR}/data/ikeback-configurator.svg)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/data/io.ikeback.ikeback-configurator.policy DESTINATION ${CMAKE_INSTALL_PREFIX}/share/polkit-1/actions)
endif()
