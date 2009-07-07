/*@HEADER
// ***********************************************************************
// 
//       Ifpack: Object-Oriented Algebraic Preconditioner Package
//                 Copyright (2009) Sandia Corporation
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
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA
// Questions? Contact Michael A. Heroux (maherou@sandia.gov) 
// 
// ***********************************************************************
//@HEADER
*/
#include "Ifpack_Hypre.h"
#if defined(HAVE_HYPRE) && defined(HAVE_MPI)


#include "Ifpack_Utils.h"
//#include "io.h"
#include "Epetra_MpiComm.h"
#include "Epetra_IntVector.h"
#include "Epetra_Import.h"
#include "Teuchos_ParameterList.hpp"
#include "Teuchos_RCP.hpp"
#ifdef IFPACK_NODE_AWARE_CODE
extern int ML_NODE_ID;
#endif

using Teuchos::RCP;
using Teuchos::rcp;



Ifpack_Hypre::Ifpack_Hypre(Epetra_RowMatrix* A):
  A_(rcp(A,false)),
  IsParallel_(false),
  IsInitialized_(false),
  IsComputed_(false),
  Label_(),
  NumInitialize_(0),
  NumCompute_(0),
  NumApplyInverse_(0),
  InitializeTime_(0.0),
  ComputeTime_(0.0),
  ApplyInverseTime_(0.0),
  ComputeFlops_(0.0),
  ApplyInverseFlops_(0.0),
  Time_(A_->Comm()),
  SolveOrPrec_(Solver)
{
  int ierr;
  IsSolverSetup_ = new bool[1];
  IsPrecondSetup_ = new bool[1];
  IsSolverSetup_[0] = false;
  IsPrecondSetup_[0] = false;
  if(Comm().NumProc() != 1) IsParallel_ = true;
  else IsParallel_ = false;
  const Epetra_MpiComm* eComm=dynamic_cast<const Epetra_MpiComm*>(&A_->Comm());
  MPI_Comm comm;
  comm = eComm->GetMpiComm();
  // First make a copy of the Epetra Matrix as a Hypre Matrix
  int ilower = A_->RowMatrixRowMap().MinMyGID();
  int iupper = A_->RowMatrixRowMap().MaxMyGID();
  printf("[%d] ilower = %d, iupper = %d\n", Comm().MyPID(), ilower, iupper);
  HYPRE_IJMatrixCreate(comm, ilower, iupper, ilower, iupper, &Hypre_A);
  HYPRE_IJMatrixSetObjectType(Hypre_A, HYPRE_PARCSR);
  HYPRE_IJMatrixInitialize(Hypre_A);
  for(int i = 0; i < A_->NumMyRows(); i++){
    int numElements;
    A_->NumMyRowEntries(i,numElements);
    std::vector<int> indices; indices.resize(numElements);
    std::vector<double> values; values.resize(numElements);
    int numEntries;
    A_->ExtractMyRowCopy(i, numElements, numEntries, &values[0], &indices[0]);
    for(int j = 0; j < numEntries; j++){
      indices[j] = A_->RowMatrixColMap().GID(indices[j]);
    }
    int GlobalRow[1];
    GlobalRow[0] = A_->RowMatrixRowMap().GID(i);
    HYPRE_IJMatrixSetValues(Hypre_A, 1, &numEntries, GlobalRow, &indices[0], &values[0]);
  }
  HYPRE_IJMatrixAssemble(Hypre_A);
  HYPRE_IJMatrixGetObject(Hypre_A, (void**)&ParMatrix_);

  // Next create vectors that will be used when ApplyInverse() is called
  ierr += HYPRE_IJVectorCreate(comm, ilower, iupper, &X_hypre);
  ierr += HYPRE_IJVectorSetObjectType(X_hypre, HYPRE_PARCSR);
  ierr += HYPRE_IJVectorInitialize(X_hypre);
  ierr += HYPRE_IJVectorAssemble(X_hypre);
  ierr += HYPRE_IJVectorGetObject(X_hypre, (void**) &par_x);

  ierr += HYPRE_IJVectorCreate(comm, ilower, iupper, &B_hypre);
  ierr += HYPRE_IJVectorSetObjectType(B_hypre, HYPRE_PARCSR);
  ierr += HYPRE_IJVectorInitialize(B_hypre);
  ierr += HYPRE_IJVectorAssemble(B_hypre);
  ierr += HYPRE_IJVectorGetObject(B_hypre, (void**) &par_b);

  x_vec = (hypre_ParVector *) hypre_IJVectorObject(((hypre_IJVector *) X_hypre));
  x_local = hypre_ParVectorLocalVector(x_vec);

  b_vec = (hypre_ParVector *) hypre_IJVectorObject(((hypre_IJVector *) B_hypre));
  b_local = hypre_ParVectorLocalVector(b_vec);
}

