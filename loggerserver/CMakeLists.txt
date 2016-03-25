##loggerserver
include_directories(${CMAKE_SOURCE_DIR}/src/utility)

set(loggerserver_sources
	loggerserver.c
)

## service target to allow loggerserver service to be used by any plugin
add_library(shadow-service-loggerserver STATIC ${loggerserver_sources})
target_link_libraries(shadow-service-loggerserver ${RT_LIBRARIES} ${GLIB_LIBRARIES})

## executable that can run outside of shadow
add_executable(loggerserver loggerserver-main.c)
target_link_libraries(loggerserver shadow-service-loggerserver ${RT_LIBRARIES} ${GLIB_LIBRARIES})
install(TARGETS loggerserver DESTINATION bin)

## create and install a shared library that can plug into shadow
add_shadow_plugin(shadow-plugin-loggerserver loggerserver-main.c)
target_link_libraries(shadow-plugin-loggerserver ${RT_LIBRARIES} ${GLIB_LIBRARIES})
install(TARGETS shadow-plugin-loggerserver DESTINATION plugins)

## the following two lines are needed if we want to allow external plug-ins to use ours
if(SHADOW_EXPORT STREQUAL ON)
    install(TARGETS shadow-service-loggerserver shadow-service-loggerserver-bitcode EXPORT shadow-externals DESTINATION lib)
    install(DIRECTORY ./ DESTINATION include/ FILES_MATCHING PATTERN "*.h")
endif(SHADOW_EXPORT STREQUAL ON)
