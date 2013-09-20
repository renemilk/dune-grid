// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:
#ifndef DUNE_GRID_ENTITYITERATOR_HH
#define DUNE_GRID_ENTITYITERATOR_HH

#include <cstddef>
#include <iterator>

#include <dune/common/dotproduct.hh> // for AlwaysVoid

#include <dune/grid/common/entitypointer.hh>

namespace Dune
{

  template< class IteratorImp, class = void >
  struct EntityIteratorCategory
  {
    typedef std::forward_iterator_tag type;
  };

  template< class IteratorImp >
  struct EntityIteratorCategory<
    IteratorImp,
    typename AlwaysVoid<typename IteratorImp::iterator_category>::type>
  {
    typedef typename IteratorImp::iterator_category type;
  };

  // forward iterators
  template< int codim, class Grid, class IteratorImp, class MostDerived,
            class category >
  class EntityIteratorBase
    : public EntityPointer< Grid, IteratorImp >
  {
    typedef EntityPointer< Grid, IteratorImp > Base;

  protected:
    using Base::realIterator;

  public:
    typedef typename Grid::template Codim< codim >::Entity Entity;

    typedef std::ptrdiff_t difference_type;
    typedef const Entity value_type;
    typedef value_type *pointer;
    typedef value_type &reference;
    typedef std::forward_iterator_tag iterator_category;

    /** \brief prefix increment operator */
    MostDerived &operator++ ()
    {
      realIterator.increment();
      return static_cast<MostDerived&>(*this);
    }

    /** \name Implementor's interface
     *  \{
     */

    /** \brief copy constructor from implementaton */
    EntityIteratorBase ( const IteratorImp &imp )
      : Base( imp )
    {}

    /** \} */
  };

  template< int codim, class Grid, class IteratorImp, class MostDerived>
  class EntityIteratorBase< codim, Grid, IteratorImp, MostDerived,
                            std::random_access_iterator_tag >
    : public EntityIteratorBase< codim, Grid, IteratorImp, MostDerived,
                                 std::forward_iterator_tag >
  {
    typedef EntityIteratorBase< codim, Grid, IteratorImp, MostDerived,
                                std::forward_iterator_tag > Base;

  protected:
    using Base::realIterator;

  public:

    typedef typename Base::difference_type difference_type;
    typedef std::random_access_iterator_tag iterator_category;

    /** \brief prefix decrement operator */
    MostDerived &operator-- ()
    {
      realIterator.increment(-1);
      return static_cast<MostDerived&>(*this);
    }

    /** \brief operator+= */
    MostDerived &operator+= (difference_type n)
    {
      realIterator.increment(n);
      return static_cast<MostDerived&>(*this);
    }

    /** \brief operator-= */
    MostDerived &operator-= (difference_type n)
    {
      realIterator.increment(-n);
      return static_cast<MostDerived&>(*this);
    }

    /** \brief operator+ */
    MostDerived operator+ (difference_type n) const
    {
      MostDerived tmp(static_cast<const MostDerived&>(*this));
      tmp += n;
      return tmp;
    }

    /** \brief operator- */
    MostDerived operator- (difference_type n) const
    {
      MostDerived tmp(static_cast<const MostDerived&>(*this));
      tmp -= n;
      return tmp;
    }

    /** \brief operator- */
    difference_type operator- (const EntityIteratorBase &other) const
    {
      return realIterator.difference(other.realIterator);
    }

    // Can't really implement this: would need Entities as first class objects
    // to do a reasonable approximation, or a proxy that implements all entity
    // methods and converts to Entity&.
    //
    // /** \brief operator[] */
    // const Entity &operator[] (difference_type n) const
    // {
    //   return realIterator.difference(other.realIterator);
    // }

    /** \brief operator< */
    bool operator< (const EntityIteratorBase &other) const
    {
      return realIterator.difference(other.realIterator) < 0;
    }

    /** \brief operator> */
    bool operator> (const EntityIteratorBase &other) const
    {
      return realIterator.difference(other.realIterator) > 0;
    }

    /** \brief operator< */
    bool operator<= (const EntityIteratorBase &other) const
    {
      return realIterator.difference(other.realIterator) <= 0;
    }

    /** \brief operator> */
    bool operator>= (const EntityIteratorBase &other) const
    {
      return realIterator.difference(other.realIterator) >= 0;
    }

    /** \name Implementor's interface
     *  \{
     */

    /** \brief copy constructor from implementaton */
    EntityIteratorBase ( const IteratorImp &imp )
      : Base( imp )
    {}

    /** \} */
  };

  /** \class EntityIterator
   *  \brief interface class for an iterator over grid entities
   *  \ingroup GIEntityPointer
   *
   *  An entity iterator is an iterator over a subset of entities within a
   *  hierarchical grid. It is an extension of the Dune::EntityPointer
   *  interface.
   *
   *  Examples of entity iterators are:
   *  - iterators over the leaf level (LeafGridView::Iterator)
   *  - iterators over a grid level (LevelGridView::Iterator)
   *  - iterators over the children of an entity (Grid::HierarchicIterator)
   *  .
   *
   *  See also the documentation of Dune::EntityPointer.
   *
   *  \tparam  codim        codimension of entities this iterator walks over
   *  \tparam  Grid         type of the grid implementation
   *  \tparam  IteratorImp  type of the iterator implementation
   */
  template< int codim, class Grid, class IteratorImp>
  class EntityIterator
    : public EntityIteratorBase< codim, Grid, IteratorImp,
                                 EntityIterator<codim, Grid, IteratorImp>,
                                 typename EntityIteratorCategory<IteratorImp>::type>
  {
    typedef EntityIteratorBase< codim, Grid, IteratorImp, EntityIterator,
                                typename EntityIteratorCategory<IteratorImp>::type> Base;

  public:

    /** \brief copy constructor from implementaton */
    EntityIterator ( const IteratorImp &imp )
      : Base( imp )
    {}

    /** \} */
  };

} // namespace Dune

#endif // #ifndef DUNE_GRID_ENTITYITERATOR_HH
