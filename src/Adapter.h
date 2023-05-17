#pragma once

#include <vector>

#include "csgjs.h"
#include "earcut.hpp"
#include "ifcpp/Geometry/Matrix.h"
#include "ifcpp/Geometry/StyleConverter.h"
#include "ifcpp/Geometry/VectorAdapter.h"
#include "ifcpp/Ifc/IfcObjectDefinition.h"


class Polyline {
public:
    std::vector<csgjscpp::Vector> m_points;
    unsigned int m_color = 0;
};

class Mesh {
public:
    std::vector<csgjscpp::Polygon> m_polygons;
    unsigned int m_color = 0;
};

class Entity {
public:
    std::shared_ptr<IFC4X3::IfcObjectDefinition> m_ifcObject;
    std::vector<std::shared_ptr<Mesh>> m_meshes;
    std::vector<std::shared_ptr<Polyline>> m_polylines;
};

class Adapter {
public:
    using TEntity = std::shared_ptr<Entity>;
    using TTriangle = csgjscpp::Polygon;
    using TPolyline = std::shared_ptr<Polyline>;
    using TMesh = std::shared_ptr<Mesh>;
    using TVector = csgjscpp::Vector;

    inline TTriangle CreateTriangle( const std::vector<TVector>& vertices, const std::vector<int>& indices ) {
        if( indices.size() != 3 ) {
            // TODO: Log error
            // WTF???
            return {};
        }
        return csgjscpp::Polygon( csgjscpp::Polygon( {
            { vertices[ indices[ 0 ] ] },// { 0, 0, 0 }, 0 },
            { vertices[ indices[ 1 ] ] },// { 0, 0, 0 }, 0 },
            { vertices[ indices[ 2 ] ] },// { 0, 0, 0 }, 0 },
        } ) );
    }
    inline TPolyline CreatePolyline( const std::vector<TVector>& vertices ) {
        return std::make_shared<Polyline>( vertices );
    }
    inline TMesh CreateMesh( const std::vector<TTriangle>& triangles ) {
        return std::make_shared<Mesh>( triangles );
    }
    inline TPolyline CreatePolyline( const TPolyline& other ) {
        return std::make_shared<Polyline>( *other );
    }
    inline TMesh CreateMesh( const TMesh& other ) {
        return std::make_shared<Mesh>( *other );
    }
    inline TEntity CreateEntity( const std::shared_ptr<IFC4X3::IfcObjectDefinition>& ifcObject, const std::vector<TMesh>& meshes,
                                 const std::vector<TPolyline>& polylines ) {
        return std::make_shared<Entity>( ifcObject, meshes, polylines );
    }

    inline void Transform( std::vector<TMesh>* meshes, const ifcpp::Matrix<TVector>& matrix ) {
        for( auto& m: *meshes ) {
            for( auto& t: m->m_polygons ) {
                for( auto& v: t.vertices ) {
                    matrix.Transform( &v.pos );
                }
                t = csgjscpp::Polygon( t.vertices );
            }
        }
    }
    inline void Transform( std::vector<TPolyline>* polylines, const ifcpp::Matrix<TVector>& matrix ) {
        for( auto& p: *polylines ) {
            for( auto& v: p->m_points ) {
                matrix.Transform( &v );
            }
        }
    }

