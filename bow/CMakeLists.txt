cmake_minimum_required (VERSION 3.22...5.1.0)

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
	cmake_policy(SET CMP0091 NEW)
endif()

project(bow
	VERSION 0.6.0
	DESCRIPTION "Native X11/Athena Webview library, and Web Browser"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 90)

include(FetchContent)
include(CheckLibraryExists)
include(CMakePackageConfigHelpers)
include(CMakeDependentOption)
include(GNUInstallDirs)

set(BUILD_DIR ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/built")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_SHARED_LIBS "Build the library as a shared (dynamically-linked) " OFF)
option(BUILD_NATIVE_XAW "Build using Native X11 Athena Widget Toolkit " OFF)
option(BUILD_QUICKJS_NG "Build `fork` QuickJS-NG, not QuickJS, for `JavaScript` runtime" ON)
option(BUILD_JS_ONLY "Build QuickJS-NG bypassing `CMakeLists.txt`, no executables" ON)
option(BUILD_APP "Build standalone `Browser` App" OFF)

if (WIN32 OR APPLE)
    message(FATAL_ERROR "Linux Only!")
else()
	include_directories(${CMAKE_SOURCE_DIR}/include)
	set(JAVASCRIPT quickjs)
	if(BUILD_QUICKJS_NG)
		set(JAVASCRIPT qjs)
		find_package(quickjs QUIET)
		if(NOT quickjs_FOUND)
			set(ENV{QJS_BUILD_EXAMPLES} "OFF")
			set(ENV{QJS_BUILD_LIBC} "ON")
			FetchContent_Declare(quickjs
				URL https://github.com/quickjs-ng/quickjs/archive/refs/tags/v0.17.0.zip
				URL_MD5 502b324207f8080d8fe76ea27e809537
			)
			if(BUILD_JS_ONLY)
				set(JAVASCRIPT quickjs)
				set(QUICKJS_SOURCES
					${quickjs_SOURCE_DIR}/dtoa.c
					${quickjs_SOURCE_DIR}/libregexp.c
					${quickjs_SOURCE_DIR}/libunicode.c
					${quickjs_SOURCE_DIR}/quickjs.c
					${quickjs_SOURCE_DIR}/quickjs-libc.c
				)
				FetchContent_GetProperties(quickjs)
				if(NOT quickjs_POPULATED)
					FetchContent_Populate(quickjs)
					if(BUILD_SHARED_LIBS)
						add_library(quickjs SHARED ${QUICKJS_SOURCES})
					else()
						add_library(quickjs STATIC ${QUICKJS_SOURCES})
					endif()
					set_property(TARGET quickjs PROPERTY C_STANDARD 99)
					target_compile_definitions(quickjs PRIVATE _GNU_SOURCE)
					target_include_directories(quickjs PRIVATE ${quickjs_SOURCE_DIR})
				endif()
				file(STRINGS ${quickjs_SOURCE_DIR}/quickjs.h quickjs_h REGEX QJS_VERSION)
				string(REGEX MATCH "QJS_VERSION_MAJOR ([0-9]*)" _ "${quickjs_h}")
				set(QJS_VERSION_MAJOR ${CMAKE_MATCH_1})
				string(REGEX MATCH "QJS_VERSION_MINOR ([0-9]*)" _ "${quickjs_h}")
				set(QJS_VERSION_MINOR ${CMAKE_MATCH_1})
				string(REGEX MATCH "QJS_VERSION_PATCH ([0-9]*)" _ "${quickjs_h}")
				set(QJS_VERSION_PATCH ${CMAKE_MATCH_1})
				set_target_properties(${JAVASCRIPT} PROPERTIES
					VERSION ${QJS_VERSION_MAJOR}.${QJS_VERSION_MINOR}.${QJS_VERSION_PATCH}
					SOVERSION ${QJS_VERSION_MAJOR}
				)
				install(FILES ${quickjs_SOURCE_DIR}/quickjs.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
				install(FILES ${quickjs_SOURCE_DIR}/quickjs-libc.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
				install(TARGETS ${JAVASCRIPT} EXPORT ${JAVASCRIPT}Config
						RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
						LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
						ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
				install(EXPORT ${JAVASCRIPT}Config DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${JAVASCRIPT})
				install(FILES  ${quickjs_SOURCE_DIR}/LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})
				install(FILES  ${quickjs_SOURCE_DIR}/README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
				target_link_libraries(${JAVASCRIPT} m dl pthread)
			else()
				FetchContent_MakeAvailable(quickjs)
			endif()
			file(COPY ${quickjs_SOURCE_DIR}/quickjs.h DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/include)
			file(COPY ${quickjs_SOURCE_DIR}/quickjs-libc.h DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/include)
		endif()
	else()
		FetchContent_Declare(quickjs
			GIT_REPOSITORY https://github.com/bellard/quickjs.git
			GIT_TAG master
		)
		set(QUICKJS_SOURCES
			${quickjs_SOURCE_DIR}/quickjs.c
			${quickjs_SOURCE_DIR}/libregexp.c
			${quickjs_SOURCE_DIR}/libunicode.c
			${quickjs_SOURCE_DIR}/cutils.c
			${quickjs_SOURCE_DIR}/dtoa.c
			${quickjs_SOURCE_DIR}/quickjs-libc.c
		)
		FetchContent_GetProperties(quickjs)
		if(NOT quickjs_POPULATED)
			FetchContent_Populate(quickjs)
			if(BUILD_SHARED_LIBS)
				add_library(quickjs SHARED ${QUICKJS_SOURCES})
			else()
				add_library(quickjs STATIC ${QUICKJS_SOURCES})
			endif()
			set_property(TARGET quickjs PROPERTY C_STANDARD 99)
			target_compile_definitions(quickjs PRIVATE _GNU_SOURCE CONFIG_VERSION="master")
			target_include_directories(quickjs PRIVATE ${quickjs_SOURCE_DIR})
		endif()
		file(STRINGS ${quickjs_SOURCE_DIR}/VERSION quickjs_h)
		set_target_properties(${JAVASCRIPT} PROPERTIES
			VERSION ${quickjs_h}
			SOVERSION ${quickjs_h}
		)
		install(FILES ${quickjs_SOURCE_DIR}/cutils.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
		install(FILES ${quickjs_SOURCE_DIR}/quickjs.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
		install(FILES ${quickjs_SOURCE_DIR}/quickjs-libc.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
		install(TARGETS ${JAVASCRIPT} EXPORT ${JAVASCRIPT}Config
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
			LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
			ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
		install(EXPORT ${JAVASCRIPT}Config DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${JAVASCRIPT})
		install(FILES ${quickjs_SOURCE_DIR}/LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})
		file(COPY ${quickjs_SOURCE_DIR}/cutils.h DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/include)
		file(COPY ${quickjs_SOURCE_DIR}/quickjs.h DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/include)
		file(COPY ${quickjs_SOURCE_DIR}/quickjs-libc.h DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/include)
		target_link_libraries(${JAVASCRIPT} m dl pthread)
	endif()

	if(BUILD_APP)
		set(app_files	${CMAKE_CURRENT_SOURCE_DIR}/src/main.c)
	endif(BUILD_APP)

	file(GLOB lib_files		${CMAKE_CURRENT_SOURCE_DIR}/src/bow/*.c)
	file(GLOB proto_files 	${CMAKE_CURRENT_SOURCE_DIR}/src/proto/*.c)
	file(GLOB plain_files 	${CMAKE_CURRENT_SOURCE_DIR}/src/plain/*.c)
	file(GLOB mxw_files 	${CMAKE_CURRENT_SOURCE_DIR}/src/mxw/*.c)
	file(GLOB image_files 	${CMAKE_CURRENT_SOURCE_DIR}/src/image/*.c)
	file(GLOB html_files 	${CMAKE_CURRENT_SOURCE_DIR}/src/html/*.c)
	file(GLOB ext_files 	${CMAKE_CURRENT_SOURCE_DIR}/src/ext/*.c)
	file(GLOB common_files 	${CMAKE_CURRENT_SOURCE_DIR}/src/common/*.c)

	if(BUILD_NATIVE_XAW)
		target_compile_definitions(${PROJECT_NAME} PUBLIC NATIVE_XAW=1)
	endif()

	if(BUILD_SHARED_LIBS)
		add_library(${PROJECT_NAME} SHARED
		${lib_files} ${proto_files} ${plain_files} ${mxw_files} ${image_files}
		${html_files} ${ext_files} ${common_files})
	else()
		add_library(${PROJECT_NAME} STATIC
		${lib_files} ${proto_files} ${plain_files} ${mxw_files} ${image_files}
		${html_files} ${ext_files} ${common_files})
	endif()
	target_include_directories(${PROJECT_NAME}
		PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		PUBLIC $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
	target_include_directories(${PROJECT_NAME} PRIVATE $<BUILD_INTERFACE:${quickjs_INCLUDE_DIRS}>)
	target_link_libraries(${PROJECT_NAME} PUBLIC ${JAVASCRIPT} ATHENA::WIDGETS)
endif()

add_library(BOW::WEB ALIAS ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES
	OUTPUT_NAME ${PROJECT_NAME}
	ARCHIVE_OUTPUT_NAME ${PROJECT_NAME}
	EXPORT_NAME ${PROJECT_NAME}
	VERSION  ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR}
)

if(NOT CMAKE_VERSION VERSION_LESS 3.27.0)
	set_target_properties(${PROJECT_NAME} PROPERTIES DLL_NAME_WITH_SOVERSION TRUE)
endif()

if(BUILD_APP)
	add_executable(${PROJECT_NAME}.app ${app_files})
	target_include_directories(${PROJECT_NAME}.app
		PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/bow>
		PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
	target_link_libraries(${PROJECT_NAME}.app ${PROJECT_NAME})
	set_target_properties(${PROJECT_NAME}.app PROPERTIES
		EXPORT_NAME ${PROJECT_NAME}.app
		OUTPUT_NAME ${PROJECT_NAME}.app
		VERSION ${PROJECT_VERSION}
		SOVERSION ${PROJECT_VERSION_MAJOR})
endif(BUILD_APP)

set(_fmt TGZ)
if(WIN32)
  set(_fmt ZIP)
endif()

set(CPACK_GENERATOR ${_fmt})
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_NUGET_COMPONENT_INSTALL ON)
set(CPACK_WIX_COMPONENT_INSTALL ON)
set(CPACK_NSIS_MODIFY_PATH ON)
set(CPACK_COMPONENTS_ALL_IN_ONE_PACKAGE 1)
set(CPACK_VERBATIM_VARIABLES YES)

set(CPACK_PACKAGE_VENDOR "https://github.com/zelang-dev/${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})

set(CMAKE_INSTALL_CONFIG_NAME ${CMAKE_BUILD_TYPE})
set(INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
    CACHE PATH "Location of header files" )

set(SYSCONFIG_INSTALL_DIR ${CMAKE_INSTALL_SYSCONFDIR}/${PROJECT_NAME}
    CACHE PATH "Location of configuration files" )

configure_package_config_file(${PROJECT_NAME}Config.cmake.in
  	${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/"${PROJECT_NAME}"
	PATH_VARS INCLUDE_INSTALL_DIR SYSCONFIG_INSTALL_DIR)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion )

set(CPACK_PACKAGE_DIRECTORY "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/Package")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PROJECT_NAME}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${PROJECT_NAME})

set(CPACK_PACKAGE_CONTACT "lstubbs@zelang.dev")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Lawrence Stubbs ${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${PROJECT__ARCH}")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libglx-dev, libglu1-mesa-dev, libx11-dev, libxt-dev")

set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_COMPONENT_LIBRARIES_DISPLAY_NAME "${PROJECT_NAME}")
set(CPACK_COMPONENT_LIBRARIES_DESCRIPTION ${PROJECT_DESCRIPTION})

set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md)
set(CPACK_RESOURCE_FILE_README ${CMAKE_CURRENT_SOURCE_DIR}/README.md)

set(CPACK_PACKAGE_VENDOR "https://github.com/zelang-dev/${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION ${PROJECT_DESCRIPTION})

set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_URL "https://zelang-dev.github.io/${PROJECT_NAME}/")
set(CPACK_RPM_PACKAGE_ARCHITECTURE "${PROJECT__ARCH}")
set(CPACK_RPM_PACKAGE_REQUIRES "mesa-libGL-devel, mesa-libGLU-devel, libX11-devel, libXt-devel")

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
	${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" )

set(EXPORTER ${PROJECT_NAME} ${JAVASCRIPT})
install(
	TARGETS ${EXPORTER}
	EXPORT "${PROJECT_NAME}Targets"
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(
	EXPORT "${PROJECT_NAME}Targets"
	FILE "${PROJECT_NAME}Targets.cmake"
	NAMESPACE BOW::
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

install(TARGETS ${PROJECT_NAME} DESTINATION lib)
if(BUILD_APP)
	install(TARGETS ${PROJECT_NAME}.app DESTINATION bin)
endif(BUILD_APP)

install(DIRECTORY include DESTINATION include)
set(CPACK_INSTALL_CMAKE_CONFIGURATIONS Release)
