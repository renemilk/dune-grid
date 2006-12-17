// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:
#include "config.h"

#include <set>

#include <dune/grid/uggrid.hh>
#include "boundaryextractor.hh"

/** \todo Remove the following include once getAllSubfaces... is gone */
#include <dune/common/sllist.hh>
#include <dune/common/stdstreams.hh>


using namespace Dune;

class LinearSegment2d : public BoundarySegment<2>
{
public:
  LinearSegment2d(const FieldVector<double,2>& a, const FieldVector<double,2>& b)
    : a_(a), b_(b)
  {}

  virtual FieldVector<double, 2> operator()(const FieldVector<double,1>& local) const {
    /** \todo Rewrite this with expression templates */
    FieldVector<double, 2> result;
    result[0] = a_[0] + local[0]*(b_[0]-a_[0]);
    result[1] = a_[1] + local[0]*(b_[1]-a_[1]);
    return result;
  }

  FieldVector<double, 2> a_, b_;
};

/** \brief Class implementing a linear quadrilateral boundary segment */
class LinearQuadSegment3d : public BoundarySegment<3>
{
public:
  LinearQuadSegment3d(const FieldVector<double,3>& a,
                      const FieldVector<double,3>& b,
                      const FieldVector<double,3>& c,
                      const FieldVector<double,3>& d)
    : a_(a), b_(b), c_(c), d_(d)
  {}

  virtual FieldVector<double, 3> operator()(const FieldVector<double,2>& local) const {
    /** \todo Rewrite this with expression templates */
    FieldVector<double, 3> result;
    for (int i=0; i<3; i++)
      result[i] = a_[i] + local[0]*(b_[i]-a_[i]) + local[1]*(d_[i]-a_[i])
                  + local[0]*local[1]*(a_[i]+c_[i]-b_[i]-d_[i]);
    return result;
  }

  FieldVector<double, 3> a_, b_, c_, d_;
};


/** \brief Class implementing a linear triangular boundary segment */
class LinearTriSegment3d : public BoundarySegment<3>
{
public:
  LinearTriSegment3d(const FieldVector<double,3>& a,
                     const FieldVector<double,3>& b,
                     const FieldVector<double,3>& c)
    : a_(a), b_(b), c_(c)
  {}

  virtual FieldVector<double, 3> operator()(const FieldVector<double,2>& local) const {
    /** \todo Rewrite this with expression templates */
    FieldVector<double, 3> result;
    for (int i=0; i<3; i++)
      result[i] = a_[i] + local[0]*(b_[i]-a_[i]) + local[1]*(c_[i]-b_[i]);
    return result;
  }

  FieldVector<double, 3> a_, b_, c_;
};

static int boundarySegmentWrapper2d(void *data, double *param, double *result)
{
  const BoundarySegment<2>* boundarySegment = static_cast<const BoundarySegment<2>*>(data);

  FieldVector<double, 2> global = (*boundarySegment)(*((FieldVector<double,1>*)param));

  result[0] = global[0];
  result[1] = global[1];

  return 0;
}

static int boundarySegmentWrapper3d(void *data, double *param, double *result)
{
  const BoundarySegment<3>* boundarySegment = static_cast<const BoundarySegment<3>*>(data);

  FieldVector<double, 3> global = (*boundarySegment)(*((FieldVector<double,2>*)param));

  result[0] = global[0];
  result[1] = global[1];
  result[2] = global[2];

  return 0;
}

//***********************************************************************
//
// --UGGrid
// --Grid
//
//***********************************************************************

template<> int Dune::UGGrid<2>::numOfUGGrids = 0;
template<> int Dune::UGGrid<3>::numOfUGGrids = 0;

template<> bool Dune::UGGrid<2>::useExistingDefaultsFile = false;
template<> bool Dune::UGGrid<3>::useExistingDefaultsFile = false;


template < int dim>
inline Dune::UGGrid < dim >::UGGrid() : multigrid_(NULL),
                                        leafIndexSet_(*this),
                                        globalIdSet_(*this),
                                        localIdSet_(*this),
                                        refinementType_(LOCAL),
                                        omitGreenClosure_(false),
                                        someElementHasBeenMarkedForRefinement_(false)
{
  init(500, 10);
}

template <int dim>
inline Dune::UGGrid < dim >::UGGrid(unsigned int heapSize, unsigned envHeapSize)
  : multigrid_(NULL),
    leafIndexSet_(*this),
    globalIdSet_(*this),
    localIdSet_(*this),
    refinementType_(LOCAL),
    omitGreenClosure_(false),
    someElementHasBeenMarkedForRefinement_(false)
{
  init(heapSize, envHeapSize);
}

template < int dim >
inline void Dune::UGGrid < dim >::init(unsigned int heapSize, unsigned envHeapSize)
{
  heapsize = heapSize;

  if (numOfUGGrids==0) {

    useExistingDefaultsFile = false;

    if (access("defaults", F_OK) == 0) {

      dverb << "Using existing UG defaults file" << std::endl;
      useExistingDefaultsFile = true;

    } else {

      // Pass the explicitly given environment heap size
      // This is only possible by passing a pseudo 'defaults'-file
      FILE* fp = fopen("defaults", "w");
      fprintf(fp, "envmemory %d000000\n", envHeapSize);
      fprintf(fp, "mutelevel -1001\n");
      fclose(fp);

    }

    // Init the UG system
    int argc = 1;
    char* arg = {"dune.exe"};
    char** argv = &arg;


    if (UG_NS<dim>::InitUg(&argc, &argv))
      DUNE_THROW(GridError, "UG" << dim << "d::InitUg() returned an error code!");

  }

  // Create a dummy problem
  typename UG_NS<dim>::CoeffProcPtr coeffs[1] = {NULL};
  typename UG_NS<dim>::UserProcPtr upp[1] = {NULL};

  // Create unique problem name
  std::stringstream numberAsAscii;
  numberAsAscii << numOfUGGrids;
  name_ = "DuneUGGrid_" + std::string((dim==2) ? "2" : "3") + std::string("d_") + numberAsAscii.str();

  std::string problemName = name_ + "_Problem";

#ifndef UG_LGMDOMAIN
  if (UG_NS<dim>::CreateBoundaryValueProblem(problemName.c_str(), 1,coeffs,1,upp) == NULL)
    DUNE_THROW(GridError, "UG" << dim << "d::CreateBoundaryValueProblem() returned and error code!");
#endif

  if (numOfUGGrids==0) {

    if (dim==2)
    {
      char* nfarg = "newformat DuneFormat2d";
      if (UG_NS<dim>::CreateFormatCmd(1, &nfarg))
        DUNE_THROW(GridError, "UG" << dim << "d::CreateFormat() returned and error code!");
    }
    if (dim==3)
    {
      char* newArgs[2];
      for (int i=0; i<2; i++)
        newArgs[i] = (char*)::malloc(50*sizeof(char));

      sprintf(newArgs[0], "newformat DuneFormat3d" );
      sprintf(newArgs[1], "V s1 : vt 1" );             // generates side vectors in 3D

      if (UG_NS<dim>::CreateFormatCmd(2, newArgs))
        DUNE_THROW(GridError, "UG" << dim << "d::CreateFormat() returned and error code!");

      for (int i=0; i<2; i++)
        free(newArgs[i]);

    }
  }

  numOfUGGrids++;

  dverb << "UGGrid<" << dim << "> with name " << name_ << " created!" << std::endl;

}

template < int dim >
inline Dune::UGGrid < dim >::~UGGrid()
{
  for (unsigned int i=0; i<boundarySegments_.size(); i++)
    delete boundarySegments_[i];

  if (multigrid_) {
    // Set UG's currBVP variable to the BVP corresponding to this
    // grid.  This is necessary if we have more than one UGGrid in use.
    // DisposeMultiGrid will crash if we don't do this
    std::string BVPName = name_ + "_Problem";
    void* thisBVP = UG_NS<dim>::BVP_GetByName(BVPName.c_str());

    if (thisBVP == NULL)
      DUNE_THROW(GridError, "Couldn't find grid's own boundary value problem!");

    UG_NS<dim>::Set_Current_BVP((void**)thisBVP);
    UG_NS<dim>::DisposeMultiGrid(multigrid_);
  }

  numOfUGGrids--;

  // Shut down UG if this was the last existing UGGrid object
  if (UGGrid<2>::numOfUGGrids + UGGrid<3>::numOfUGGrids == 0) {

    UG_NS<dim>::ExitUg();

    // remove defaults file, if we wrote one on startup
    if (!useExistingDefaultsFile)
      system("rm defaults");

  }

  // Delete levelIndexSets
  for (unsigned int i=0; i<levelIndexSets_.size(); i++)
    if (levelIndexSets_[i])
      delete levelIndexSets_[i];
}

