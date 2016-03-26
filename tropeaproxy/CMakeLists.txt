## hello world plug-in

## create and install a shared library that can plug into shadow
add_shadow_plugin(shadow-plugin-tproxy tproxy.c)
target_link_libraries(shadow-plugin-tproxy ${GLIB_LIBRARIES})
install(TARGETS shadow-plugin-tproxy DESTINATION plugins)

## create and install an executable that can run outside of shadow
add_executable(tproxy tproxy.c)
target_link_libraries(tproxy ${GLIB_LIBRARIES})
install(TARGETS tproxy DESTINATION bin)
