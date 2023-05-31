#pragma once

#include <memory>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include "Adapter.h"


const char* vertexSource =
    "#version 330                                                                                                                                           \n"
    "layout ( location = 0 ) in vec3 a_position;                                                                                                            \n"
    "layout ( location = 1 ) in vec4 a_vertex_color;                                                                                                        \n"
    "out vec4 v_vertex_color;                                                                                                                               \n"
    "void main() {                                                                                                                                          \n"
    "   v_vertex_color = a_vertex_color;                                                                                                                    \n"
    "   gl_Position = vec4( a_position, 1 );                                                                                                                \n"
    "}                                                                                                                                                      \n";

const char* geometrySource =
    "#version 330                                                                                                                                           \n"
    "layout ( triangles ) in;                                                                                                                               \n"
    "layout ( triangle_strip, max_vertices = 3 ) out;                                                                                                       \n"
    "in vec4 v_vertex_color[];                                                                                                                              \n"
    "out vec4 v_color;                                                                                                                                      \n"
    "uniform mat4 m_transform;                                                                                                                              \n"
    "void main() {                                                                                                                                          \n"
    "   vec3 a = gl_in[0].gl_Position.xyz - gl_in[1].gl_Position.xyz;                                                                                       \n"
    "   vec3 b = gl_in[2].gl_Position.xyz - gl_in[1].gl_Position.xyz;                                                                                       \n"
    "   vec3 v_normal = normalize( cross(a, b) );                                                                                                           \n"
    "   vec3 v_lightDirection = normalize( vec3( 0.2, 0.5, -1 ) );                                                                                          \n"
    "   float diffuse = ( dot( v_normal, v_lightDirection ) + 1 ) * 0.5;                                                                                    \n"
    "   vec3 diffuseLight = diffuse * v_vertex_color[0].rgb * 0.8;                                                                                          \n"
    "   vec3 ambientLight = v_vertex_color[0].rgb * 0.2;                                                                                                    \n"
    "   v_color = vec4( ambientLight + diffuseLight, v_vertex_color[0].a );                                                                                 \n"
    "   gl_Position = m_transform * gl_in[0].gl_Position;                                                                                                   \n"
    "   EmitVertex();                                                                                                                                       \n"
    "   gl_Position = m_transform * gl_in[1].gl_Position;                                                                                                   \n"
    "   EmitVertex();                                                                                                                                       \n"
    "   gl_Position = m_transform * gl_in[2].gl_Position;                                                                                                   \n"
    "   EmitVertex();                                                                                                                                       \n"
    "   EndPrimitive();                                                                                                                                     \n"
    "}                                                                                                                                                      \n";

const char* fragmentSource =
    "#version 330                                                                                                                                           \n"
    "in vec4 v_color;                                                                                                                                       \n"
    "out vec4 FragColor;                                                                                                                                    \n"
    "void main() {                                                                                                                                          \n"
    "   FragColor = v_color;                                                                                                                                \n"
    "}                                                                                                                                                      \n";



const float moveSpeed = 5;
const float rotateViewSpeed = 2;

GLFWwindow* window = nullptr;
unsigned int vaoId;
unsigned int vboId;
unsigned int cboId;
unsigned int iboId;
int iboSize;
int iboTransparentStartIdx;
unsigned int program;
bool wireframeMode = false;

glm::vec3 cameraPosition;
float horizontalAngle;
float verticalAngle;
glm::vec3 viewDir;
glm::vec3 upDir;
glm::vec3 rightDir;