template < int dim >
inline int Dune::UGGrid < dim >::maxLevel() const
{
  if (!multigrid_)
    DUNE_THROW(GridError, "The grid has not been properly initialized!");

  return multigrid_->topLevel;
}



template<int dim>
template<int codim>
typename Dune::UGGrid<dim>::Traits::template Codim<codim>::LevelIterator
Dune::UGGrid<dim>::lbegin (int level) const
{
  if (!multigrid_)
    DUNE_THROW(GridError, "The grid has not been properly initialized!");

  const typename UG_NS<dim>::Grid* theGrid = multigrid_->grids[level];

  if (!theGrid)
    DUNE_THROW(GridError, "LevelIterator in nonexisting level " << level << " requested!");

  if (codim==0)
    // The seemingly pointless cast make the code compile in the cases where this if-clause
    // does _not_ get executed.
    return UGGridLevelIterator<codim, All_Partition, const UGGrid<dim> >((typename UG_NS<dim>::template Entity<codim>::T*)UG_NS<dim>::PFirstElement(theGrid));
  else if (codim==dim)
    // The seemingly pointless cast make the code compile in the cases where this if-clause
    // does _not_ get executed.
    return UGGridLevelIterator<codim, All_Partition, const UGGrid<dim> >((typename UG_NS<dim>::template Entity<codim>::T*)UG_NS<dim>::PFirstNode(theGrid));

  DUNE_THROW(GridError, "UGGrid doesn't support level iterators of codim " << codim);
}

template<int dim>
template<int codim, Dune::PartitionIteratorType PiType>
inline typename Dune::UGGrid<dim>::Traits::template Codim<codim>::template Partition<PiType>::LevelIterator
Dune::UGGrid<dim>::lbegin (int level) const
{
  if (!multigrid_)
    DUNE_THROW(GridError, "The grid has not been properly initialized!");

  typename UG_NS<dim>::Grid* theGrid = multigrid_->grids[level];

  if (!theGrid)
    DUNE_THROW(GridError, "LevelIterator in nonexisting level " << level << " requested!");

  if (codim==0) {

    typename UG_NS<dim>::Element* firstElement =
      (PiType==All_Partition || PiType==Ghost_Partition)
      ? UG_NS<dim>::PFirstElement(theGrid)
      : UG_NS<dim>::FirstElement(theGrid);

    // The seemingly pointless cast make the code compile in the cases where this if-clause
    // does _not_ get executed.
    return UGGridLevelIterator<codim, PiType, const UGGrid<dim> >((typename UG_NS<dim>::template Entity<codim>::T*)firstElement);

  } else if (codim==dim) {

    // The seemingly pointless cast make the code compile in the cases where this if-clause
    // does _not_ get executed.
    /** \todo Parallel vertex level iterators not properly implemented yet! */
    return UGGridLevelIterator<codim, PiType, const UGGrid<dim> >((typename UG_NS<dim>::template Entity<codim>::T*)UG_NS<dim>::PFirstNode(theGrid));

  }

  DUNE_THROW(GridError, "UGGrid doesn't support level iterators of codim " << codim);
}

template < int dim >
template<int codim>
typename Dune::UGGrid<dim>::Traits::template Codim<codim>::LevelIterator
Dune::UGGrid < dim >::lend (int level) const
{
  return UGGridLevelIterator<codim,All_Partition, const UGGrid<dim> >();
}

template < int dim >
template<int codim, Dune::PartitionIteratorType PiType>
inline typename Dune::UGGrid<dim>::Traits::template Codim<codim>::template Partition<PiType>::LevelIterator
Dune::UGGrid < dim >::lend (int level) const
{
  return UGGridLevelIterator<codim,PiType, const UGGrid<dim> >();
}

template < int dim >
inline int Dune::UGGrid < dim >::size (int level, int codim) const
{
#ifndef ModelP
  if(codim == 0)
  {
    return levelIndexSet(level).size(GeometryType(GeometryType::simplex,dim))
           + levelIndexSet(level).size(GeometryType(GeometryType::cube,dim))
           + levelIndexSet(level).size(GeometryType(GeometryType::pyramid,dim))
           + levelIndexSet(level).size(GeometryType(GeometryType::prism,dim));
  }
  if(codim == dim)
  {
    return this->levelIndexSet(level).size(GeometryType(0));
  }
  if (codim == dim-1)
  {
    return this->levelIndexSet(level).size(GeometryType(1));
  }
  if (codim == 1)
  {
    return levelIndexSet(level).size(GeometryType(GeometryType::cube,dim-1))
           + levelIndexSet(level).size(GeometryType(GeometryType::simplex,dim-1));
  }
  DUNE_THROW(GridError, "UGGrid<" << dim << ">::size(int level, int codim) is only implemented"
                                  << " for codim==0 and codim==dim!");
#else

  int numberOfElements = 0;

  if(codim == 0)
  {
    typename Traits::template Codim<0>::LevelIterator it = lbegin<0>(level);
    typename Traits::template Codim<0>::LevelIterator endit = lend<0>(level);
    for (; it != endit; ++it)
      numberOfElements++;

  } else
  if(codim == dim)
  {
    typename Traits::template Codim<dim>::LevelIterator it    = lbegin<dim>(level);
    typename Traits::template Codim<dim>::LevelIterator endit = lend<dim>(level);
    for(; it != endit; ++it)
      numberOfElements++;
  }
  else
  {
    DUNE_THROW(GridError, "UGGrid<" << dim << ", " << dimworld
                                    << ">::size(int level, int codim) is only implemented"
                                    << " for codim==0 and codim==dim!");
  }

  return numberOfElements;
#endif
}


template < int dim >
bool Dune::UGGrid < dim >::mark(int refCount,
                                const typename Traits::template Codim<0>::EntityPointer & e )
{
  typename UG_NS<dim>::Element* target = getRealImplementation(*e).target_;

  // No refinement requested
  if (refCount==0) {
    if (UG_NS<dim>::MarkForRefinement(target,
                                      UG_NS<dim>::NO_REFINEMENT,      // unset
                                      0)      // Irrelevant if refinement rule is not BLUE
        ) DUNE_THROW(GridError, "UG" << dim << "d::MarkForRefinement returned error code!");
    return true;
  }

  // Check whether element can be marked for refinement
  if (!EstimateHere(target))
    return false;

  if (refCount==1) {
    if (UG_NS<dim>::MarkForRefinement(target,
                                      UG_NS<dim>::RED,      // red refinement rule
                                      0)      // Irrelevant if refinement rule is not BLUE
        ) DUNE_THROW(GridError, "UG" << dim << "d::MarkForRefinement returned error code!");

    someElementHasBeenMarkedForRefinement_ = true;
    return true;
  } else if (refCount==-1) {

    if (UG_NS<dim>::MarkForRefinement(target,
                                      UG_NS<dim>::COARSE,      // coarsen the element
                                      0)      // Irrelevant if refinement rule is not BLUE
        ) DUNE_THROW(GridError, "UG" << dim << "d::MarkForRefinement returned error code!");

    someElementHasBeenMarkedForRefinement_ = true;
    return true;
  } else
    DUNE_THROW(GridError, "UGGrid only supports refCount values -1, 0, and 1 for mark()!");

}

template < int dim >
bool Dune::UGGrid < dim >::mark(const typename Traits::template Codim<0>::EntityPointer & e,
                                typename UG_NS<dim>::RefinementRule rule,
                                int side)
{
  typename UG_NS<dim>::Element* target = getRealImplementation(*e).target_;

  if (!UG_NS<dim>::isLeaf(target))
    return false;

  someElementHasBeenMarkedForRefinement_ = true;

  return UG_NS<dim>::MarkForRefinement(target, rule, side);

}

template <int dim>
int Dune::UGGrid<dim>::getMark(const typename Traits::template Codim<0>::Entity& e) const
{
  typename UG_NS<dim>::Element* target = getRealImplementation(e).target_;

  // Return -1 if element is marked for coarsening
  if (UG_NS<dim>::ReadCW(target,UG_NS<dim>::COARSEN_CE))
    return -1;

#if 1  // in rm.c
  if (dim==2)
    target = (typename UG_NS<dim>::Element*) UG::D2::ELEMENT_TO_MARK((UG::D2::element*)target);
  else
    target = (typename UG_NS<dim>::Element*) UG::D3::ELEMENT_TO_MARK((UG::D3::element*)target);
#endif

  // Return 0 if element is not marked at all
  if (UG_NS<dim>::ReadCW(target,UG_NS<dim>::MARK_CE)==UG_NS<dim>::NO_REFINEMENT)
    return 0;

  // Else return 1
  return 1;
}

