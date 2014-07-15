## HTTP file server and client for transfering files through shadow
include_directories(${CMAKE_SOURCE_DIR}/src/utility)

set(autosys_sources
	autosys.c
#    shd-filetransfer.c 
#    shd-fileserver.c 
#    shd-service-filegetter.c 
#    shd-filegetter.c
#    ../../utility/shd-cdf.c
#    ../../utility/shd-utility.c
)

## service target to allow filetransfer service to be used by any plugin
add_library(shadow-service-autosys STATIC ${autosys_sources})
add_dependencies(shadow-service-autosys shadow-util)
target_link_libraries(shadow-service-autosys ${RT_LIBRARIES} ${GLIB_LIBRARIES})

## executable that can run outside of shadow
add_executable(shadow-autosys autosys-main.c)
target_link_libraries(shadow-autosys shadow-service-autosys ${RT_LIBRARIES} ${GLIB_LIBRARIES})
install(TARGETS shadow-autosys DESTINATION bin)

## build bitcode - other plugins may use the service bitcode target
add_bitcode(shadow-service-autosys-bitcode ${autosys_sources})
add_bitcode(shadow-plugin-autosys-bitcode autosys-plugin.c)

## create and install a shared library that can plug into shadow
add_plugin(shadow-plugin-autosys shadow-plugin-autosys-bitcode shadow-service-autosys-bitcode)
target_link_libraries(shadow-plugin-autosys ${RT_LIBRARIES} ${GLIB_LIBRARIES})
install(TARGETS shadow-plugin-autosys DESTINATION plugins)

## the following two lines are needed if we want to allow external plug-ins to use ours
if(SHADOW_EXPORT STREQUAL ON)
    install(TARGETS shadow-service-autosys shadow-service-autosys-bitcode EXPORT shadow-externals DESTINATION lib)
    install(DIRECTORY ./ DESTINATION include/ FILES_MATCHING PATTERN "*.h")
endif(SHADOW_EXPORT STREQUAL ON)
