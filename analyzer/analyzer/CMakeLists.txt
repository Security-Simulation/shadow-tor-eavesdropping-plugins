## analyzer plug-in

## create and install a shared library that can plug into shadow
add_bitcode(shadow-plugin-analyzer-bitcode src/analyzer-plugin.c src/analyzer.c)
add_plugin(shadow-plugin-analyzer shadow-plugin-analyzer-bitcode)
target_link_libraries(shadow-plugin-analyzer)
install(TARGETS shadow-plugin-analyzer DESTINATION plugins)

## create and install an executable that can run outside of shadow
add_executable(analyzer-tor src/analyzer-main.c src/analyzer.c)
target_link_libraries(analyzer-tor)
install(TARGETS analyzer-tor DESTINATION bin)