template <int dim>
int Dune::UGGrid<dim>::getMark(const typename Traits::template Codim<0>::EntityPointer & ep) const
{
  typename UG_NS<dim>::Element* target = getRealImplementation(*ep).target_;

  // Return -1 if element is marked for coarsening
  if (UG_NS<dim>::ReadCW(target,UG_NS<dim>::COARSEN_CE))
    return -1;

#if 1  // in rm.c
  if (dim==2)
    target = (typename UG_NS<dim>::Element*) UG::D2::ELEMENT_TO_MARK((UG::D2::element*)target);
  else
    target = (typename UG_NS<dim>::Element*) UG::D3::ELEMENT_TO_MARK((UG::D3::element*)target);
#endif

  // Return 0 if element is not marked at all
  if (UG_NS<dim>::ReadCW(target,UG_NS<dim>::MARK_CE)==UG_NS<dim>::NO_REFINEMENT)
    return 0;

  // Else return 1
  return 1;
}

template <int dim>
bool Dune::UGGrid <dim>::preAdapt()
{
  return someElementHasBeenMarkedForRefinement_;
}

template < int dim >
bool Dune::UGGrid < dim >::adapt()
{

  int rv;
  int mode;

  assert(multigrid_);

  // Set UG's currBVP variable to the BVP corresponding to this
  // grid.  This is necessary if we have more than one UGGrid in use.
  std::string BVPName = name_ + "_Problem";
  void* thisBVP = UG_NS<dim>::BVP_GetByName(BVPName.c_str());

  if (thisBVP == NULL)
    DUNE_THROW(GridError, "Couldn't find grid's own boundary value problem!");

  UG_NS<dim>::Set_Current_BVP((void**)thisBVP);

  mode = UG_NS<dim>::GM_REFINE_TRULY_LOCAL;

  if (refinementType_==COPY)
    mode = mode | UG_NS<dim>::GM_COPY_ALL;

  if (omitGreenClosure_)
    mode = mode | UG_NS<dim>::GM_REFINE_NOT_CLOSED;

  // I don't really know what this means
  int seq = UG_NS<dim>::GM_REFINE_PARALLEL;

  // I don't really know what this means either
  int mgtest = UG_NS<dim>::GM_REFINE_NOHEAPTEST;

  rv = AdaptMultiGrid(multigrid_,mode,seq,mgtest);

  if (rv!=0)
    DUNE_THROW(GridError, "UG::adapt() returned with error code " << rv);

  // Renumber everything
  setIndices();

  // The Dune grid interface specifies that adapt() should return true iff the grid has
  // been changed.  It is difficult to get that information from UG.  Therefore we
  // return true iff some element had been marked for some form of refinement before.
  // If this errs, it errs conservatively.
  // What do you need that return value for anyways?
  bool returnValue = someElementHasBeenMarkedForRefinement_;
  someElementHasBeenMarkedForRefinement_ = false;

  return returnValue;
}

template <int dim>
void Dune::UGGrid <dim>::postAdapt()
{
  for (int i=0; i<=maxLevel(); i++) {

    typename Traits::template Codim<0>::LevelIterator eIt    = lbegin<0>(i);
    typename Traits::template Codim<0>::LevelIterator eEndIt = lend<0>(i);

    for (; eIt!=eEndIt; ++eIt)
      UG_NS<dim>::WriteCW(getRealImplementation(*eIt).target_, UG_NS<dim>::NEWEL_CE, 0);

  }
}


template <int dim>
void Dune::UGGrid <dim>::adaptWithoutClosure()
{
  bool omitClosureBackup = omitGreenClosure_;
  omitGreenClosure_ = true;
  adapt();
  omitGreenClosure_ = omitClosureBackup;
}

template < int dim >
void Dune::UGGrid < dim >::globalRefine(int n)
{
  for (int i=0; i<n; i++) {

    // mark all entities for grid refinement
    typename Traits::template Codim<0>::LeafIterator iIt    = leafbegin<0>();
    typename Traits::template Codim<0>::LeafIterator iEndIt = leafend<0>();

    for (; iIt!=iEndIt; ++iIt)
      mark(1, iIt);

    this->preAdapt();
    adapt();
    this->postAdapt();

  }

}

template <int dim>
void Dune::UGGrid<dim>::getChildrenOfSubface(typename Traits::template Codim<0>::EntityPointer & e,
                                             int elementSide,
                                             int maxl,
                                             std::vector<typename Traits::template Codim<0>::EntityPointer>& childElements,
                                             std::vector<unsigned char>& childElementSides) const
{

  typedef std::pair<typename UG_NS<dim>::Element*,int> ListEntryType;

  SLList<ListEntryType> list;

  // //////////////////////////////////////////////////////////////////////
  //   Change the input face number from Dune numbering to UG numbering
  // //////////////////////////////////////////////////////////////////////

  elementSide = UGGridRenumberer<dim>::facesDUNEtoUG(elementSide, e->geometry().type());

  // ///////////////
  //   init list
  // ///////////////
  if (!e->isLeaf()   // Get_Sons_of_ElementSide returns GM_FATAL when called for a leaf !?!
      && e->level() < maxl) {

    typename UG_NS<dim>::Element* theElement = getRealImplementation(*e).target_;

    int Sons_of_Side = 0;
    typename UG_NS<dim>::Element* SonList[UG_NS<dim>::MAX_SONS];
    int SonSides[UG_NS<dim>::MAX_SONS];

    int rv = Get_Sons_of_ElementSide(theElement,
                                     elementSide,
                                     &Sons_of_Side,
                                     SonList,          // the output elements
                                     SonSides,         // Output element side numbers
                                     true,            // Element sons are not precomputed
                                     true);            // ioflag: I have no idea what this is supposed to do
    if (rv!=0)
      DUNE_THROW(GridError, "Get_Sons_of_ElementSide returned with error value " << rv);

    for (int i=0; i<Sons_of_Side; i++)
      list.push_back(ListEntryType(SonList[i],SonSides[i]));

  }

  // //////////////////////////////////////////////////
  //   Traverse and collect all children of the side
  // //////////////////////////////////////////////////

  typename SLList<ListEntryType>::iterator f = list.begin();
  for (; f!=list.end(); ++f) {

    typename::UG_NS<dim>::Element* theElement = f->first;
    int side                                  = f->second;

    int Sons_of_Side = 0;
    typename::UG_NS<dim>::Element* SonList[UG_NS<dim>::MAX_SONS];
    int SonSides[UG_NS<dim>::MAX_SONS];

    if (UG_NS<dim>::myLevel(theElement) < maxl) {

      Get_Sons_of_ElementSide(theElement,
                              side,             // Input element side number
                              &Sons_of_Side,       // Number of topological sons of the element side
                              SonList,            // Output elements
                              SonSides,           // Output element side numbers
                              true,
                              true);

      for (int i=0; i<Sons_of_Side; i++)
        list.push_back(ListEntryType(SonList[i],SonSides[i]));

    }

  }

  // //////////////////////////////
  //   Extract result from stack
  // //////////////////////////////

  /** \todo Initialized with a dummy value because EntityPointer isn't default constructible */
  childElements.resize(list.size(), lbegin<0>(0));
  childElementSides.resize(list.size());

  int i=0;
  for (f = list.begin(); f!=list.end(); ++f, ++i) {

    // Set element
    this->getRealImplementation(childElements[i]).setToTarget(f->first);

    int side = f->second;

    // Dune numbers the faces of several elements differently than UG.
    // The following switch does the transformation
    childElementSides[i] = UGGridRenumberer<dim>::facesUGtoDUNE(side, childElements[i]->geometry().type());

  }

}

template < int dim >
void Dune::UGGrid < dim >::loadBalance(int strategy, int minlevel, int depth, int maxLevel, int minelement)
{
  /** \todo Test for valid arguments */
  std::string argStrings[4];
  std::stringstream numberAsAscii[4];

  numberAsAscii[0] << strategy;
  argStrings[0] = "lb " + numberAsAscii[0].str();

  numberAsAscii[1] << minlevel;
  argStrings[1] = "c " + numberAsAscii[1].str();

  numberAsAscii[2] << depth;
  argStrings[2] = "d " + numberAsAscii[2].str();

  numberAsAscii[3] << minelement;
  argStrings[3] = "e " + numberAsAscii[3].str();

  const char* argv[4] = {argStrings[0].c_str(),
                         argStrings[1].c_str(),
                         argStrings[2].c_str(),
                         argStrings[3].c_str()};

  int errCode = UG_NS<dim>::LBCommand(4, argv);

  if (errCode)
    DUNE_THROW(GridError, "UG" << dim << "d::LBCommand returned error code " << errCode);

  // Renumber everything
  setIndices();

}

