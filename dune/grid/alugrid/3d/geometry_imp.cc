// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:
#include <dune/grid/genericgeometry/conversion.hh>

#include "grid.hh"
#include "mappings.hh"

namespace Dune {
  // --Geometry

  template <int mydim, int cdim, class GridImp>
  inline ALU3dGridGeometry<mydim, cdim, GridImp> ::
  ALU3dGridGeometry()
    : geoImpl_(),
      volume_(1.0)
  {}

  template< int mydim, int cdim, class GridImp>
  inline GeometryType
  ALU3dGridGeometry< mydim, cdim, GridImp > :: type () const
  {
    return GeometryType( (elementType == tetra) ?
                         GeometryType :: simplex : GeometryType :: cube, mydim );
  }

  template< int mydim, int cdim, class GridImp>
  inline int
  ALU3dGridGeometry<mydim, cdim, GridImp >::corners() const {
    return corners_;
  }

  template< int mydim, int cdim, class GridImp>
  inline const FieldVector<alu3d_ctype, cdim>&
  ALU3dGridGeometry<mydim, cdim, GridImp >::
  operator[] (int i) const
  {
    typedef GenericGeometry::MapNumberingProvider< mydim > Numbering;
    const unsigned int tid = GenericGeometry::topologyId( type() );
    const int j = Numbering::template dune2generic< mydim >( tid, i );
    return geoImpl_[ j ];
  }

  template< int mydim, int cdim, class GridImp>
  inline FieldVector<alu3d_ctype, cdim>
  ALU3dGridGeometry<mydim, cdim, GridImp >::
  corner (int i) const
  {
    return geoImpl_[ i ];
  }


  template< int mydim, int cdim, class GridImp>
  inline FieldVector<alu3d_ctype, cdim>
  ALU3dGridGeometry<mydim, cdim, GridImp >::
  global (const FieldVector<alu3d_ctype, mydim>& local) const
  {
    FieldVector<alu3d_ctype, cdim> global;
    geoImpl_.mapping().map2world(local, global);
    return global;
  }

  template< int mydim, int cdim, class GridImp >
  inline FieldVector<alu3d_ctype, mydim>
  ALU3dGridGeometry<mydim, cdim, GridImp >::
  local (const FieldVector<alu3d_ctype, cdim>& global) const
  {
    FieldVector<alu3d_ctype, mydim> local;
    geoImpl_.mapping().world2map(global, local);
    return local;
  }

  template< int mydim, int cdim, class GridImp>
  inline alu3d_ctype
  ALU3dGridGeometry<mydim, cdim, GridImp >::
  integrationElement (const FieldVector<alu3d_ctype, mydim>& local) const
  {
    return geoImpl_.mapping().det( local );
  }

  // specialization for tetrahedrons (det constant and known)
  template<>
  inline alu3d_ctype
  ALU3dGridGeometry<3, 3, const ALU3dGrid<3, 3, tetra> >::
  integrationElement (const FieldVector<alu3d_ctype, 3>& local) const
  {
    return 6.0 * volume_;
  }

  template<>
  inline alu3d_ctype
  ALU3dGridGeometry<0, 3, const ALU3dGrid<3, 3, hexa> >::
  integrationElement (const FieldVector<alu3d_ctype, 0>& local) const
  {
    return 1.0;
  }

  template<>
  inline alu3d_ctype
  ALU3dGridGeometry<0, 3, const ALU3dGrid<3, 3, tetra> >::
  integrationElement (const FieldVector<alu3d_ctype, 0>& local) const
  {
    return 1.0;
  }

  template<>
  inline alu3d_ctype
  ALU3dGridGeometry<3, 3, const ALU3dGrid<3, 3, hexa> >::
  volume() const
  {
    return volume_;
  }

  template<>
  inline alu3d_ctype
  ALU3dGridGeometry<3, 3, const ALU3dGrid<3, 3, tetra> >::
  volume() const
  {
    return volume_;
  }

  // specialization for triangles faces
  template<>
  inline alu3d_ctype
  ALU3dGridGeometry<2, 3, const ALU3dGrid<3, 3, tetra> >::
  volume () const
  {
    enum { factor = Factorial<2>::factorial };
    // local vector does not affect the result
    const FieldVector<alu3d_ctype, 2> dummy(0.0);
    return integrationElement( dummy ) / ((alu3d_ctype) factor);
  }

  template<int mydim, int cdim, class GridImp>
  inline alu3d_ctype
  ALU3dGridGeometry<mydim, cdim, GridImp >::
  volume () const
  {
    return integrationElement(FieldVector<alu3d_ctype, mydim> (0.5));
  }

