## simpleserver plug-in

## create and install a shared library that can plug into shadow
add_bitcode(shadow-plugin-simpleserver-bitcode src/simpleserver-plugin.c src/simpleserver.c)
add_plugin(shadow-plugin-simpleserver shadow-plugin-simpleserver-bitcode)
target_link_libraries(shadow-plugin-simpleserver ${GLIB_LIBRARIES})
install(TARGETS shadow-plugin-simpleserver DESTINATION plugins)

## create and install an executable that can run outside of shadow
add_executable(simple-server src/simpleserver-main.c src/simpleserver.c)
target_link_libraries(simple-server ${GLIB_LIBRARIES})
install(TARGETS simple-server DESTINATION bin)