    inline void AddStyles( std::vector<TMesh>* meshes, const std::vector<std::shared_ptr<ifcpp::Style>>& styles ) {
        std::shared_ptr<ifcpp::Style> style;
        for( const auto& s: styles ) {
            if( s->m_type == ifcpp::Style::SURFACE_FRONT ) {
                style = s;
                break;
            }
            if( s->m_type == ifcpp::Style::SURFACE_BACK ) {
                style = s;
                //                for( auto& m: *meshes ) {
                //                    for( auto& t: m.m_polygons ) {
                //                        t.flip();
                //                    }
                //                }
                break;
            }
            if( s->m_type == ifcpp::Style::SURFACE_BOTH ) {
                style = s;
                //                for( auto& m: *meshes ) {
                //                    auto triangles = m.m_polygons;
                //                    for( auto& t: triangles ) {
                //                        t.flip();
                //                    }
                //                    std::copy( triangles.begin(), triangles.end(), std::back_inserter( m.m_polygons ) );
                //                }
                break;
            }
        }
        if( !style ) {
            return;
        }
        for( auto& m: *meshes ) {
            if( !m->m_color ) {
                m->m_color = (int)( 255.0f * style->m_color.a ) << 24 | (int)( 255.0f * style->m_color.b ) << 16 | (int)( 255.0f * style->m_color.g ) << 8 |
                    (int)( 255.0f * style->m_color.r );
            }
        }
    }
    inline void AddStyles( std::vector<TPolyline>* polylines, const std::vector<std::shared_ptr<ifcpp::Style>>& styles ) {
        std::shared_ptr<ifcpp::Style> style;
        for( const auto& s: styles ) {
            if( s->m_type == ifcpp::Style::CURVE ) {
                style = s;
                break;
            }
        }
        if( !style ) {
            return;
        }
        for( auto& p: *polylines ) {
            if( !p->m_color ) {
                p->m_color = (int)( 255.0f * style->m_color.a ) << 24 | (int)( 255.0f * style->m_color.b ) << 16 | (int)( 255.0f * style->m_color.g ) << 8 |
                    (int)( 255.0f * style->m_color.r );
            }
        }
    }

    inline std::vector<int> Triangulate( const std::vector<TVector>& loop ) {
        if( loop.size() < 3 ) {
            return {};
        }

        TVector normal( 0, 0, 0 );
        TVector origin = loop[ 0 ];
        for( const auto& a: loop ) {
            for( const auto& b: loop ) {
                for( const auto& c: loop ) {
                    const auto n = csgjscpp::cross( b - a, c - b );
                    if( csgjscpp::lengthsquared( n ) > csgjscpp::lengthsquared( normal ) ) {
                        normal = n;
                    }
                    if( csgjscpp::lengthsquared( normal ) > 1e-12 ) {
                        goto BREAK;
                    }
                }
            }
        }
    BREAK:
        normal = csgjscpp::unit( normal );

        auto right = csgjscpp::cross( { 0.0f, 0.0f, 1.0f }, normal );
        if( csgjscpp::lengthsquared( right ) < 1e-6 ) {
            right = csgjscpp::cross( normal, { 0.0f, -1.0f, 0.0f } );
        }
        right = csgjscpp::unit( right );
        auto up = csgjscpp::unit( csgjscpp::cross( normal, right ) );


        std::vector<std::tuple<float, float>> outer;

        float minx = std::numeric_limits<float>::max(), miny = std::numeric_limits<float>::max();

        for( const auto& p: loop ) {
            float x = csgjscpp::dot( right, p - origin );
            float y = csgjscpp::dot( up, p - origin );
            outer.emplace_back( x, y );
            minx = std::min( minx, x );
            miny = std::min( miny, y );
        }

        for( auto& p: outer ) {
            const auto [ x, y ] = p;
            p = { x - minx, y - miny };
        }

        float s = 0.0f;
        outer.push_back( outer[ 0 ] );
        for( int i = 1; i < outer.size(); i++ ) {
            auto [ x1, y1 ] = outer[ i - 1 ];
            auto [ x2, y2 ] = outer[ i ];
            s += ( y1 + y2 ) * 0.5f * ( x1 - x2 );
        }
        outer.pop_back();


        std::vector<std::vector<std::tuple<float, float>>> polygon = { outer };
        auto result = mapbox::earcut<int>( polygon );
        if( s < 0 ) {
            std::reverse( std::begin( result ), std::end( result ) );
        }
        for( int i = 1; i < result.size(); i += 3 ) {
            const auto& a = loop[ result[ i - 1 ] ];
            const auto& b = loop[ result[ i ] ];
            const auto& c = loop[ result[ i + 1 ] ];
            if( csgjscpp::lengthsquared( csgjscpp::cross( b - a, c - b ) ) < 1e-12 ) {
                result.erase( result.begin() + i - 1, result.begin() + i + 2 );
                i -= 3;
            }
        }
        return result;
    }