void Ifpack_Hypre::Destroy(){
  HYPRE_IJMatrixDestroy(Hypre_A);
  HYPRE_IJVectorDestroy(X_hypre);
  HYPRE_IJVectorDestroy(B_hypre);
  if(IsSolverSetup_[0]){
    SolverDestroyPtr_(Solver_);
  }
  if(IsPrecondSetup_[0]){
    PrecondDestroyPtr_(Preconditioner_);
  }
  delete[] IsSolverSetup_;
  delete[] IsPrecondSetup_;
}



int Ifpack_Hypre::Initialize(){
  IsInitialized_=true;
  NumInitialize_ = NumInitialize_ + 1;
  return 0;
}

int Ifpack_Hypre::SetParameters(Teuchos::ParameterList& list){
  List_ = list;
  Hypre_Solver solType = list.get("Solver", PCG);
  SetSolverType(solType);
  Hypre_Solver precType = list.get("Preconditioner", Euclid);
  SetPrecondType(precType);
  Hypre_Chooser chooser = list.get("SolveOrPrecondition", Solver);
  SolveOrPrec_ = chooser;
  bool SetPrecond = list.get("SetPreconditioner", false);
  if(SetPrecond){
    SetPreconditioner();
  }
  int NumFunctions = list.get("NumFunctions", 0);
  if(NumFunctions > 0){
    RCP<FunctionParameter>* params = list.get<RCP<FunctionParameter>*>("Functions");
    for(int i = 0; i < NumFunctions; i++){
      params[i]->CallFunction(Solver_, Preconditioner_);
    }
  }
  return 0;
}


int Ifpack_Hypre::Compute(){
  Time_.ResetStartTime();
  if(SolveOrPrec_ == Solver){
    SolverSetupPtr_(Solver_, ParMatrix_, par_x, par_b);
    IsSolverSetup_[0] = true;
  } else {
    PrecondSetupPtr_(Preconditioner_, ParMatrix_, par_x, par_b);
    IsPrecondSetup_[0] = true;
  }
  NumCompute_ = NumCompute_ + 1;
  ComputeTime_ = ComputeTime_ + Time_.ElapsedTime();
  return 0;
}




