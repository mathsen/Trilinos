// @HEADER
// ************************************************************************
//
//               Rapid Optimization Library (ROL) Package
//                 Copyright (2014) Sandia Corporation
//
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
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
// Questions? Contact lead developers:
//              Drew Kouri   (dpkouri@sandia.gov) and
//              Denis Ridzal (dridzal@sandia.gov)
//
// ************************************************************************
// @HEADER

#ifndef ROL_ALGORITHM_G_DEF_H
#define ROL_ALGORITHM_G_DEF_H

#include "ROL_SlacklessObjective.hpp"
#include "ROL_ConstraintManager.hpp"
#include "ROL_ReduceLinearConstraint.hpp"
#include "ROL_ConstraintStatusTest.hpp"

namespace ROL {

template<typename Real>
Algorithm_G<Real>::Algorithm_G( void )
  : status_(makePtr<CombinedStatusTest<Real>>()),
    state_(makePtr<AlgorithmState_G<Real>>()),
    proj_(nullPtr) {
  status_->reset();
  status_->add(makePtr<ConstraintStatusTest<Real>>());
}

template<typename Real>
void Algorithm_G<Real>::initialize(const Vector<Real> &x,
                                   const Vector<Real> &g,
                                   const Vector<Real> &mul,
                                   const Vector<Real> &c) {
  if (state_->iterateVec == nullPtr) {
    state_->iterateVec = x.clone();
  }
  state_->iterateVec->set(x);
  if (state_->lagmultVec == nullPtr) {
    state_->lagmultVec = mul.clone();
  }
  state_->lagmultVec->set(mul);
  if (state_->stepVec == nullPtr) {
    state_->stepVec = x.clone();
  }
  state_->stepVec->zero();
  if (state_->gradientVec == nullPtr) {
    state_->gradientVec = g.clone();
  }
  if (state_->constraintVec == nullPtr) {
    state_->constraintVec = c.clone();
  }
  state_->constraintVec->zero();
  state_->gradientVec->set(g);
  if (state_->minIterVec == nullPtr) {
    state_->minIterVec = x.clone();
  }
  state_->minIterVec->set(x);
  state_->minIter = state_->iter;
  state_->minValue = state_->value;
}

template<typename Real>
void Algorithm_G<Real>::setStatusTest(const Ptr<StatusTest<Real>> &status,
                                      const bool combineStatus) {
  if (!combineStatus) { // Do not combine status tests
    status_->reset();
  }
  status_->add(status); // Add user-defined StatusTest
}

template<typename Real>
std::vector<std::string> Algorithm_G<Real>::run( Vector<Real>          &x,
                                                 Objective<Real>       &obj,
                                                 BoundConstraint<Real> &bnd,
                                                 Constraint<Real>      &econ,
                                                 Vector<Real>          &emul,
                                                 std::ostream          &outStream ) {
  return run(x,x.dual(),obj,bnd,econ,emul,emul.dual(),outStream);
}

template<typename Real>
std::vector<std::string> Algorithm_G<Real>::run( Vector<Real>          &x,
                                                 Objective<Real>       &obj,
                                                 Constraint<Real>      &icon,
                                                 Vector<Real>          &imul,
                                                 BoundConstraint<Real> &ibnd,
                                                 std::ostream          &outStream ) {
  return run(x,x.dual(),obj,icon,imul,ibnd,imul.dual(),outStream);
}

template<typename Real>
std::vector<std::string> Algorithm_G<Real>::run( Vector<Real>          &x,
                                                 Objective<Real>       &obj,
                                                 BoundConstraint<Real> &bnd,
                                                 Constraint<Real>      &icon,
                                                 Vector<Real>          &imul,
                                                 BoundConstraint<Real> &ibnd,
                                                 std::ostream          &outStream ) {
  return run(x,x.dual(),obj,bnd,icon,imul,ibnd,imul.dual(),outStream);
}

template<typename Real>
std::vector<std::string> Algorithm_G<Real>::run( Vector<Real>          &x,
                                                 Objective<Real>       &obj,
                                                 Constraint<Real>      &econ,
                                                 Vector<Real>          &emul,
                                                 Constraint<Real>      &icon,
                                                 Vector<Real>          &imul,
                                                 BoundConstraint<Real> &ibnd,
                                                 std::ostream          &outStream ) {
  return run(x,x.dual(),obj,econ,emul,emul.dual(),icon,imul,ibnd,imul.dual(),outStream);
}

template<typename Real>
std::vector<std::string> Algorithm_G<Real>::run( Vector<Real>          &x,
                                                 Objective<Real>       &obj,
                                                 BoundConstraint<Real> &bnd,
                                                 Constraint<Real>      &econ,
                                                 Vector<Real>          &emul,
                                                 Constraint<Real>      &icon,
                                                 Vector<Real>          &imul,
                                                 BoundConstraint<Real> &ibnd,
                                                 std::ostream          &outStream ) {
  return run(x,x.dual(),obj,bnd,econ,emul,emul.dual(),icon,imul,ibnd,imul.dual(),outStream);
}



template<typename Real>
std::vector<std::string> Algorithm_G<Real>::run( Vector<Real>          &x,
                                                 const Vector<Real>    &g,
                                                 Objective<Real>       &obj,
                                                 Constraint<Real>      &icon,
                                                 Vector<Real>          &imul,
                                                 BoundConstraint<Real> &ibnd,
                                                 const Vector<Real>    &ires,
                                                 std::ostream          &outStream ) {
  ConstraintManager<Real> cm(makePtrFromRef(icon),makePtrFromRef(imul),
                             makePtrFromRef(ibnd),makePtrFromRef(x));
  Ptr<Constraint<Real>>      econ = cm.getConstraint();
  Ptr<Vector<Real>>          emul = cm.getMultiplier();
  Ptr<BoundConstraint<Real>> xbnd = cm.getBoundConstraint();
  Ptr<Vector<Real>>          xvec = cm.getOptVector();
  Ptr<Objective<Real>>       sobj = makePtr<SlacklessObjective<Real>>(makePtrFromRef(obj));
  Ptr<Vector<Real>>         xdual = xvec->dual().clone();
  return run(*xvec,*xdual,*sobj,*xbnd,*econ,*emul,emul->dual(),outStream);
}

template<typename Real>
std::vector<std::string> Algorithm_G<Real>::run( Vector<Real>          &x,
                                                 const Vector<Real>    &g,
                                                 Objective<Real>       &obj,
                                                 BoundConstraint<Real> &bnd,
                                                 Constraint<Real>      &icon,
                                                 Vector<Real>          &imul,
                                                 BoundConstraint<Real> &ibnd,
                                                 const Vector<Real>    &ires,
                                                 std::ostream          &outStream ) {
  ConstraintManager<Real> cm(makePtrFromRef(icon),makePtrFromRef(imul),
                             makePtrFromRef(ibnd),makePtrFromRef(x),
                             makePtrFromRef(bnd));
  Ptr<Constraint<Real>>      econ = cm.getConstraint();
  Ptr<Vector<Real>>          emul = cm.getMultiplier();
  Ptr<BoundConstraint<Real>> xbnd = cm.getBoundConstraint();
  Ptr<Vector<Real>>          xvec = cm.getOptVector();
  Ptr<Objective<Real>>       sobj = makePtr<SlacklessObjective<Real>>(makePtrFromRef(obj));
  Ptr<Vector<Real>>         xdual = xvec->dual().clone();
  return run(*xvec,*xdual,*sobj,*xbnd,*econ,*emul,emul->dual(),outStream);
}

template<typename Real>
std::vector<std::string> Algorithm_G<Real>::run( Vector<Real>          &x,
                                                 const Vector<Real>    &g,
                                                 Objective<Real>       &obj,
                                                 Constraint<Real>      &econ,
                                                 Vector<Real>          &emul,
                                                 const Vector<Real>    &eres,
                                                 Constraint<Real>      &icon,
                                                 Vector<Real>          &imul,
                                                 BoundConstraint<Real> &ibnd,
                                                 const Vector<Real>    &ires,
                                                 std::ostream          &outStream ) {
  std::vector<Ptr<Constraint<Real>>>
    cvec = {makePtrFromRef(econ), makePtrFromRef(icon)};
  std::vector<Ptr<Vector<Real>>>
    lvec = {makePtrFromRef(emul), makePtrFromRef(imul)};
  std::vector<Ptr<BoundConstraint<Real>>>
    bvec = {             nullPtr, makePtrFromRef(ibnd)};
  ConstraintManager<Real> cm(cvec,lvec,bvec,makePtrFromRef(x));
  Ptr<Constraint<Real>>       con = cm.getConstraint();
  Ptr<Vector<Real>>           mul = cm.getMultiplier();
  Ptr<BoundConstraint<Real>> xbnd = cm.getBoundConstraint();
  Ptr<Vector<Real>>          xvec = cm.getOptVector();
  Ptr<Objective<Real>>       sobj = makePtr<SlacklessObjective<Real>>(makePtrFromRef(obj));
  Ptr<Vector<Real>>         xdual = xvec->dual().clone();
  return run(*xvec,*xdual,*sobj,*xbnd,*con,*mul,mul->dual(),outStream);
}

template<typename Real>
std::vector<std::string> Algorithm_G<Real>::run( Vector<Real>          &x,
                                                 const Vector<Real>    &g,
                                                 Objective<Real>       &obj,
                                                 BoundConstraint<Real> &bnd,
                                                 Constraint<Real>      &econ,
                                                 Vector<Real>          &emul,
                                                 const Vector<Real>    &eres,
                                                 Constraint<Real>      &icon,
                                                 Vector<Real>          &imul,
                                                 BoundConstraint<Real> &ibnd,
                                                 const Vector<Real>    &ires,
                                                 std::ostream          &outStream ) {
  std::vector<Ptr<Constraint<Real>>>
    cvec = {makePtrFromRef(econ), makePtrFromRef(icon)};
  std::vector<Ptr<Vector<Real>>>
    lvec = {makePtrFromRef(emul), makePtrFromRef(imul)};
  std::vector<Ptr<BoundConstraint<Real>>>
    bvec = {             nullPtr, makePtrFromRef(ibnd)};
  ConstraintManager<Real> cm(cvec,lvec,bvec,makePtrFromRef(x),makePtrFromRef(bnd));
  Ptr<Constraint<Real>>       con = cm.getConstraint();
  Ptr<Vector<Real>>           mul = cm.getMultiplier();
  Ptr<BoundConstraint<Real>> xbnd = cm.getBoundConstraint();
  Ptr<Vector<Real>>          xvec = cm.getOptVector();
  Ptr<Objective<Real>>       sobj = makePtr<SlacklessObjective<Real>>(makePtrFromRef(obj));
  Ptr<Vector<Real>>         xdual = xvec->dual().clone();
  return run(*xvec,*xdual,*sobj,*xbnd,*con,*mul,mul->dual(),outStream);
}



//template<typename Real>
//std::vector<std::string> Algorithm_G<Real>::run( Vector<Real>          &x,
//                                                 Objective<Real>       &obj,
//                                                 BoundConstraint<Real> &bnd,
//                                                 Constraint<Real>      &econ,
//                                                 Vector<Real>          &emul,
//                                                 Constraint<Real>      &linear_econ,
//                                                 Vector<Real>          &linear_emul,
//                                                 std::ostream          &outStream ) {
//  return run(x,x.dual(),obj,bnd,econ,emul,emul.dual(),
//             linear_econ,linear_emul,linear_emul.dual(),outStream);
//}

template<typename Real>
std::vector<std::string> Algorithm_G<Real>::run( Vector<Real>          &x,
                                                 Objective<Real>       &obj,
                                                 Constraint<Real>      &icon,
                                                 Vector<Real>          &imul,
                                                 BoundConstraint<Real> &ibnd,
                                                 Constraint<Real>      &linear_econ,
                                                 Vector<Real>          &linear_emul,
                                                 std::ostream          &outStream ) {
  return run(x,x.dual(),obj,icon,imul,ibnd,imul.dual(),
             linear_econ,linear_emul,linear_emul.dual(),outStream);
}

//template<typename Real>
//std::vector<std::string> Algorithm_G<Real>::run( Vector<Real>          &x,
//                                                 Objective<Real>       &obj,
//                                                 BoundConstraint<Real> &bnd,
//                                                 Constraint<Real>      &icon,
//                                                 Vector<Real>          &imul,
//                                                 BoundConstraint<Real> &ibnd,
//                                                 Constraint<Real>      &linear_econ,
//                                                 Vector<Real>          &linear_emul,
//                                                 std::ostream          &outStream ) {
//  return run(x,x.dual(),obj,bnd,icon,imul,ibnd,imul.dual(),
//             linear_econ,linear_emul,linear_emul.dual(),outStream);
//}

template<typename Real>
std::vector<std::string> Algorithm_G<Real>::run( Vector<Real>          &x,
                                                 Objective<Real>       &obj,
                                                 Constraint<Real>      &econ,
                                                 Vector<Real>          &emul,
                                                 Constraint<Real>      &icon,
                                                 Vector<Real>          &imul,
                                                 BoundConstraint<Real> &ibnd,
                                                 Constraint<Real>      &linear_econ,
                                                 Vector<Real>          &linear_emul,
                                                 std::ostream          &outStream ) {
  return run(x,x.dual(),obj,econ,emul,emul.dual(),icon,imul,ibnd,imul.dual(),
             linear_econ,linear_emul,linear_emul.dual(),outStream);
}

//template<typename Real>
//std::vector<std::string> Algorithm_G<Real>::run( Vector<Real>          &x,
//                                                 Objective<Real>       &obj,
//                                                 BoundConstraint<Real> &bnd,
//                                                 Constraint<Real>      &econ,
//                                                 Vector<Real>          &emul,
//                                                 Constraint<Real>      &icon,
//                                                 Vector<Real>          &imul,
//                                                 BoundConstraint<Real> &ibnd,
//                                                 Constraint<Real>      &linear_econ,
//                                                 Vector<Real>          &linear_emul,
//                                                 std::ostream          &outStream ) {
//  return run(x,x.dual(),obj,bnd,econ,emul,emul.dual(),icon,imul,ibnd,imul.dual(),
//             linear_econ,linear_emul,linear_emul.dual(),outStream);
//}



//template<typename Real>
//std::vector<std::string> Algorithm_G<Real>::run( Vector<Real>          &x,
//                                                 const Vector<Real>    &g,
//                                                 Objective<Real>       &obj,
//                                                 BoundConstraint<Real> &bnd,
//                                                 Constraint<Real>      &econ,
//                                                 Vector<Real>          &emul,
//                                                 const Vector<Real>    &eres,
//                                                 Constraint<Real>      &linear_econ,
//                                                 Vector<Real>          &linear_emul,
//                                                 const Vector<Real>    &linear_eres,
//                                                 std::ostream          &outStream ) {
//  Ptr<Vector<Real>> xfeas = x.clone(); xfeas->set(x);
//  ReduceLinearConstraint<Real> rlc(makePtrFromRef(linear_econ),xfeas,makePtrFromRef(linear_eres));
//  Ptr<Vector<Real>> s = x.clone(); s->zero();
//  std::vector<std::string> output = run(*s,g,*rlc.transform(makePtrFromRef(obj)),bnd,
//                                        *rlc.transform(makePtrFromRef(econ)),emul,eres,outStream);
//  rlc.project(x,*s);
//  x.plus(*rlc.getFeasibleVector());
//  return output;
//}

template<typename Real>
std::vector<std::string> Algorithm_G<Real>::run( Vector<Real>          &x,
                                                 const Vector<Real>    &g,
                                                 Objective<Real>       &obj,
                                                 Constraint<Real>      &icon,
                                                 Vector<Real>          &imul,
                                                 BoundConstraint<Real> &ibnd,
                                                 const Vector<Real>    &ires,
                                                 Constraint<Real>      &linear_econ,
                                                 Vector<Real>          &linear_emul,
                                                 const Vector<Real>    &linear_eres,
                                                 std::ostream          &outStream ) {
  Ptr<Vector<Real>> xfeas = x.clone(); xfeas->set(x);
  ReduceLinearConstraint<Real> rlc(makePtrFromRef(linear_econ),xfeas,makePtrFromRef(linear_eres));
  Ptr<Vector<Real>> s = x.clone(); s->zero();
  std::vector<std::string> output = run(*s,g,*rlc.transform(makePtrFromRef(obj)),
                                        *rlc.transform(makePtrFromRef(icon)),imul,ibnd,ires,outStream);
  rlc.project(x,*s);
  x.plus(*rlc.getFeasibleVector());
  return output;
}

//template<typename Real>
//std::vector<std::string> Algorithm_G<Real>::run( Vector<Real>          &x,
//                                                 const Vector<Real>    &g,
//                                                 Objective<Real>       &obj,
//                                                 BoundConstraint<Real> &bnd,
//                                                 Constraint<Real>      &icon,
//                                                 Vector<Real>          &imul,
//                                                 BoundConstraint<Real> &ibnd,
//                                                 const Vector<Real>    &ires,
//                                                 Constraint<Real>      &linear_econ,
//                                                 Vector<Real>          &linear_emul,
//                                                 const Vector<Real>    &linear_eres,
//                                                 std::ostream          &outStream ) {
//  Ptr<Vector<Real>> xfeas = x.clone(); xfeas->set(x);
//  ReduceLinearConstraint<Real> rlc(makePtrFromRef(linear_econ),xfeas,makePtrFromRef(linear_eres));
//  Ptr<Vector<Real>> s = x.clone(); s->zero();
//  std::vector<std::string> output = run(*s,g,*rlc.transform(makePtrFromRef(obj)),bnd,
//                                        *rlc.transform(makePtrFromRef(icon)),imul,ibnd,ires,outStream);
//  rlc.project(x,*s);
//  x.plus(*rlc.getFeasibleVector());
//  return output;
//}

template<typename Real>
std::vector<std::string> Algorithm_G<Real>::run( Vector<Real>          &x,
                                                 const Vector<Real>    &g,
                                                 Objective<Real>       &obj,
                                                 Constraint<Real>      &econ,
                                                 Vector<Real>          &emul,
                                                 const Vector<Real>    &eres,
                                                 Constraint<Real>      &icon,
                                                 Vector<Real>          &imul,
                                                 BoundConstraint<Real> &ibnd,
                                                 const Vector<Real>    &ires,
                                                 Constraint<Real>      &linear_econ,
                                                 Vector<Real>          &linear_emul,
                                                 const Vector<Real>    &linear_eres,
                                                 std::ostream          &outStream ) {
  Ptr<Vector<Real>> xfeas = x.clone(); xfeas->set(x);
  ReduceLinearConstraint<Real> rlc(makePtrFromRef(linear_econ),xfeas,makePtrFromRef(linear_eres));
  Ptr<Vector<Real>> s = x.clone(); s->zero();
  std::vector<std::string> output = run(*s,g,*rlc.transform(makePtrFromRef(obj)),
                                        *rlc.transform(makePtrFromRef(econ)),emul,eres,
                                        *rlc.transform(makePtrFromRef(icon)),imul,ibnd,ires,outStream);
  rlc.project(x,*s);
  x.plus(*rlc.getFeasibleVector());
  return output;
}

//template<typename Real>
//std::vector<std::string> Algorithm_G<Real>::run( Vector<Real>          &x,
//                                                 const Vector<Real>    &g,
//                                                 Objective<Real>       &obj,
//                                                 BoundConstraint<Real> &bnd,
//                                                 Constraint<Real>      &econ,
//                                                 Vector<Real>          &emul,
//                                                 const Vector<Real>    &eres,
//                                                 Constraint<Real>      &icon,
//                                                 Vector<Real>          &imul,
//                                                 BoundConstraint<Real> &ibnd,
//                                                 const Vector<Real>    &ires,
//                                                 Constraint<Real>      &linear_econ,
//                                                 Vector<Real>          &linear_emul,
//                                                 const Vector<Real>    &linear_eres,
//                                                 std::ostream          &outStream ) {
//  Ptr<Vector<Real>> xfeas = x.clone(); xfeas->set(x);
//  ReduceLinearConstraint<Real> rlc(makePtrFromRef(linear_econ),xfeas,makePtrFromRef(linear_eres));
//  Ptr<Vector<Real>> s = x.clone(); s->zero();
//  std::vector<std::string> output = run(*s,g,*rlc.transform(makePtrFromRef(obj)),bnd,
//                                        *rlc.transform(makePtrFromRef(econ)),emul,eres,
//                                        *rlc.transform(makePtrFromRef(icon)),imul,ibnd,ires,outStream);
//  rlc.project(x,*s);
//  x.plus(*rlc.getFeasibleVector());
//  return output;
//}


template<typename Real>
std::string Algorithm_G<Real>::printHeader( void ) const {
  std::stringstream hist;
  hist << "  ";
  hist << std::setw(6)  << std::left << "iter";
  hist << std::setw(15) << std::left << "value";
  hist << std::setw(15) << std::left << "gnorm";
  hist << std::setw(15) << std::left << "snorm";
  hist << std::setw(10) << std::left << "#fval";
  hist << std::setw(10) << std::left << "#grad";
  hist << std::endl;
  return hist.str();
}

template<typename Real>
std::string Algorithm_G<Real>::printName( void ) const {
  throw Exception::NotImplemented(">>> ROL::Algorithm_U::printName() is not implemented!");
}

template<typename Real>
std::string Algorithm_G<Real>::print( const bool print_header ) const {
  std::stringstream hist;
  hist << std::scientific << std::setprecision(6);
  if ( print_header ) {
    hist << printHeader();
  }
  if ( state_->iter == 0 ) {
    hist << "  ";
    hist << std::setw(6)  << std::left << state_->iter;
    hist << std::setw(15) << std::left << state_->value;
    hist << std::setw(15) << std::left << state_->gnorm;
    hist << std::endl;
  }
  else {
    hist << "  "; 
    hist << std::setw(6)  << std::left << state_->iter;  
    hist << std::setw(15) << std::left << state_->value; 
    hist << std::setw(15) << std::left << state_->gnorm; 
    hist << std::setw(15) << std::left << state_->snorm; 
    hist << std::setw(10) << std::left << state_->nfval;              
    hist << std::setw(10) << std::left << state_->ngrad;              
    hist << std::endl;
  }
  return hist.str();
}

template<typename Real>
std::string Algorithm_G<Real>::printExitStatus( void ) const {
  std::stringstream hist;
  hist << "Optimization Terminated with Status: ";
  hist << EExitStatusToString(state_->statusFlag);
  hist << std::endl;
  return hist.str();
}

template<typename Real>
Ptr<const AlgorithmState<Real>> Algorithm_G<Real>::getState(void) const {
  return state_;
}

template<typename Real>
void Algorithm_G<Real>::reset(void) {
  state_->reset();
}

} // namespace ROL

#endif
