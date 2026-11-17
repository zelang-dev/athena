# The executables
set(TARGET_LIST
	athena_ui
	skeleton
	csstest
	html5test
)

foreach (TARGET ${TARGET_LIST})
    add_executable(${TARGET} ${TARGET}.c)
	target_include_directories(${TARGET}
		PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>)
    target_link_libraries(${TARGET} ${PROJECT_NAME})
	set_target_properties(${TARGET} PROPERTIES
		EXPORT_NAME ${TARGET}
		OUTPUT_NAME ${TARGET}
		VERSION ${PROJECT_VERSION}
		SOVERSION ${PROJECT_VERSION_MAJOR}
	)
endforeach()

if(NOT EXISTS "${BUILD_DIR}/js")
	file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/js DESTINATION ${BUILD_DIR})
endif()