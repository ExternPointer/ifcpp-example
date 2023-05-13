cmake_minimum_required( VERSION 3.20 )
project( ifcpp-example )
set( CMAKE_CXX_STANDARD 23 )

add_subdirectory( third_party/glew-cmake )
add_subdirectory( third_party/glfw )
add_subdirectory( third_party/glm )
add_subdirectory( third_party/ifcpp )

add_definitions( -DGLEW_STATIC )
add_definitions( -DIFCQUERY_STATIC_LIB )
add_definitions( -D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS )
add_definitions( -D_LIBCPP_DISABLE_DEPRECATION_WARNINGS )
set( GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE )
set( GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE )
set( GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE )
find_package( OpenGL REQUIRED )

set( SOURCES
        src/main.cpp )

set( HEADERS
        src/csg.hpp
        src/earcut.hpp
        src/Adapter.h )

add_executable( ${PROJECT_NAME} ${SOURCES} )
target_link_libraries( ${PROJECT_NAME} PRIVATE OpenGL::GL ifcpp glfw libglew_static glm )
