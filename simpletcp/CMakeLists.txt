## Simple random tcp connection client/server
include_directories(${CMAKE_SOURCE_DIR}/src/utility)

set(simpletcp_sources
	simpletcp.c
)

## service target to allow simpletcp service to be used by any plugin
add_library(shadow-service-simpletcp STATIC ${simpletcp_sources})
add_dependencies(shadow-service-simpletcp shadow-util)
target_link_libraries(shadow-service-simpletcp ${RT_LIBRARIES} ${GLIB_LIBRARIES})

## executable that can run outside of shadow
add_executable(shadow-simpletcp simpletcp-main.c)
target_link_libraries(shadow-simpletcp shadow-service-simpletcp ${RT_LIBRARIES} ${GLIB_LIBRARIES})
install(TARGETS shadow-simpletcp DESTINATION bin)

## build bitcode - other plugins may use the service bitcode target
add_bitcode(shadow-service-simpletcp-bitcode ${simpletcp_sources})
add_bitcode(shadow-plugin-simpletcp-bitcode simpletcp-plugin.c)

## create and install a shared library that can plug into shadow
add_plugin(shadow-plugin-simpletcp shadow-plugin-simpletcp-bitcode shadow-service-simpletcp-bitcode)
target_link_libraries(shadow-plugin-simpletcp ${RT_LIBRARIES} ${GLIB_LIBRARIES})
install(TARGETS shadow-plugin-simpletcp DESTINATION plugins)

## the following two lines are needed if we want to allow external plug-ins to use ours
if(SHADOW_EXPORT STREQUAL ON)
    install(TARGETS shadow-service-simpletcp shadow-service-simpletcp-bitcode EXPORT shadow-externals DESTINATION lib)
    install(DIRECTORY ./ DESTINATION include/ FILES_MATCHING PATTERN "*.h")
endif(SHADOW_EXPORT STREQUAL ON)
