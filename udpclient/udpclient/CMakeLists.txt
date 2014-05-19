## udpclient plug-in

## create and install a shared library that can plug into shadow
add_bitcode(shadow-plugin-udpclient-bitcode src/udpclient-plugin.c src/udpclient.c)
add_plugin(shadow-plugin-udpclient shadow-plugin-udpclient-bitcode)
target_link_libraries(shadow-plugin-udpclient)
install(TARGETS shadow-plugin-udpclient DESTINATION plugins)

## create and install an executable that can run outside of shadow
add_executable(udpclient-tor src/udpclient-main.c src/udpclient.c)
target_link_libraries(udpclient-tor)
install(TARGETS udpclient-tor DESTINATION bin)
