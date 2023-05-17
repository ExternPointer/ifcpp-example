#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <iostream>

#define CSGJSCPP_IMPLEMENTATION
#include "csgjs.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/matrix.hpp>

#include <ifcpp/Model/BuildingModel.h>
#include <ifcpp/Reader/ReaderSTEP.h>

#include "Adapter.h"
#include <ifcpp/Geometry/GeometryGenerator.h>


const char* vertexSource = "#version 330                                                               \n"
                           "layout ( location = 0 ) in vec3 a_position;                                \n"
                           "layout ( location = 1 ) in vec4 a_vertex_color;                            \n"
                           "out vec4 v_vertex_color;                                                   \n"
                           "void main() {                                                              \n"
                           "   v_vertex_color = a_vertex_color;                                        \n"
                           "   gl_Position = vec4( a_position, 1 );                                    \n"
                           "}                                                                          \n";

const char* geometrySource = "#version 330                                                               \n"
                             "layout ( triangles ) in;                                                   \n"
                             "layout ( triangle_strip, max_vertices = 3 ) out;                           \n"
                             "in vec4 v_vertex_color[];                                                  \n"
                             "out vec4 v_color;                                                          \n"
                             "uniform mat4 m_transform;                                                  \n"
                             "void main() {                                                              \n"
                             "   vec3 a = gl_in[0].gl_Position.xyz - gl_in[1].gl_Position.xyz;           \n"
                             "   vec3 b = gl_in[2].gl_Position.xyz - gl_in[1].gl_Position.xyz;           \n"
                             "   vec3 v_normal = normalize( cross(a, b) );                               \n"
                             "   vec3 v_lightDirection = normalize( vec3( 0.2, 0.5, -1 ) );              \n"
                             "   float diffuse = ( dot( v_normal, v_lightDirection ) + 1 ) * 0.5;        \n"
                             "   vec3 diffuseLight = diffuse * v_vertex_color[0].rgb * 0.8;              \n"
                             "   vec3 ambientLight = v_vertex_color[0].rgb * 0.2;                        \n"
                             "   v_color = vec4( ambientLight + diffuseLight, v_vertex_color[0].a );     \n"
                             "   gl_Position = m_transform * gl_in[0].gl_Position;                       \n"
                             "   EmitVertex();                                                           \n"
                             "   gl_Position = m_transform * gl_in[1].gl_Position;                       \n"
                             "   EmitVertex();                                                           \n"
                             "   gl_Position = m_transform * gl_in[2].gl_Position;                       \n"
                             "   EmitVertex();                                                           \n"
                             "   EndPrimitive();                                                         \n"
                             "}                                                                          \n";

const char* fragmentSource = "#version 330                                                               \n"
                             "in vec4 v_color;                                                           \n"
                             "out vec4 FragColor;                                                        \n"
                             "void main() {                                                              \n"
                             "   FragColor = v_color;                                                    \n"
                             "}                                                                          \n";
bool wireframe = false;