#ifdef ModelP
namespace Dune {

  template <class T, template <class> class P, int GridDim>
  class UGDataCollector {
  public:

    static int gather(DDD_OBJ obj, void* data)
    {
      int codim=0;

      P<T>* p = (P<T>*)data;

      int index = 0;
      switch (codim) {
      case 0 :
        index = UG_NS<GridDim>::index((typename UG_NS<GridDim>::Element*)obj);
        break;
      case GridDim :
        index = UG_NS<GridDim>::index((typename UG_NS<GridDim>::Node*)obj);
        break;
      default :
        DUNE_THROW(GridError, "UGGrid::communicate only implemented for this codim");
      }

      p->gather(*dataArray, index);

      return 0;
    }

    static int scatter(DDD_OBJ obj, void* data)
    {
      int codim=0;

      P<T>* p = (P<T>*)data;

      int index = 0;
      switch (codim) {
      case 0 :
        index = UG_NS<GridDim>::index((typename UG_NS<GridDim>::Element*)obj);
        break;
      case GridDim :
        index = UG_NS<GridDim>::index((typename UG_NS<GridDim>::Node*)obj);
        break;
      default :
        DUNE_THROW(GridError, "UGGrid::communicate only implemented for codim 0 and dim");
      }

      p->scatter(*dataArray, index);

      return 0;
    }

    static T* dataArray;

  };

}   // end namespace Dune

template <class T, template<class> class P, int GridDim>
T* Dune::UGDataCollector<T,P,GridDim>::dataArray = 0;
#endif

/** \todo How do I incorporate the level argument? */
template < int dim >
template<class T, template<class> class P, int codim>
void Dune::UGGrid < dim >::communicate (T& t, InterfaceType iftype, CommunicationDirection dir, int level)
{
#ifdef ModelP

  // Currently only elementwise communication is supported
  if (codim != 0)
    DUNE_THROW(GridError, "Currently UG supports only element-wise communication!");

  UGDataCollector<T,P,dim>::dataArray = &t;

  // Translate the communication direction from Dune-Speak to UG-Speak
  DDD_IF_DIR UGIfDir = (dir==ForwardCommunication) ? IF_FORWARD : IF_BACKWARD;

  // Trigger communication
  DDD_IFOneway(UG::ElementVHIF,
               UGIfDir,
               sizeof(P<T>),
               &UGDataCollector<T,P,dim>::gather,
               &UGDataCollector<T,P,dim>::scatter);

#endif
}


template < int dim >
void Dune::UGGrid < dim >::createBegin()
{
  // //////////////////////////////////////////////////////////
  //   Clear all buffers used during coarse grid creation
  // //////////////////////////////////////////////////////////
  boundarySegments_.resize(0);
  boundarySegmentVertices_.resize(0);
  elementTypes_.resize(0);
  elementVertices_.resize(0);
  vertexPositions_.resize(0);
}


template < int dim >
void Dune::UGGrid < dim >::createEnd()
{
#ifdef UG_LGMDOMAIN
  DUNE_THROW(GridError, "You cannot call createLGMGrid() when your UGGrid has been configured for LGM!");
#else

  // ///////////////////////////////////////////
  //   Extract grid boundary segments
  // ///////////////////////////////////////////
  std::set<FieldVector<int,2*dim-2>, CompareBoundarySegments<dim> > boundarySegments;
  typedef typename std::set<FieldVector<int,2*dim-2>, CompareBoundarySegments<dim> >::iterator SetIterator;

  BoundaryExtractor::detectBoundarySegments(elementTypes_, elementVertices_, boundarySegments);
  if (boundarySegments.size() == 0)
    DUNE_THROW(GridError, "Couldn't extract grid boundary.");

  std::vector<int> isBoundaryNode;
  BoundaryExtractor::detectBoundaryNodes(boundarySegments, vertexPositions_.size(), isBoundaryNode);

  dverb << boundarySegments.size() << " boundary segments were found!" << std::endl;

  // Count number of nodes on the boundary
  int noOfBNodes = 0;
  for (unsigned int i=0; i<isBoundaryNode.size(); i++) {
    if (isBoundaryNode[i] != -1)
      noOfBNodes++;
  }

  // ///////////////////////////////////////////
  //   Create the domain data structure
  // ///////////////////////////////////////////
  unsigned int noOfBSegments = boundarySegments.size();
  std::string domainName = name_ + "_Domain";
  const double midPoint[2] = {0, 0};

  if (UG_NS<dim>::CreateDomain(domainName.c_str(),     // The domain name
                               midPoint,               // Midpoint of a circle enclosing the grid, only needed for the UG graphics
                               1,                      // Radius of the enclosing circle
                               noOfBSegments,
                               noOfBNodes,
                               false) == NULL)                 // The domain is not convex
    DUNE_THROW(GridError, "Calling UG" << dim << "d::CreateDomain failed!");

  // ///////////////////////////////////////////
  //   Insert the boundary segments
  // ///////////////////////////////////////////
  unsigned int i;
  for (i=0; i<boundarySegments_.size(); i++) {

    /** \todo Due to some UG weirdness, in 3d, CreateBoundarySegment always expects
        this array to have four entries, even if only a triangular segment is
        inserted.  If not, undefined values are will be introduced. */
    int vertices_c_style[dim*2-2];
    for (int j=0; j<dim*2-2; j++)
      vertices_c_style[j] = boundarySegmentVertices_[i][j];

    // Create dummy parameter ranges
    const double alpha[2] = {0, 0};
    const double beta[2]  = {1, 1};

    // Create some boundary segment name
    char segmentName[20];
    if(sprintf(segmentName, "BS %d", i) < 0)
      DUNE_THROW(GridError, "sprintf returned error code!");

    // Actually create the segment
    if (UG_NS<dim>::CreateBoundarySegment(segmentName,                     // internal name of the boundary segment
                                          1,                               //  id of left subdomain
                                          2,                               //  id of right subdomain
                                          i,         // Index of the segment
                                          1,                        // Resolution, only for the UG graphics
                                          vertices_c_style,
                                          alpha,
                                          beta,
                                          (dim==2)
                                          ? boundarySegmentWrapper2d
                                          : boundarySegmentWrapper3d,
                                          const_cast<BoundarySegment<dim>*>(boundarySegments_[i]))==NULL) {
      DUNE_THROW(GridError, "Calling UG" << dim << "d::CreateBoundarySegment failed!");
    }

    // /////////////////////////////////////////////////////////////////////
    //   Remove this segment from the set of computed boundary segments.
    // /////////////////////////////////////////////////////////////////////

    FieldVector<int, 2*dim-2> thisSegment;
    /** \todo Not nice: we need to copy because the array types are different */
    for (int j=0; j<2*dim-2; j++)
      thisSegment[j] = boundarySegmentVertices_[i][j];
    boundarySegments.erase(thisSegment);
  }


  // ///////////////////////////////////////////////////////////////////////
  //   The boundary segments remaining in the std::set boundarySegments
  //   have not been provided with an explicit parametrization.  They are
  //   inserted into the domain as straight boundary segments.
  // ///////////////////////////////////////////////////////////////////////
  SetIterator it = boundarySegments.begin();

  for (; it != boundarySegments.end(); ++it, ++i) {

    const FieldVector<int, 2*dim-2>& thisSegment = *it;

    int numVertices = (dim==2) ? 2 : ((thisSegment[3] == -1) ? 3 : 4);

    std::vector<FieldVector<double,dim> > coordinates(numVertices);
    std::vector<int> vertices(numVertices);

    for (int j=0; j<numVertices; j++) {
      coordinates[j] = vertexPositions_[thisSegment[j]];
      vertices[j]    = isBoundaryNode[thisSegment[j]];
    }

    insertLinearSegment(vertices, coordinates, i);

  }


  // ///////////////////////////////////////////
  //   Call configureCommand and newCommand
  // ///////////////////////////////////////////

  //configure @PROBLEM $d @DOMAIN;
  std::string configureArgs[2] = {"configure " + name_ + "_Problem", "d " + name_ + "_Domain"};
  const char* configureArgs_c[2] = {configureArgs[0].c_str(), configureArgs[1].c_str()};

  if (UG_NS<dim>::ConfigureCommand(2, configureArgs_c))
    DUNE_THROW(GridError, "Calling UG" << dim << "d::ConfigureCommand failed!");

  //new @PROBLEM $b @PROBLEM $f @FORMAT $h @HEAP;
  char* newArgs[4];
  for (int i=0; i<4; i++)
    newArgs[i] = (char*)::malloc(50*sizeof(char));

  sprintf(newArgs[0], "new %s", name_.c_str());

  sprintf(newArgs[1], "b %s_Problem", name_.c_str());
  sprintf(newArgs[2], "f DuneFormat%dd", dim);
  sprintf(newArgs[3], "h %dM", heapsize);

  if (UG_NS<dim>::NewCommand(4, newArgs))
    DUNE_THROW(GridError, "UGGrid::makeNewMultigrid failed!");

  for (int i=0; i<4; i++)
    free(newArgs[i]);

  // Get a direct pointer to the newly created multigrid
  multigrid_ = UG_NS<dim>::GetMultigrid(name_.c_str());
  if (!multigrid_)
    DUNE_THROW(GridError, "UGGrid::makeNewMultigrid failed!");

  // ///////////////////////////////////////////////////////////////
  // If we are in a parallel setting and we are _not_ the master
  // process we can stop here.
  // ///////////////////////////////////////////////////////////////
  if (PPIF::me!=0)
    return;

  // ////////////////////////////////////////////////
  //   Actually insert the interior vertices
  // ////////////////////////////////////////////////
  int nodeCounter = noOfBNodes;
  for (size_t i=0; i<vertexPositions_.size(); i++) {
    if (isBoundaryNode[i] != -1)
      continue;
    if (UG_NS<dim>::InsertInnerNode(multigrid_->grids[0], &((vertexPositions_[i])[0])) == NULL)
      DUNE_THROW(GridError, "Inserting a vertex into UGGrid failed!");

    isBoundaryNode[i] = nodeCounter++;
  }

  vertexPositions_.resize(0);

  // ////////////////////////////////////////////////
  //   Actually insert all the elements
  // ////////////////////////////////////////////////

  int idx = 0;
  for (size_t i=0; i<elementTypes_.size(); i++) {

    int vertices_C_style[elementTypes_[i]];
    for (size_t j=0; j<elementTypes_[i]; j++)
      vertices_C_style[j] = isBoundaryNode[elementVertices_[idx++]];

    if (InsertElementFromIDs(multigrid_->grids[0], elementTypes_[i], vertices_C_style, NULL)==NULL)
      DUNE_THROW(GridError, "Inserting element into UGGrid failed!");

  }

  // Not needed any more
  elementTypes_.resize(0);
  elementVertices_.resize(0);

  // /////////////////////////////////////////
  // set the subdomainIDs
  // /////////////////////////////////////////
  typename UG_NS<dim>::Element* theElement;
  for (theElement=multigrid_->grids[0]->elements[0]; theElement!=NULL; theElement=theElement->ge.succ)
    UG_NS<dim>::SetSubdomain(theElement, 1);

  // Complete the subdomain structure
  // From namespace UG?d
  SetEdgeAndNodeSubdomainFromElements(multigrid_->grids[0]);

  // Complete the UG-internal grid data structure
  if (CreateAlgebra(multigrid_) != UG_NS<dim>::GM_OK)
    DUNE_THROW(IOError, "Call of 'UG::CreateAlgebra' failed!");

  /* here all temp memory since CreateMultiGrid is released */
  Release(multigrid_->theHeap, UG::FROM_TOP, multigrid_->MarkKey);
  multigrid_->MarkKey = 0;

  // Set the local indices
  setIndices();

  // Clear refinement flags
  typename Traits::template Codim<0>::LevelIterator eIt    = lbegin<0>(0);
  typename Traits::template Codim<0>::LevelIterator eEndIt = lend<0>(0);

  for (; eIt!=eEndIt; ++eIt)
    UG_NS<dim>::WriteCW(getRealImplementation(*eIt).target_, UG_NS<dim>::NEWEL_CE, 0);

#endif
}

