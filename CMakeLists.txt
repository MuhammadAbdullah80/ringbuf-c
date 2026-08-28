cmake_minimum_required(VERSION 3.14)

project(ringbuf
    VERSION 0.1.0
    DESCRIPTION "A fixed-capacity circular byte buffer"
    LANGUAGES C)

# Only set these when ringbuf is the top-level project. A parent adding this
# with add_subdirectory() owns its own standard and warning policy, and
# stamping over it from a dependency is rude.
if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_C_STANDARD 99)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_C_EXTENSIONS OFF)
endif()

add_library(ringbuf STATIC ringbuf.c)
add_library(ringbuf::ringbuf ALIAS ringbuf)

target_compile_features(ringbuf PUBLIC c_std_99)

# BUILD_INTERFACE/INSTALL_INTERFACE so the target works both from a build tree
# and after install, without the consumer knowing which they have.
target_include_directories(ringbuf PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>)

# Warnings are PRIVATE: they apply to building ringbuf itself and are not
# inflicted on anything that links it.
if(MSVC)
    target_compile_options(ringbuf PRIVATE /W4)
else()
    target_compile_options(ringbuf PRIVATE -Wall -Wextra -pedantic)
endif()

set_target_properties(ringbuf PROPERTIES
    PUBLIC_HEADER ringbuf.h
    VERSION ${PROJECT_VERSION})

# Tests only when top-level, so a parent project does not inherit them.
include(CTest)
if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
    add_executable(ringbuf-test test/test_ringbuf.c)
    target_link_libraries(ringbuf-test PRIVATE ringbuf)
    add_test(NAME ringbuf-test COMMAND ringbuf-test)
endif()

include(GNUInstallDirs)
install(TARGETS ringbuf
    EXPORT ringbufTargets
    ARCHIVE       DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

set(RINGBUF_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/ringbuf)

install(EXPORT ringbufTargets
    FILE        ringbufTargets.cmake
    NAMESPACE   ringbuf::
    DESTINATION ${RINGBUF_CMAKE_DIR})

# The targets file alone is not enough: find_package(ringbuf) looks for
# ringbufConfig.cmake, and without one it reports the package as missing even
# though everything else installed correctly. The config file is what pulls the
# targets file in.
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ringbufConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ringbufConfig.cmake
    INSTALL_DESTINATION ${RINGBUF_CMAKE_DIR})

# SameMinorVersion: patch releases are drop-in, a new minor may add symbols a
# consumer pinned against an older one will not find.
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ringbufConfigVersion.cmake
    VERSION       ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ringbufConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ringbufConfigVersion.cmake
    DESTINATION ${RINGBUF_CMAKE_DIR})
