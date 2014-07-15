## simpletcp world plug-in  

## create and install a shared library that can plug into shadow
add_bitcode(shadow-plugin-simpletcp-bitcode src/simpletcp-plugin.c src/simpletcp.c)
add_plugin(shadow-plugin-simpletcp shadow-plugin-simpletcp-bitcode)
target_link_libraries(shadow-plugin-simpletcp ${GLIB_LIBRARIES})
install(TARGETS shadow-plugin-simpletcp DESTINATION plugins)

## create and install an executable that can run outside of shadow
add_executable(simpletcp src/simpletcp-main.c src/simpletcp.c)
target_link_libraries(simpletcp ${GLIB_LIBRARIES})
install(TARGETS simpletcp DESTINATION bin)
