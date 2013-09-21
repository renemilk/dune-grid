// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:

#ifndef DUNE_GRID_TEST_CHECKRANDOMACCESSITERATORS_HH
#define DUNE_GRID_TEST_CHECKRANDOMACCESSITERATORS_HH

#include <iostream>
#include <iterator>
#include <ostream>

#include <dune/common/forloop.hh>
#include <dune/common/typetraits.hh>

#include <dune/grid/common/capabilities.hh>

// CheckCodimIterators
// -------------------

template< class GridView, int codim, class = void >
struct RACheckCodimIterators
{
  static void apply ( const GridView &gv, bool &haveRandomAccess,
                      bool &result )
  { }
};

template< class GridView, int codim >
struct RACheckCodimIterators<
  GridView, codim,
  typename Dune::enable_if<
    Dune::Capabilities::hasEntity<
      typename GridView::Grid, codim
      >::v &&
    Dune::IsBaseOf<
      std::random_access_iterator_tag,
      typename std::iterator_traits<
        typename GridView::template Codim<codim>::Iterator
        >::iterator_category
      >::value
    >::type>
{
  static void apply ( const GridView &gv, bool &haveRandomAccess,
                      bool &result )
  {
    haveRandomAccess = true;

    std::cout << "Checking random-access iterators for codim " << codim
              << std::endl;

    typedef typename GridView::template Codim<codim>::Iterator Iterator;
    typedef typename std::iterator_traits<Iterator>::difference_type
      Difference;

    Difference size = gv.size(codim);
    if(gv.template end<codim>() - gv.template begin<codim>() != size)
    {
      std::cerr << "Error: Codim " << codim << ": end() - begin() != size()."
                << std::endl;
      result = false;
    };

    if(gv.template begin<codim>() + size != gv.template end<codim>())
    {
      std::cerr << "Error: Codim " << codim << ": begin() + size() != end()."
                << std::endl;
      result = false;
    };

    if(gv.template end<codim>() - size != gv.template begin<codim>())
    {
      std::cerr << "Error: Codim " << codim << ": end() - size() != begin()."
                << std::endl;
      result = false;
    };

    {
      Iterator it = gv.template begin<codim>();
      it += size;
      if(it != gv.template end<codim>())
      {
        std::cerr << "Error: Codim " << codim << ": it = begin(); it += "
                  << "size() != end()." << std::endl;
        result = false;
      };
    }

    {
      Iterator it = gv.template end<codim>();
      it -= size;
      if(it != gv.template begin<codim>())
      {
        std::cerr << "Error: Codim " << codim << ": it = end(); it -= size() "
                  << "!= begin()." << std::endl;
        result = false;
      };
    }
  }
};


// CheckIterators
// --------------

template< class GridView >
class RACheckIterators
{
  template< int codim >
  struct CheckCodim;

public:
  static void apply ( const GridView &gv, bool &haveRandomAccess,
                      bool &result )
  {
    std::cout << "Checking iterators for higher codimension..." << std::endl;
    Dune::ForLoop< CheckCodim, 0, GridView::dimension >::
      apply( gv, haveRandomAccess, result );
  }
};


// CheckIterators::CheckCodim
// --------------------------

template< class GridView >
template< int codim >
struct RACheckIterators< GridView >::CheckCodim
{
  static void apply ( const GridView &gv, bool &haveRandomAccess,
                      bool &result )
  {
    RACheckCodimIterators< GridView, codim >::
      apply( gv, haveRandomAccess, result );
  }
};


template< class GridView >
inline bool checkRandomAccessIterators ( const GridView &gv )
{
  bool haveRandomAccess = false;
  bool result = true;
  RACheckIterators< GridView >::apply( gv, haveRandomAccess, result );

  if(!haveRandomAccess)
  {
    std::cerr << "Error: None of the iterators for any codim declare "
              << "themselves as random access" << std::endl;
    result = false;
  }

  return result;
}

#endif // DUNE_GRID_TEST_CHECKRANDOMACCESSITERATORS_HH
