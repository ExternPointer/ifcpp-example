#pragma once

// Original CSG.JS library by Evan Wallace (http://madebyevan.com), under the MIT license.
// GitHub: https://github.com/evanw/csg.js/
//
// C++ port by Tomasz Dabrowski (http://28byteslater.com), under the MIT license.
// GitHub: https://github.com/dabroz/csgjscpp-cpp/
//
// Constructive Solid Geometry (CSG) is a modeling technique that uses Boolean
// operations like union and intersection to combine 3D solids. This library
// implements CSG operations on meshes elegantly and concisely using BSP trees,
// and is meant to serve as an easily understandable implementation of the
// algorithm. All edge cases involving overlapping coplanar polygons in both
// solids are correctly handled.
//
// modified by dazza - 200421
//
// modified by ExternPointer for ifcpp - remove color and normal interpolating,
// performance improvements

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <vector>


namespace csg {

const double TOLERANCE = 0.0001f;


struct Vector {
    double x, y, z;

    Vector()
        : x( 0.0f )
        , y( 0.0f )
        , z( 0.0f ) {
    }
    Vector( double x, double y, double z )
        : x( x )
        , y( y )
        , z( z ) {
    }
};


inline bool ApproxEqual( double a, double b ) {
    return fabs( a - b ) < TOLERANCE;
}

inline bool operator==( const Vector& a, const Vector& b ) {
    return ApproxEqual( a.x, b.x ) && ApproxEqual( a.y, b.y ) && ApproxEqual( a.z, b.z );
}

inline bool operator!=( const Vector& a, const Vector& b ) {
    return !ApproxEqual( a.x, b.x ) || !ApproxEqual( a.y, b.y ) || !ApproxEqual( a.z, b.z );
}

inline Vector operator+( const Vector& a, const Vector& b ) {
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

inline Vector operator-( const Vector& a, const Vector& b ) {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

inline Vector operator*( const Vector& a, double b ) {
    return { a.x * b, a.y * b, a.z * b };
}

inline Vector operator/( const Vector& a, double b ) {
    return a * ( (double)1.0 / b );
}

inline double Dot( const Vector& a, const Vector& b ) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline double Length( const Vector& a ) {
    return (double)sqrt( Dot( a, a ) );
}

inline double LengthSquared( const Vector& a ) {
    return Dot( a, a );
}

inline Vector Normalized( const Vector& a ) {
    return a / Length( a );
}

inline Vector Cross( const Vector& a, const Vector& b ) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

inline Vector operator-( const Vector& a ) {
    return { -a.x, -a.y, -a.z };
}


struct Plane {
    Vector normal;
    double w = 0;

    Plane() = default;

    Plane( const Vector& a, const Vector& b, const Vector& c ) {
        this->normal = Normalized( Cross( b - a, c - a ) );
        this->w = Dot( this->normal, a );
    }

    [[nodiscard]] inline bool IsValid() const {
        return LengthSquared( this->normal ) > 0.0f;
    }

    inline void Flip() {
        this->normal = -this->normal;
        this->w *= -1.0f;
    }

    enum Classification { COPLANAR = 0, FRONT = 1, BACK = 2, SPANNING = 3 };
    [[nodiscard]] inline Classification ClassifyPoint( const Vector& p ) const {
        double t = Dot( normal, p ) - this->w;
        Classification c = ( t < -TOLERANCE ) ? BACK : ( ( t > TOLERANCE ) ? FRONT : COPLANAR );
        return c;
    }
};

struct Polygon {
    std::vector<Vector> vertices;
    Plane plane;

    Polygon() = default;

    Polygon( Polygon&& other ) {
        this->vertices = std::move( other.vertices );
        this->plane = other.plane;
    }

    Polygon( const Polygon& other ) = default;

    Polygon& operator=( Polygon&& other ) {
        this->vertices = std::move( other.vertices );
        this->plane = other.plane;
        return *this;
    }

    Polygon& operator=( const Polygon& other ) = default;

    explicit Polygon( const std::vector<Vector>& list )
        : vertices( list )
        , plane( vertices[ 0 ], vertices[ 1 ], vertices[ 2 ] ) {
    }

    Polygon( const std::vector<Vector>& list, const Plane& plane )
        : vertices( list )
        , plane( plane ) {
    }

    inline void Flip() {
        std::reverse( vertices.begin(), vertices.end() );
        plane.Flip();
    }
};

namespace details {

    inline void SplitPolygon( const Plane& plane, const Polygon& poly, std::vector<Polygon>& coplanarFront, std::vector<Polygon>& coplanarBack,
                              std::vector<Polygon>& front, std::vector<Polygon>& back ) {

        int polygonType = 0;
        for( const auto& v: poly.vertices ) {
            polygonType |= plane.ClassifyPoint( v );
        }

        if( poly.plane.normal == plane.normal && ApproxEqual( poly.plane.w, plane.w ) ||
            poly.plane.normal == -plane.normal && ApproxEqual( poly.plane.w, -plane.w ) ) {
            polygonType = Plane::COPLANAR;
        }

        switch( polygonType ) {
        case Plane::COPLANAR: {
            if( Dot( plane.normal, poly.plane.normal ) > 0 )
                coplanarFront.push_back( poly );
            else
                coplanarBack.push_back( poly );
            break;
        }
        case Plane::FRONT: {
            front.push_back( poly );
            break;
        }
        case Plane::BACK: {
            back.push_back( poly );
            break;
        }
        case Plane::SPANNING: {
            std::vector<Vector> f, b;

            for( size_t i = 0; i < poly.vertices.size(); i++ ) {

                size_t j = ( i + 1 ) % poly.vertices.size();

                const auto& vi = poly.vertices[ i ];
                const auto& vj = poly.vertices[ j ];

                int ti = plane.ClassifyPoint( vi );
                int tj = plane.ClassifyPoint( vj );

                if( ti != Plane::BACK ) {
                    f.push_back( vi );
                }
                if( ti != Plane::FRONT ) {
                    b.push_back( vi );
                }
                if( ( ti | tj ) == Plane::SPANNING ) {
                    double t = ( plane.w - Dot( plane.normal, vi ) ) / Dot( plane.normal, vj - vi );
                    const auto v = vi + ( vj - vi ) * t;
                    if( f.empty() || f[ f.size() - 1 ] != v ) {
                        f.push_back( v );
                    }
                    if( b.empty() || b[ b.size() - 1 ] != v ) {
                        b.push_back( v );
                    }
                }
            }
            if( f.size() >= 3 )
                front.emplace_back( f, poly.plane );
            if( b.size() >= 3 )
                back.emplace_back( b, poly.plane );
            break;
        }
        default:
            break;
        }
    }

    inline const Plane& FindOptimalSplittingPlane( const std::vector<Polygon>& polygons ) {
        Vector min( std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max() );
        Vector max = -min;

        for( const auto& p: polygons ) {
            for( const auto& v: p.vertices ) {
                min.x = std::min( min.x, v.x );
                min.y = std::min( min.y, v.y );
                min.z = std::min( min.z, v.z );
                max.x = std::max( max.x, v.x );
                max.y = std::max( max.y, v.y );
                max.z = std::max( max.z, v.z );
            }
        }

        Vector center = ( max + min ) * 0.5;

        size_t resultIdx = 0;
        double delta = -std::numeric_limits<double>::max();

        for( int i = 0; i < polygons.size(); i++ ) {
            const auto& p = polygons[ i ];
            auto d = std::fabs( Dot( p.plane.normal, center ) - p.plane.w );
            if( d > delta ) {
                resultIdx = i;
                delta = d;
            }
        }

        return polygons[ resultIdx ].plane;
    }

    struct CSGNode {
        std::vector<Polygon> polygons;
        CSGNode* front;
        CSGNode* back;
        Plane plane;

        CSGNode()
            : front( nullptr )
            , back( nullptr ) {
        }

        explicit CSGNode( const std::vector<Polygon>& list )
            : front( nullptr )
            , back( nullptr ) {
            Build( list );
        }

        ~CSGNode() {
            std::deque<CSGNode*> nodes_to_delete;
            std::deque<CSGNode*> nodes_to_disassemble;

            nodes_to_disassemble.push_back( this );
            while( !nodes_to_disassemble.empty() ) {
                CSGNode* me = nodes_to_disassemble.front();
                nodes_to_disassemble.pop_front();

                if( me->front ) {
                    nodes_to_disassemble.push_back( me->front );
                    nodes_to_delete.push_back( me->front );
                    me->front = nullptr;
                }
                if( me->back ) {
                    nodes_to_disassemble.push_back( me->back );
                    nodes_to_delete.push_back( me->back );
                    me->back = nullptr;
                }
            }

            for( auto& it: nodes_to_delete ) {
                delete it;
            }
        }

        [[nodiscard]] inline bool IsEmpty() const {
            return !this->front && this->back && this->polygons.empty();
        }

        inline void Clear() {
            delete this->back;
            delete this->front;
            this->polygons = {};
        }

        [[nodiscard]] inline CSGNode* Clone() const {
            auto ret = new CSGNode();

            std::deque<std::pair<const CSGNode*, CSGNode*>> nodes;
            nodes.emplace_back( this, ret );
            while( !nodes.empty() ) {
                const CSGNode* original = nodes.front().first;
                CSGNode* clone = nodes.front().second;
                nodes.pop_front();

                clone->polygons = original->polygons;
                clone->plane = original->plane;
                if( original->front ) {
                    clone->front = new CSGNode();
                    nodes.emplace_back( original->front, clone->front );
                }
                if( original->back ) {
                    clone->back = new CSGNode();
                    nodes.emplace_back( original->back, clone->back );
                }
            }

            return ret;
        }

        inline void ClipTo( const CSGNode* other ) {
            std::deque<CSGNode*> nodes;
            nodes.push_back( this );
            while( !nodes.empty() ) {
                CSGNode* me = nodes.front();
                nodes.pop_front();

                me->polygons = other->clippolygons( me->polygons );
                if( me->front )
                    nodes.push_back( me->front );
                if( me->back )
                    nodes.push_back( me->back );
            }
        }

        inline void Invert() {
            std::deque<CSGNode*> nodes;
            nodes.push_back( this );
            while( !nodes.empty() ) {
                CSGNode* me = nodes.front();
                nodes.pop_front();

                for( auto& polygon: me->polygons ) {
                    polygon.Flip();
                }
                me->plane.Flip();
                std::swap( me->front, me->back );
                if( me->front ) {
                    nodes.push_back( me->front );
                }
                if( me->back ) {
                    nodes.push_back( me->back );
                }
            }
        }

        inline void Build( const std::vector<Polygon>& ilist ) {
            if( ilist.empty() ) {
                return;
            }

            std::deque<std::pair<CSGNode*, std::vector<Polygon>>> builds;
            builds.emplace_back( this, ilist );

            while( !builds.empty() ) {
                CSGNode* me = builds.front().first;
                const std::vector<Polygon>& list = builds.front().second;

                if( !me->plane.IsValid() )
                    me->plane = FindOptimalSplittingPlane( list );
                std::vector<Polygon> list_front, list_back;

                for( const auto& p: list ) {
                    SplitPolygon( me->plane, p, me->polygons, me->polygons, list_front, list_back );
                }

                if( !list_front.empty() ) {
                    if( !me->front ) {
                        me->front = new CSGNode();
                    }
                    builds.emplace_back( me->front, std::move( list_front ) );
                }
                if( !list_back.empty() ) {
                    if( !me->back ) {
                        me->back = new CSGNode();
                    }
                    builds.emplace_back( me->back, std::move( list_back ) );
                }

                builds.pop_front();
            }
        }

        [[nodiscard]] inline std::vector<Polygon> clippolygons( const std::vector<Polygon>& ilist ) const {
            std::vector<Polygon> result;

            std::deque<std::pair<const CSGNode* const, std::vector<Polygon>>> clips;
            clips.emplace_back( this, ilist );
            while( !clips.empty() ) {
                const CSGNode* me = clips.front().first;
                const std::vector<Polygon>& list = clips.front().second;

                if( !me->plane.IsValid() ) {
                    result.insert( result.end(), list.begin(), list.end() );
                    clips.pop_front();
                    continue;
                }

                std::vector<Polygon> list_front, list_back;
                for( const auto& i: list ) {
                    SplitPolygon( me->plane, i, list_front, list_back, list_front, list_back );
                }

                if( me->front ) {
                    clips.emplace_back( me->front, std::move( list_front ) );
                } else {
                    result.insert( result.end(), list_front.begin(), list_front.end() );
                }

                if( me->back ) {
                    clips.emplace_back( me->back, std::move( list_back ) );
                }

                clips.pop_front();
            }

            return result;
        }

        [[nodiscard]] inline std::vector<Polygon> allpolygons() const {
            std::vector<Polygon> result;

            std::deque<const CSGNode*> nodes;
            nodes.push_back( this );
            while( !nodes.empty() ) {
                const CSGNode* me = nodes.front();
                nodes.pop_front();

                result.insert( result.end(), me->polygons.begin(), me->polygons.end() );
                if( me->front )
                    nodes.push_back( me->front );
                if( me->back )
                    nodes.push_back( me->back );
            }

            return result;
        }
    };


    inline void UnionInplace( CSGNode* a, const CSGNode* b1 ) {
        if( a->IsEmpty() ) {
            *a = *b1;
            return;
        }
        if( b1->IsEmpty() ) {
            return;
        }
        CSGNode* b = b1->Clone();
        a->ClipTo( b );
        b->ClipTo( a );
        b->Invert();
        b->ClipTo( a );
        b->Invert();
        a->Build( b->allpolygons() );
        delete b;
    }

    [[nodiscard]] inline CSGNode* Union( const CSGNode* a1, const CSGNode* b1 ) {
        CSGNode* a = a1->Clone();
        UnionInplace( a, b1 );
        return a;
    }


    inline void DifferenceInplace( CSGNode* a, const CSGNode* b1 ) {
        if( a->IsEmpty() || b1->IsEmpty() ) {
            return;
        }
        CSGNode* b = b1->Clone();
        a->Invert();
        a->ClipTo( b );
        b->ClipTo( a );
        b->Invert();
        b->ClipTo( a );
        b->Invert();
        a->Build( b->allpolygons() );
        a->Invert();
        delete b;
    }

    [[nodiscard]] inline CSGNode* Difference( const CSGNode* a1, const CSGNode* b1 ) {
        CSGNode* a = a1->Clone();
        DifferenceInplace( a, b1 );
        return a;
    }

    inline void IntersectionInplace( CSGNode* a, const CSGNode* b1 ) {
        if( a->IsEmpty() || b1->IsEmpty() ) {
            a->Clear();
            return;
        }
        CSGNode* b = b1->Clone();
        a->Invert();
        b->ClipTo( a );
        b->Invert();
        a->ClipTo( b );
        b->ClipTo( a );
        a->Build( b->allpolygons() );
        a->Invert();
        delete b;
    }

    [[nodiscard]] inline CSGNode* Intersection( const CSGNode* a1, const CSGNode* b1 ) {
        CSGNode* a = a1->Clone();
        IntersectionInplace( a, b1 );
        return a;
    }

    inline std::vector<Polygon> DoCsgOperation( const std::vector<Polygon>& apoly, const std::vector<Polygon>& bpoly,
                                                const std::function<CSGNode*( const CSGNode* a1, const CSGNode* b1 )>& fun ) {

        CSGNode A( apoly );
        CSGNode B( bpoly );
        std::unique_ptr<CSGNode> AB( fun( &A, &B ) );
        return AB->allpolygons();
    }

}

[[nodiscard]] inline std::vector<Polygon> Union( const std::vector<Polygon>& a, const std::vector<Polygon>& b ) {
    return DoCsgOperation( a, b, details::Union );
}

[[nodiscard]] inline std::vector<Polygon> Intersection( const std::vector<Polygon>& a, const std::vector<Polygon>& b ) {
    return DoCsgOperation( a, b, details::Intersection );
}

[[nodiscard]] inline std::vector<Polygon> Difference( const std::vector<Polygon>& a, const std::vector<Polygon>& b ) {
    return DoCsgOperation( a, b, details::Difference );
}


} // namespace csgjscpp