template < int dim >
void Dune::UGGrid < dim >::createLGMGrid(const std::string& name)
{
#ifndef UG_LGMDOMAIN
  DUNE_THROW(GridError, "You cannot call createLGMGrid() when your UGGrid hasn't been configured for LGM!");
#else
  // ////////////////////////////////////////////////
  //   Create a boundary value problem (BVP)
  // ////////////////////////////////////////////////
  if (dim==2) {
    if (!UG::D2::CreateProblem(name.c_str(),NULL,NULL,NULL,0,NULL,0,NULL))
      DUNE_THROW(GridError, "LGM::CreateProblem() returned error code!");
  } else {
    if (!UG::D3::CreateProblem(name.c_str(),NULL,NULL,NULL,0,NULL,0,NULL))
      DUNE_THROW(GridError, "LGM::CreateProblem() returned error code!");
  }

  // ///////////////////////////////////////////
  //   Call configureCommand and newCommand
  // ///////////////////////////////////////////
#if 0 // Do we need this?
      //configure @PROBLEM $d @DOMAIN;
  std::string configureArgs[2] = {"configure " + name_ + "_Problem", "d " + name_ + "_Domain"};
  const char* configureArgs_c[2] = {configureArgs[0].c_str(), configureArgs[1].c_str()};

  if (UG_NS<dim>::ConfigureCommand(2, configureArgs_c))
    DUNE_THROW(GridError, "Calling UG" << dim << "d::ConfigureCommand failed!");
#endif

  //new @PROBLEM $b @PROBLEM $f @FORMAT $h @HEAP;
  char* newArgs[4];
  for (int i=0; i<4; i++)
    newArgs[i] = (char*)::malloc(50*sizeof(char));

  sprintf(newArgs[0], "new %s", name_.c_str());

  sprintf(newArgs[1], "b %s", name.c_str());
  sprintf(newArgs[2], "f DuneFormat%dd", dim);
  sprintf(newArgs[3], "h %dM", heapsize);

  if (UG_NS<dim>::NewCommand(4, newArgs))
    DUNE_THROW(GridError, "UGGrid::makeNewMultigrid failed!");

  for (int i=0; i<4; i++)
    free(newArgs[i]);

  // Get a direct pointer to the newly created multigrid
  multigrid_ = UG_NS<dim>::GetMultigrid(name_.c_str());
  if (!multigrid_)
    DUNE_THROW(GridError, "UGGrid::makeNewMultigrid failed!");

  // Complete the UG-internal grid data structure
  if (CreateAlgebra(multigrid_) != UG_NS<dim>::GM_OK)
    DUNE_THROW(IOError, "Call of 'UG::CreateAlgebra' failed!");

  /* here all temp memory since CreateMultiGrid is released */
  Release(multigrid_->theHeap, UG::FROM_TOP, multigrid_->MarkKey);
  multigrid_->MarkKey = 0;

  // Set the local indices
  setIndices();

#endif
}