int Ifpack_Hypre::ApplyInverse(const Epetra_MultiVector& X, Epetra_MultiVector& Y) const{
  Time_.ResetStartTime();
  int ierr = 0;
  bool SameVectors = false;
  int NumVectors = X.NumVectors();
  if (NumVectors != Y.NumVectors()) return -1;  // X and Y must have same number of vectors
  if(X.Pointers() == Y.Pointers()){
    SameVectors = true;
  }
  for(int VecNum = 0; VecNum < NumVectors; VecNum++) {
    //Get values for current vector in multivector.
    double * x_values;
    ierr += (*X(VecNum)).ExtractView(&x_values);
    double * b_values;
    if(!SameVectors){
      ierr += (*Y(VecNum)).ExtractView(&b_values);
    } else {
      b_values = new double[X.MyLength()];
    }
    // Temporarily make a pointer to data in Hypre for end
    double *x_temp = x_local->data;
    // Replace data in Hypre vectors with epetra values
    x_local->data = x_values;
    double *b_temp = b_local->data;
    b_local->data = b_values;

    HYPRE_ParVectorSetConstantValues(par_b, 0.0);
    if(SolveOrPrec_ == Solver){
      // Use the solver methods
      ierr += SolverSolvePtr_(Solver_, ParMatrix_, par_x, par_b);
    } else {
      // Apply the preconditioner
      ierr += PrecondSolvePtr_(Preconditioner_, ParMatrix_, par_x, par_b);
    }
    TEST_FOR_EXCEPTION(ierr != 0, std::logic_error, "Hypre solve failed.");

    if(SameVectors){
      int NumEntries = Y.MyLength();
      std::vector<double> new_values; new_values.resize(NumEntries);
      std::vector<int> new_indices; new_indices.resize(NumEntries);
      for(int i = 0; i < NumEntries; i++){
        new_values[i] = b_values[i];
        new_indices[i] = i;
      }
      (*Y(VecNum)).ReplaceMyValues(NumEntries, &new_values[0], &new_indices[0]);
      delete[] b_values;
    }
    x_local->data = x_temp;
    b_local->data = b_temp;
  }
  NumApplyInverse_ = NumApplyInverse_ + 1;
  ApplyInverseTime_ = ApplyInverseTime_ + Time_.ElapsedTime();
  return(ierr);
}


ostream& Ifpack_Hypre::Print(ostream& os) const{
  os<<"Need to add meaningful output"<<endl;
  return os;
}


double Ifpack_Hypre::Condest(const Ifpack_CondestType CT, 
                             const int MaxIters,
                             const double Tol,
                             Epetra_RowMatrix* Matrix_in){
  return -1.0;
}

