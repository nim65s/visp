/****************************************************************************
 *
 * $Id: vpMeLine.h,v 1.2 2006-05-30 08:40:47 fspindle Exp $
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
 * Moving edges.
 *
 * Authors:
 * Eric Marchand
 *
 *****************************************************************************/

#ifndef vpMeLine_HH
#define vpMeLine_HH


#include <math.h>
#include <iostream>


#include <visp/vpConfig.h>
#include <visp/vpMath.h>
#include <visp/vpMatrix.h>
#include <visp/vpMeTracker.h>



class VISP_EXPORT vpMeLine : public vpMeTracker
{
private:
  vpMeSite PExt[2] ;

  double rho, theta ;
  double delta ;

public:
  double a, b, c ; // ai + bj + c = 0


public:
  vpMeLine() ;
  ~vpMeLine() ;

  void display(vpImage<unsigned char>& I,  int col) ;

  void track(vpImage<unsigned char>& Im);

  void sample(vpImage<unsigned char>&image);
  void reSample(vpImage<unsigned char> &I) ;
  void leastSquare(vpImage<unsigned char> &I) ;
  void updateDelta();
  void setExtremities() ;
  void seekExtremities(vpImage<unsigned char> &I) ;
  void suppressPoints() ;

  void initTracking(vpImage<unsigned char> &I) ;
  void initTracking(vpImage<unsigned char> &I, int i1,int j1, int i2, int j2) ;

  void computeRhoTheta(vpImage<unsigned char> &I) ;
  double getRho() const ;
  double getTheta() const ;
};




#endif


