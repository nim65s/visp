/*
 * ViSP, open source Visual Servoing Platform software.
 * Copyright (C) 2005 - 2025 by Inria. All rights reserved.
 *
 * This software is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file LICENSE.txt at the root directory of this source
 * distribution for additional information about the GNU GPL.
 *
 * For using ViSP with software that can not be combined with the GNU
 * GPL, please contact Inria about acquiring a ViSP Professional
 * Edition License.
 *
 * See https://visp.inria.fr for more information.
 *
 * This software was developed at:
 * Inria Rennes - Bretagne Atlantique
 * Campus Universitaire de Beaulieu
 * 35042 Rennes Cedex
 * France
 *
 * If you have questions regarding the use of this file, please contact
 * Inria at visp@inria.fr
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Description:
 * Median Absolute Deviation (MAD), MPDE, Mean shift kernel density
 * estimation.
 */

/*!
 \file vpScale.h
*/

// =========================================================
/*!
 * \brief Contains various estimators for scale.
 * \n Methods : Median Absolute Deviation (MAD),
 *              MPDE, Mean shift kernel
 * density estimation. \author Andrew Comport \date 24/10/03
 */
// ==========================================================

#ifndef VP_SCALE_H
#define VP_SCALE_H

#include <math.h>
#include <visp3/core/vpColVector.h>

BEGIN_VISP_NAMESPACE
/*!
  \class vpScale
  \ingroup group_core_robust

  \brief Contains different methods for estimating the robust scale of
  an error distribution.

  Methods of Median Absolute Deviation and Density Gradient estimation
  using the Mean Shift method.

  \author Andrew Comport
  \date 24/10/03
*/
class VISP_EXPORT vpScale
{

private:
  double bandwidth;
  unsigned int dimension;

public:
  //! Constructor
  vpScale();
  vpScale(double kernel_bandwidth, unsigned int dim = 1);
#if (VISP_CXX_STANDARD > VISP_CXX_STANDARD_98)
  vpScale(const vpScale &) = default;
  virtual ~vpScale() = default;
#else
  virtual ~vpScale() { }
#endif

  double MeanShift(vpColVector &error);
  double KernelDensity(vpColVector &error, unsigned int position);
  double KernelDensityGradient(vpColVector &error, unsigned int position);

  double KernelDensity_EPANECHNIKOV(vpColVector &X);
  double KernelDensityGradient_EPANECHNIKOV(double X, unsigned int n);

#if (VISP_CXX_STANDARD > VISP_CXX_STANDARD_98)
  vpScale &operator=(const vpScale &) = default;
#endif
};
END_VISP_NAMESPACE
#endif