int Ifpack_Hypre::SetSolverType(Hypre_Solver Solver){
  switch(Solver) {
    case BoomerAMG:
      if(IsSolverSetup_[0]){
        SolverDestroyPtr_(Solver_);
        IsSolverSetup_[0] = false;
      }
      SolverCreatePtr_ = &Ifpack_Hypre::Hypre_BoomerAMGCreate;
      SolverDestroyPtr_ = &HYPRE_BoomerAMGDestroy;
      SolverSetupPtr_ = &HYPRE_BoomerAMGSetup;
      SolverPrecondPtr_ = NULL;
      SolverSolvePtr_ = &HYPRE_BoomerAMGSolve;
      break;
    case AMS:
      if(IsSolverSetup_[0]){
        SolverDestroyPtr_(Solver_);
        IsSolverSetup_[0] = false;
      }
      SolverCreatePtr_ = &Ifpack_Hypre::Hypre_AMSCreate;
      SolverDestroyPtr_ = &HYPRE_AMSDestroy;
      SolverSetupPtr_ = &HYPRE_AMSSetup;
      SolverSolvePtr_ = &HYPRE_AMSSolve;
      SolverPrecondPtr_ = NULL;
      break;
    case Hybrid:
      if(IsSolverSetup_[0]){
        SolverDestroyPtr_(Solver_);
        IsSolverSetup_[0] = false;
      }
      SolverCreatePtr_ = &Ifpack_Hypre::Hypre_ParCSRHybridCreate;
      SolverDestroyPtr_ = &HYPRE_ParCSRHybridDestroy;
      SolverSetupPtr_ = &HYPRE_ParCSRHybridSetup;
      SolverSolvePtr_ = &HYPRE_ParCSRHybridSolve;
      SolverPrecondPtr_ = &HYPRE_ParCSRHybridSetPrecond;
      break;
    case PCG:
      if(IsSolverSetup_[0]){
        SolverDestroyPtr_(Solver_);
        IsSolverSetup_[0] = false;
      }
      SolverCreatePtr_ = &Ifpack_Hypre::Hypre_ParCSRPCGCreate;
      SolverDestroyPtr_ = &HYPRE_ParCSRPCGDestroy;
      SolverSetupPtr_ = &HYPRE_ParCSRPCGSetup;
      SolverSolvePtr_ = &HYPRE_ParCSRPCGSolve;
      SolverPrecondPtr_ = &HYPRE_ParCSRPCGSetPrecond;
      break;
    case GMRES:
      if(IsSolverSetup_[0]){
        SolverDestroyPtr_(Solver_);
        IsSolverSetup_[0] = false;
      }
      SolverCreatePtr_ = &Ifpack_Hypre::Hypre_ParCSRGMRESCreate;
      SolverDestroyPtr_ = &HYPRE_ParCSRGMRESDestroy;
      SolverSetupPtr_ = &HYPRE_ParCSRGMRESSetup;
      SolverSolvePtr_ = &HYPRE_ParCSRGMRESSolve;
      SolverPrecondPtr_ = &HYPRE_ParCSRGMRESSetPrecond;
      break;
    case FlexGMRES:
      if(IsSolverSetup_[0]){
        SolverDestroyPtr_(Solver_);
        IsSolverSetup_[0] = false;
      }
      SolverCreatePtr_ = &Ifpack_Hypre::Hypre_ParCSRFlexGMRESCreate;
      SolverDestroyPtr_ = &HYPRE_ParCSRFlexGMRESDestroy;
      SolverSetupPtr_ = &HYPRE_ParCSRFlexGMRESSetup;
      SolverSolvePtr_ = &HYPRE_ParCSRFlexGMRESSolve;
      SolverPrecondPtr_ = &HYPRE_ParCSRFlexGMRESSetPrecond;
      break;
    case LGMRES:
      if(IsSolverSetup_[0]){
        SolverDestroyPtr_(Solver_);
        IsSolverSetup_[0] = false;
      }
      SolverCreatePtr_ = &Ifpack_Hypre::Hypre_ParCSRLGMRESCreate;
      SolverDestroyPtr_ = &HYPRE_ParCSRLGMRESDestroy;
      SolverSetupPtr_ = &HYPRE_ParCSRLGMRESSetup;
      SolverSolvePtr_ = &HYPRE_ParCSRLGMRESSolve;
      SolverPrecondPtr_ = &HYPRE_ParCSRLGMRESSetPrecond;
      break;
    case BiCGSTAB:
      if(IsSolverSetup_[0]){
        SolverDestroyPtr_(Solver_);
        IsSolverSetup_[0] = false;
      }
      SolverCreatePtr_ = &Ifpack_Hypre::Hypre_ParCSRBiCGSTABCreate;
      SolverDestroyPtr_ = &HYPRE_ParCSRBiCGSTABDestroy;
      SolverSetupPtr_ = &HYPRE_ParCSRBiCGSTABSetup;
      SolverSolvePtr_ = &HYPRE_ParCSRBiCGSTABSolve;
      SolverPrecondPtr_ = &HYPRE_ParCSRBiCGSTABSetPrecond;
      break;
    default:
      return -1;
    }
  CreateSolver();
  return 0;
}

int Ifpack_Hypre::SetPrecondType(Hypre_Solver Precond){
  switch(Precond) {
    case BoomerAMG:
      if(IsPrecondSetup_[0]){
        PrecondDestroyPtr_(Preconditioner_);
        IsPrecondSetup_[0] = false;
      }
      PrecondCreatePtr_ = &Ifpack_Hypre::Hypre_BoomerAMGCreate;
      PrecondDestroyPtr_ = &HYPRE_BoomerAMGDestroy;
      PrecondSetupPtr_ = &HYPRE_BoomerAMGSetup;
      PrecondSolvePtr_ = &HYPRE_BoomerAMGSolve;
      break;
    case ParaSails:
      if(IsPrecondSetup_[0]){
        PrecondDestroyPtr_(Preconditioner_);
        IsPrecondSetup_[0] = false;
      }
      PrecondCreatePtr_ = &Ifpack_Hypre::Hypre_ParaSailsCreate;
      PrecondDestroyPtr_ = &HYPRE_ParaSailsDestroy;
      PrecondSetupPtr_ = &HYPRE_ParaSailsSetup;
      PrecondSolvePtr_ = &HYPRE_ParaSailsSolve;
      break;
    case Euclid:
      if(IsPrecondSetup_[0]){
        PrecondDestroyPtr_(Preconditioner_);
        IsPrecondSetup_[0] = false;
      }
      PrecondCreatePtr_ = &Ifpack_Hypre::Hypre_EuclidCreate;
      PrecondDestroyPtr_ = &HYPRE_EuclidDestroy;
      PrecondSetupPtr_ = &HYPRE_EuclidSetup;
      PrecondSolvePtr_ = &HYPRE_EuclidSolve;
      break;
    case AMS:
      if(IsPrecondSetup_[0]){
        PrecondDestroyPtr_(Preconditioner_);
        IsPrecondSetup_[0] = false;
      }
      PrecondCreatePtr_ = &Ifpack_Hypre::Hypre_AMSCreate;
      PrecondDestroyPtr_ = &HYPRE_AMSDestroy;
      PrecondSetupPtr_ = &HYPRE_AMSSetup;
      PrecondSolvePtr_ = &HYPRE_AMSSolve;
      break;
    default:
      return -1;
    }
  CreatePrecond();
  return 0;

}