int main() {
    // Read IFC, generate geometry and VAO
    auto ifcModel = std::make_shared<BuildingModel>();
    auto reader = std::make_shared<ReaderSTEP>();
    auto parsingStartTime = std::chrono::high_resolution_clock::now();
    reader->loadModelFromFile( "12.ifc", ifcModel );
    auto parsingFinishTime = std::chrono::high_resolution_clock::now();
    auto parsingTime = parsingFinishTime - parsingStartTime;
    std::cout << "parsing: " << std::chrono::duration_cast<std::chrono::milliseconds>( parsingTime ).count() << " milliseconds ("
              << std::chrono::duration_cast<std::chrono::seconds>( parsingTime ).count() << " seconds)" << std::endl;


    auto parameters = std::make_shared<ifcpp::Parameters>( ifcpp::Parameters {
        1e-6,
        14,
        5,
        10000,
        4,
    } );
    auto adapter = std::make_shared<Adapter>();
    auto styleConverter = std::make_shared<ifcpp::StyleConverter>();
    auto geomUtils = std::make_shared<ifcpp::GeomUtils<csgjscpp::Vector>>( parameters );
    auto primitivesConverter = std::make_shared<ifcpp::PrimitiveTypesConverter<csgjscpp::Vector>>();
    auto splineConverter = std::make_shared<ifcpp::SplineConverter<csgjscpp::Vector>>( primitivesConverter, geomUtils, parameters );
    auto curveConverter = std::make_shared<ifcpp::CurveConverter<csgjscpp::Vector>>( primitivesConverter, geomUtils, splineConverter, parameters );
    auto extruder = std::make_shared<ifcpp::Extruder<csgjscpp::Vector>>( geomUtils, parameters );
    auto profileConverter = std::make_shared<ifcpp::ProfileConverter<csgjscpp::Vector>>( curveConverter, geomUtils, primitivesConverter, parameters );
    auto geometryConverter = std::make_shared<ifcpp::GeometryConverter<csgjscpp::Vector>>( curveConverter, primitivesConverter, splineConverter, geomUtils,
                                                                                           extruder, profileConverter, parameters );
    auto solidConverter = std::make_shared<ifcpp::SolidConverter<Adapter>>( primitivesConverter, curveConverter, profileConverter, extruder, geometryConverter,
                                                                            adapter, geomUtils, styleConverter, parameters );
    auto geometryGenerator =
        std::make_shared<ifcpp::GeometryGenerator<Adapter>>( ifcModel, adapter, curveConverter, extruder, geometryConverter, geomUtils, primitivesConverter,
                                                             profileConverter, solidConverter, splineConverter, styleConverter, parameters );

    // auto generator = std::make_shared<ifcpp::GeometryGenerator<ifcpp::Adapter>>(ifcModel, adapter);
    auto generationStartTime = std::chrono::high_resolution_clock::now();
    auto entities = geometryGenerator->GenerateGeometry();
    auto generationFinishTime = std::chrono::high_resolution_clock::now();
    auto generationTime = generationFinishTime - generationStartTime;
    std::cout << "geometry generation: " << std::chrono::duration_cast<std::chrono::milliseconds>( generationTime ).count() << " milliseconds ("
              << std::chrono::duration_cast<std::chrono::seconds>( generationTime ).count() << " seconds)" << std::endl;

    glm::vec<3, double, glm::defaultp> center( 0, 0, 0 );
    std::vector<float> vbo;
    std::vector<unsigned int> ibo;
    std::vector<unsigned int> iboTransparent;
    std::vector<unsigned int> cbo;
    for( auto& e: entities ) {
        for( auto& m: e->m_meshes ) {
            if( m->m_color == 0 ) {
                // No material
                continue;
            }
            bool opaque = ( m->m_color >> 24 ) == 255;
            for( const auto& p: m->m_polygons ) {
                if( opaque ) {
                    for( int i = 1; i < p.vertices.size() - 1; i++ ) {
                        ibo.push_back( vbo.size() / 3 );
                        ibo.push_back( i + vbo.size() / 3 );
                        ibo.push_back( i + 1 + vbo.size() / 3 );
                    }
                } else {
                    for( int i = 1; i < p.vertices.size() - 1; i++ ) {
                        iboTransparent.push_back( vbo.size() / 3 );
                        iboTransparent.push_back( i + vbo.size() / 3 );
                        iboTransparent.push_back( i + 1 + vbo.size() / 3 );
                    }
                }
                for( const auto& v: p.vertices ) {
                    center = center + glm::vec<3, double, glm::defaultp>( v.pos.x, v.pos.y, v.pos.z );
                    vbo.push_back( v.pos.x );
                    vbo.push_back( v.pos.y );
                    vbo.push_back( v.pos.z );
                    cbo.push_back( m->m_color );
                }
            }
        }
    }
    unsigned int transparentStartIdx = ibo.size();
    std::copy( iboTransparent.begin(), iboTransparent.end(), std::back_inserter( ibo ) );
    center = center / (double)( vbo.size() / 3 );
    //  Create window
    glfwInit();
    glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 3 );
    glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE );
    glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
    GLFWwindow* window = glfwCreateWindow( 800, 800, "ifcpp-example", nullptr, nullptr );
    if( window == nullptr ) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent( window );
    if( glewInit() != GLEW_OK ) {
        fprintf( stderr, "Failed to initialize GLEW\n" );
        return -1;
    }
    glViewport( 0, 0, 800, 800 );
    glfwSetInputMode( window, GLFW_STICKY_KEYS, GL_TRUE );
    // Compile shaders
    GLuint vertex = glCreateShader( GL_VERTEX_SHADER );
    glShaderSource( vertex, 1, &vertexSource, nullptr );
    glCompileShader( vertex );
    GLuint geometryShader = glCreateShader( GL_GEOMETRY_SHADER );
    glShaderSource( geometryShader, 1, &geometrySource, nullptr );
    glCompileShader( geometryShader );
    GLuint fragment = glCreateShader( GL_FRAGMENT_SHADER );
    glShaderSource( fragment, 1, &fragmentSource, nullptr );
    glCompileShader( fragment );
    auto program = glCreateProgram();
    glAttachShader( program, vertex );
    glAttachShader( program, geometryShader );
    glAttachShader( program, fragment );
    glLinkProgram( program );
    glDeleteShader( geometryShader );
    glDeleteShader( vertex );
    glDeleteShader( fragment );
    // Create buffers
    unsigned int vaoId;
    unsigned int vboId;
    unsigned int cboId;
    unsigned int iboId;
    glGenVertexArrays( 1, &vaoId );
    glBindVertexArray( vaoId );
    glGenBuffers( 1, &vboId );
    glGenBuffers( 1, &cboId );
    glGenBuffers( 1, &iboId );
    // Fill buffers
    glBindBuffer( GL_ARRAY_BUFFER, vboId );
    glBufferData( GL_ARRAY_BUFFER, (int)( sizeof( float ) * vbo.size() ), &vbo[ 0 ], GL_STATIC_DRAW );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );
    glBindBuffer( GL_ARRAY_BUFFER, cboId );
    glBufferData( GL_ARRAY_BUFFER, (int)( sizeof( unsigned int ) * cbo.size() ), &cbo[ 0 ], GL_STATIC_DRAW );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, iboId );
    glBufferData( GL_ELEMENT_ARRAY_BUFFER, (int)( sizeof( unsigned int ) * ibo.size() ), &ibo[ 0 ], GL_STATIC_DRAW );
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
    // Camera position
    glm::vec3 position( center );
    glm::vec3 viewDirection = glm::normalize( glm::vec3 { -1, -1, -1 } );
    // OpenGL setup
    glClearColor( 0.07f, 0.13f, 0.17f, 1.0f );
    glEnable( GL_DEPTH_TEST );
    glEnable( GL_CULL_FACE );
    //    glDisable( GL_CULL_FACE );
    //    glDepthFunc(GL_LEQUAL);
    // Main loop
    while( !glfwWindowShouldClose( window ) ) {
        // Render VAO
        if( wireframe ) {
            glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
        } else {
            glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
        }
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
        glUseProgram( program );
        auto projectionMatrix = glm::perspective( glm::radians( 60.0f ), 800.0f / 800.0f, 0.1f, 500.0f );
        auto viewMatrix = glm::lookAt( position, position + viewDirection, glm::vec3 { 0.0f, 0.0f, 1.0f } );
        auto mvp = projectionMatrix * viewMatrix;
        glUniformMatrix4fv( glGetUniformLocation( program, "m_transform" ), 1, GL_FALSE, glm::value_ptr( mvp ) );
        glBindBuffer( GL_ARRAY_BUFFER, vboId );
        glEnableVertexAttribArray( 0 );
        glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, nullptr );
        glBindBuffer( GL_ARRAY_BUFFER, cboId );
        glEnableVertexAttribArray( 1 );
        glVertexAttribPointer( 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, nullptr );
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, iboId );

        glDrawElements( GL_TRIANGLES, transparentStartIdx, GL_UNSIGNED_INT, nullptr );

        if( !iboTransparent.empty() ) {
            glEnable( GL_BLEND );
            glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
            glDrawElements( GL_TRIANGLES, ibo.size() - transparentStartIdx, GL_UNSIGNED_INT, (void*)( transparentStartIdx * sizeof( unsigned int ) ) );
            glDisable( GL_BLEND );
        }

        glBindBuffer( GL_ARRAY_BUFFER, 0 );
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
        glDisableVertexAttribArray( 0 );
        glDisableVertexAttribArray( 1 );
        glUseProgram( 0 );


        // Movement
        auto rightDir = glm::normalize( glm::cross( viewDirection, { 0, 0, 1 } ) );
        glfwSetKeyCallback( window, []( GLFWwindow* window, int key, int scancode, int action, int mods ) {
            if( key == GLFW_KEY_Z && action == GLFW_PRESS ) {
                wireframe = !wireframe;
            }
        } );
        if( glfwGetKey( window, GLFW_KEY_W ) ) {
            position += viewDirection * 0.05f;
        }
        if( glfwGetKey( window, GLFW_KEY_S ) ) {
            position -= viewDirection * 0.05f;
        }
        if( glfwGetKey( window, GLFW_KEY_A ) ) {
            position -= rightDir * 0.05f;
        }
        if( glfwGetKey( window, GLFW_KEY_D ) ) {
            position += rightDir * 0.05f;
        }
        if( glfwGetKey( window, GLFW_KEY_LEFT_SHIFT ) ) {
            position += glm::vec3( 0, 0, 1 ) * 0.05f;
        }
        if( glfwGetKey( window, GLFW_KEY_LEFT_CONTROL ) ) {
            position -= glm::vec3( 0, 0, 1 ) * 0.05f;
        }
        if( glfwGetKey( window, GLFW_KEY_LEFT ) ) {
            glm::mat4 rot = glm::rotate( glm::mat4( 1.0f ), glm::radians( 0.5f ), { 0, 0, 1 } );
            viewDirection = rot * glm::vec4( viewDirection.x, viewDirection.y, viewDirection.z, 0.0f );
        }
        if( glfwGetKey( window, GLFW_KEY_RIGHT ) ) {
            glm::mat4 rot = glm::rotate( glm::mat4( 1.0f ), glm::radians( -0.5f ), { 0, 0, 1 } );
            viewDirection = rot * glm::vec4( viewDirection.x, viewDirection.y, viewDirection.z, 0.0f );
        }
        if( glfwGetKey( window, GLFW_KEY_UP ) ) {
            glm::mat4 rot = glm::rotate( glm::mat4( 1.0f ), glm::radians( 0.5f ), rightDir );
            viewDirection = rot * glm::vec4( viewDirection.x, viewDirection.y, viewDirection.z, 0.0f );
        }
        if( glfwGetKey( window, GLFW_KEY_DOWN ) ) {
            glm::mat4 rot = glm::rotate( glm::mat4( 1.0f ), glm::radians( -0.5f ), rightDir );
            viewDirection = rot * glm::vec4( viewDirection.x, viewDirection.y, viewDirection.z, 0.0f );
        }
        glfwSwapBuffers( window );
        glfwPollEvents();
    }
    glfwDestroyWindow( window );
    glfwTerminate();
    return 0;
}
