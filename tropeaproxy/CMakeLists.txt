## Static proxy eavesdropper
include_directories(${CMAKE_SOURCE_DIR}/src/utility)

set(tproxy_sources
	tproxy.c
)

## service target to allow tproxy service to be used by any plugin ??XXX what's this for?
add_library(shadow-service-tproxy STATIC ${tproxy_sources})
target_link_libraries(shadow-service-tproxy ${RT_LIBRARIES} ${GLIB_LIBRARIES})

## executable that can run outside of shadow
add_executable(tproxy tproxy-main.c)
target_link_libraries(tproxy shadow-service-tproxy ${RT_LIBRARIES} ${GLIB_LIBRARIES})
install(TARGETS tproxy DESTINATION bin)


## create and install a shared library that can plug into shadow
add_shadow_plugin(shadow-plugin-tproxy tproxy-main.c)
target_link_libraries(shadow-plugin-tproxy ${RT_LIBRARIES} ${GLIB_LIBRARIES})
install(TARGETS shadow-plugin-tproxy DESTINATION plugins)

## the following two lines are needed if we want to allow external plug-ins to use ours
if(SHADOW_EXPORT STREQUAL ON)
    install(TARGETS shadow-service-tproxy shadow-service-tproxy-bitcode EXPORT shadow-externals DESTINATION lib)
    install(DIRECTORY ./ DESTINATION include/ FILES_MATCHING PATTERN "*.h")
endif(SHADOW_EXPORT STREQUAL ON)
