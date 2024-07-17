// @HEADER
// *****************************************************************************
//                           Stokhos Package
//
// Copyright 2009 NTESS and the Stokhos contributors.
// SPDX-License-Identifier: BSD-3-Clause
// *****************************************************************************
// @HEADER

#ifndef STOKHOS_INTERLACED_OPERATOR_HPP
#define STOKHOS_INTERLACED_OPERATOR_HPP

#include "Stokhos_SGOperator.hpp"
#include "EpetraExt_MultiComm.h"
#include "Stokhos_OrthogPolyBasis.hpp"
#include "Stokhos_EpetraSparse3Tensor.hpp"
#include "Teuchos_ParameterList.hpp"
#include "EpetraExt_BlockCrsMatrix.h"
#include <vector>

namespace Stokhos {
    
  /*! 
   * \brief An Epetra operator representing the block stochastic Galerkin
   * operator generated by fully assembling the matrix. The ordering of this
   * operator is interlaced.  That means that all stochastic degrees of freedom
   * associated with a deterministic degree of freedom are interlaced. The result
   * is a large sparse matrix that is composed of small (relatively) dense blocks.
   */
  class InterlacedOperator : 
    public Stokhos::SGOperator, 
    public EpetraExt::BlockCrsMatrix {
      
  public:

    //! Constructor 
    InterlacedOperator(
      const Teuchos::RCP<const EpetraExt::MultiComm>& sg_comm,
      const Teuchos::RCP<const Stokhos::OrthogPolyBasis<int,double> >& sg_basis,
      const Teuchos::RCP<const Stokhos::EpetraSparse3Tensor>& epetraCijk,
      const Teuchos::RCP<const Epetra_CrsGraph>& base_graph,
      const Teuchos::RCP<Teuchos::ParameterList>& params);
    
    //! Destructor
    virtual ~InterlacedOperator();

    /** \name Stokhos::SGOperator methods */
    //@{

    //! Setup operator
    virtual void setupOperator(
      const Teuchos::RCP<Stokhos::EpetraOperatorOrthogPoly >& poly);

    //! Get SG polynomial
    virtual Teuchos::RCP< Stokhos::EpetraOperatorOrthogPoly > 
    getSGPolynomial();

    //! Get SG polynomial
    virtual Teuchos::RCP<const Stokhos::EpetraOperatorOrthogPoly > 
    getSGPolynomial() const;

    //@}

    //! Sum into global matrix 
    void SumIntoGlobalBlock_Deterministic(double alpha,const Epetra_RowMatrix & determBlock,int Row,int Col);

  private:
    
    //! Private to prohibit copying
    InterlacedOperator(const InterlacedOperator&);
    
    //! Private to prohibit copying
    InterlacedOperator& operator=(const InterlacedOperator&);
    
  protected:

    //! Stores SG parallel communicator
    Teuchos::RCP<const EpetraExt::MultiComm> sg_comm;

    //! Stochastic Galerking basis
    Teuchos::RCP<const Stokhos::OrthogPolyBasis<int,double> > sg_basis;

    //! Stores Epetra Cijk tensor
    Teuchos::RCP<const Stokhos::EpetraSparse3Tensor> epetraCijk;

    //! Short-hand for Cijk
    typedef Stokhos::Sparse3Tensor<int,double> Cijk_type;

    //! Stores triple product tensor
    Teuchos::RCP<const Cijk_type> Cijk;

    //! Stores operators
    Teuchos::RCP<Stokhos::EpetraOperatorOrthogPoly > block_ops;

    //! Flag indicating whether operator be scaled with <\psi_i^2>
    bool scale_op;

    //! Flag indicating whether to include mean term
    bool include_mean;
    
    //! Flag indicating whether to only use linear terms
    bool only_use_linear;

    int determOffset_;

  }; // class InterlacedOperator
  
} // namespace Stokhos

#endif // STOKHOS_INTERLACED_OPERATOR_HPP
