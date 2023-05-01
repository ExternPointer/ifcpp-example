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
        return csgjscpp::Polygon( csgjscpp::Polygon( {
            { vertices[ indices[ 0 ] ], { 0, 0, 0 }, 0 },
            { vertices[ indices[ 1 ] ], { 0, 0, 0 }, 0 },
            { vertices[ indices[ 2 ] ], { 0, 0, 0 }, 0 },
        } ) );
    }
    inline TMesh CreateMesh( const std::vector<TTriangle>& triangles ) {
        return { triangles };
    }
    inline TEntity CreateEntity( const std::shared_ptr<IFC4X3::IfcObjectDefinition>& ifcObject, const std::vector<TMesh>& meshes,
                                 const std::vector<TPolyline>& polylines ) {
        return { ifcObject, meshes, polylines };
    }

    inline void Transform( std::vector<TMesh>* meshes, ifcpp::Matrix<TVector> matrix ) {
        for( auto& m: *meshes ) {
            for( auto& t: m.m_polygons ) {
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
                //                                for( auto& m: *meshes ) {
                //                                    for( auto& t: m.m_triangles ) {
                //                                        t.flip();
                //                                    }
                //                                }
                break;
            }
            if( s->m_type == ifcpp::Style::SURFACE_BOTH ) {
                style = s;
                //                                for( auto& m: *meshes ) {
                //                                    auto triangles = m.m_triangles;
                //                                    for( auto& t: triangles ) {
                //                                        t.flip();
                //                                    }
                //                                    std::copy( triangles.begin(), triangles.end(), std::back_inserter( m.m_triangles ) );
                //                                }
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
                    if( csgjscpp::lengthsquared( normal ) > 1e-6 ) {
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

        for( const auto& p: loop ) {
            float x = csgjscpp::dot( right, p - origin );
            float y = csgjscpp::dot( up, p - origin );
            outer.emplace_back( x, y );
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
            if( csgjscpp::lengthsquared( csgjscpp::cross( b - a, c - b ) ) < 1e-8 ) {
                result.erase( result.begin() + i - 1, result.begin() + i + 2 );
                i -= 3;
            }
        }
        return result;
    }

    inline std::vector<TMesh> ComputeUnion( const std::vector<TMesh>& operand1, const std::vector<TMesh>& operand2 ) {
        TMesh result;
        for( const auto& o: operand1 ) {
            result.m_polygons = this->ComputeUnion( result.m_polygons, o.m_polygons );
            result.m_color = o.m_color;
        }
        for( const auto& o: operand2 ) {
            result.m_polygons = this->ComputeUnion( result.m_polygons, o.m_polygons );
        }
        return { result };
    }

    inline std::vector<TMesh> ComputeIntersection( const std::vector<TMesh>& operand1, const std::vector<TMesh>& operand2 ) {
        std::vector<TMesh> result;
        std::vector<TTriangle> operand2Triangles;
        for( const auto& o: operand2 ) {
            std::copy( o.m_polygons.begin(), o.m_polygons.end(), std::back_inserter( operand2Triangles ) );
        }
        for( const auto& o: operand1 ) {
            const auto triangles = this->ComputeIntersection( o.m_polygons, operand2Triangles );
            if( !triangles.empty() ) {
                result.push_back( { triangles, o.m_color } );
            }
        }
        return result;
    }

    inline std::vector<TMesh> ComputeDifference( std::vector<TMesh> operand1, const std::vector<TMesh>& operand2 ) {
        std::vector<TTriangle> triangles;
        for( const auto& o: operand1 ) {
            std::copy( o.m_polygons.begin(), o.m_polygons.end(), std::back_inserter( triangles ) );
        }
        for( const auto& o: operand2 ) {
            std::copy( o.m_polygons.begin(), o.m_polygons.end(), std::back_inserter( triangles ) );
        }
        const auto offsetAndScale = this->CalculateOffsetAndScale( triangles );

        std::vector<csgjscpp::CSGNode> operand2nodes;
        operand2nodes.reserve( operand2.size() );
        for( const auto& o: operand2 ) {
            operand2nodes.emplace_back( csgjscpp::modeltopolygons( csgjscpp::modelfrompolygons( this->GetMovedAndScaled( o.m_polygons, offsetAndScale ) ) ) );
        }

        for( auto& o1: operand1 ) {
            std::unique_ptr<csgjscpp::CSGNode> resultNode( new csgjscpp::CSGNode( csgjscpp::modeltopolygons( csgjscpp::modelfrompolygons( this->GetMovedAndScaled( o1.m_polygons, offsetAndScale ) ) ) ) );
            for( const auto& o2: operand2nodes ) {
                resultNode = std::unique_ptr<csgjscpp::CSGNode>( csgjscpp::csg_subtract( resultNode.get(), &o2 ) );
            }
            o1.m_polygons = this->GetResotred( resultNode->allpolygons(), offsetAndScale );
        }
        return operand1;

        //        for( auto& o1: operand1 ) {
        //            for( const auto& o2: operand2 ) {
        //                o1.m_triangles = this->ComputeDifference( o1.m_triangles, o2.m_triangles );
        //            }
        //        }
        //        return operand1;


        //        std::vector<TTriangle> operand2triangles;
        //        for( const auto& o: operand2 ) {
        //            std::copy( o.m_triangles.begin(), o.m_triangles.end(), std::back_inserter( operand2triangles ) );
        //        }
        //        for( auto& o: operand1 ) {
        //            o.m_triangles = this->ComputeDifference( o.m_triangles, operand2triangles );
        //        }
        //        return operand1;
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

    std::tuple<csgjscpp::Vector, csgjscpp::Vector> CalculateOffsetAndScale( const std::vector<TTriangle>& triangles ) {
        csgjscpp::Vector min( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );
        csgjscpp::Vector max( -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() );

        for( const auto& p: triangles ) {
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

        const auto offset = -(min + max) * 0.5f;
        auto e = max - min;
        const float t = 2.0f;
        auto scale = csgjscpp::Vector( e.x > 0 ? t / e.x : 1.0f, e.y > 0 ? t / e.y : 1.0f, e.z > 0 ? t / e.z : 1.0f );

        return { offset, scale };
    }

    std::vector<TTriangle> GetMovedAndScaled( const std::vector<TTriangle>& triangles, const std::tuple<csgjscpp::Vector, csgjscpp::Vector>& offsetAndScale ) {
        auto result = triangles;
        const auto [ offset, scale ] = offsetAndScale;
        for( auto& p: result ) {
            for( auto& v: p.vertices ) {
                v.pos = v.pos + offset;
                v.pos.x *= scale.x;
                v.pos.y *= scale.y;
                v.pos.z *= scale.z;
            }
        }
        return result;
    }

    std::vector<TTriangle> GetResotred( const std::vector<TTriangle>& triangles, const std::tuple<csgjscpp::Vector, csgjscpp::Vector>& offsetAndScale ) {
        auto [ offset, scale ] = offsetAndScale;
        offset = -offset;
        scale.x = 1.0f / scale.x;
        scale.y = 1.0f / scale.y;
        scale.z = 1.0f / scale.z;
        auto result = triangles;
        for( auto& p: result ) {
            for( auto& v: p.vertices ) {
                v.pos.x *= scale.x;
                v.pos.y *= scale.y;
                v.pos.z *= scale.z;
                v.pos = v.pos + offset;
            }
        }
        return result;
    }

    //         offset            scale
    std::tuple<csgjscpp::Vector, csgjscpp::Vector> MoveOperandsToOrigin( std::vector<TTriangle>& operand1, std::vector<TTriangle>& operand2 ) {
        csgjscpp::Vector min( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() );
        csgjscpp::Vector max( -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() );

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

        const auto offset = -(max + min) * 0.5f;
        auto e = max - min;
        const float t = 2.0f;
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