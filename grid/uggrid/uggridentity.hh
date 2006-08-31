// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:
#ifndef DUNE_UGGRIDENTITY_HH
#define DUNE_UGGRIDENTITY_HH

/** \file
 * \brief The UGGridEntity class and its specializations
 */

#include "uggridrenumberer.hh"

#include <dune/grid/common/referenceelements.hh>


namespace Dune {

  // Forward declarations
  template<int codim, int dim, class GridImp>
  class UGGridEntity;
  template<int dim, int dimworld>
  class UGGrid;
  template<int codim, class GridImp>
  class UGGridEntityPointer;
  template<int codim, PartitionIteratorType pitype, class GridImp>
  class UGGridLevelIterator;
  template<class GridImp>
  class UGGridLevelIntersectionIterator;
  template<class GridImp>
  class UGGridLeafIntersectionIterator;
  template<class GridImp>
  class UGGridHierarchicIterator;

  template<int codim, int dim, class GridImp>
  class UGMakeableEntity :
    public GridImp::template Codim<codim>::Entity
  {
  public:

    UGMakeableEntity(typename UG_NS<dim>::template Entity<codim>::T* target) :
      GridImp::template Codim<codim>::Entity (UGGridEntity<codim, dim, const GridImp>())
    {
      this->realEntity.setToTarget(target);
    }

    UGMakeableEntity() :
      GridImp::template Codim<codim>::Entity (UGGridEntity<codim, dim, const GridImp>())
    {}

    void setToTarget(typename UG_NS<dim>::template Entity<codim>::T* target) {
      this->realEntity.setToTarget(target);
    }

    typename UG_NS<dim>::template Entity<codim>::T* getTarget() {
      return this->realEntity.target_;
    }

  };

  //**********************************************************************
  //
  // --UGGridEntity
  // --Entity
  //
  /** \brief The implementation of entities in a UGGrid
     \ingroup UGGrid

     A Grid is a container of grid entities. An entity is parametrized by the codimension.
     An entity of codimension c in dimension d is a d-c dimensional object.

   */
  template<int codim, int dim, class GridImp>
  class UGGridEntity :
    public EntityDefaultImplementation <codim,dim,GridImp,UGGridEntity>
  {

    template <int codim_, PartitionIteratorType PiType_, class GridImp_>
    friend class UGGridLevelIterator;

    friend class UGMakeableEntity<codim,dim,GridImp>;

    friend class UGGrid<dim, dim>;

    template <class GridImp_>
    friend class UGGridLevelIndexSet;

    template <class GridImp_>
    friend class UGGridLeafIndexSet;

    typedef typename GridImp::ctype UGCtype;

  public:

    typedef typename GridImp::template Codim<codim>::Geometry Geometry;

    //! level of this element
    int level () const {
      return UG_NS<dim>::myLevel(target_);
    }

    int levelIndex() const {
      return UG_NS<dim>::levelIndex(target_);
    }

    int leafIndex() const {
      return UG_NS<dim>::leafIndex(target_);
    }

    unsigned int localId() const {
      return UG_NS<dim>::id(target_);
    }

    unsigned int globalId() const {
#ifdef ModelP
      return target_->ddd.gid;
#else
      return UG_NS<dim>::id(target_);
#endif
    }


    /** \brief The partition type for parallel computing
     * \todo So far it always returns InteriorEntity */
    PartitionType partitionType () const { return InteriorEntity; }

    /*! Intra-element access to entities of codimension cc > codim. Return number of entities
       with codimension cc.
     */
    //!< Default codim 1 Faces and codim == dim Vertices
    template<int cc> int count () const;

    //! return index of sub entity with codim = cc and local number i
    //! i.e. return global number of vertex i
    /** \todo So far only implemented for cc==dim */
    template<int cc> int subIndex (int i) const;

    //! geometry of this entity
    const Geometry& geometry () const {return geo_;}

    UGGridEntityPointer<0,GridImp> ownersFather() const
    {
      UGGridEntityPointer<0,GridImp> myfather;
      if (UG_NS<dim>::NFather(target_)!=0)
        if (UG_NS<dim>::myLevel(UG_NS<dim>::NFather(target_)) == this->level()-1)
        {
          myfather.setToTarget(UG_NS<dim>::NFather(target_));
          return myfather;
        }
      DUNE_THROW(NotImplemented, "ownersFather for anything else than new vertices");
      return myfather;
    }

    /** \brief Location of this vertex within a mesh entity of codimension 0 on the coarse grid.
     *
       This can speed up on-the-fly interpolation for linear conforming elements
       Possibly this is sufficient for all applications we want on-the-fly.
     */
    const FieldVector<UGCtype, dim>& positionInOwnersFather() const
    {
      if (UG_NS<dim>::NFather(target_)!=0)
        if (UG_NS<dim>::myLevel(UG_NS<dim>::NFather(target_)) == this->level()-1)
        {
          UG_NS<dim>::PositionInFather(target_,pos_);
          return pos_;
        }
      DUNE_THROW(NotImplemented, "positionInOwnersFather for anything else than new vertices");
      return pos_;
    }



  private:


    void setToTarget(typename UG_NS<dim>::template Entity<codim>::T* target) {
      target_ = target;
      geo_.setToTarget(target);
    }

    //! the current geometry
    UGMakeableGeometry<dim-codim,dim,GridImp> geo_;

    FieldVector<UGCtype, dim> localFatherCoords_;

    typename UG_NS<dim>::template Entity<codim>::T* target_;

    mutable FieldVector<UGCtype, dim> pos_;

  };

