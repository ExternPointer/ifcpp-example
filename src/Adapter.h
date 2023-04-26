#pragma once

#include <vector>

#include "csgjs.h"
#include "earcut.hpp"
#include "ifcpp/Geometry/Matrix.h"
#include "ifcpp/Geometry/StylesConverter.h"
#include "ifcpp/Geometry/VectorAdapter.h"
#include "ifcpp/Ifc/IfcObjectDefinition.h"

namespace ifcpp {

class Polyline {
public:
    std::vector<csgjscpp::Vector> m_points;
    int m_color = 0;
};

class Entity {
public:
    std::shared_ptr<IFC4X3::IfcObjectDefinition> m_ifcObject;
    std::vector<csgjscpp::Polygon> m_polygons;
    std::vector<Polyline> m_polylines;
};


class Adapter {
public:
    using TEntity = Entity;
    using TPolygon = csgjscpp::Polygon;
    using TPolyline = Polyline;
    using TVector = csgjscpp::Vector;

    inline TPolyline CreatePolyline( const std::vector<TVector>& vertices ) {
        return { vertices };
    }
    inline TPolygon CreatePolygon( std::vector<TVector> vertices, std::vector<int> indices ) {
        if( indices.size() != 3 ) {
            // TODO: Log error
            // WTF???
            return {};
        }
        return csgjscpp::Polygon( { { vertices[ indices[ 0 ] ] }, { vertices[ indices[ 1 ] ] }, { vertices[ indices[ 2 ] ] } } );
    }
    inline TEntity CreateEntity( const std::shared_ptr<IFC4X3::IfcObjectDefinition>& ifcObject, const std::vector<TPolygon>& polygons,
                                 const std::vector<TPolyline>& polylines ) {
        return { ifcObject, polygons, polylines };
    }

    inline void Transform( std::vector<TPolygon>* polygons, Matrix<TVector> matrix ) {
        for( auto& p: *polygons ) {
            for( auto& v: p.vertices ) {
                matrix.Transform( &v.pos );
            }
            p = csgjscpp::Polygon( p.vertices );
        }
    }
    inline void Transform( std::vector<TPolyline>* polylines, Matrix<TVector> matrix ) {
        for( auto& p: *polylines ) {
            for( auto& v: p.m_points ) {
                matrix.Transform( &v );
            }
        }
    }

    inline void ApplyStyle( std::vector<TPolygon>* polygons, const std::shared_ptr<Style> &style, bool rewrite = false ) {
        for( auto& p: *polygons ) {
            for( auto& v: p.vertices ) {
                if( !rewrite && v.col != 0) {
                    continue;
                }
                v.col = (int)( 255.0f * style->m_color.a ) << 24 | (int)( 255.0f * style->m_color.b ) << 16 | (int)( 255.0f * style->m_color.g ) << 8 |
                    (int)( 255.0f * style->m_color.r );
            }
        }
    }
    inline void ApplyStyle( std::vector<TPolyline>* polylines, const std::shared_ptr<Style>& style, bool rewrite = false ) {
        for( auto& p: *polylines ) {
            if( !rewrite && p.m_color != 0) {
                continue;
            }
            p.m_color = (int)( 255.0f * style->m_color.a ) << 24 | (int)( 255.0f * style->m_color.b ) << 16 | (int)( 255.0f * style->m_color.g ) << 8 |
                (int)( 255.0f * style->m_color.r );
        }
    }

    inline std::vector<int> Triangulate( std::vector<TVector> loop ) {
        if( loop.size() < 3 ) {
            // WTF, TODO: Log error
            return {};
        }

        TVector normal;
        for( int i = 1; i < loop.size() - 1; i++ ) {
            normal = normal - csgjscpp::cross( loop[ i - 1 ] - loop[ i ], loop[ i + 1 ] - loop[ i ] );
        }
        normal = csgjscpp::unit( normal );
        auto right = csgjscpp::cross( { 0.0f, 0.0f, 1.0f }, normal );
        if( csgjscpp::lengthsquared( right ) < csgjscpp::csgjs_EPSILON ) {
            right = csgjscpp::cross( { 1.0f, 0.0f, 0.0f }, normal );
        }
        if( csgjscpp::lengthsquared( right ) < csgjscpp::csgjs_EPSILON ) {
            // WTF, |normal| == 0 ?????
            return {};
        }
        auto up = csgjscpp::cross( normal, right );
        right = csgjscpp::unit( right );
        up = csgjscpp::unit( up );
        std::vector<std::vector<std::tuple<float, float>>> polygon;
        std::vector<std::tuple<float, float>> outer;
        for( const auto& p: loop ) {
            outer.emplace_back( csgjscpp::dot( right, p ), csgjscpp::dot( up, p ) );
        }


        polygon.push_back( outer );
        const auto result = mapbox::earcut<int>( polygon );
        return result;
    }

    inline std::vector<TPolygon> ComputeUnion( std::vector<TPolygon> operand1, std::vector<TPolygon> operand2 ) {
        if( operand1.empty() ) {
            return operand2;
        }
        if( operand2.empty() ) {
            return operand1;
        }
        const auto [ o, s ] = this->MoveOperandsToOrigin( operand1, operand2 );
        auto result = csgjscpp::modeltopolygons( csgjscpp::csgunion( csgjscpp::modelfrompolygons( operand1 ), csgjscpp::modelfrompolygons( operand2 ) ) );
        this->RestoreResult( result, o, s );
        return result;
    }

    inline std::vector<TPolygon> ComputeIntersection( std::vector<TPolygon> operand1, std::vector<TPolygon> operand2 ) {
        if( operand1.empty() || operand2.empty() ) {
            return {};
        }
        const auto [ o, s ] = this->MoveOperandsToOrigin( operand1, operand2 );
        auto result =
            csgjscpp::modeltopolygons( csgjscpp::csgintersection( csgjscpp::modelfrompolygons( operand1 ), csgjscpp::modelfrompolygons( operand2 ) ) );
        this->RestoreResult( result, o, s );
        return result;
    }

    inline std::vector<TPolygon> ComputeDifference( std::vector<TPolygon> operand1, std::vector<TPolygon> operand2 ) {
        if( operand1.empty() || operand2.empty() ) {
            return operand1;
        }
        const auto [ o, s ] = this->MoveOperandsToOrigin( operand1, operand2 );
        auto result = csgjscpp::modeltopolygons( csgjscpp::csgsubtract( csgjscpp::modelfrompolygons( operand1 ), csgjscpp::modelfrompolygons( operand2 ) ) );
        this->RestoreResult( result, o, s );
        return result;
    }

private:
    //         offset            scale
    std::tuple<csgjscpp::Vector, float> MoveOperandsToOrigin( std::vector<TPolygon>& operand1, std::vector<TPolygon>& operand2 ) {
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

        const auto offset = -( min + max ) * 0.5f;
        const auto scale = sqrtf( 3 ) / csgjscpp::length( max - min );

        for( auto& p: operand1 ) {
            for( auto& v: p.vertices ) {
                v.pos = ( v.pos + offset ) * scale;
            }
        }
        for( auto& p: operand2 ) {
            for( auto& v: p.vertices ) {
                v.pos = ( v.pos + offset ) * scale;
            }
        }

        return { offset, scale };
    }

    void RestoreResult( std::vector<TPolygon>& result, csgjscpp::Vector offset, float scale ) {
        scale = 1.0f / scale;
        offset = -offset;
        for( auto& p: result ) {
            for( auto& v: p.vertices ) {
                v.pos = v.pos * scale + offset;
            }
        }
    }
};

}