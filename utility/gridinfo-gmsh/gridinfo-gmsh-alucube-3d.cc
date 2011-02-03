// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string>

#include <dune/grid/alugrid.hh>

const std::string programName = "dune-gridinfo-gmsh-alucube-3d";
typedef Dune::ALUCubeGrid<3, 3> Grid;

#include <dune/grid/utility/gridinfo-gmsh-main.hh>
