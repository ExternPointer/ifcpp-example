#define CSG_FIX_POLYGON_ORIENTATIONS_EXPERIMENTAL

#include <chrono>
#include <iostream>
#include <spdlog/spdlog.h>
#include <ifcpp/ModelLoader.h>
#include "Engine.h"

using namespace IfcppExample;


std::vector<std::shared_ptr<Entity>> LoadModel( const std::string& filePath );

int main() {
    InitEngine();

    auto entities = LoadModel( "example.ifc" );
    SendToGpu( entities );

    while( !glfwWindowShouldClose( window ) ) {
        int width, height;
        glfwGetFramebufferSize( window, &width, &height );

        Update();
        Render( width, height );

        glfwSwapBuffers( window );
        glfwPollEvents();
    }

    glfwDestroyWindow( window );
    glfwTerminate();
    return 0;
}

std::vector<std::shared_ptr<Entity>> LoadModel( const std::string& filePath ) {
    auto onProgressChanged = []( double progress ) { spdlog::info( "progress changed: {}", progress ); };

    auto parameters = std::make_shared<ifcpp::Parameters>( ifcpp::Parameters { 1e-6, 14, 5, 10000, 4 } );

    auto processingStartTime = std::chrono::high_resolution_clock::now();
    auto entities = ifcpp::LoadModel<Adapter>( filePath, parameters, onProgressChanged );
    auto processingFinishTime = std::chrono::high_resolution_clock::now();

    auto processingTime = processingFinishTime - processingStartTime;
    auto processingTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>( processingTime ).count();
    auto processingTimeSec = std::chrono::duration_cast<std::chrono::seconds>( processingTime ).count();
    spdlog::info( "model processing: {} milliseconds ({} seconds)", processingTimeMs, processingTimeSec );

    return entities;
}
