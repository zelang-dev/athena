cmake_minimum_required (VERSION 3.22...5.1.0)

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
	cmake_policy(SET CMP0091 NEW)
endif()

project(athena
	VERSION 0.6.0
	DESCRIPTION "X11/Athena `More` Widget Toolset for rapid GUI Applications"
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
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_SHARED_LIBS "Build the library as a shared (dynamically-linked) " OFF)
option(BUILD_NATIVE_XAW "Linux: Build using Native X11 Athena Widget Toolkit " OFF)
option(BUILD_QUICKJS_NG "Build `fork` QuickJS-NG, not QuickJS, for `JavaScript` runtime" ON)
option(BUILD_APP "Build standalone `Browser` App" OFF)

if (WIN32 OR APPLE)
    message(FATAL_ERROR "Linux Only!")
else()
    execute_process(
      COMMAND ${CMAKE_C_COMPILER} -dumpmachine
      RESULT_VARIABLE RESULT
      OUTPUT_VARIABLE PROJECT__ARCH
      ERROR_QUIET
    )
    if (RESULT)
      message(FATAL_ERROR "Failed to determine target architecture triplet: ${RESULT}")
    endif()

    string(REGEX MATCH "([^-]+).*" ARCH_MATCH ${PROJECT__ARCH})
    if (NOT CMAKE_MATCH_1 OR NOT ARCH_MATCH)
      message(FATAL_ERROR "Failed to match the target architecture triplet: ${PROJECT__ARCH}")
    endif()

    set(PROJECT__ARCH ${CMAKE_MATCH_1})
	message(STATUS "Target architecture - ${PROJECT__ARCH}")

	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -rdynamic -no-pie -D USE_DEBUG")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-format -fno-strict-aliasing -D NeedFunctionPrototypes")

	find_package(opentls QUIET)
	if(NOT opentls_FOUND)
		FetchContent_Declare(opentls
			URL https://github.com/zelang-dev/opentls/archive/refs/heads/main.zip
			URL_MD5 b670326e488f0911aabd7dec311a4ceb
		)
		FetchContent_MakeAvailable(opentls)
	endif()

	find_package(OpenGL QUIET COMPONENTS GLX)
	find_package(TIFF REQUIRED)
	find_package(LIBAVIF REQUIRED COMPONENTS AOM)
	find_package(WebP REQUIRED)
	find_package(FFmpeg QUIET)
	if(NOT FFmpeg_FOUND)
		message(FATAL_ERROR "FFmpeg not found, installation required:\n
		`sudo apt install libavcodec-dev libavformat-dev libswscale-dev libavutil-dev`\n
		`dnf install ffmpeg-devel`\n")
	endif()
	find_package(ALSA QUIET)
	if(NOT ALSA_FOUND)
		message(FATAL_ERROR "ALSA/asound not found, installation required:\n
		`sudo apt-get install libasound2-dev`\n
		`dnf install alsa-lib-devel`\n")
	endif()
	find_package(Threads REQUIRED)
	find_package(ZLIB REQUIRED)

	if(BUILD_QUICKJS_NG)
		find_package(quickjs QUIET)
		if(NOT quickjs_FOUND)
			set(ENV{QJS_BUILD_EXAMPLES} "OFF")
			set(ENV{QJS_BUILD_LIBC} "ON")
			FetchContent_Declare(quickjs
				URL https://github.com/quickjs-ng/quickjs/archive/refs/tags/v0.17.0.zip
				URL_MD5 502b324207f8080d8fe76ea27e809537
			)
			FetchContent_MakeAvailable(quickjs)
		endif()
		set(JAVASCRIPT qjs)
	else()
		FetchContent_Declare(quick_js
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

		FetchContent_GetProperties(quick_js)
		if(NOT quick_js_POPULATED)
			FetchContent_Populate(quick_js)
			if(BUILD_SHARED_LIBS)
				add_library(quick_js SHARED ${QUICKJS_SOURCES})
			else()
				add_library(quick_js STATIC ${QUICKJS_SOURCES})
			endif()
			target_compile_definitions(quick_js PRIVATE _GNU_SOURCE CONFIG_VERSION="master")
			#target_include_directories(quick_js PUBLIC ${quickjs_SOURCE_DIR})
		endif()
		set(JAVASCRIPT quick_js)
	endif()

	file(GLOB mowitz_files
		${CMAKE_CURRENT_SOURCE_DIR}/src/Mowitz/*.c)
	set(athena_files
		${CMAKE_CURRENT_SOURCE_DIR}/src/athena.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/libsmallregex.c)
	file(GLOB Xaw95_files
		${CMAKE_CURRENT_SOURCE_DIR}/src/Xaw95/*.c)

	if(BUILD_SHARED_LIBS)
		add_library(Xaw95 SHARED ${Xaw95_files})
		add_library(${PROJECT_NAME} SHARED ${athena_files} ${mowitz_files})
	else()
		add_library(Xaw95 STATIC ${Xaw95_files})
		add_library(${PROJECT_NAME} STATIC ${athena_files} ${mowitz_files})
	endif()
	target_include_directories(${PROJECT_NAME}
		PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		PUBLIC $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

	find_package(X11 REQUIRED COMPONENTS Xmu Xt)
	if(BUILD_NATIVE_XAW)
		find_library(X11_Xaw_LIB Xaw)
	endif(BUILD_NATIVE_XAW)

	target_include_directories(Xaw95
		PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		PUBLIC $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

	target_include_directories(${PROJECT_NAME}
		PRIVATE $<BUILD_INTERFACE:${WebP_INCLUDE_DIRS}
		$<INSTALL_INTERFACE:${WebP_INCLUDE_DIRS})

	target_include_directories(${PROJECT_NAME}
		PRIVATE $<BUILD_INTERFACE:${libavif_INCLUDE_DIRS}
		$<INSTALL_INTERFACE:${libavif_INCLUDE_DIRS})

	target_include_directories(${PROJECT_NAME}
		PRIVATE $<BUILD_INTERFACE:${TIFF_INCLUDE_DIRS}
		$<INSTALL_INTERFACE:${TIFF_INCLUDE_DIRS})

	target_include_directories(${PROJECT_NAME}
		PRIVATE $<BUILD_INTERFACE:${${FFMPEG_INCLUDE_DIRS}}
		$<INSTALL_INTERFACE:${${FFMPEG_INCLUDE_DIRS}})

	target_include_directories(${PROJECT_NAME}
		PRIVATE $<BUILD_INTERFACE:${${ALSA_INCLUDE_DIRS}}
		$<INSTALL_INTERFACE:${${ALSA_INCLUDE_DIRS}})

	target_include_directories(${PROJECT_NAME}
		PRIVATE $<BUILD_INTERFACE:${OPENSSL_INCLUDE_DIR}
		$<INSTALL_INTERFACE:${OPENSSL_INCLUDE_DIR})

	if(BUILD_QUICKJS_NG)
		target_include_directories(${PROJECT_NAME}
			PRIVATE $<BUILD_INTERFACE:${quickjs_INCLUDE_DIRS}
			$<INSTALL_INTERFACE:${quickjs_INCLUDE_DIRS})
	endif()

	if(BUILD_NATIVE_XAW)
		target_compile_definitions(${PROJECT_NAME}
			PUBLIC MOWITZ_DATA="${CMAKE_INSTALL_FULL_DATAROOTDIR}/athena"
			PUBLIC DEFAULT_PIXPATH="${CMAKE_INSTALL_FULL_DATAROOTDIR}/athena/pixmaps"
			PUBLIC NATIVE_XAW=1)
		target_link_libraries(${PROJECT_NAME}
			Xaw X11::Xmu X11::Xt Xpm X11::X11 OpenGL::GL OpenGL::GLU
			#Xaw X11::Xmu X11::Xt Xpm X11::X11
			${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY} opentls crypto dl m Threads::Threads
			TIFF::TIFF avif ${AOM_LIBRARIES} ${WebP_LIBRARY} ${WebP_DECODER_LIBRARY}
			ALSA::ALSA avformat avcodec swscale avutil swresample ${FFMPEG_LIBRARIES}
			${JAVASCRIPT}
			)
	else()
		target_compile_definitions(${PROJECT_NAME}
			PUBLIC MOWITZ_DATA="${CMAKE_INSTALL_FULL_DATAROOTDIR}/athena"
			PUBLIC DEFAULT_PIXPATH="${CMAKE_INSTALL_FULL_DATAROOTDIR}/athena/pixmaps")
		target_link_libraries(${PROJECT_NAME}
			Xaw95 X11::Xmu X11::Xt Xpm X11::X11 OpenGL::GL OpenGL::GLU
			#Xaw95 X11::Xmu X11::Xt Xpm X11::X11
			${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY} opentls crypto dl m Threads::Threads
			TIFF::TIFF avif ${AOM_LIBRARIES} ${WebP_LIBRARY} ${WebP_DECODER_LIBRARY}
			ALSA::ALSA avformat avcodec swscale avutil swresample ${FFMPEG_LIBRARIES}
			${JAVASCRIPT}
			)
	endif(BUILD_NATIVE_XAW)
endif()

add_library(ATHENA::WIDGETS ALIAS ${PROJECT_NAME})
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

add_subdirectory(bow)

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
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Lawrence Stubbs <${CPACK_PACKAGE_CONTACT}>")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${PROJECT__ARCH}")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libasound2-dev,libavcodec-dev,libavformat-dev,libswscale-dev,libavutil-dev,libglx-dev,libglu1-mesa-dev,libx11-dev,libxt-dev")

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
set(CPACK_RPM_PACKAGE_REQUIRES "alsa-lib-devel,ffmpeg-devel,mesa-libGL-devel,mesa-libGLU-devel,libX11-devel,libXt-devel")

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
	${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" )

if(BUILD_NATIVE_XAW)
	set(EXPORTER ${PROJECT_NAME} ${JAVASCRIPT})
else()
	set(EXPORTER ${PROJECT_NAME} ${JAVASCRIPT} Xaw95)
endif(BUILD_NATIVE_XAW)

message("-- Linux: Location for `fonts, xpm` and other required files:\t${CMAKE_INSTALL_FULL_DATAROOTDIR}/athena")
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
	NAMESPACE ATHENA::
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

if(NOT EXISTS "${BUILD_DIR}/share")
	file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/share DESTINATION ${BUILD_DIR})
endif()

install(TARGETS ${PROJECT_NAME} DESTINATION lib)
install(DIRECTORY include DESTINATION include)
install(DIRECTORY share DESTINATION share)
set(CPACK_INSTALL_CMAKE_CONFIGURATIONS Release)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    option(BUILD_EXAMPLES "whether or not examples should be built" ON)

    if(BUILD_EXAMPLES)
        add_subdirectory(examples)
    endif()
endif()
