/*!
 *
 * \brief Defines an ML preconditioner as a Epetra_Operator derived class.
 *
 * ML offers two preconditioners suitable for the solution of 
 * Epetra_LinearProblem objects. This file define one the two, called
 * MultiLevelOperator (in the ML_Epetra namespace). This preconditioner is
 * simple wrapper of the ML_Solve() function, so that ML can be applied to
 * Epetra_MultiVector's. 
 *
 * When you should use MultiLevelOperator:
 * - when your code already defines the required ML objects, with the optimal
 *   choice of parameters, and you want to use ML for Epetra_LinearProblem or
 *   AztecOO problems;
 *
 * When you should use MultiLevelPreconditioner:  
 * - when you have an Epetra_RowMatrix, and you don't want to code the
 *   conversion to ML_Operator, the creation of the hierarchy and the
 *   aggregates, and/or you want to experiment various combinations of the
 *   parameters, simply changing some parameters in a Teuchos::ParameterList.
 *
 * 
 * \date Last update to Doxygen: 22-Jul-04
 *
 */
/* ******************************************************************** */
/* See the file COPYRIGHT for a complete copyright notice, contact      */
/* person and disclaimer.                                               */        
/* ******************************************************************** */

#ifndef STOKHOS_STOCHGAL_PRECON_H
#define STOKHOS_STOCHGAL_PRECON_H

//class Epetra_MultiVector;
//class Epetra_BlockMap;
//class Epetra_Comm;

#include "Stokhos.hpp"
#include "Epetra_ConfigDefs.h"
#ifdef HAVE_MPI
#include "mpi.h"
#include "Epetra_MpiComm.h"
#else
#include "Epetra_SerialComm.h"
#endif
#include "Epetra_Map.h"
#include "Epetra_Vector.h"
#include "Epetra_CrsMatrix.h"
#include "Epetra_SerialDenseMatrix.h"
#include "Epetra_MultiVector.h"
#include "ml_include.h"
#include "ml_MultiLevelPreconditioner.h"
//#include "Epetra_LinearProblem.h"
//#include "Epetra_Object.h"

  
//! MultiLevelOperator: An implementation of the Epetra_Operator class.
/*! MultiLevelOperator class implements Epetra_Operator using a
    pre-constructed ML solver object. This allows ML to be used as 
    preconditioner within an AztecOO solver object.
*/    

namespace Stokhos{

class StochGalerkinPrecon: public virtual Epetra_Operator {
      
 public:

  //@{ \name Constructor.
  StochGalerkinPrecon(const Epetra_CrsMatrix& mean_op, const std::vector<double>& norms, const Epetra_Comm& Comm, const Epetra_Map& DMap, const Epetra_Map& RMap);
  //@{ \name Destructor.
    //! Destructor
  ~StochGalerkinPrecon(){}
  //@}
  //@{ \name Atribute set methods.

    //! If set true, transpose of this operator will be applied.
    /*! This flag allows the transpose of the given operator to be used
        implicitly.  Setting this flag affects only the Apply() and
        ApplyInverse() methods.  If the implementation of this interface
        does not support transpose use, this method should return a
        value of -1.
      
	\param UseTranspose (In) - If true, multiply by the transpose of
	operator, otherwise just use operator.

    \warning - This method has no effect and returns -1 as error code.
  */
  int SetUseTranspose(bool UseTranspose){return(-1);}
  //@}
  
  //@{ \name Mathematical functions.

  //! Returns the result of a Operator applied to a Epetra_MultiVector X in Y.
  /*! 
    \param X (In) - A Epetra_MultiVector of dimension NumVectors to multiply with matrix.
    \param Y (Out) -A Epetra_MultiVector of dimension NumVectors containing result.
    \warning - This method has no effect and returns -1 as error code.
  */
  int Apply(const Epetra_MultiVector& X, Epetra_MultiVector& Y) const {return -1;};

  //! Returns the result of a Operator inverse applied to an Epetra_MultiVector X in Y.
  /*! 
    \param X (In) - A Epetra_MultiVector of dimension NumVectors to solve for.
    \param Y (Out) -A Epetra_MultiVector of dimension NumVectors containing result.
    
    \return Integer error code, set to 0 if successful.
  */


//  int ApplyInverse(const Epetra_MultiVector& X, Epetra_MultiVector& Y, int iBlockSize = WKC) const;
  int ApplyInverse(const Epetra_MultiVector& X, Epetra_MultiVector& Y) const;

  //int ApplyInverse(const Epetra_MultiVector& X, Epetra_MultiVector& Y) const;
  
  //! Returns the infinity norm of the global matrix.
  /* Returns the quantity \f$ \| A \|_\infty\f$ such that
     \f[\| A \|_\infty = \max_{1\lei\lem} \sum_{j=1}^n |a_{ij}| \f].
     
     \warning This method must not be called unless HasNormInf() returns true.
  */ 
  double NormInf() const {return(0.0);};
  //@}
  
  //@{ \name Atribute access functions

  //! Returns a character string describing the operator
  const char * Label() const{return(Label_);};
  
  //! Returns the current UseTranspose setting.
  bool UseTranspose() const {return(false);};
  
  //! Returns true if the \e this object can provide an approximate Inf-norm, false otherwise.
  bool HasNormInf() const{return(false);};
  
  //! Returns a pointer to the Epetra_Comm communicator associated with this operator.
  const Epetra_Comm & Comm() const{return(Comm_);};
  
  //! Returns the Epetra_Map object associated with the domain of this operator.
  const Epetra_Map & OperatorDomainMap() const {return(DomainMap_);};
  //! Returns the Epetra_Map object associated with the range of this operator.
  const Epetra_Map & OperatorRangeMap() const {return(RangeMap_);};
  //@}
  
 private:
  //! Copy constructor (NOT DEFINED)
  //StochGalerkinFiniteDiffSystem(const StochGalerkinFiniteDiffSystem& RHS) :
  //  Comm_(RHS.Comm()),
  //  DomainMap_(RHS.OperatorDomainMap()),
  //  RangeMap_(RHS.OperatorRangeMap())
  //{ }

  //! Operator= (NOT DEFINED)
  StochGalerkinPrecon& operator=(const StochGalerkinPrecon&)
  {
    return(*this);
  }

  //! Reference to Epetra communicator.
  const Epetra_Comm& Comm_;
  const char * Label_;
  //! Reference to Domain Map.
  const Epetra_Map& DomainMap_;
  //! Reference to Range Map.
  const Epetra_Map& RangeMap_;

  const std::vector<double>& norms_;

  ML_Epetra::MultiLevelPreconditioner* MLPrec;

};
} 
#include "Stokhos_StochGalerkin_PreconImp.hpp"

#endif /* SACADO_STOCHGAL_DIFF_H*/
