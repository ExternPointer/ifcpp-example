cmake_minimum_required( VERSION 3.20 )
project( ifcpp-example )
set( CMAKE_CXX_STANDARD 23 )

add_subdirectory( third_party/glew-cmake )
add_subdirectory( third_party/glfw )
add_subdirectory( third_party/glm )
add_subdirectory( third_party/ifcpp )

add_definitions( -DGLEW_STATIC )
add_definitions( -DIFCQUERY_STATIC_LIB )
find_package( OpenGL REQUIRED )

set( SOURCES
        src/main.cpp )

add_executable( ${PROJECT_NAME} ${SOURCES} )
target_link_libraries( ${PROJECT_NAME} PRIVATE ${OPENGL_LIBRARY} ifcpp glfw libglew_static glm )
