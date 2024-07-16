// @HEADER
// *****************************************************************************
//                           Sacado Package
//
// Copyright 2006 NTESS and the Sacado contributors.
// SPDX-License-Identifier: LGPL-2.1-or-later
// *****************************************************************************
// @HEADER

#ifndef SACADO_ELRCACHEFAD_DFAD_HPP
#define SACADO_ELRCACHEFAD_DFAD_HPP

#include "Sacado_ELRCacheFad_GeneralFadExpr.hpp"
#include "Sacado_ELRCacheFad_DFadTraits.hpp"
#include "Sacado_Fad_DynamicStorage.hpp"

#define FAD_NS ELRCacheFad
#include "Sacado_Fad_DFad_tmpl.hpp"
#undef FAD_NS

#include "Sacado_ELRCacheFad_ViewFad.hpp"

#endif // SACADO_ELRCACHEFAD_DFAD_HPP