int Ifpack_Hypre::CreateSolver(){
  MPI_Comm comm;
  HYPRE_ParCSRMatrixGetComm(ParMatrix_, &comm);
  return (this->*SolverCreatePtr_)(comm, &Solver_);
  return 0;
}

int Ifpack_Hypre::CreatePrecond(){
  MPI_Comm comm;
  HYPRE_ParCSRMatrixGetComm(ParMatrix_, &comm);
  return (this->*PrecondCreatePtr_)(comm, &Preconditioner_);
}

int Ifpack_Hypre::SetPreconditioner(){
  if(SolverPrecondPtr_ == NULL){
    return -1;
  } else {
    SolverPrecondPtr_(Solver_, PrecondSolvePtr_, PrecondSetupPtr_, Preconditioner_);
    return 0;
  }
}

int Ifpack_Hypre::Hypre_BoomerAMGCreate(MPI_Comm comm, HYPRE_Solver *solver){
  return HYPRE_BoomerAMGCreate(solver);
}

int Ifpack_Hypre::Hypre_ParaSailsCreate(MPI_Comm comm, HYPRE_Solver *solver){
  return HYPRE_ParaSailsCreate(comm, solver);
}

int Ifpack_Hypre::Hypre_EuclidCreate(MPI_Comm comm, HYPRE_Solver *solver){
  return HYPRE_EuclidCreate(comm, solver);
}

int Ifpack_Hypre::Hypre_AMSCreate(MPI_Comm comm, HYPRE_Solver *solver){
  return HYPRE_AMSCreate(solver);
}

int Ifpack_Hypre::Hypre_ParCSRHybridCreate(MPI_Comm comm, HYPRE_Solver *solver){
  return HYPRE_ParCSRHybridCreate(solver);
}

int Ifpack_Hypre::Hypre_ParCSRPCGCreate(MPI_Comm comm, HYPRE_Solver *solver){
  return HYPRE_ParCSRPCGCreate(comm, solver);
}

int Ifpack_Hypre::Hypre_ParCSRGMRESCreate(MPI_Comm comm, HYPRE_Solver *solver){
  return HYPRE_ParCSRGMRESCreate(comm, solver);
}

int Ifpack_Hypre::Hypre_ParCSRFlexGMRESCreate(MPI_Comm comm, HYPRE_Solver *solver){
  return HYPRE_ParCSRFlexGMRESCreate(comm, solver);
}

int Ifpack_Hypre::Hypre_ParCSRLGMRESCreate(MPI_Comm comm, HYPRE_Solver *solver){
  return HYPRE_ParCSRLGMRESCreate(comm, solver);
}

int Ifpack_Hypre::Hypre_ParCSRBiCGSTABCreate(MPI_Comm comm, HYPRE_Solver *solver){
  return HYPRE_ParCSRBiCGSTABCreate(comm, solver);
}

#endif // HAVE_HYPRE && HAVE_MPI