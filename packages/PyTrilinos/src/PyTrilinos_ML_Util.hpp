// @HEADER
// ***********************************************************************
//
//          PyTrilinos: Python Interfaces to Trilinos Packages
//                 Copyright (2014) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia
// Corporation, the U.S. Government retains certain rights in this
// software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact William F. Spotz (wfspotz@sandia.gov)
//
// ***********************************************************************
// @HEADER

#ifndef ML_PYMATRIX_HPP
#define ML_PYMATRIX_HPP

#include "ml_common.h"
#include "MLAPI_BaseOperator.h"
#include "MLAPI_Operator.h"
#include "Teuchos_RCP.hpp"
#include "Epetra_Map.h"
#include "Epetra_FECrsMatrix.h"

using namespace MLAPI;

namespace PyTrilinos
{

class PyMatrix : public MLAPI::Operator
{
public:
  //@{ \name Constructors and destructors.

  //! Constructor with given already computed ML_Operator pointer.
  PyMatrix(const Space& RowSpace, const Space& ColSpace);

  //! Destructor.
  ~PyMatrix();

  const Space & GetRowSpace() const;

  void SetElement(int row, int col, double value);
  
  void FillComplete();

  Teuchos::RCP< Epetra_FECrsMatrix > GetMatrix();

private:

  //! Col space.
  Space ColSpace_;
  //! Row space.
  Space RowSpace_;
  Teuchos::RCP< Epetra_Map >         ColMap_;
  Teuchos::RCP< Epetra_Map >         RowMap_;
  Teuchos::RCP< Epetra_FECrsMatrix > Matrix_;

};  // PyMatrix

}  // Namespace PyTrilinos

#endif 

#if defined(PyTrilinos_SHOW_DEPRECATED_WARNINGS)
#ifdef __GNUC__
#warning "The PyTrilinos package is deprecated"
#endif
#endif

