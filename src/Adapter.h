#pragma once

#include <vector>

#include "csg.hpp"


inline csg::vertex operator+( const csg::vertex& a, const csg::vertex& b ) {
    return csg::vertex( a.x + b.x, a.y + b.y, a.z + b.z );
}
inline csg::vertex operator-( const csg::vertex& a, const csg::vertex& b ) {
    return csg::vertex( a.x - b.x, a.y - b.y, a.z - b.z );
}
inline csg::vertex operator*( const csg::vertex& a, double b ) {
    return csg::vertex( a.x * b, a.y * b, a.z * b );
}
inline csg::vertex operator-( const csg::vertex& a ) {
    return csg::vertex( -a.x, -a.y, -a.z );
}

#include "earcut.hpp"
#include "ifcpp/Geometry/Matrix.h"
#include "ifcpp/Geometry/StyleConverter.h"
#include "ifcpp/Geometry/VectorAdapter.h"
#include "ifcpp/Ifc/IfcObjectDefinition.h"


class Polyline {
public:
    std::vector<csg::vertex> m_points;
    unsigned int m_color = 0;
};

class Mesh {
public:
    std::vector<csg::triangle> m_polygons;
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
    using TTriangle = csg::triangle;
    using TPolyline = Polyline;
    using TMesh = Mesh;
    using TVector = csg::vertex;

    inline TTriangle CreateTriangle( std::vector<TVector> vertices, std::vector<int> indices ) {
        if( indices.size() != 3 ) {
            // TODO: Log error
            // WTF???
            return { {}, {}, {} };
        }
        return {
            vertices[ indices[ 0 ] ],
            vertices[ indices[ 1 ] ],
            vertices[ indices[ 2 ] ],
        };
    }
    inline TPolyline CreatePolyline( const std::vector<TVector>& vertices ) {
        return { vertices };
    }
    inline TMesh CreateMesh( const std::vector<TTriangle>& triangles ) {
        return { triangles };
    }
    inline TPolyline CreatePolyline( const TPolyline& other ) {
        return other;
    }
    inline TMesh CreateMesh( const TMesh& other ) {
        return other;
    }
    inline TEntity CreateEntity( const std::shared_ptr<IFC4X3::IfcObjectDefinition>& ifcObject, const std::vector<TMesh>& meshes,
                                 const std::vector<TPolyline>& polylines ) {
        return { ifcObject, meshes, polylines };
    }

    inline bool IsPolygonValid( const TTriangle& p ) {
        TVector normal;

        auto a = p.a_;
        auto b = p.b_;
        auto c = p.c_;
        auto n = csg::impl::cross( a - b, c - b );

        auto l = csg::impl::magnitudeSquared( n );
        if( isnan( l ) || isinf( l ) || l < 1e-12 )
            return false;
        return true;
    }

    inline void Transform( std::vector<TMesh>* meshes, ifcpp::Matrix<TVector> matrix ) {
        for( auto& m: *meshes ) {
            for( auto& t: m.m_polygons ) {
                matrix.Transform( &t.a_ );
                matrix.Transform( &t.b_ );
                matrix.Transform( &t.c_ );
            }
            for( int i = m.m_polygons.size() - 1; i >= 0; i-- ) {
                if( !IsPolygonValid( m.m_polygons[ i ] ) ) {
                    m.m_polygons.erase( m.m_polygons.begin() + i );
                }
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

    inline bool IsPointValid( const TVector& v ) {
        return v.x == v.x && v.y == v.y && v.z == v.z;
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
                    const auto n = csg::impl::cross( b - a, c - b );
                    if( csg::impl::magnitudeSquared( n ) > csg::impl::magnitudeSquared( normal ) ) {
                        normal = n;
                    }
                    if( csg::impl::magnitudeSquared( normal ) > 1e-12 ) {
                        goto BREAK;
                    }
                }
            }
        }
    BREAK:
        normal = csg::impl::unitise( normal );

        auto right = csg::impl::cross( { 0.0f, 0.0f, 1.0f }, normal );
        if( csg::impl::magnitudeSquared( right ) < 1e-6 ) {
            right = csg::impl::cross( normal, { 0.0f, -1.0f, 0.0f } );
        }
        right = csg::impl::unitise( right );
        auto up = csg::impl::unitise( csg::impl::cross( normal, right ) );


        std::vector<std::tuple<double, double>> outer;

        double minx = std::numeric_limits<double>::max(), miny = std::numeric_limits<double>::max();

        for( const auto& p: loop ) {
            double x = csg::impl::dot( right, p - origin );
            double y = csg::impl::dot( up, p - origin );
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
            if( !IsPointValid( a ) || !IsPointValid( b ) || !IsPointValid( c ) || csg::impl::magnitudeSquared( csg::impl::cross( b - a, c - b ) ) < 1e-12 ) {
                result.erase( result.begin() + i - 1, result.begin() + i + 2 );
                i -= 3;
            }
        }
        return result;
    }

    inline std::vector<TMesh> ComputeUnion( std::vector<TMesh>& operand1, std::vector<TMesh>& operand2 ) {
        this->RemoveEmptyOperands( &operand1, &operand2 );
        if( operand1.empty() ) {
            return operand2;
        }
        TMesh result;
        result.m_color = operand1[ 0 ].m_color;
        operand1.erase( operand1.begin() );
        for( const auto& o: operand1 ) {
            try {
                result.m_polygons = *csg::Union( result.m_polygons, o.m_polygons, 0.001 );
            } catch( std::exception ) {
            }
        }
        for( const auto& o: operand2 ) {
            try {
                result.m_polygons = *csg::Union( result.m_polygons, o.m_polygons, 0.001 );
            } catch( std::exception ) {
            }
        }
        return { result };
    }
    inline std::vector<TMesh> ComputeIntersection( std::vector<TMesh> operand1, std::vector<TMesh> operand2 ) {
        this->RemoveEmptyOperands( &operand1, &operand2 );
        if( operand1.empty() || operand2.empty() ) {
            return {};
        }

        for( auto& o1: operand1 ) {
            for( const auto& o2: operand2 ) {
                try {
                    o1.m_polygons = *csg::Intersection( o1.m_polygons, o2.m_polygons, 0.001 );
                } catch( std::exception ) {
                }
            }
        }

        this->RemoveEmptyOperands( &operand1 );

        return operand1;
    }
    inline std::vector<TMesh> ComputeDifference( std::vector<TMesh> operand1, std::vector<TMesh> operand2 ) {
        this->RemoveEmptyOperands( &operand1, &operand2 );
        if( operand1.empty() || operand2.empty() ) {
            return operand1;
        }

        for( auto& o1: operand1 ) {
            for( const auto& o2: operand2 ) {
                try {
                    o1.m_polygons = *csg::Difference( o1.m_polygons, o2.m_polygons, 0.001 );
                } catch( std::exception ) {
                }
            }
        }

        this->RemoveEmptyOperands( &operand1 );

        return operand1;
    }

private:
    inline void RemoveEmptyOperands( std::vector<TMesh>* operand1, std::vector<TMesh>* operand2 ) {
        this->RemoveEmptyOperands( operand1 );
        this->RemoveEmptyOperands( operand2 );
    }
    inline void RemoveEmptyOperands( std::vector<TMesh>* operand ) {
        for( int i = 0; i < operand->size(); i++ ) {
            if( operand->at( i ).m_polygons.empty() ) {
                operand->erase( operand->begin() + i );
                i--;
            }
        }
    }
};