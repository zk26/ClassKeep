if(IKEBACK_BUILD_WINDOWS)
	add_subdirectory(ultravnc-builtin)
endif()

if(IKEBACK_BUILD_LINUX)
	add_subdirectory(x11vnc-builtin)
	add_subdirectory(pipewire)
endif()

add_subdirectory(external)
add_subdirectory(headless)