void SendToGpu( const std::vector<std::shared_ptr<Entity>>& entities ) {
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
            bool isTransparent = ( m->m_color >> 24 ) != 255;
            for( const auto& p: m->m_polygons ) {
                auto targetIbo = &ibo;
                if( isTransparent ) {
                    targetIbo = &iboTransparent;
                }
                for( int i = 1; i < p.vertices.size() - 1; i++ ) {
                    targetIbo->push_back( vbo.size() / 3 );
                    targetIbo->push_back( i + vbo.size() / 3 );
                    targetIbo->push_back( i + 1 + vbo.size() / 3 );
                }
                for( const auto& v: p.vertices ) {
                    center = center + glm::vec<3, double, glm::defaultp>( v.x, v.y, v.z );
                    vbo.push_back( (float)v.x );
                    vbo.push_back( (float)v.y );
                    vbo.push_back( (float)v.z );
                    cbo.push_back( m->m_color );
                }
            }
        }
    }
    iboTransparentStartIdx = (int)ibo.size();
    std::copy( iboTransparent.begin(), iboTransparent.end(), std::back_inserter( ibo ) );
    iboSize = (int)ibo.size();

    center = center / ( (double)vbo.size() / 3 );

    // Camera position
    cameraPosition = center;
    horizontalAngle = 0;
    verticalAngle = 0;

    glDeleteBuffers( 1, &vboId );
    glDeleteBuffers( 1, &cboId );
    glDeleteBuffers( 1, &iboId );
    glGenBuffers( 1, &vboId );
    glGenBuffers( 1, &cboId );
    glGenBuffers( 1, &iboId );

    glBindBuffer( GL_ARRAY_BUFFER, vboId );
    glBufferData( GL_ARRAY_BUFFER, (int)( sizeof( float ) * vbo.size() ), vbo.data(), GL_STATIC_DRAW );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );
    glBindBuffer( GL_ARRAY_BUFFER, cboId );
    glBufferData( GL_ARRAY_BUFFER, (int)( sizeof( unsigned int ) * cbo.size() ), cbo.data(), GL_STATIC_DRAW );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, iboId );
    glBufferData( GL_ELEMENT_ARRAY_BUFFER, (int)( sizeof( unsigned int ) * ibo.size() ), ibo.data(), GL_STATIC_DRAW );
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
}


void InitEngine() {
    //  Create window
    glfwInit();
    glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 3 );
    glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE );
    glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
    window = glfwCreateWindow( 800, 800, "ifcpp-example", nullptr, nullptr );
    if( window == nullptr ) {
        spdlog::error( "Failed to create GLFW window" );
        glfwTerminate();
        exit( -1 );
    }
    glfwMakeContextCurrent( window );
    if( glewInit() != GLEW_OK ) {
        spdlog::error( "Failed to initialize GLEW" );
        exit( -1 );
    }
    glViewport( 0, 0, 800, 800 );
    glfwSetInputMode( window, GLFW_STICKY_KEYS, GL_TRUE );

    // Compile shaders
    GLuint vertex = glCreateShader( GL_VERTEX_SHADER );
    glShaderSource( vertex, 1, &vertexSource, nullptr );
    glCompileShader( vertex );
    GLuint geometry = glCreateShader( GL_GEOMETRY_SHADER );
    glShaderSource( geometry, 1, &geometrySource, nullptr );
    glCompileShader( geometry );
    GLuint fragment = glCreateShader( GL_FRAGMENT_SHADER );
    glShaderSource( fragment, 1, &fragmentSource, nullptr );
    glCompileShader( fragment );
    program = glCreateProgram();
    glAttachShader( program, vertex );
    glAttachShader( program, geometry );
    glAttachShader( program, fragment );
    glLinkProgram( program );
    glDetachShader( program, vertex );
    glDetachShader( program, geometry );
    glDetachShader( program, fragment );
    glDeleteShader( geometry );
    glDeleteShader( vertex );
    glDeleteShader( fragment );

    // Create buffers
    glGenVertexArrays( 1, &vaoId );
    glGenBuffers( 1, &vboId );
    glGenBuffers( 1, &cboId );
    glGenBuffers( 1, &iboId );

    // Set clear color, enable depth testing and culling
    glClearColor( 0.07f, 0.13f, 0.17f, 1.0f );
    glEnable( GL_DEPTH_TEST );
    glEnable( GL_CULL_FACE );

    // Set callback to key Z (switch normal/wireframe mode)
    glfwSetKeyCallback( window, []( GLFWwindow* window, int key, int scancode, int action, int mods ) {
        if( key == GLFW_KEY_Z && action == GLFW_PRESS ) {
            wireframeMode = !wireframeMode;
        }
    } );
}

