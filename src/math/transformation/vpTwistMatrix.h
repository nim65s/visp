/****************************************************************************
 *
 * $Id: vpTwistMatrix.h,v 1.3 2006-05-30 08:40:44 fspindle Exp $
 *
 * Copyright (C) 1998-2006 Inria. All rights reserved.
 *
 * This software was developed at:
 * IRISA/INRIA Rennes
 * Projet Lagadic
 * Campus Universitaire de Beaulieu
 * 35042 Rennes Cedex
 * http://www.irisa.fr/lagadic
 *
 * This file is part of the ViSP toolkit.
 *
 * This file may be distributed under the terms of the Q Public License
 * as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE included in the packaging of this file.
 *
 * Licensees holding valid ViSP Professional Edition licenses may
 * use this file in accordance with the ViSP Commercial License
 * Agreement provided with the Software.
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Contact visp@irisa.fr if any conditions of this licensing are
 * not clear to you.
 *
 * Description:
 * Twist transformation matrix.
 *
 * Authors:
 * Eric Marchand
 *
 *****************************************************************************/

#ifndef vpTWISTMATRIX_HH
#define vpTWISTMATRIX_HH

#include <visp/vpConfig.h>
#include <visp/vpMatrix.h>
#include <visp/vpColVector.h>
#include <visp/vpHomogeneousMatrix.h>
#include <visp/vpRotationMatrix.h>


class VISP_EXPORT vpTwistMatrix : public vpMatrix
{
    friend class vpMatrix;

public:
    //! Basic initialisation (identity)
    void init() ;

    //! Basic initialisation (identity)
    void setIdentity() ;
    //! basic constructor
    vpTwistMatrix()   ;
    //! basic constructor
    vpTwistMatrix(const vpTwistMatrix &M) ;
    //! copy constructor
    vpTwistMatrix(const vpHomogeneousMatrix &M) ;

    vpTwistMatrix operator*(const vpTwistMatrix &mat) const ;

    vpColVector operator*(const vpColVector &v) const ;

    //! Construction from Translation and rotation (ThetaU parameterization)
    vpTwistMatrix(const vpTranslationVector &T, const vpThetaUVector &R) ;
    //! Construction from Translation and rotation (euler parameterization)
    vpTwistMatrix(const vpTranslationVector &T, const vpEulerVector &R) ;
    //! Construction from Translation and rotation (matrix parameterization)
    vpTwistMatrix(const vpTranslationVector &T, const vpRotationMatrix &R) ;
    vpTwistMatrix(const double tux,  const double tuy,  const double tuz,
		  const double Tx,   const double Ty,   const double Tz) ;

   //! Assigment
    vpTwistMatrix &operator<<(const vpTwistMatrix &m);

    //! copy operator from vpMatrix (handle with care)
    //  vpTwistMatrix &operator=(const vpMatrix &m);
    //! copy operator from vpMatrix (handle with care)
    vpTwistMatrix &operator=(const vpTwistMatrix &m);



    vpTwistMatrix buildFrom(const vpTranslationVector &t,
			    const vpRotationMatrix &R);
    vpTwistMatrix buildFrom(const vpTranslationVector &t,
			    const vpEulerVector &e);
    vpTwistMatrix buildFrom(const vpTranslationVector &t,
			    const vpThetaUVector &e);
    vpTwistMatrix buildFrom(const vpHomogeneousMatrix &M) ;


} ;

#endif

/*
 * Local variables:
 * c-basic-offset: 2
 * End:
 */
