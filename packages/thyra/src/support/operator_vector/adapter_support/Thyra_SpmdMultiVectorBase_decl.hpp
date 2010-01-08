// @HEADER
// ***********************************************************************
// 
//    Thyra: Interfaces and Support for Abstract Numerical Algorithms
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
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA
// Questions? Contact Michael A. Heroux (maherou@sandia.gov) 
// 
// ***********************************************************************
// @HEADER

#ifndef THYRA_SPMD_MULTI_VECTOR_BASE_DECL_HPP
#define THYRA_SPMD_MULTI_VECTOR_BASE_DECL_HPP

#include "Thyra_MultiVectorDefaultBase_decl.hpp"
#include "Thyra_SingleScalarEuclideanLinearOpBaseDecl.hpp"
#include "Teuchos_BLAS.hpp"

namespace Thyra {

/** \brief . */
template<class Scalar> class SpmdVectorSpaceBase;

/** \brief Base class for SPMD multi-vectors.
 *
 * By inheriting from this base class, multi-vector implementations allow
 * their multi-vector objects to be seamlessly combined with other SPMD
 * multi-vector objects (of different concrete types) in <tt>applyOp()</tt>
 * and <tt>apply()</tt>.  A big part of this protocol is that every
 * multi-vector object can expose an <tt>SpmdVectorSpaceBase</tt> object
 * through the virtual function <tt>spmdSpace()</tt>.
 *
 * This base class contains an implementation of <tt>applyOp()</tt> that
 * relies on implementations of the <tt>const</tt> functions
 * <tt>acquireDetachedView()</tt> and <tt>releaseDetachedView()</tt>, and the
 * non-<tt>const</tt> functions <tt>acquireDetachedView()</tt> and
 * <tt>commitDetachedView()</tt> (which all have default implementations in
 * this subclass).  In essence, this implementation will only call the
 * <tt>acquireDetachedView()</tt> functions using a range of (global) indexes
 * for elements that exist in the local process.  As long as the number of
 * local elements in each process is fairly large, the virtual function call
 * overhead will be minimal and this will result in a near optimal
 * implementation.
 *
 * <b>Notes to subclass developers</b>
 *
 * Concrete subclasses must override only five functions:
 * <tt>spmdSpace()</tt>, <tt>getLocalData(const Scalar**,Index*)</tt>,
 * <tt>freeLocalData(const Scalar**,Index*)</tt>,
 * <tt>getLocalData(Scalar**,Index*)</tt>, and
 * <tt>commitLocalData(Scalar**,Index*)</tt>.  Note that overriding the
 * <tt>spmdSpace()</tt> function requires implementing or using a
 * pre-implemented concrete <tt>SpmdVectorSpaceBase</tt> subclass.
 *
 * If the <tt>acquireDetachedView()</tt> functions are ever called with index
 * ranges outside of those of the local process, then the default
 * implementations in <tt>MultiVectorBase</tt> of all of the functions
 * (<tt>const</tt>) <tt>MultiVectorBase::acquireDetachedView()</tt>,
 * <tt>MultiVectorBase::releaseDetachedView()</tt>, (non-<tt>const</tt>)
 * <tt>MultiVectorBase::acquireDetachedView()</tt> and
 * <tt>MultiVectorBase::commitDetachedView()</tt> are called in instead.
 * Alternatively, a subclass could provide more specialized implementations of
 * these functions (for more efficient gather/scatter operations) if desired
 * but this should not be needed for most use cases.
 *
 * It is interesting to note that in the above use case that the
 * explicit subvector access functions call on its default
 * implementation defined in <tt>MultiVectorBase</tt> (which calls on
 * <tt>applyOp()</tt>) and the operator will be properly applied since
 * the version of <tt>applyOp()</tt> implemented in this class will
 * only request local vector data and hence there will only be two
 * levels of recursion for any call to an explicit subvector access
 * function.  This is a truly elegant result.
 *
 * Note that a multi-vector subclass derived from this base class must only be
 * directly used in SPMD mode for this to work properly.
 *
 * \ingroup Thyra_Op_Vec_adapters_Spmd_support_grp
 */
template<class Scalar>
class SpmdMultiVectorBase
  : virtual public MultiVectorDefaultBase<Scalar>,
    virtual public SingleScalarEuclideanLinearOpBase<Scalar>
{
public:

#ifdef THYRA_INJECT_USING_DECLARATIONS
  using SingleScalarEuclideanLinearOpBase<Scalar>::euclideanApply;
  using SingleScalarEuclideanLinearOpBase<Scalar>::apply;
#endif

  /** @name  Constructors / initializers / accessors */
  //@{

  /** \brief . */
  SpmdMultiVectorBase();

  //@}

  /** @name Pure virtual functions to be overridden by subclasses */
  //@{

  /** \brief Returns the SPMD vector space object for the range of
   * <tt>*this</tt> multi-vector.
   */
  virtual RCP<const SpmdVectorSpaceBase<Scalar> > spmdSpace() const = 0;

  /** \brief Returns a non-<tt>const</tt> pointer to a Fortran-style view of
   * the local multi-vector data.
   *
   * @param  localValues
   *           [out] On output <tt>*localValues</tt> will point to the first
   *           element in the first column of the local multi-vector stored as
   *           a column-major dense Fortran-style matrix.
   * @param  leadingDim
   *           [out] On output <tt>*leadingDim</tt> gives the leading
   *           dimension of the Fortran-style local multi-vector.
   *
   * Preconditions:<ul>
   * <li> <tt>localValues!=NULL</tt>
   * <li> <tt>leadingDim!=NULL</tt>
   * </ul>
   *
   * Preconditions:<ul>
   * <li> <tt>*localValues!=NULL</tt>
   * <li> <tt>*leadingDim!=0</tt>
   * </ul>
   *
   * The function <tT>commitLocalData()</tt> must be called to
   * commit changes to the data.
   */
  virtual void getNonconstLocalData(
    const Ptr<ArrayRCP<Scalar> > &localValues, const Ptr<Index> &leadingDim
    ) = 0;

  /** \brief Returns a <tt>const</tt> pointer to a Fortran-style view of the
   * local multi-vector data.
   *
   * @param  localValues
   *           [out] On output <tt>*localValues</tt> will point to the first
   *           element in the first column of the local multi-vector stored as
   *           a column-major dense Fortran-style matrix.
   * @param  leadingDim
   *           [out] On output <tt>*leadingDim</tt> gives the leading
   *           dimension of the Fortran-style local multi-vector.
   *
   * Preconditions:<ul>
   * <li> <tt>localValues!=NULL</tt>
   * <li> <tt>leadingDim!=NULL</tt>
   * </ul>
   *
   * Preconditions:<ul>
   * <li> <tt>*localValues!=NULL</tt>
   * <li> <tt>*leadingDim!=0</tt>
   * </ul>
   */
  virtual void getLocalData(
    const Ptr<ArrayRCP<const Scalar> > &localValues, const Ptr<Index> &leadingDim
    ) const = 0;

  //@}

  /** @name Overridden from EuclideanLinearOpBase */
  //@{

  /// Returns <tt>spmdSpace</tt>.
  RCP< const ScalarProdVectorSpaceBase<Scalar> > rangeScalarProdVecSpc() const;

  //@}

  /** @name Overridden from LinearOpBase */
  //@{

  /** \brief Calls <tt>EuclideanLinearOpBase::apply()</tt> to disambiguate
   * <tt>apply()</tt>
   */
  void apply(
    const EOpTransp M_trans,
    const MultiVectorBase<Scalar> &X,
    MultiVectorBase<Scalar> *Y,
    const Scalar alpha,
    const Scalar beta
    ) const;

  //@}

  /** @name Overridden from MultiVectorBase */
  //@{
  /** \brief . */
  void mvMultiReductApplyOpImpl(
    const RTOpPack::RTOpT<Scalar> &primary_op,
    const ArrayView<const Ptr<const MultiVectorBase<Scalar> > > &multi_vecs,
    const ArrayView<const Ptr<MultiVectorBase<Scalar> > > &targ_multi_vecs,
    const ArrayView<const Ptr<RTOpPack::ReductTarget> > &reduct_objs,
    const Index primary_first_ele,
    const Index primary_sub_dim,
    const Index primary_global_offset,
    const Index secondary_first_ele,
    const Index secondary_sub_dim
    ) const;
  /** \brief . */
  void acquireDetachedMultiVectorViewImpl(
    const Range1D &rowRng,
    const Range1D &colRng
    ,RTOpPack::ConstSubMultiVectorView<Scalar> *sub_mv
    ) const;
  /** \brief . */
  void releaseDetachedMultiVectorViewImpl(
    RTOpPack::ConstSubMultiVectorView<Scalar>* sub_mv
    ) const;
  /** \brief . */
  void acquireNonconstDetachedMultiVectorViewImpl(
    const Range1D &rowRng,
    const Range1D &colRng,
    RTOpPack::SubMultiVectorView<Scalar> *sub_mv
    );
  /** \brief . */
  void commitNonconstDetachedMultiVectorViewImpl(
    RTOpPack::SubMultiVectorView<Scalar>* sub_mv
    );
  //@}

  /** \name Deprecated. */
  //@{

  /** \brief Deprecated. */
  THYRA_DEPRECATED virtual void getLocalData( Scalar **localValues_out, Index *leadingDim_out )
    {
      using Teuchos::outArg;
      ArrayRCP<Scalar> localValues;
      this->getNonconstLocalData(outArg(localValues), outArg(*leadingDim_out));
      *localValues_out = localValues.getRawPtr();
    }
  
  /** \brief Deprecated. */
  THYRA_DEPRECATED virtual void commitLocalData( Scalar *localValues )
    {
      // Do nothing!
    }

  /** \brief Deprecated. */
  THYRA_DEPRECATED virtual void getLocalData(
    const Scalar **localValues_out, Index *leadingDim_out
    ) const
    {
      using Teuchos::outArg;
      ArrayRCP<const Scalar> localValues;
      this->getLocalData(outArg(localValues), outArg(*leadingDim_out));
      *localValues_out = localValues.getRawPtr();

    }

  /** \brief Deprecated. */
  THYRA_DEPRECATED virtual void freeLocalData( const Scalar *localValues ) const
    {
      // Do nothing!
    }

  //@}

protected:

  /** @name Overridden from SingleScalarEuclideanLinearOpBase */
  //@{

  /** \brief For complex <tt>Scalar</tt> types returns <tt>true</tt> for
   * <tt>NOTRANS</tt>, <tt>TRANS</tt>, and <tt>CONJTRANS</tt> and for real
   * types returns <tt>true</tt> for all values of <tt>M_trans</tt>.
   */
  bool opSupported(EOpTransp M_trans) const;

  /** \brief Uses <tt>GEMM()</tt> and <tt>Teuchos::reduceAll()</tt> to
   * implement.
   *
   * ToDo: Finish documentation!
   */
  void euclideanApply(
    const EOpTransp                     M_trans
    ,const MultiVectorBase<Scalar>    &X
    ,MultiVectorBase<Scalar>          *Y
    ,const Scalar                     alpha
    ,const Scalar                     beta
    ) const;

  //@}

  /** @name Protected functions for subclasses to call. */
  //@{

  /** \brief Subclasses should call whenever the structure of the
   * VectorSpaceBase changes.
   *
   * <b>WARNING!</b> This function can be overridden by subclasses but this
   * particular function implementation must be called back from within any
   * override (i.e. call
   * <tt>SpmdMultiVectorBase<Scalar>::updateSpmdSpace();</tt>).
   */
  virtual void updateSpmdSpace();

  /** \brief Validate and resize the row range.
   *
   * This function throws an exception if the input range is invalid
   */
  Range1D validateRowRange( const Range1D& rowRng ) const;

  /** \brief Validate and resize the column range.
   *
   * This function throws an exception if the input range is invalid
   */
  Range1D validateColRange( const Range1D& rowCol ) const;

  //@}
  
private:
  
  // ///////////////////////////////////////
  // Private data members
  
  mutable bool in_applyOp_;

  mutable Teuchos::BLAS<int,Scalar> blas_;

  // cached
  Index  globalDim_;
  Index  localOffset_;
  Index  localSubDim_;
  Index  numCols_;
  
}; // end class SpmdMultiVectorBase

} // end namespace Thyra

#endif // THYRA_SPMD_MULTI_VECTOR_BASE_DECL_HPP