template <int dim>
void Dune::UGGrid<dim>::
insertLinearSegment(const std::vector<int>& vertices,
                    const std::vector<FieldVector<double,dim> >& coordinates,
                    unsigned int segmentIndex)
{
#ifndef UG_LGMDOMAIN
  /** \todo Make sure this is the current multigrid, so that CreateBoundarySegment
      really inserts boundary segments into this grid.*/

  // Copy the vertices into a C-style array
  /** \todo Due to some UG weirdness, in 3d, CreateBoundarySegment always expects
      this array to have four entries, even if only a triangular segment is
      inserted.  If not, undefined values are will be introduced. */
  int vertices_c_style[4] = {-1, -1, -1, -1};
  for (unsigned int i=0; i<vertices.size(); i++)
    vertices_c_style[i] = vertices[i];

  // Create dummy parameter ranges
  const double alpha[2] = {0, 0};
  const double beta[2]  = {1, 1};

  // Create some boundary segment name
  char segmentName[20];
  if(sprintf(segmentName, "BS %d", segmentIndex) < 0)
    DUNE_THROW(GridError, "sprintf returned error code!");

  // Choose the method which implements the shape of the boundary segment
  typename UG_NS<dim>::BndSegFuncPtr boundarySegmentFunction;

  if (dim==3) {
    boundarySegmentFunction = boundarySegmentWrapper3d;
    if (vertices.size()==3) {
      // Cast to a type which may be wrong, just to make the code compile.
      // But this code only gets executed when the cast is correct anyways.
      boundarySegments_.push_back((BoundarySegment<dim>*) new LinearTriSegment3d(*(FieldVector<double,3>*)(&coordinates[0]),
                                                                                 *(FieldVector<double,3>*)(&coordinates[1]),
                                                                                 *(FieldVector<double,3>*)(&coordinates[2])));
    } else {
      // Cast to a type which may be wrong, just to make the code compile.
      // But this code only gets executed when the cast is correct anyways.
      boundarySegments_.push_back((BoundarySegment<dim>*) new LinearQuadSegment3d(*(FieldVector<double,3>*)(&coordinates[0]),
                                                                                  *(FieldVector<double,3>*)(&coordinates[1]),
                                                                                  *(FieldVector<double,3>*)(&coordinates[2]),
                                                                                  *(FieldVector<double,3>*)(&coordinates[3])));
    }
  } else {
    boundarySegmentFunction = boundarySegmentWrapper2d;
    // Cast to a type which may be wrong, just to make the code compile.
    // But this code only gets executed when the cast is correct anyways.
    boundarySegments_.push_back((BoundarySegment<dim>*) new LinearSegment2d(*(FieldVector<double,2>*)(&coordinates[0]),
                                                                            *(FieldVector<double,2>*)(&coordinates[1])));
  }

  // Actually create the segment
  if (UG_NS<dim>::CreateBoundarySegment(segmentName,            // internal name of the boundary segment
                                        1,                      //  id of left subdomain
                                        2,                      //  id of right subdomain
                                        segmentIndex,           // Index of the segment
                                        1,                      // Resolution, only for the UG graphics
                                        vertices_c_style,       // Vertex indeces
                                        alpha,                  // The local coordinates range
                                        beta,                   //    of the boundary segment
                                        boundarySegmentFunction,
                                        const_cast<BoundarySegment<dim>*>(boundarySegments_.back()))==NULL) {
    DUNE_THROW(GridError, "Calling UG" << dim << "d::CreateBoundarySegment failed!");
  }

#if 0
  // It would be a lot smarter to use this way of describing
  // boundary segments.  But as of yet, UG crashes when using
  // linear segments.
  double paramCoords[3][2] = {{0,0}, {1,0}, {0,1}};
  if (UG3d::CreateLinearSegment(segmentName,
                                left,               /*id of left subdomain */
                                right,              /*id of right subdomain*/
                                i,                  /*id of segment*/
                                4,                  // Number of corners
                                point,
                                paramCoords
                                )==NULL)
    DUNE_THROW(IOError, "Error calling CreateLinearSegment");

#endif

#endif

}

template <int dim>
void Dune::UGGrid<dim>::insertBoundarySegment(const std::vector<unsigned int> vertices,
                                              const BoundarySegment<dim>* boundarySegment)
{
  FixedArray<unsigned int, dim*2-2> segmentVertices = -1;
  for (size_t i=0; i<vertices.size(); i++)
    segmentVertices[i] = vertices[i];

  // DUNE --> UG vertex renumbering for quadrilateral boundary segments
  if (vertices.size()==4) {
    segmentVertices[2] = vertices[3];
    segmentVertices[3] = vertices[2];
  }

  boundarySegmentVertices_.push_back(segmentVertices);

  // Append boundary segment class to the boundary segment class list, so we can
  // delete them all in the destructor
  boundarySegments_.push_back(boundarySegment);

}

template <int dim>
void Dune::UGGrid<dim>::insertVertex(const FieldVector<double,dim>& pos)
{
  vertexPositions_.push_back(pos);
}

template <int dim>
void Dune::UGGrid<dim>::insertElement(GeometryType type,
                                      const std::vector<unsigned int>& vertices)
{
  if (dim!=type.dim())
    DUNE_THROW(GridError, "You cannot insert a " << type
                                                 << " into a UGGrid<" << dim << ">!");

  int newIdx = elementVertices_.size();

  elementTypes_.push_back(vertices.size());
  for (unsigned int i=0; i<vertices.size(); i++)
    elementVertices_.push_back(vertices[i]);

  if (type.isTriangle()) {
    // Everything alright
    if (vertices.size() != 3)
      DUNE_THROW(GridError, "You have requested to enter a triangle, but you"
                 << " have provided " << vertices.size() << " vertices!");

  } else if (type.isQuadrilateral()) {

    if (vertices.size() != 4)
      DUNE_THROW(GridError, "You have requested to enter a quadrilateral, but you"
                 << " have provided " << vertices.size() << " vertices!");

    // DUNE and UG numberings differ --> reorder the vertices
    elementVertices_[newIdx+2] = vertices[3];
    elementVertices_[newIdx+3] = vertices[2];

  } else if (type.isTetrahedron()) {

    if (vertices.size() != 4)
      DUNE_THROW(GridError, "You have requested to enter a tetrahedron, but you"
                 << " have provided " << vertices.size() << " vertices!");

  } else if (type.isPyramid()) {

    if (vertices.size() != 5)
      DUNE_THROW(GridError, "You have requested to enter a pyramid, but you"
                 << " have provided " << vertices.size() << " vertices!");

  } else if (type.isPrism()) {

    if (vertices.size() != 6)
      DUNE_THROW(GridError, "You have requested to enter a prism, but you"
                 << " have provided " << vertices.size() << " vertices!");

  } else if (type.isHexahedron()) {

    if (vertices.size() != 8)
      DUNE_THROW(GridError, "You have requested to enter a hexahedron, but you"
                 << " have provided " << vertices.size() << " vertices!");

    // DUNE and UG numberings differ --> reorder the vertices
    elementVertices_[newIdx+2] = vertices[3];
    elementVertices_[newIdx+3] = vertices[2];
    elementVertices_[newIdx+6] = vertices[7];
    elementVertices_[newIdx+7] = vertices[6];

  } else {
    DUNE_THROW(GridError, "You cannot insert a " << type
                                                 << " into a UGGrid<" << dim << ">!");
  }

}

template < int dim >
void Dune::UGGrid < dim >::setPosition(typename Traits::template Codim<dim>::EntityPointer& e,
                                       const FieldVector<double, dim>& pos)
{
  typename UG_NS<dim>::Node* target = getRealImplementation(*e).target_;

  for (int i=0; i<dim; i++)
    target->myvertex->iv.x[i] = pos[i];
}

template < int dim >
void Dune::UGGrid < dim >::setIndices()
{
  /** \todo The code in the following if-clause is a workaround to fix FlySpray issue 170
      (inconsistent codim 1 subIndices).  UGGrid uses UG's SideVector data structure to
      store the codim 1 subIndices of elements.  However the UG SideVector code is buggy
      and SideVectors are not correctly created.  There are two reasons why I provide
      a workaround instead of fixing UG directly:
      - it is not trivial to fix in UG, and I'd probably screw up other things if I
      started to mess around in the UG refinement routines
      - AFAIK Stefan Lang is rewriting that part of UG anyways.
      As soon as UGGrid does not rely on SideVectors anymore to store indices the following
      if-clause can be deleted.
   */
  if (dim==3) {

    for (int i=1; i<=maxLevel(); i++) {

      typename Traits::template Codim<0>::LevelIterator eIt = lbegin<0>(i);
      typename Traits::template Codim<0>::LevelIterator eEndIt = lend<0>(i);

      for (; eIt!=eEndIt; ++eIt) {

        typename UG_NS<dim>::Element* elem0 = getRealImplementation(*eIt).target_;

        typename Traits::template Codim<0>::Entity::LevelIntersectionIterator nIt = eIt->ilevelbegin();
        typename Traits::template Codim<0>::Entity::LevelIntersectionIterator nEndIt = eIt->ilevelend();

        for (; nIt!=nEndIt; ++nIt) {

          if (!nIt.neighbor())
            continue;

          typename UG_NS<dim>::Element* elem1 = getRealImplementation(*nIt.outside()).target_;

          int side0 = UGGridRenumberer<dim>::facesDUNEtoUG(nIt.numberInSelf(), eIt->geometry().type());
          int side1 = UGGridRenumberer<dim>::facesDUNEtoUG(nIt.numberInNeighbor(), nIt.outside()->geometry().type());

          UG::D3::DisposeDoubledSideVector((typename UG_NS<3>::Grid*)multigrid_->grids[i],
                                           (typename UG_NS<3>::Element*)elem0,
                                           side0,
                                           (typename UG_NS<3>::Element*)elem1,
                                           side1);

        }

      }

    }

  }

  // Create new level index sets if necessary
  for (int i=levelIndexSets_.size(); i<=maxLevel(); i++)
    levelIndexSets_.push_back(new UGGridLevelIndexSet<const UGGrid<dim> >());

  // Update all LevelIndexSets
  for (int i=0; i<=maxLevel(); i++)
    if (levelIndexSets_[i])
      levelIndexSets_[i]->update(*this, i);

  leafIndexSet_.update();

  localIdSet_.update();

  globalIdSet_.update();

}

