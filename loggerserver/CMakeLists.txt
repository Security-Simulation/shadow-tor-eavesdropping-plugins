## hello world plug-in

## create and install a shared library that can plug into shadow
add_shadow_plugin(shadow-plugin-loggerserver loggerserver.c)
target_link_libraries(shadow-plugin-loggerserver)
install(TARGETS shadow-plugin-loggerserver DESTINATION plugins)

## create and install an executable that can run outside of shadow
add_executable(loggerserver loggerserver.c)
target_link_libraries(loggerserver)
install(TARGETS loggerserver DESTINATION bin)
