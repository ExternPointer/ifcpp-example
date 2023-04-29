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
    std::vector<csgjscpp::Polygon> m_triangles;
    unsigned int m_color = 0;
};
class Entity {
public:
    std::shared_ptr<IFC4X3::IfcObjectDefinition> m_ifcObject;
    std::vector<Mesh> m_meshes;
    std::vector<Polyline> m_polylines;
};
class Adapter {
public:
    using TEntity = Entity;
    using TTriangle = csgjscpp::Polygon;
    using TPolyline = Polyline;
    using TMesh = Mesh;
    using TVector = csgjscpp::Vector;

    inline TPolyline CreatePolyline( const std::vector<TVector>& vertices ) {
        return { vertices };
    }
    inline TTriangle CreateTriangle( std::vector<TVector> vertices, std::vector<int> indices ) {
        if( indices.size() != 3 ) {
            // TODO: Log error
            // WTF???
            return {};
        }
        return csgjscpp::Polygon( {
            { vertices[ indices[ 0 ] ], { 0, 0, 0 }, 0 },
            { vertices[ indices[ 1 ] ], { 0, 0, 0 }, 0 },
            { vertices[ indices[ 2 ] ], { 0, 0, 0 }, 0 },
        } );
    }
    inline TMesh CreateMesh( const std::vector<TTriangle>& triangles ) {
        return { triangles };
    }
    inline TEntity CreateEntity( const std::shared_ptr<IFC4X3::IfcObjectDefinition>& ifcObject, const std::vector<TMesh>& meshes,
                                 const std::vector<TPolyline>& polylines ) {
        return { ifcObject, meshes, polylines };
    }