// /////////////////////////////////////////////////////////////////////////////////
//   Explicit instantiation of the dimensions that are actually supported by UG.
//   g++-4.0 wants them to be _after_ the method implementations.
// /////////////////////////////////////////////////////////////////////////////////

template class Dune::UGGrid<2>;
template class Dune::UGGrid<3>;

// ////////////////////////////////////////////////////////////////////////////////////
//   Explicitly instantiate the necessary member templates contained in UGGrid<2>
// ////////////////////////////////////////////////////////////////////////////////////
template Dune::UGGrid<2>::Traits::Codim<0>::LevelIterator Dune::UGGrid<2>::lbegin<0>(int level) const;
template Dune::UGGrid<2>::Traits::Codim<2>::LevelIterator Dune::UGGrid<2>::lbegin<2>(int level) const;

template Dune::UGGrid<2>::Traits::Codim<0>::LevelIterator Dune::UGGrid<2>::lend<0>(int level) const;
template Dune::UGGrid<2>::Traits::Codim<2>::LevelIterator Dune::UGGrid<2>::lend<2>(int level) const;

template Dune::UGGrid<2>::Traits::Codim<0>::LeafIterator Dune::UGGrid<2>::leafbegin<0>() const;
template Dune::UGGrid<2>::Traits::Codim<2>::LeafIterator Dune::UGGrid<2>::leafbegin<2>() const;

template Dune::UGGrid<2>::Traits::Codim<0>::LeafIterator Dune::UGGrid<2>::leafend<0>() const;
template Dune::UGGrid<2>::Traits::Codim<2>::LeafIterator Dune::UGGrid<2>::leafend<2>() const;


// Element level iterators
template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::Interior_Partition>::LevelIterator
Dune::UGGrid<2>::lbegin<0,Dune::Interior_Partition>(int level) const;
template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::InteriorBorder_Partition>::LevelIterator
Dune::UGGrid<2>::lbegin<0,Dune::InteriorBorder_Partition>(int level) const;
template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::Overlap_Partition>::LevelIterator
Dune::UGGrid<2>::lbegin<0,Dune::Overlap_Partition>(int level) const;
template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::OverlapFront_Partition>::LevelIterator
Dune::UGGrid<2>::lbegin<0,Dune::OverlapFront_Partition>(int level) const;
template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::All_Partition>::LevelIterator
Dune::UGGrid<2>::lbegin<0,Dune::All_Partition>(int level) const;
template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::Ghost_Partition>::LevelIterator
Dune::UGGrid<2>::lbegin<0,Dune::Ghost_Partition>(int level) const;

template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::Interior_Partition>::LevelIterator
Dune::UGGrid<2>::lend<0,Dune::Interior_Partition>(int level) const;
template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::InteriorBorder_Partition>::LevelIterator
Dune::UGGrid<2>::lend<0,Dune::InteriorBorder_Partition>(int level) const;
template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::Overlap_Partition>::LevelIterator
Dune::UGGrid<2>::lend<0,Dune::Overlap_Partition>(int level) const;
template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::OverlapFront_Partition>::LevelIterator
Dune::UGGrid<2>::lend<0,Dune::OverlapFront_Partition>(int level) const;
template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::All_Partition>::LevelIterator
Dune::UGGrid<2>::lend<0,Dune::All_Partition>(int level) const;
template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::Ghost_Partition>::LevelIterator
Dune::UGGrid<2>::lend<0,Dune::Ghost_Partition>(int level) const;

// Vertex level iterators
template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::Interior_Partition>::LevelIterator
Dune::UGGrid<2>::lbegin<2,Dune::Interior_Partition>(int level) const;
template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::InteriorBorder_Partition>::LevelIterator
Dune::UGGrid<2>::lbegin<2,Dune::InteriorBorder_Partition>(int level) const;
template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::Overlap_Partition>::LevelIterator
Dune::UGGrid<2>::lbegin<2,Dune::Overlap_Partition>(int level) const;
template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::OverlapFront_Partition>::LevelIterator
Dune::UGGrid<2>::lbegin<2,Dune::OverlapFront_Partition>(int level) const;
template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::All_Partition>::LevelIterator
Dune::UGGrid<2>::lbegin<2,Dune::All_Partition>(int level) const;
template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::Ghost_Partition>::LevelIterator
Dune::UGGrid<2>::lbegin<2,Dune::Ghost_Partition>(int level) const;

template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::Interior_Partition>::LevelIterator
Dune::UGGrid<2>::lend<2,Dune::Interior_Partition>(int level) const;
template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::InteriorBorder_Partition>::LevelIterator
Dune::UGGrid<2>::lend<2,Dune::InteriorBorder_Partition>(int level) const;
template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::Overlap_Partition>::LevelIterator
Dune::UGGrid<2>::lend<2,Dune::Overlap_Partition>(int level) const;
template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::OverlapFront_Partition>::LevelIterator
Dune::UGGrid<2>::lend<2,Dune::OverlapFront_Partition>(int level) const;
template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::All_Partition>::LevelIterator
Dune::UGGrid<2>::lend<2,Dune::All_Partition>(int level) const;
template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::Ghost_Partition>::LevelIterator
Dune::UGGrid<2>::lend<2,Dune::Ghost_Partition>(int level) const;

// Element leaf iterators
template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::Interior_Partition>::LeafIterator
Dune::UGGrid<2>::leafbegin<0,Dune::Interior_Partition>() const;
template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::InteriorBorder_Partition>::LeafIterator
Dune::UGGrid<2>::leafbegin<0,Dune::InteriorBorder_Partition>() const;
template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::Overlap_Partition>::LeafIterator
Dune::UGGrid<2>::leafbegin<0,Dune::Overlap_Partition>() const;
template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::OverlapFront_Partition>::LeafIterator
Dune::UGGrid<2>::leafbegin<0,Dune::OverlapFront_Partition>() const;
template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::All_Partition>::LeafIterator
Dune::UGGrid<2>::leafbegin<0,Dune::All_Partition>() const;
template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::Ghost_Partition>::LeafIterator
Dune::UGGrid<2>::leafbegin<0,Dune::Ghost_Partition>() const;

template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::Interior_Partition>::LeafIterator
Dune::UGGrid<2>::leafend<0,Dune::Interior_Partition>() const;
template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::InteriorBorder_Partition>::LeafIterator
Dune::UGGrid<2>::leafend<0,Dune::InteriorBorder_Partition>() const;
template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::Overlap_Partition>::LeafIterator
Dune::UGGrid<2>::leafend<0,Dune::Overlap_Partition>() const;
template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::OverlapFront_Partition>::LeafIterator
Dune::UGGrid<2>::leafend<0,Dune::OverlapFront_Partition>() const;
template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::All_Partition>::LeafIterator
Dune::UGGrid<2>::leafend<0,Dune::All_Partition>() const;
template Dune::UGGrid<2>::Traits::Codim<0>::Partition<Dune::Ghost_Partition>::LeafIterator
Dune::UGGrid<2>::leafend<0,Dune::Ghost_Partition>() const;

// Vertex leaf iterators
template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::Interior_Partition>::LeafIterator
Dune::UGGrid<2>::leafbegin<2,Dune::Interior_Partition>() const;
template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::InteriorBorder_Partition>::LeafIterator
Dune::UGGrid<2>::leafbegin<2,Dune::InteriorBorder_Partition>() const;
template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::Overlap_Partition>::LeafIterator
Dune::UGGrid<2>::leafbegin<2,Dune::Overlap_Partition>() const;
template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::OverlapFront_Partition>::LeafIterator
Dune::UGGrid<2>::leafbegin<2,Dune::OverlapFront_Partition>() const;
template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::All_Partition>::LeafIterator
Dune::UGGrid<2>::leafbegin<2,Dune::All_Partition>() const;
template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::Ghost_Partition>::LeafIterator
Dune::UGGrid<2>::leafbegin<2,Dune::Ghost_Partition>() const;

template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::Interior_Partition>::LeafIterator
Dune::UGGrid<2>::leafend<2,Dune::Interior_Partition>() const;
template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::InteriorBorder_Partition>::LeafIterator
Dune::UGGrid<2>::leafend<2,Dune::InteriorBorder_Partition>() const;
template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::Overlap_Partition>::LeafIterator
Dune::UGGrid<2>::leafend<2,Dune::Overlap_Partition>() const;
template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::OverlapFront_Partition>::LeafIterator
Dune::UGGrid<2>::leafend<2,Dune::OverlapFront_Partition>() const;
template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::All_Partition>::LeafIterator
Dune::UGGrid<2>::leafend<2,Dune::All_Partition>() const;
template Dune::UGGrid<2>::Traits::Codim<2>::Partition<Dune::Ghost_Partition>::LeafIterator
Dune::UGGrid<2>::leafend<2,Dune::Ghost_Partition>() const;


