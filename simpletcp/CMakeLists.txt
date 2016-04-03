## hello world plug-in

## create and install a shared library that can plug into shadow
add_shadow_plugin(shadow-plugin-randomecho simpletcp.c)
target_link_libraries(shadow-plugin-randomecho ${GLIB_LIBRARIES})
install(TARGETS shadow-plugin-randomecho DESTINATION plugins)

## create and install an executable that can run outside of shadow
add_executable(randomecho simpletcp.c)
target_link_libraries(randomecho ${GLIB_LIBRARIES})
install(TARGETS randomecho DESTINATION bin)