    inline std::vector<TMesh> ComputeUnion( const std::vector<TMesh>& operand1, const std::vector<TMesh>& operand2 ) {
        //return operand1;
        //this->RemoveEmptyOperands( &operand1, &operand2 );
        if( operand1.empty() ) {
            return operand2;
        } else if( operand2.empty() ) {
            return operand1;
        }

        auto resultNode = std::make_unique<csgjscpp::CSGNode>( operand1[ 0 ]->m_polygons );
        for( int i = 1; i < operand1.size(); i++ ) {
            auto o = csgjscpp::CSGNode( operand1[ i ]->m_polygons );
            resultNode = std::unique_ptr<csgjscpp::CSGNode>( csgjscpp::csg_union( resultNode.get(), &o ) );
        }
        for( int i = 0; i < operand2.size(); i++ ) {
            auto o = csgjscpp::CSGNode( operand2[ i ]->m_polygons );
            resultNode = std::unique_ptr<csgjscpp::CSGNode>( csgjscpp::csg_union( resultNode.get(), &o ) );
        }

        TMesh result = std::make_shared<Mesh>();
        result->m_polygons = resultNode->allpolygons();
        // TODO: Fix styles (m_color) when we have several operand1 meshes
        result->m_color = operand1[ 0 ]->m_color;
        return { result };
    }
    inline std::vector<TMesh> ComputeIntersection( const std::vector<TMesh>& operand1, const std::vector<TMesh>& operand2 ) {
        //return operand1;
        //this->RemoveEmptyOperands( &operand1, &operand2 );
        if( operand1.empty() || operand2.empty() ) {
            return {};
        }

        auto operand2node = std::make_unique<csgjscpp::CSGNode>( operand2[ 0 ]->m_polygons );
        for( int i = 1; i < operand2.size(); i++ ) {
            auto o = csgjscpp::CSGNode( operand2[ i ]->m_polygons );
            operand2node = std::unique_ptr<csgjscpp::CSGNode>( csgjscpp::csg_union( operand2node.get(), &o ) );
        }

        for( auto& o1: operand1 ) {
            auto resultNode = std::make_unique<csgjscpp::CSGNode>( o1->m_polygons );
            resultNode = std::unique_ptr<csgjscpp::CSGNode>( csgjscpp::csg_intersect( resultNode.get(), operand2node.get() ) );
            o1->m_polygons = resultNode->allpolygons();
        }

        //this->RemoveEmptyOperands( &operand1 );

        return operand1;
    }
    inline std::vector<TMesh> ComputeDifference( const std::vector<TMesh>& operand1, const std::vector<TMesh>& operand2 ) {
        //return operand1;
        //this->RemoveEmptyOperands( &operand1, &operand2 );
        if( operand1.empty() || operand2.empty() ) {
            return operand1;
        }

        std::vector<csgjscpp::CSGNode> operand2nodes;
        operand2nodes.reserve( operand2.size() );
        for( const auto& o: operand2 ) {
            operand2nodes.emplace_back( o->m_polygons );
        }

        for( auto& o1: operand1 ) {
            auto resultNode = std::make_unique<csgjscpp::CSGNode>( o1->m_polygons );
            for( const auto& o2: operand2nodes ) {
                csgjscpp::csg_subtract_inplace( resultNode.get(), &o2 );
            }
            o1->m_polygons = std::move( resultNode->allpolygons() );
        }

        //this->RemoveEmptyOperands( &operand1 );

        return operand1;
    }

private:
    inline void RemoveEmptyOperands( std::vector<TMesh>* operand1, std::vector<TMesh>* operand2 ) {
        this->RemoveEmptyOperands( operand1 );
        this->RemoveEmptyOperands( operand2 );
    }
    inline void RemoveEmptyOperands( std::vector<TMesh>* operand ) {
        for( int i = 0; i < operand->size(); i++ ) {
            if( operand->at( i )->m_polygons.empty() ) {
                operand->erase( operand->begin() + i );
                i--;
            }
        }
    }
};