// ////////////////////////////////////////////////////////////////////////////////////
//   Explicitly instantiate the necessary member templates contained in UGGrid<3>
// ////////////////////////////////////////////////////////////////////////////////////
template Dune::UGGrid<3>::Traits::Codim<0>::LevelIterator Dune::UGGrid<3>::lbegin<0>(int level) const;
template Dune::UGGrid<3>::Traits::Codim<3>::LevelIterator Dune::UGGrid<3>::lbegin<3>(int level) const;

template Dune::UGGrid<3>::Traits::Codim<0>::LevelIterator Dune::UGGrid<3>::lend<0>(int level) const;
template Dune::UGGrid<3>::Traits::Codim<3>::LevelIterator Dune::UGGrid<3>::lend<3>(int level) const;

template Dune::UGGrid<3>::Traits::Codim<0>::LeafIterator Dune::UGGrid<3>::leafbegin<0>() const;
template Dune::UGGrid<3>::Traits::Codim<3>::LeafIterator Dune::UGGrid<3>::leafbegin<3>() const;

template Dune::UGGrid<3>::Traits::Codim<0>::LeafIterator Dune::UGGrid<3>::leafend<0>() const;
template Dune::UGGrid<3>::Traits::Codim<3>::LeafIterator Dune::UGGrid<3>::leafend<3>() const;

// element level iterators
template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::Interior_Partition>::LevelIterator
Dune::UGGrid<3>::lbegin<0,Dune::Interior_Partition>(int level) const;
template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::InteriorBorder_Partition>::LevelIterator
Dune::UGGrid<3>::lbegin<0,Dune::InteriorBorder_Partition>(int level) const;
template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::Overlap_Partition>::LevelIterator
Dune::UGGrid<3>::lbegin<0,Dune::Overlap_Partition>(int level) const;
template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::OverlapFront_Partition>::LevelIterator
Dune::UGGrid<3>::lbegin<0,Dune::OverlapFront_Partition>(int level) const;
template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::All_Partition>::LevelIterator
Dune::UGGrid<3>::lbegin<0,Dune::All_Partition>(int level) const;
template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::Ghost_Partition>::LevelIterator
Dune::UGGrid<3>::lbegin<0,Dune::Ghost_Partition>(int level) const;

template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::Interior_Partition>::LevelIterator
Dune::UGGrid<3>::lend<0,Dune::Interior_Partition>(int level) const;
template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::InteriorBorder_Partition>::LevelIterator
Dune::UGGrid<3>::lend<0,Dune::InteriorBorder_Partition>(int level) const;
template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::Overlap_Partition>::LevelIterator
Dune::UGGrid<3>::lend<0,Dune::Overlap_Partition>(int level) const;
template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::OverlapFront_Partition>::LevelIterator
Dune::UGGrid<3>::lend<0,Dune::OverlapFront_Partition>(int level) const;
template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::All_Partition>::LevelIterator
Dune::UGGrid<3>::lend<0,Dune::All_Partition>(int level) const;
template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::Ghost_Partition>::LevelIterator
Dune::UGGrid<3>::lend<0,Dune::Ghost_Partition>(int level) const;

// Vertex level iterators
template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::Interior_Partition>::LevelIterator
Dune::UGGrid<3>::lbegin<3,Dune::Interior_Partition>(int level) const;
template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::InteriorBorder_Partition>::LevelIterator
Dune::UGGrid<3>::lbegin<3,Dune::InteriorBorder_Partition>(int level) const;
template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::Overlap_Partition>::LevelIterator
Dune::UGGrid<3>::lbegin<3,Dune::Overlap_Partition>(int level) const;
template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::OverlapFront_Partition>::LevelIterator
Dune::UGGrid<3>::lbegin<3,Dune::OverlapFront_Partition>(int level) const;
template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::All_Partition>::LevelIterator
Dune::UGGrid<3>::lbegin<3,Dune::All_Partition>(int level) const;
template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::Ghost_Partition>::LevelIterator
Dune::UGGrid<3>::lbegin<3,Dune::Ghost_Partition>(int level) const;

template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::Interior_Partition>::LevelIterator
Dune::UGGrid<3>::lend<3,Dune::Interior_Partition>(int level) const;
template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::InteriorBorder_Partition>::LevelIterator
Dune::UGGrid<3>::lend<3,Dune::InteriorBorder_Partition>(int level) const;
template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::Overlap_Partition>::LevelIterator
Dune::UGGrid<3>::lend<3,Dune::Overlap_Partition>(int level) const;
template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::OverlapFront_Partition>::LevelIterator
Dune::UGGrid<3>::lend<3,Dune::OverlapFront_Partition>(int level) const;
template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::All_Partition>::LevelIterator
Dune::UGGrid<3>::lend<3,Dune::All_Partition>(int level) const;
template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::Ghost_Partition>::LevelIterator
Dune::UGGrid<3>::lend<3,Dune::Ghost_Partition>(int level) const;

// Element leaf iterators
template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::Interior_Partition>::LeafIterator
Dune::UGGrid<3>::leafbegin<0,Dune::Interior_Partition>() const;
template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::InteriorBorder_Partition>::LeafIterator
Dune::UGGrid<3>::leafbegin<0,Dune::InteriorBorder_Partition>() const;
template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::Overlap_Partition>::LeafIterator
Dune::UGGrid<3>::leafbegin<0,Dune::Overlap_Partition>() const;
template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::OverlapFront_Partition>::LeafIterator
Dune::UGGrid<3>::leafbegin<0,Dune::OverlapFront_Partition>() const;
template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::All_Partition>::LeafIterator
Dune::UGGrid<3>::leafbegin<0,Dune::All_Partition>() const;
template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::Ghost_Partition>::LeafIterator
Dune::UGGrid<3>::leafbegin<0,Dune::Ghost_Partition>() const;

template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::Interior_Partition>::LeafIterator
Dune::UGGrid<3>::leafend<0,Dune::Interior_Partition>() const;
template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::InteriorBorder_Partition>::LeafIterator
Dune::UGGrid<3>::leafend<0,Dune::InteriorBorder_Partition>() const;
template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::Overlap_Partition>::LeafIterator
Dune::UGGrid<3>::leafend<0,Dune::Overlap_Partition>() const;
template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::OverlapFront_Partition>::LeafIterator
Dune::UGGrid<3>::leafend<0,Dune::OverlapFront_Partition>() const;
template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::All_Partition>::LeafIterator
Dune::UGGrid<3>::leafend<0,Dune::All_Partition>() const;
template Dune::UGGrid<3>::Traits::Codim<0>::Partition<Dune::Ghost_Partition>::LeafIterator
Dune::UGGrid<3>::leafend<0,Dune::Ghost_Partition>() const;

// Vertex leaf iterators
template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::Interior_Partition>::LeafIterator
Dune::UGGrid<3>::leafbegin<3,Dune::Interior_Partition>() const;
template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::InteriorBorder_Partition>::LeafIterator
Dune::UGGrid<3>::leafbegin<3,Dune::InteriorBorder_Partition>() const;
template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::Overlap_Partition>::LeafIterator
Dune::UGGrid<3>::leafbegin<3,Dune::Overlap_Partition>() const;
template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::OverlapFront_Partition>::LeafIterator
Dune::UGGrid<3>::leafbegin<3,Dune::OverlapFront_Partition>() const;
template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::All_Partition>::LeafIterator
Dune::UGGrid<3>::leafbegin<3,Dune::All_Partition>() const;
template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::Ghost_Partition>::LeafIterator
Dune::UGGrid<3>::leafbegin<3,Dune::Ghost_Partition>() const;

template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::Interior_Partition>::LeafIterator
Dune::UGGrid<3>::leafend<3,Dune::Interior_Partition>() const;
template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::InteriorBorder_Partition>::LeafIterator
Dune::UGGrid<3>::leafend<3,Dune::InteriorBorder_Partition>() const;
template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::Overlap_Partition>::LeafIterator
Dune::UGGrid<3>::leafend<3,Dune::Overlap_Partition>() const;
template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::OverlapFront_Partition>::LeafIterator
Dune::UGGrid<3>::leafend<3,Dune::OverlapFront_Partition>() const;
template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::All_Partition>::LeafIterator
Dune::UGGrid<3>::leafend<3,Dune::All_Partition>() const;
template Dune::UGGrid<3>::Traits::Codim<3>::Partition<Dune::Ghost_Partition>::LeafIterator
Dune::UGGrid<3>::leafend<3,Dune::Ghost_Partition>() const;
