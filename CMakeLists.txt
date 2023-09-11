cmake_minimum_required( VERSION 3.20 )
project( ifcpp-example )
set( CMAKE_CXX_STANDARD 23 )

if( APPLE )
    set( CMAKE_MACOSX_RPATH ON )
endif()

if( MSVC )
    string( APPEND CMAKE_CXX_FLAGS " /Zc:__cplusplus /MP" )
endif()

set( GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE )
set( GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE )
set( GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE )
set( SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE )
set( SPDLOG_BUILD_EXAMPLE_HO OFF CACHE BOOL "" FORCE )
set( SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE )
set( SPDLOG_BUILD_TESTS_HO OFF CACHE BOOL "" FORCE )
set( glew-cmake_BUILD_SHARED OFF CACHE BOOL "" FORCE )

add_subdirectory( third_party/glew-cmake )
add_subdirectory( third_party/glfw )
add_subdirectory( third_party/glm )
add_subdirectory( third_party/ifcpp )
add_subdirectory( third_party/spdlog )

add_definitions( -DGLEW_STATIC )
add_definitions( -DIFCQUERY_STATIC_LIB )
add_definitions( -D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS )
add_definitions( -D_LIBCPP_DISABLE_DEPRECATION_WARNINGS )
find_package( OpenGL REQUIRED )

set( SOURCES
        src/main.cpp )

set( HEADERS
        src/csgjs.h
        src/earcut.hpp
        src/Adapter.h
        src/Engine.h )

add_executable( ${PROJECT_NAME} ${SOURCES} ${HEADERS} )
target_link_libraries( ${PROJECT_NAME} PRIVATE OpenGL::GL ifcpp glfw libglew_static glm spdlog::spdlog_header_only )
