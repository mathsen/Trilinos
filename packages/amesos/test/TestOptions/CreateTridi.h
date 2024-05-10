// @HEADER
// ***********************************************************************
// 
//                Amesos: Direct Sparse Solver Package
//                 Copyright (2004) Sandia Corporation
// 
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
// 
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version.
//  
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//  
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301
// USA
// Questions? Contact Michael A. Heroux (maherou@sandia.gov) 
// 
// ***********************************************************************
// @HEADER

//
//  CreateTridi populates an empty EpetraCrsMatrix with a tridiagonal with 
//  -1 on the off-diagonals and 2 on the diagonal.  
//
//  CreateTridiPlus creates the same matrix as CreateTridi except that it adds
//  -1 in the two off diagonal corners.
//
//  This code was plaguerized from epetra/example/petra_power_method/cxx_main.cpp
//  presumably written by Mike Heroux.
//
//  Adapted by Ken Stanley - Aug 2003 
//
#include "Epetra_CrsMatrix.h"

int CreateTridi(Epetra_CrsMatrix& A) ;

int CreateTridiPlus(Epetra_CrsMatrix& A) ;

#if defined(Amesos_SHOW_DEPRECATED_WARNINGS)
#ifdef __GNUC__
#warning "The Amesos package is deprecated"
#endif
#endif