  template< int mydim, int cdim, class GridImp>
  inline bool
  ALU3dGridGeometry<mydim, cdim, GridImp >::
  affine() const
  {
    return geoImpl_.mapping().affine();
  }

  template< int mydim, int cdim, class GridImp>
  inline const FieldMatrix<alu3d_ctype, cdim, mydim>&
  ALU3dGridGeometry<mydim, cdim, GridImp >::
  jacobianInverseTransposed (const FieldVector<alu3d_ctype, mydim>& local) const
  {
    return geoImpl_.mapping().jacobianInverseTransposed( local );
  }

  template< int mydim, int cdim, class GridImp>
  inline const FieldMatrix<alu3d_ctype, mydim, cdim>&
  ALU3dGridGeometry<mydim, cdim, GridImp >::
  jacobianTransposed (const FieldVector<alu3d_ctype, mydim>& local) const
  {
    return geoImpl_.mapping().jacobianTransposed( local );
  }

  template <int mydim, int cdim, class GridImp>
  inline void
  ALU3dGridGeometry<mydim, cdim, GridImp >::
  print (std::ostream& ss) const
  {
    const char* charElType = (elementType == tetra) ? "tetra" : "hexa";
    ss << "ALU3dGridGeometry<" << mydim << "," << cdim << ", " << charElType << "> = {\n";
    for(int i=0; i<corners(); i++)
    {
      ss << " corner " << i << " ";
      ss << "{" << ((*this)[i]) << "}"; ss << std::endl;
    }
    ss << "} \n";
  }

  // built Geometry
  template <int mydim, int cdim, class GridImp>
  template <class GeometryType>
  inline bool
  ALU3dGridGeometry<mydim, cdim, GridImp >::
  buildGeomInFather(const GeometryType &fatherGeom , const GeometryType & myGeom)
  {
    // update geo impl
    geoImpl_.updateInFather( fatherGeom, myGeom );

    // my volume is a part of 1 for hexas, for tetra adjust with factor
    volume_ = myGeom.volume() / fatherGeom.volume();
    if( elementType == tetra )
    {
      volume_ /= 6.0;
#ifndef NDEBUG
      FieldVector< alu3d_ctype, mydim> local( 0.0 );
      assert( std::abs( 6.0 * volume_ - integrationElement( local ) ) < 1e-12 );
#endif
    }

    return true;
  }

  //--hexaBuildGeom
  template <>
  inline bool
  ALU3dGridGeometry<3, 3, const ALU3dGrid<3, 3, hexa> >::
  buildGeom(const IMPLElementType& item)
  {
    // if this assertion is thrown, use ElementTopo::dune2aluVertex instead
    // of number when calling myvertex
    assert( ElementTopo::dune2aluVertex(0) == 0 );
    assert( ElementTopo::dune2aluVertex(1) == 1 );
    assert( ElementTopo::dune2aluVertex(2) == 3 );
    assert( ElementTopo::dune2aluVertex(3) == 2 );
    assert( ElementTopo::dune2aluVertex(4) == 4 );
    assert( ElementTopo::dune2aluVertex(5) == 5 );
    assert( ElementTopo::dune2aluVertex(6) == 7 );
    assert( ElementTopo::dune2aluVertex(7) == 6 );

    // update geo impl
    geoImpl_.update( item.myvertex(0)->Point(),
                     item.myvertex(1)->Point(),
                     item.myvertex(3)->Point(),
                     item.myvertex(2)->Point(),
                     item.myvertex(4)->Point(),
                     item.myvertex(5)->Point(),
                     item.myvertex(7)->Point(),
                     item.myvertex(6)->Point() );

    // get volume of element
    volume_ = item.volume();

    return true;
  }

  //--tetraBuildGeom
  template <>
  inline bool
  ALU3dGridGeometry<3, 3, const ALU3dGrid<3, 3, tetra> >::
  buildGeom(const IMPLElementType& item)
  {
    // if this assertion is thrown, use ElementTopo::dune2aluVertex instead
    // of number when calling myvertex
    assert( ElementTopo::dune2aluVertex(0) == 0 );
    assert( ElementTopo::dune2aluVertex(1) == 1 );
    assert( ElementTopo::dune2aluVertex(2) == 2 );
    assert( ElementTopo::dune2aluVertex(3) == 3 );

    // update geo impl
    geoImpl_.update( item.myvertex(0)->Point(),
                     item.myvertex(1)->Point(),
                     item.myvertex(2)->Point(),
                     item.myvertex(3)->Point() );

    // get volume of element
    volume_ = item.volume();

    return true;
  }

