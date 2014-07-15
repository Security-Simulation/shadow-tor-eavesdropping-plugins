## Eavesdrop analyzer server
include_directories(${CMAKE_SOURCE_DIR}/src/utility)

set(analyzer_sources
	analyzer.c
)

## service target to allow analyzer service to be used by any plugin
add_library(shadow-service-analyzer STATIC ${analyzer_sources})
add_dependencies(shadow-service-analyzer shadow-util)
target_link_libraries(shadow-service-analyzer ${RT_LIBRARIES} ${GLIB_LIBRARIES})

## executable that can run outside of shadow
add_executable(shadow-analyzer analyzer-main.c)
target_link_libraries(shadow-analyzer shadow-service-analyzer ${RT_LIBRARIES} ${GLIB_LIBRARIES})
install(TARGETS shadow-analyzer DESTINATION bin)

## build bitcode - other plugins may use the service bitcode target
add_bitcode(shadow-service-analyzer-bitcode ${analyzer_sources})
add_bitcode(shadow-plugin-analyzer-bitcode analyzer-plugin.c)

## create and install a shared library that can plug into shadow
add_plugin(shadow-plugin-analyzer shadow-plugin-analyzer-bitcode shadow-service-analyzer-bitcode)
target_link_libraries(shadow-plugin-analyzer ${RT_LIBRARIES} ${GLIB_LIBRARIES})
install(TARGETS shadow-plugin-analyzer DESTINATION plugins)

## the following two lines are needed if we want to allow external plug-ins to use ours
if(SHADOW_EXPORT STREQUAL ON)
    install(TARGETS shadow-service-analyzer shadow-service-analyzer-bitcode EXPORT shadow-externals DESTINATION lib)
    install(DIRECTORY ./ DESTINATION include/ FILES_MATCHING PATTERN "*.h")
endif(SHADOW_EXPORT STREQUAL ON)
