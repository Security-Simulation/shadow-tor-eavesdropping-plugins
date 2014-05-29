## as plug-in

## create and install a shared library that can plug into shadow
add_bitcode(shadow-plugin-as-bitcode src/as-plugin.c src/as.c)
add_plugin(shadow-plugin-as shadow-plugin-as-bitcode)
target_link_libraries(shadow-plugin-as ${GLIB_LIBRARIES})
install(TARGETS shadow-plugin-as DESTINATION plugins)

## create and install an executable that can run outside of shadow
add_executable(autosys src/as-main.c src/as.c)
target_link_libraries(autosys ${GLIB_LIBRARIES})
install(TARGETS autosys DESTINATION bin)
