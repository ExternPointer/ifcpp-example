#pragma once

#include <vector>
#include <memory>

#include "csgjs.h"
#include "earcut.hpp"
#include "ifcpp/Geometry/Matrix.h"
#include "ifcpp/Geometry/StyleConverter.h"
#include "ifcpp/Geometry/VectorAdapter.h"
#include "ifcpp/Ifc/IfcObjectDefinition.h"


namespace IfcppExample {


class Polyline {
public:
    std::vector<csg::Vector> m_points;
    unsigned int m_color = 0;
};

class Mesh {
public:
    std::vector<csg::Polygon> m_polygons;
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
    using TTriangle = csg::Polygon;
    using TPolyline = std::shared_ptr<Polyline>;
    using TMesh = std::shared_ptr<Mesh>;
    using TVector = csg::Vector;

    inline TTriangle CreateTriangle( const std::vector<TVector>& vertices, const std::vector<int>& indices ) {
        if( indices.size() != 3 ) {
            // TODO: Log error
            // WTF???
            return {};
        }
        return csg::Polygon( { vertices[ indices[ 0 ] ], vertices[ indices[ 1 ] ], vertices[ indices[ 2 ] ] } );
    }
    inline TPolyline CreatePolyline( const std::vector<TVector>& vertices ) {
        return std::make_shared<Polyline>( Polyline { vertices } );
    }
    inline TMesh CreateMesh( const std::vector<TTriangle>& triangles ) {
        return std::make_shared<Mesh>( Mesh { triangles } );
    }
    inline TPolyline CreatePolyline( const TPolyline& other ) {
        return std::make_shared<Polyline>( Polyline { other->m_points, other->m_color } );
    }
    inline TMesh CreateMesh( const TMesh& other ) {
        return std::make_shared<Mesh>( Mesh { other->m_polygons, other->m_color } );
    }
    inline TEntity CreateEntity( const std::shared_ptr<IFC4X3::IfcObjectDefinition>& ifcObject, const std::vector<TMesh>& meshes,
                                 const std::vector<TPolyline>& polylines ) {
        return std::make_shared<Entity>( Entity { ifcObject, meshes, polylines } );
    }

    inline void Transform( std::vector<TMesh>* meshes, const ifcpp::Matrix<TVector>& matrix ) {
        for( auto& m: *meshes ) {
            for( auto& t: m->m_polygons ) {
                for( auto& v: t.vertices ) {
                    matrix.Transform( &v );
                }
                t.plane = csg::Plane( t.vertices );
            }
            for( long long int i = m->m_polygons.size() - 1; i >= 0; i-- ) {
                if( !m->m_polygons[ i ].plane.IsValid() ) {
                    m->m_polygons.erase( m->m_polygons.begin() + i );
                }
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
                    const auto n = -csg::Cross( a - b, c - b );
                    if( csg::LengthSquared( n ) > csg::LengthSquared( normal ) ) {
                        normal = n;
                    }
                    if( csg::LengthSquared( normal ) > 1e-6 ) {
                        goto BREAK;
                    }
                }
            }
        }
    BREAK:
        normal = csg::Normalized( normal );

        auto right = csg::Cross( { 0.0f, 0.0f, 1.0f }, normal );
        if( csg::LengthSquared( right ) < 1e-6 ) {
            right = csg::Cross( normal, { 0.0f, -1.0f, 0.0f } );
        }
        right = csg::Normalized( right );
        auto up = csg::Normalized( csg::Cross( normal, right ) );


        std::vector<std::tuple<double, double>> outer;

        double minx = std::numeric_limits<double>::max(), miny = std::numeric_limits<double>::max();

        for( const auto& p: loop ) {
            double x = csg::Dot( right, p - origin );
            double y = csg::Dot( up, p - origin );
            outer.emplace_back( x, y );
            minx = std::min( minx, x );
            miny = std::min( miny, y );
        }

        for( auto& p: outer ) {
            const auto [ x, y ] = p;
            p = { x - minx, y - miny };
        }

        double s = 0.0f;
        outer.push_back( outer[ 0 ] );
        for( int i = 1; i < outer.size(); i++ ) {
            auto [ x1, y1 ] = outer[ i - 1 ];
            auto [ x2, y2 ] = outer[ i ];
            s += ( y1 + y2 ) * 0.5f * ( x1 - x2 );
        }
        outer.pop_back();


        std::vector<std::vector<std::tuple<double, double>>> polygon = { outer };
        auto result = mapbox::earcut<int>( polygon );
        if( s < 0 ) {
            std::reverse( std::begin( result ), std::end( result ) );
        }
        for( int i = 1; i < result.size(); i += 3 ) {
            const auto& a = loop[ result[ i - 1 ] ];
            const auto& b = loop[ result[ i ] ];
            const auto& c = loop[ result[ i + 1 ] ];
            if( csg::LengthSquared( csg::Cross( b - a, c - b ) ) < 1e-12 ) {
                result.erase( result.begin() + i - 1, result.begin() + i + 2 );
                i -= 3;
            }
        }
        return result;
    }

    inline std::vector<TMesh> ComputeUnion( const std::vector<TMesh>& operand1, const std::vector<TMesh>& operand2 ) {
        if( operand1.empty() ) {
            return operand2;
        } else if( operand2.empty() ) {
            return operand1;
        }

        csg::details::CSGNode resultNode;
        for( const auto& operand: operand1 ) {
            auto n = csg::details::CSGNode( operand->m_polygons );
            csg::details::UnionInplace( &resultNode, &n );
        }
        for( const auto& operand: operand2 ) {
            auto n = csg::details::CSGNode( operand->m_polygons );
            csg::details::UnionInplace( &resultNode, &n );
        }

        // TODO: Fix styles (m_color) when we have several operand1 meshes
        TMesh result = std::make_shared<Mesh>( Mesh { resultNode.allpolygons(), operand1[ 0 ]->m_color } );
        return { result };
    }
    inline std::vector<TMesh> ComputeIntersection( const std::vector<TMesh>& operand1, const std::vector<TMesh>& operand2 ) {
        if( operand1.empty() || operand2.empty() ) {
            return {};
        }

        csg::details::CSGNode operand2node;
        for( const auto& operand: operand2 ) {
            auto n = csg::details::CSGNode( operand->m_polygons );
            csg::details::UnionInplace( &operand2node, &n );
        }

        for( auto& operand: operand1 ) {
            auto resultNode = std::make_unique<csg::details::CSGNode>( operand->m_polygons );
            csg::details::IntersectionInplace( resultNode.get(), &operand2node );
            operand->m_polygons = resultNode->allpolygons();
        }

        return operand1;
    }
    inline std::vector<TMesh> ComputeDifference( const std::vector<TMesh>& operand1, const std::vector<TMesh>& operand2 ) {
        if( operand1.empty() || operand2.empty() ) {
            return operand1;
        }

        std::vector<csg::details::CSGNode> operand2nodes;
        operand2nodes.reserve( operand2.size() );
        for( const auto& o: operand2 ) {
            operand2nodes.emplace_back( o->m_polygons );
        }

        for( auto& o1: operand1 ) {
            auto resultNode = std::make_unique<csg::details::CSGNode>( o1->m_polygons );
            for( const auto& o2: operand2nodes ) {
                csg::details::DifferenceInplace( resultNode.get(), &o2 );
            }
            o1->m_polygons = resultNode->allpolygons();
        }

        return operand1;
    }
};

};
