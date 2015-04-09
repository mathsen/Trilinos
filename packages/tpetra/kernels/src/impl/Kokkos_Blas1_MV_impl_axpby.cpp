/*
//@HEADER
// ************************************************************************
//
//          Kokkos: Node API and Parallel Node Kernels
//              Copyright (2008) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
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
// Questions? Contact Michael A. Heroux (maherou@sandia.gov)
//
// ************************************************************************
//@HEADER
*/

#include <Kokkos_Blas1_MV_impl_axpby.hpp>
#include <climits>

namespace KokkosBlas {
namespace Impl {

#ifdef KOKKOS_HAVE_SERIAL
#define KOKKOSBLAS_IMPL_MV_EXEC_SPACE Kokkos::Serial
#define KOKKOSBLAS_IMPL_MV_MEM_SPACE Kokkos::HostSpace

void
Axpby<Kokkos::View<double**,
                   Kokkos::LayoutLeft,
                   Kokkos::Device<KOKKOSBLAS_IMPL_MV_EXEC_SPACE, KOKKOSBLAS_IMPL_MV_MEM_SPACE>,
                   Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                   Kokkos::Impl::ViewDefault>,
      double,
      Kokkos::View<const double**,
                   Kokkos::LayoutLeft,
                   Kokkos::Device<KOKKOSBLAS_IMPL_MV_EXEC_SPACE, KOKKOSBLAS_IMPL_MV_MEM_SPACE>,
                   Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                   Kokkos::Impl::ViewDefault>,
      double,
      Kokkos::View<const double**,
                   Kokkos::LayoutLeft,
                   Kokkos::Device<KOKKOSBLAS_IMPL_MV_EXEC_SPACE, KOKKOSBLAS_IMPL_MV_MEM_SPACE>,
                   Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                   Kokkos::Impl::ViewDefault>,
      2>::
axpby (const RMV& R, const XMV::non_const_value_type& alpha,
       const XMV& X, const YMV::non_const_value_type& beta,
       const YMV& Y)
{
  const size_type numRows = X.dimension_0 ();
  const size_type numCols = X.dimension_1 ();
  int a, b;
  if (alpha == ATA::zero ()) {
    a = 0;
  }
  else if (alpha == -ATA::one ()) {
    a = -1;
  }
  else if (alpha == ATA::one ()) {
    a = 1;
  }
  else {
    a = 2;
  }
  if (beta == ATB::zero ()) {
    b = 0;
  }
  else if (beta == -ATB::one ()) {
    b = -1;
  }
  else if (beta == ATB::one ()) {
    b = 1;
  }
  else {
    b = 2;
  }

  if (numRows < static_cast<size_type> (INT_MAX) &&
      numRows * numCols < static_cast<size_type> (INT_MAX)) {
    typedef int index_type;
    MV_Axpby_Invoke_Left<RMV, XMV::non_const_value_type, XMV,
      YMV::non_const_value_type, YMV, index_type> (R, alpha, X,
                                                   beta, Y, a, b);
  }
  else {
    typedef XMV::size_type index_type;
    MV_Axpby_Invoke_Left<RMV, XMV::non_const_value_type, XMV,
      YMV::non_const_value_type, YMV, index_type> (R, alpha, X,
                                                   beta, Y, a, b);
  }
}

#undef KOKKOSBLAS_IMPL_MV_EXEC_SPACE
#undef KOKKOSBLAS_IMPL_MV_MEM_SPACE
#endif // KOKKOS_HAVE_SERIAL


#ifdef KOKKOS_HAVE_OPENMP
#define KOKKOSBLAS_IMPL_MV_EXEC_SPACE Kokkos::OpenMP
#define KOKKOSBLAS_IMPL_MV_MEM_SPACE Kokkos::HostSpace

void
Axpby<Kokkos::View<double**,
                   Kokkos::LayoutLeft,
                   Kokkos::Device<KOKKOSBLAS_IMPL_MV_EXEC_SPACE, KOKKOSBLAS_IMPL_MV_MEM_SPACE>,
                   Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                   Kokkos::Impl::ViewDefault>,
      double,
      Kokkos::View<const double**,
                   Kokkos::LayoutLeft,
                   Kokkos::Device<KOKKOSBLAS_IMPL_MV_EXEC_SPACE, KOKKOSBLAS_IMPL_MV_MEM_SPACE>,
                   Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                   Kokkos::Impl::ViewDefault>,
      double,
      Kokkos::View<const double**,
                   Kokkos::LayoutLeft,
                   Kokkos::Device<KOKKOSBLAS_IMPL_MV_EXEC_SPACE, KOKKOSBLAS_IMPL_MV_MEM_SPACE>,
                   Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                   Kokkos::Impl::ViewDefault>,
      2>::
axpby (const RMV& R, const XMV::non_const_value_type& alpha,
       const XMV& X, const YMV::non_const_value_type& beta,
       const YMV& Y)
{
  const size_type numRows = X.dimension_0 ();
  const size_type numCols = X.dimension_1 ();
  int a, b;
  if (alpha == ATA::zero ()) {
    a = 0;
  }
  else if (alpha == -ATA::one ()) {
    a = -1;
  }
  else if (alpha == ATA::one ()) {
    a = 1;
  }
  else {
    a = 2;
  }
  if (beta == ATB::zero ()) {
    b = 0;
  }
  else if (beta == -ATB::one ()) {
    b = -1;
  }
  else if (beta == ATB::one ()) {
    b = 1;
  }
  else {
    b = 2;
  }

  if (numRows < static_cast<size_type> (INT_MAX) &&
      numRows * numCols < static_cast<size_type> (INT_MAX)) {
    typedef int index_type;
    MV_Axpby_Invoke_Left<RMV, XMV::non_const_value_type, XMV,
      YMV::non_const_value_type, YMV, index_type> (R, alpha, X,
                                                   beta, Y, a, b);
  }
  else {
    typedef XMV::size_type index_type;
    MV_Axpby_Invoke_Left<RMV, XMV::non_const_value_type, XMV,
      YMV::non_const_value_type, YMV, index_type> (R, alpha, X,
                                                   beta, Y, a, b);
  }
}

#undef KOKKOSBLAS_IMPL_MV_EXEC_SPACE
#undef KOKKOSBLAS_IMPL_MV_MEM_SPACE
#endif // KOKKOS_HAVE_OPENMP


#ifdef KOKKOS_HAVE_PTHREAD
#define KOKKOSBLAS_IMPL_MV_EXEC_SPACE Kokkos::Threads
#define KOKKOSBLAS_IMPL_MV_MEM_SPACE Kokkos::HostSpace

void
Axpby<Kokkos::View<double**,
                   Kokkos::LayoutLeft,
                   Kokkos::Device<KOKKOSBLAS_IMPL_MV_EXEC_SPACE, KOKKOSBLAS_IMPL_MV_MEM_SPACE>,
                   Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                   Kokkos::Impl::ViewDefault>,
      double,
      Kokkos::View<const double**,
                   Kokkos::LayoutLeft,
                   Kokkos::Device<KOKKOSBLAS_IMPL_MV_EXEC_SPACE, KOKKOSBLAS_IMPL_MV_MEM_SPACE>,
                   Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                   Kokkos::Impl::ViewDefault>,
      double,
      Kokkos::View<const double**,
                   Kokkos::LayoutLeft,
                   Kokkos::Device<KOKKOSBLAS_IMPL_MV_EXEC_SPACE, KOKKOSBLAS_IMPL_MV_MEM_SPACE>,
                   Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                   Kokkos::Impl::ViewDefault>,
      2>::
axpby (const RMV& R, const XMV::non_const_value_type& alpha,
       const XMV& X, const YMV::non_const_value_type& beta,
       const YMV& Y)
{
  const size_type numRows = X.dimension_0 ();
  const size_type numCols = X.dimension_1 ();
  int a, b;
  if (alpha == ATA::zero ()) {
    a = 0;
  }
  else if (alpha == -ATA::one ()) {
    a = -1;
  }
  else if (alpha == ATA::one ()) {
    a = 1;
  }
  else {
    a = 2;
  }
  if (beta == ATB::zero ()) {
    b = 0;
  }
  else if (beta == -ATB::one ()) {
    b = -1;
  }
  else if (beta == ATB::one ()) {
    b = 1;
  }
  else {
    b = 2;
  }

  if (numRows < static_cast<size_type> (INT_MAX) &&
      numRows * numCols < static_cast<size_type> (INT_MAX)) {
    typedef int index_type;
    MV_Axpby_Invoke_Left<RMV, XMV::non_const_value_type, XMV,
      YMV::non_const_value_type, YMV, index_type> (R, alpha, X,
                                                   beta, Y, a, b);
  }
  else {
    typedef XMV::size_type index_type;
    MV_Axpby_Invoke_Left<RMV, XMV::non_const_value_type, XMV,
      YMV::non_const_value_type, YMV, index_type> (R, alpha, X,
                                                   beta, Y, a, b);
  }
}

#undef KOKKOSBLAS_IMPL_MV_EXEC_SPACE
#undef KOKKOSBLAS_IMPL_MV_MEM_SPACE
#endif // KOKKOS_HAVE_PTHREAD


#ifdef KOKKOS_HAVE_CUDA
#define KOKKOSBLAS_IMPL_MV_EXEC_SPACE Kokkos::Cuda
#define KOKKOSBLAS_IMPL_MV_MEM_SPACE Kokkos::CudaSpace

void
Axpby<Kokkos::View<double**,
                   Kokkos::LayoutLeft,
                   Kokkos::Device<KOKKOSBLAS_IMPL_MV_EXEC_SPACE, KOKKOSBLAS_IMPL_MV_MEM_SPACE>,
                   Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                   Kokkos::Impl::ViewDefault>,
      double,
      Kokkos::View<const double**,
                   Kokkos::LayoutLeft,
                   Kokkos::Device<KOKKOSBLAS_IMPL_MV_EXEC_SPACE, KOKKOSBLAS_IMPL_MV_MEM_SPACE>,
                   Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                   Kokkos::Impl::ViewDefault>,
      double,
      Kokkos::View<const double**,
                   Kokkos::LayoutLeft,
                   Kokkos::Device<KOKKOSBLAS_IMPL_MV_EXEC_SPACE, KOKKOSBLAS_IMPL_MV_MEM_SPACE>,
                   Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                   Kokkos::Impl::ViewDefault>,
      2>::
axpby (const RMV& R, const XMV::non_const_value_type& alpha,
       const XMV& X, const YMV::non_const_value_type& beta,
       const YMV& Y)
{
  const size_type numRows = X.dimension_0 ();
  const size_type numCols = X.dimension_1 ();
  int a, b;
  if (alpha == ATA::zero ()) {
    a = 0;
  }
  else if (alpha == -ATA::one ()) {
    a = -1;
  }
  else if (alpha == ATA::one ()) {
    a = 1;
  }
  else {
    a = 2;
  }
  if (beta == ATB::zero ()) {
    b = 0;
  }
  else if (beta == -ATB::one ()) {
    b = -1;
  }
  else if (beta == ATB::one ()) {
    b = 1;
  }
  else {
    b = 2;
  }

  if (numRows < static_cast<size_type> (INT_MAX) &&
      numRows * numCols < static_cast<size_type> (INT_MAX)) {
    typedef int index_type;
    MV_Axpby_Invoke_Left<RMV, XMV::non_const_value_type, XMV,
      YMV::non_const_value_type, YMV, index_type> (R, alpha, X,
                                                   beta, Y, a, b);
  }
  else {
    typedef XMV::size_type index_type;
    MV_Axpby_Invoke_Left<RMV, XMV::non_const_value_type, XMV,
      YMV::non_const_value_type, YMV, index_type> (R, alpha, X,
                                                   beta, Y, a, b);
  }
}

#undef KOKKOSBLAS_IMPL_MV_EXEC_SPACE
#undef KOKKOSBLAS_IMPL_MV_MEM_SPACE
#endif // KOKKOS_HAVE_CUDA


#ifdef KOKKOS_HAVE_CUDA
#define KOKKOSBLAS_IMPL_MV_EXEC_SPACE Kokkos::Cuda
#define KOKKOSBLAS_IMPL_MV_MEM_SPACE Kokkos::CudaUVMSpace

void
Axpby<Kokkos::View<double**,
                   Kokkos::LayoutLeft,
                   Kokkos::Device<KOKKOSBLAS_IMPL_MV_EXEC_SPACE, KOKKOSBLAS_IMPL_MV_MEM_SPACE>,
                   Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                   Kokkos::Impl::ViewDefault>,
      double,
      Kokkos::View<const double**,
                   Kokkos::LayoutLeft,
                   Kokkos::Device<KOKKOSBLAS_IMPL_MV_EXEC_SPACE, KOKKOSBLAS_IMPL_MV_MEM_SPACE>,
                   Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                   Kokkos::Impl::ViewDefault>,
      double,
      Kokkos::View<const double**,
                   Kokkos::LayoutLeft,
                   Kokkos::Device<KOKKOSBLAS_IMPL_MV_EXEC_SPACE, KOKKOSBLAS_IMPL_MV_MEM_SPACE>,
                   Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                   Kokkos::Impl::ViewDefault>,
      2>::
axpby (const RMV& R, const XMV::non_const_value_type& alpha,
       const XMV& X, const YMV::non_const_value_type& beta,
       const YMV& Y)
{
  const size_type numRows = X.dimension_0 ();
  const size_type numCols = X.dimension_1 ();
  int a, b;
  if (alpha == ATA::zero ()) {
    a = 0;
  }
  else if (alpha == -ATA::one ()) {
    a = -1;
  }
  else if (alpha == ATA::one ()) {
    a = 1;
  }
  else {
    a = 2;
  }
  if (beta == ATB::zero ()) {
    b = 0;
  }
  else if (beta == -ATB::one ()) {
    b = -1;
  }
  else if (beta == ATB::one ()) {
    b = 1;
  }
  else {
    b = 2;
  }

  if (numRows < static_cast<size_type> (INT_MAX) &&
      numRows * numCols < static_cast<size_type> (INT_MAX)) {
    typedef int index_type;
    MV_Axpby_Invoke_Left<RMV, XMV::non_const_value_type, XMV,
      YMV::non_const_value_type, YMV, index_type> (R, alpha, X,
                                                   beta, Y, a, b);
  }
  else {
    typedef XMV::size_type index_type;
    MV_Axpby_Invoke_Left<RMV, XMV::non_const_value_type, XMV,
      YMV::non_const_value_type, YMV, index_type> (R, alpha, X,
                                                   beta, Y, a, b);
  }
}

#undef KOKKOSBLAS_IMPL_MV_EXEC_SPACE
#undef KOKKOSBLAS_IMPL_MV_MEM_SPACE
#endif // KOKKOS_HAVE_CUDA

} // namespace Impl
} // namespace KokkosBlas