void Update() {
    static auto lastFrameTime = std::chrono::system_clock::now();
    auto timeNow = std::chrono::system_clock::now();
    float deltaTime = (float)std::chrono::duration_cast<std::chrono::milliseconds>( timeNow - lastFrameTime ).count() / 1000.0f;
    lastFrameTime = timeNow;

    if( glfwGetKey( window, GLFW_KEY_LEFT ) ) {
        horizontalAngle += rotateViewSpeed * deltaTime;
        if( horizontalAngle >= (float)M_PI * 2 ) {
            horizontalAngle -= (float)M_PI * 2;
        }
    }
    if( glfwGetKey( window, GLFW_KEY_RIGHT ) ) {
        horizontalAngle -= rotateViewSpeed * deltaTime;
        if( horizontalAngle < 0 ) {
            horizontalAngle += (float)M_PI * 2;
        }
    }
    if( glfwGetKey( window, GLFW_KEY_UP ) ) {
        verticalAngle += rotateViewSpeed * deltaTime;
        if( verticalAngle >= (float)M_PI_2 ) {
            verticalAngle = (float)M_PI_2;
        }
    }
    if( glfwGetKey( window, GLFW_KEY_DOWN) ) {
        verticalAngle -= rotateViewSpeed * deltaTime;
        if( verticalAngle <= -(float)M_PI_2 ) {
            verticalAngle = -(float)M_PI_2;
        }
    }

    glm::mat4 horizontalRotation = glm::rotate( glm::mat4( 1.0f ), horizontalAngle, { 0, 0, 1 } );
    rightDir = horizontalRotation * glm::vec4( 1, 0, 0, 0 );
    glm::mat4 verticalRotation = glm::rotate( glm::mat4( 1.0f ), verticalAngle, rightDir );
    upDir = verticalRotation * glm::vec4( 0, 0, 1, 0 );
    viewDir = glm::normalize( glm::cross( upDir, rightDir ) );

    if( glfwGetKey( window, GLFW_KEY_W ) ) {
        cameraPosition += viewDir * moveSpeed * deltaTime;
    }
    if( glfwGetKey( window, GLFW_KEY_S ) ) {
        cameraPosition -= viewDir * moveSpeed * deltaTime;
    }
    if( glfwGetKey( window, GLFW_KEY_A ) ) {
        cameraPosition -= rightDir * moveSpeed * deltaTime;
    }
    if( glfwGetKey( window, GLFW_KEY_D ) ) {
        cameraPosition += rightDir * moveSpeed * deltaTime;
    }
}

void Render( int width, int height ) {
    glViewport( 0, 0, width, height );

    if( wireframeMode ) {
        glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
    } else {
        glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
    }

    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    glUseProgram( program );

    auto projectionMatrix = glm::perspective( glm::radians( 60.0f ), (float)width / (float)height, 0.1f, 500.0f );

    auto viewMatrix = glm::lookAt( cameraPosition, cameraPosition + viewDir, upDir);

    auto mvp = projectionMatrix * viewMatrix;

    glUniformMatrix4fv( glGetUniformLocation( program, "m_transform" ), 1, GL_FALSE, glm::value_ptr( mvp ) );
    glBindVertexArray( vaoId );
    glEnableVertexAttribArray( 0 );
    glEnableVertexAttribArray( 1 );
    glBindBuffer( GL_ARRAY_BUFFER, vboId );
    glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, nullptr );
    glBindBuffer( GL_ARRAY_BUFFER, cboId );
    glVertexAttribPointer( 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, nullptr );
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, iboId );

    glDrawElements( GL_TRIANGLES, iboTransparentStartIdx, GL_UNSIGNED_INT, nullptr );

    if( iboTransparentStartIdx < iboSize ) {
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        glDrawElements( GL_TRIANGLES, iboSize - iboTransparentStartIdx, GL_UNSIGNED_INT, (void*)( iboTransparentStartIdx * sizeof( unsigned int ) ) );
        glDisable( GL_BLEND );
    }

    glBindBuffer( GL_ARRAY_BUFFER, 0 );
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
    glDisableVertexAttribArray( 0 );
    glDisableVertexAttribArray( 1 );
    glBindVertexArray( 0 );
    glUseProgram( 0 );
}