  template <>
  inline bool
  ALU3dGridGeometry<2,3, const ALU3dGrid<3, 3, hexa> > ::
  buildGeom(const HFaceType & item, int twist, int duneFace )
  {
    // get geo face
    const GEOFaceType& face = static_cast<const GEOFaceType&> (item);

    //assert( duneFace >= 0 && duneFace < 6 );
    if(duneFace < 0 ) duneFace = 0;

    // for all vertices of this face get rotatedIndex
    int rotatedALUIndex[4];
    for (int i = 0; i < 4; ++i)
    {
      // Transform Dune index to ALU index and apply twist
      int localALUIndex = ElementTopo::dune2aluFaceVertex(duneFace,i);
      rotatedALUIndex[i] = FaceTopo::twist(localALUIndex, twist);
    }

    // update geometry implementation
    geoImpl_.update( face.myvertex(rotatedALUIndex[0])->Point(),
                     face.myvertex(rotatedALUIndex[1])->Point(),
                     face.myvertex(rotatedALUIndex[2])->Point(),
                     face.myvertex(rotatedALUIndex[3])->Point() );

    return true;
  }

  template <>
  inline bool
  ALU3dGridGeometry<2,3, const ALU3dGrid<3, 3, tetra> > ::
  buildGeom(const HFaceType & item, int twist, int duneFace )
  {
    // get geo face
    const GEOFaceType& face = static_cast<const GEOFaceType&> (item);

    //assert( duneFace >= 0 && duneFace < 4 );
    if(duneFace < 0 ) duneFace = 0;

    // for all vertices of this face get rotatedIndex
    int rotatedALUIndex[3];
    for (int i = 0; i < 3; ++i)
    {
      // Transform Dune index to ALU index and apply twist
      int localALUIndex = ElementTopo::dune2aluFaceVertex(duneFace,i);
      rotatedALUIndex[i] = FaceTopo::twist(localALUIndex, twist);
    }

    // update geometry implementation
    geoImpl_.update( face.myvertex(rotatedALUIndex[0])->Point(),
                     face.myvertex(rotatedALUIndex[1])->Point(),
                     face.myvertex(rotatedALUIndex[2])->Point());

    return true;
  }
  template <>
  template <class coord_t>
  inline bool
  ALU3dGridGeometry<2,3, const ALU3dGrid<3, 3, hexa> > ::
  buildGeom(const coord_t& p0,
            const coord_t& p1,
            const coord_t& p2,
            const coord_t& p3)
  {
    // update geometry implementation
    geoImpl_.update( p0, p1, p2, p3 );
    return true;
  }

  template <>
  template <class coord_t>
  inline bool
  ALU3dGridGeometry<2,3, const ALU3dGrid<3, 3, tetra> > ::
  buildGeom(const coord_t& p0,
            const coord_t& p1,
            const coord_t& p2)
  {
    // update geometry implementation
    geoImpl_.update( p0, p1, p2 );
    return true;
  }


  template <>
  inline bool
  ALU3dGridGeometry<2,3, const ALU3dGrid<3, 3, hexa> > ::
  buildGeom(const FaceCoordinatesType& coords)
  {
    return buildGeom( coords[0], coords[1], coords[2], coords[3] );
  }

  template <>
  inline bool
  ALU3dGridGeometry<2,3, const ALU3dGrid<3, 3, tetra> > ::
  buildGeom(const FaceCoordinatesType& coords)
  {
    return buildGeom( coords[0], coords[1], coords[2] );
  }

  template <int mydim, int cdim, class GridImp> // for edges
  inline bool
  ALU3dGridGeometry<mydim, cdim, GridImp >::
  buildGeom(const HEdgeType & item, int twist, int)
  {
    const GEOEdgeType & edge = static_cast<const GEOEdgeType &> (item);
    // update geometry implementation
    geoImpl_.update( edge.myvertex((twist)  %2)->Point(),
                     edge.myvertex((1+twist)%2)->Point() );
    return true;
  }

  template <int mydim, int cdim, class GridImp> // for Vertices ,i.e. Points
  inline bool
  ALU3dGridGeometry<mydim, cdim, GridImp >::
  buildGeom(const VertexType & item, int twist, int)
  {
    // update geometry implementation
    geoImpl_.update( static_cast<const GEOVertexType &> (item).Point() );
    return true;
  }

} // end namespace Dune