    inline void Transform( std::vector<TMesh>* meshes, ifcpp::Matrix<TVector> matrix ) {
        return;
        for( auto& m: *meshes ) {
            for( auto& t: m.m_triangles ) {
                for( auto& v: t.vertices ) {
                    matrix.Transform( &v.pos );
                }
                t = csgjscpp::Polygon( t.vertices );
            }
        }
    }
    inline void Transform( std::vector<TPolyline>* polylines, ifcpp::Matrix<TVector> matrix ) {
        for( auto& p: *polylines ) {
            for( auto& v: p.m_points ) {
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
                //                    for( auto& t: m.m_triangles ) {
                //                        t.flip();
                //                    }
                //                }
                break;
            }
            if( s->m_type == ifcpp::Style::SURFACE_BOTH ) {
                style = s;
                //                for( auto& m: *meshes ) {
                //                    auto triangles = m.m_triangles;
                //                    for( auto& t: triangles ) {
                //                        t.flip();
                //                    }
                //                    std::copy( triangles.begin(), triangles.end(), std::back_inserter( m.m_triangles ) );
                //                }
                break;
            }
        }
        if( !style ) {
            return;
        }
        for( auto& m: *meshes ) {
            if( !m.m_color ) {
                m.m_color = (int)( 255.0f * style->m_color.a ) << 24 | (int)( 255.0f * style->m_color.b ) << 16 | (int)( 255.0f * style->m_color.g ) << 8 |
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
            if( !p.m_color ) {
                p.m_color = (int)( 255.0f * style->m_color.a ) << 24 | (int)( 255.0f * style->m_color.b ) << 16 | (int)( 255.0f * style->m_color.g ) << 8 |
                    (int)( 255.0f * style->m_color.r );
            }
        }
    }

    bool pointInTriangle( csgjscpp::Vector a, csgjscpp::Vector b, csgjscpp::Vector c, csgjscpp::Vector p ) {
        float t = 1e-1;
        return ( c.x - p.x ) * ( a.y - p.y ) - t >= ( a.x - p.x ) * ( c.y - p.y ) && ( a.x - p.x ) * ( b.y - p.y ) - t >= ( b.x - p.x ) * ( a.y - p.y ) &&
            ( b.x - p.x ) * ( c.y - p.y ) - t >= ( c.x - p.x ) * ( b.y - p.y );
    }

    inline std::vector<int> Triangulate( std::vector<TVector> loop ) {
//        if( loop.size() < 3 ) {
//            // WTF, TODO: Log error
//            return {};
//        }
//        if( csgjscpp::lengthsquared(loop[0] - loop[loop.size() - 1] ) > 1e-3 ) {
//            loop.push_back( loop[ 0 ] );
//        }
//        TVector normal( 0, 0, 0 );
//        TVector origin = loop[ 0 ];
//
//        for( int i = 1; i < loop.size() - 1; i++ ) {
//            normal = normal + csgjscpp::cross( ( loop[ i - 1 ] - loop[ i ] ), ( loop[ i + 1 ] - loop[ i ] ) );
//        }
//        if( csgjscpp::lengthsquared( normal ) < 1e-6 ) {
//            return {};
//        }
//        normal = -csgjscpp::unit( normal );
//
//        auto right = csgjscpp::cross( { 0.0f, 0.0f, 1.0f }, normal );
//        if( csgjscpp::lengthsquared( right ) < 1e-6 ) {
//            right = csgjscpp::cross( normal, { 0.0f, -1.0f, 0.0f } );
//        }
//
//        right = csgjscpp::unit( right );
//        auto up = csgjscpp::unit( csgjscpp::cross( normal, right ) );
//
//        float minx = std::numeric_limits<float>::max();
//        float miny = std::numeric_limits<float>::max();
//        float maxx = -std::numeric_limits<float>::max();
//        float maxy = -std::numeric_limits<float>::max();
//
//        for( auto& p: loop ) {
//            float x = csgjscpp::dot( right, p - origin );
//            float y = csgjscpp::dot( up, p - origin );
//            p.x = x;
//            p.y = y;
//            p.z = 0;
//            minx = std::min( minx, x );
//            miny = std::min( miny, y );
//            maxx = std::max( maxx, x );
//            maxy = std::max( maxy, y );
//        }
//        float ex = maxx - minx;
//        float ey = maxy - miny;
//        float t = 9;
//        float sx = ex > 0 ? t / ex : 1.0f;
//        float sy = ey > 0 ? t / ey : 1.0f;
//        float ox = -minx;
//        float oy = -miny;
//
//        for( auto& v: loop ) {
//            v.x = ( v.x + ox ) * sx;
//            v.y = ( v.y + oy ) * sy;
//        }
//
//
//        std::vector<int> indices;
//        std::vector<int> result;
//        for( int i = 0; i < loop.size(); i++ ) {
//            indices.push_back( i );
//        }
//
//        while( loop.size() > 3 ) {
//            bool ok = true;
//            for( int i = 1; i < loop.size() - 1; i++ ) {
//                const auto a = loop[ i - 1 ];
//                const auto b = loop[ i ];
//                const auto c = loop[ i + 1 ];
//                ok = false;
//                if( csgjscpp::cross( b - a, c - b ).z < 0 ) {
//                    continue;
//                }
//                ok = true;
//
//                for( const auto& v: loop ) {
//                    if( pointInTriangle( a, b, c, v ) ) {
//                        ok = false;
//                        break;
//                    }
//                }
//                if( ok ) {
//                    result.push_back( indices[ i - 1 ] );
//                    result.push_back( indices[ i ] );
//                    result.push_back( indices[ i + 1 ] );
//                    loop.erase( loop.begin() + i );
//                    indices.erase( indices.begin() + i );
//                    break;
//                }
//            }
//            if( !ok ) {
//                break;
//            }
//        }
//
//        result.push_back( indices[ 0 ] );
//        result.push_back( indices[ 1 ] );
//        result.push_back( indices[ 2 ] );
//
//        return result;
//





                if( loop.size() < 3 ) {
                    // WTF, TODO: Log error
                    return {};
                }


                TVector normal( 0, 0, 0 );
                TVector origin = loop[ 0 ];

                for( int i = 1; i < loop.size() - 1; i++ ) {
                    normal = normal + csgjscpp::cross( ( loop[ i - 1 ] - loop[ i ] ), ( loop[ i + 1 ] - loop[ i ] ) );
                }

                if( csgjscpp::lengthsquared(loop[0] - loop[loop.size() - 1] ) < 1e-3 ) {
                    loop.pop_back();
                }
        //        for( const auto& a: loop ) {
        //            for( const auto& b: loop ) {
        //                for( const auto& c: loop ) {
        //                    const auto n = csgjscpp::cross( b - a, c - b );
        //                    if( csgjscpp::lengthsquared( n ) > csgjscpp::lengthsquared( normal ) ) {
        //                        normal = n;
        //                    }
        //                }
        //            }
        //        }
                if( csgjscpp::lengthsquared( normal ) < 1e-12 ) {
                    return {};
                }
                normal = csgjscpp::unit( normal );


                auto right = csgjscpp::cross( { 0.0f, 0.0f, 1.0f }, normal );
                if( csgjscpp::lengthsquared( right ) < 1e-6 ) {
                    right = csgjscpp::cross( normal, { 0.0f, -1.0f, 0.0f } );
                }

                right = csgjscpp::unit( right );
                auto up = csgjscpp::unit( csgjscpp::cross( normal, right ) );


                std::vector<std::vector<std::tuple<float, float>>> polygon;
                std::vector<std::tuple<float, float>> outer;

                float minx = std::numeric_limits<float>::max();
                float miny = std::numeric_limits<float>::max();
                float maxx = -std::numeric_limits<float>::max();
                float maxy = -std::numeric_limits<float>::max();

                for( const auto& p: loop ) {
                    float x = csgjscpp::dot( right, p - origin );
                    float y = csgjscpp::dot( up, p - origin );
                    outer.emplace_back( x, y );
                    minx = std::min( minx, x );
                    miny = std::min( miny, y );
                    maxx = std::max( maxx, x );
                    maxy = std::max( maxy, y );
                }
                float ex = maxx - minx;
                float ey = maxy - miny;
                float t = 9;
                float sx = ex > 0 ? t / ex : 1.0f;
                float sy = ey > 0 ? t / ey : 1.0f;
                float ox = -minx;
                float oy = -miny;

                for( auto& v: outer ) {
                    auto [ x, y ] = v;
                    x = ( x + ox ) * sx;
                    y = ( y + oy ) * sy;
                    v = { x, y };
                }

                polygon.push_back( outer );
                auto result = mapbox::earcut<int>( polygon );
                std::reverse( std::begin( result ), std::end( result ) );
                return result;
    }

    inline std::vector<TMesh> ComputeUnion( const std::vector<TMesh>& operand1, const std::vector<TMesh>& operand2 ) {
        TMesh result;
        for( const auto& o: operand1 ) {
            result.m_triangles = this->ComputeUnion( result.m_triangles, o.m_triangles );
            result.m_color = o.m_color;
        }
        for( const auto& o: operand2 ) {
            result.m_triangles = this->ComputeUnion( result.m_triangles, o.m_triangles );
            result.m_color = o.m_color;
        }
        return { result };
    }

    inline std::vector<TMesh> ComputeIntersection( const std::vector<TMesh>& operand1, const std::vector<TMesh>& operand2 ) {
        std::vector<TMesh> result;
        std::vector<TTriangle> operand2Triangles;
        for( const auto& o: operand2 ) {
            std::copy( o.m_triangles.begin(), o.m_triangles.end(), std::back_inserter( operand2Triangles ) );
        }
        for( const auto& o: operand1 ) {
            const auto triangles = this->ComputeIntersection( o.m_triangles, operand2Triangles );
            if( !triangles.empty() ) {
                result.push_back( { triangles, o.m_color } );
            }
        }
        return result;
    }

    inline std::vector<TMesh> ComputeDifference( const std::vector<TMesh>& operand1, const std::vector<TMesh>& operand2 ) {
        return {};
        auto result = operand1;
        for( auto& o1: result ) {
            for( const auto& o2: operand2 ) {
                o1.m_triangles = this->ComputeDifference( o1.m_triangles, o2.m_triangles );
            }
        }
        return result;
    }

private:
    inline std::vector<TTriangle> ComputeUnion( std::vector<TTriangle> operand1, std::vector<TTriangle> operand2 ) {
        if( operand1.empty() ) {
            return operand2;
        }
        if( operand2.empty() ) {
            return operand1;
        }
        const auto restoreInfo = this->MoveOperandsToOrigin( operand1, operand2 );
        auto result = csgjscpp::modeltopolygons( csgjscpp::csgunion( csgjscpp::modelfrompolygons( operand1 ), csgjscpp::modelfrompolygons( operand2 ) ) );
        this->RestoreResult( result, restoreInfo );
        return result;
    }

    inline std::vector<TTriangle> ComputeIntersection( std::vector<TTriangle> operand1, std::vector<TTriangle> operand2 ) {
        if( operand1.empty() || operand2.empty() ) {
            return {};
        }
        const auto restoreInfo = this->MoveOperandsToOrigin( operand1, operand2 );
        auto result =
            csgjscpp::modeltopolygons( csgjscpp::csgintersection( csgjscpp::modelfrompolygons( operand1 ), csgjscpp::modelfrompolygons( operand2 ) ) );
        this->RestoreResult( result, restoreInfo );
        return result;
    }

    inline std::vector<TTriangle> ComputeDifference( std::vector<TTriangle> operand1, std::vector<TTriangle> operand2 ) {
        if( operand1.empty() || operand2.empty() ) {
            return operand1;
        }
        const auto restoreInfo = this->MoveOperandsToOrigin( operand1, operand2 );
        auto result = csgjscpp::modeltopolygons( csgjscpp::csgsubtract( csgjscpp::modelfrompolygons( operand1 ), csgjscpp::modelfrompolygons( operand2 ) ) );
        this->RestoreResult( result, restoreInfo );
        return result;
    }

    //         offset            scale
    std::tuple<csgjscpp::Vector, csgjscpp::Vector> MoveOperandsToOrigin( std::vector<TTriangle>& operand1, std::vector<TTriangle>& operand2 ) {
        csgjscpp::Vector min( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );
        csgjscpp::Vector max( -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() );

        for( const auto& p: operand1 ) {
            for( const auto v: p.vertices ) {
                const auto pos = v.pos;
                min.x = std::min( min.x, pos.x );
                min.y = std::min( min.y, pos.y );
                min.z = std::min( min.z, pos.z );
                max.x = std::max( max.x, pos.x );
                max.y = std::max( max.y, pos.y );
                max.z = std::max( max.z, pos.z );
            }
        }
        for( const auto& p: operand2 ) {
            for( const auto& v: p.vertices ) {
                const auto& pos = v.pos;
                min.x = std::min( min.x, pos.x );
                min.y = std::min( min.y, pos.y );
                min.z = std::min( min.z, pos.z );
                max.x = std::max( max.x, pos.x );
                max.y = std::max( max.y, pos.y );
                max.z = std::max( max.z, pos.z );
            }
        }

        const auto offset = -min;
        const auto e = max - min;
        const float t = 9.0f;
        auto scale = csgjscpp::Vector( e.x > 0 ? t / e.x : 1.0f, e.y > 0 ? t / e.y : 1.0f, e.z > 0 ? t / e.z : 1.0f );

        for( auto& p: operand1 ) {
            for( auto& v: p.vertices ) {
                v.pos = ( v.pos + offset );
                v.pos.x *= scale.x;
                v.pos.y *= scale.y;
                v.pos.z *= scale.z;
            }
        }
        for( auto& p: operand2 ) {
            for( auto& v: p.vertices ) {
                v.pos = ( v.pos + offset );
                v.pos.x *= scale.x;
                v.pos.y *= scale.y;
                v.pos.z *= scale.z;
            }
        }

        return { offset, scale };
    }

    void RestoreResult( std::vector<TTriangle>& result, const std::tuple<csgjscpp::Vector, csgjscpp::Vector>& offsetAndScale ) {
        auto [ offset, scale ] = offsetAndScale;
        scale.x = 1.0f / scale.x;
        scale.y = 1.0f / scale.y;
        scale.z = 1.0f / scale.z;
        offset = -offset;
        for( auto& p: result ) {
            for( auto& v: p.vertices ) {
                v.pos.x *= scale.x;
                v.pos.y *= scale.y;
                v.pos.z *= scale.z;
                v.pos = v.pos + offset;
            }
        }
    }
};