  //***********************
  //
  //  --UGGridEntity
  //  --0Entity
  //
  //***********************

  /** \brief Specialization for codim-0-entities.
   * \ingroup UGGrid
   *
   * This class embodies the topological parts of elements of the grid.
   * It has an extended interface compared to the general entity class.
   * For example, Entities of codimension 0  allow to visit all neighbors.
   *
   * UGGrid only implements the cases dim==dimworld==2 and dim=dimworld==3.
   */
  template<int dim, class GridImp>
  class UGGridEntity<0,dim,GridImp> :
    public EntityDefaultImplementation<0,dim,GridImp, UGGridEntity>
  {
    friend class UGGrid < dim , dim>;
    friend class UGGridLeafIntersectionIterator <GridImp>;
    friend class UGGridHierarchicIterator <GridImp>;

    template <int codim_, PartitionIteratorType PiType_, class GridImp_>
    friend class UGGridLevelIterator;

    friend class UGMakeableEntity<0,dim,GridImp>;

    typedef typename GridImp::ctype UGCtype;

  public:

    typedef typename GridImp::template Codim<0>::Geometry Geometry;

    //! The Iterator over neighbors on this level
    typedef UGGridLevelIntersectionIterator<GridImp> LevelIntersectionIterator;

    //! The Iterator over neighbors on the leaf level
    typedef UGGridLeafIntersectionIterator<GridImp> LeafIntersectionIterator;

    //! Iterator over descendants of the entity
    typedef UGGridHierarchicIterator<GridImp> HierarchicIterator;

    //! Level of this element
    int level () const {
      return UG_NS<dim>::myLevel(target_);
    }

    int levelIndex() const {
      return UG_NS<dim>::levelIndex(target_);
    }

    int leafIndex() const {
      return UG_NS<dim>::leafIndex(target_);
    }

    unsigned int localId() const {
      return UG_NS<dim>::id(target_);
    }

    unsigned int globalId() const {
#ifdef ModelP
      return target_->ge.ddd.gid;
#else
      return UG_NS<dim>::id(target_);
#endif
    }

    /** \brief The partition type for parallel computing */
    PartitionType partitionType () const {
#ifndef ModelP
      return InteriorEntity;
#else
#define PARHDRE(p) (&((p)->ge.ddd))
#define EPRIO(e) DDD_InfoPriority(PARHDRE(e))
      if (EPRIO(target_) == UG::PrioHGhost
          || EPRIO(target_) == UG::PrioVGhost
          || EPRIO(target_) == UG::PrioVHGhost)
        return GhostEntity;
      else
        return InteriorEntity;
#undef EPRIO
#undef PARHDRE
#endif
    }

    //! Geometry of this entity
    const Geometry& geometry () const;

    /** \brief Return the number of subEntities of codimension cc.
     */
    template<int cc>
    int count () const;

    /** \brief Return level index of sub entity with codim = cc and local number i
     */
    template<int cc>
    int subIndex (int i) const {
      assert(i>=0 && i<count<cc>());

      if (cc==dim)
        return UG_NS<dim>::levelIndex(UG_NS<dim>::Corner(target_,UGGridRenumberer<dim>::verticesDUNEtoUG(i,geometry().type())));
      if (cc==0)
        return UG_NS<dim>::levelIndex(target_);
      if (cc==dim-1)
      {
        int a=ReferenceElements<double,dim>::general(geometry().type()).subEntity(i,dim-1,0,dim);
        int b=ReferenceElements<double,dim>::general(geometry().type()).subEntity(i,dim-1,1,dim);
        return UG_NS<dim>::levelIndex(UG_NS<dim>::GetEdge(UG_NS<dim>::Corner(target_,
                                                                             UGGridRenumberer<dim>::verticesDUNEtoUG(a,geometry().type())),
                                                          UG_NS<dim>::Corner(target_,
                                                                             UGGridRenumberer<dim>::verticesDUNEtoUG(b,geometry().type()))));
      }
      if (cc==1)
        return UG_NS<dim>::levelIndex(UG_NS<dim>::SideVector(target_,UGGridRenumberer<dim>::facesDUNEtoUG(i,geometry().type())));

      DUNE_THROW(GridError, "UGGrid<" << dim << "," << dim << ">::subIndex isn't implemented for cc==" << cc );
    }

    /** \brief Return leaf index of sub entity with codim = cc and local number i
     */
    template<int cc>
    int subLeafIndex (int i) const {
      assert(i>=0 && i<count<cc>());

      if (cc==dim)
        return UG_NS<dim>::leafIndex(UG_NS<dim>::Corner(target_,UGGridRenumberer<dim>::verticesDUNEtoUG(i,geometry().type())));
      if (cc==0)
        return UG_NS<dim>::leafIndex(target_);
      if (cc==dim-1)
      {
        int a=ReferenceElements<double,dim>::general(geometry().type()).subEntity(i,dim-1,0,dim);
        int b=ReferenceElements<double,dim>::general(geometry().type()).subEntity(i,dim-1,1,dim);
        return UG_NS<dim>::leafIndex(UG_NS<dim>::GetEdge(UG_NS<dim>::Corner(target_,
                                                                            UGGridRenumberer<dim>::verticesDUNEtoUG(a,geometry().type())),
                                                         UG_NS<dim>::Corner(target_,
                                                                            UGGridRenumberer<dim>::verticesDUNEtoUG(b,geometry().type()))));
      }
      if (cc==1)
        return UG_NS<dim>::leafIndex(UG_NS<dim>::SideVector(target_,UGGridRenumberer<dim>::facesDUNEtoUG(i,geometry().type())));

      DUNE_THROW(GridError, "UGGrid<" << dim << "," << dim << ">::subLeafIndex isn't implemented for cc==" << cc );
    }

    /** \brief Return global id of sub entity with codim = cc and local number i
     */
    template<int cc>
    unsigned int subGlobalId (int i) const {
      assert(i>=0 && i<count<cc>());

      if (cc==0)
      {
#ifdef ModelP
        return target_->ge.ddd.gid;
#else
        return UG_NS<dim>::id(target_);
#endif
      }
      if (cc==dim)
      {
#ifdef ModelP
        return UG_NS<dim>::Corner(target_,UGGridRenumberer<dim>::verticesDUNEtoUG(i,geometry().type()))->ddd.gid;
#else
        return UG_NS<dim>::id(UG_NS<dim>::Corner(target_,UGGridRenumberer<dim>::verticesDUNEtoUG(i,geometry().type())));
#endif
      }

      DUNE_THROW(GridError, "UGGrid<" << dim << "," << dim << ">::subGlobalId isn't implemented for cc==" << cc );
      return 0;
    }

    /** \brief Return local id of sub entity with codim = cc and local number i
        \todo Only called from the id set
     */
    template<int cc>
    unsigned int subLocalId (int i) const {
      assert(i>=0 && i<count<cc>());

      if (cc==dim)
        return UG_NS<dim>::id(UG_NS<dim>::Corner(target_,UGGridRenumberer<dim>::verticesDUNEtoUG(i,geometry().type())));
      else if (cc==0)
        return UG_NS<dim>::id(target_);
      else
        DUNE_THROW(GridError, "UGGrid<" << dim << "," << dim << ">::subLocalId isn't implemented for cc==" << cc );
    }


    /** \brief Provide access to sub entity i of given codimension. Entities
     *  are numbered 0 ... count<cc>()-1
     */
    template<int cc>
    typename GridImp::template Codim<cc>::EntityPointer entity (int i) const;

    UGGridLeafIntersectionIterator<GridImp> ileafbegin () const {
      return UGGridLeafIntersectionIterator<GridImp>(target_, 0, level());
    }

    UGGridLevelIntersectionIterator<GridImp> ilevelbegin () const {
      return UGGridLevelIntersectionIterator<GridImp>(target_, 0, level());
    }

    //! Reference to one past the last neighbor
    UGGridLeafIntersectionIterator<GridImp> ileafend () const {
      return UGGridLeafIntersectionIterator<GridImp>(target_, -1, -1);
    }

    UGGridLevelIntersectionIterator<GridImp> ilevelend () const {
      return UGGridLevelIntersectionIterator<GridImp>(target_, -1, -1);
    }

    //! returns true if Entity has NO children
    bool isLeaf() const {
      return UG_NS<dim>::isLeaf(target_);
    }

    //! returns true if element is of regular type
    bool isRegular() const {
      return UG_NS<dim>::isRegular(target_);
    }

    //! Inter-level access to father element on coarser grid.
    //! Assumes that meshes are nested.
    typename GridImp::template Codim<0>::EntityPointer father () const {
      return typename GridImp::template Codim<0>::EntityPointer (UG_NS<dim>::EFather(target_));
    }

    /*! Location of this element relative to the reference element element of the father.
       This is sufficient to interpolate all dofs in conforming case.
       Nonconforming may require access to neighbors of father and
       computations with local coordinates.
       On the fly case is somewhat inefficient since dofs  are visited several times.
       If we store interpolation matrices, this is tolerable. We assume that on-the-fly
       implementation of numerical algorithms is only done for simple discretizations.
       Assumes that meshes are nested.
     */
    const Geometry& geometryInFather () const;

    /*! Inter-level access to son elements on higher levels<=maxlevel.
       This is provided for sparsely stored nested unstructured meshes.
       Returns iterator to first son.
     */
    UGGridHierarchicIterator<GridImp> hbegin (int maxlevel) const;

    //! Returns iterator to one past the last son
    UGGridHierarchicIterator<GridImp> hend (int maxlevel) const;

    //***************************************************************
    //  Interface for Adaptation
    //***************************************************************

    //! returns true, if entity was refined during last adaptation cycle
    bool wasRefined() const;

    /*! \brief
       returns true, if entity might be coarsened during next adaptation
       cycle, which is true for entities that have been marked for
       coarsening or for entities that are not regular (i.e. isRegular
       returns false) */
    bool mightBeCoarsened() const;

    //!
    void setToTarget(typename UG_NS<dim>::Element* target);

    //! the current geometry
    UGMakeableGeometry<dim,GridImp::dimensionworld,GridImp> geo_;

    //! geometry for mapping into father's reference element
    mutable UGMakeableGeometry<dim,GridImp::dimensionworld,GridImp> geometryInFather_;

    typename UG_NS<dim>::Element* target_;


  }; // end of UGGridEntity codim = 0

} // namespace Dune

#endif
