/****************************************************************************
 *
 * $Id: vpPlane.cpp,v 1.2 2006-05-30 08:40:46 fspindle Exp $
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
 * Plane geometrical structure.
 *
 * Authors:
 * Eric Marchand
 *
 *****************************************************************************/


/*!
  \file vpPlane.cpp
  \brief definition of the vpPlane class member functions
  \ingroup libtools
*/

/*!
  \class vpPlane

  \brief  definition of the vpPlane class member functions

  \author Eric Marchand  (Eric.Marchand@irisa.fr) Irisa / Inria Rennes

  This class defines the container for a plane geometrical structure

  A plane is given by the equation
  ax + by + cz + d = 0
  where
  (x,y,z) is a point of R^3

*/

#ifndef vpPlane_hh
#include <visp/vpPlane.h>

#define DEBUG_LEVEL1 0


/*!
  \brief copy operator
*/
vpPlane&
vpPlane::operator =(const vpPlane& p)
{

  A = p.A ;
  B = p.B ;
  C = p.C ;
  D = p.D ;

  return *this ;
}

/*!
  \brief Basic Constructor
*/
vpPlane::vpPlane()
{

  setA(0) ;
  setB(0) ;
  setC(0) ;
  setD(0) ;

}

/*!
  \brief Constructor

  \param a parameters of the plane
  \param b
  \param c
  \param d

*/
vpPlane::vpPlane(const double a,const double b,const double c, const double d)
{
  setA(a) ;
  setB(b) ;
  setC(c) ;
  setD(d) ;
}

/*!
  \brief copy constructor
*/
vpPlane::vpPlane(vpPlane& P)
{
  setA(P.getA()) ;
  setB(P.getB()) ;
  setC(P.getC()) ;
  setD(P.getD()) ;

}

vpPlane::vpPlane(vpPoint& P,vpColVector &n)
{
  //Equation of the plane is given by:
  A = n[0];
  B = n[1];
  C = n[2];

  D=-(A*P.get_X()+B*P.get_Y()+C*P.get_Z());
}

void vpPlane::init(vpPlane& P)
{
  setA(P.getA()) ;
  setB(P.getB()) ;
  setC(P.getC()) ;
  setD(P.getD()) ;
}

void vpPlane::init(vpColVector & P,vpColVector &n)
{
  //Equation of the plane is given by:
  A = n[0];
  B = n[1];
  C = n[2];

  D=-(A*P[0]+B*P[1]+C*P[2]);
}

/*!
  \brief Compute the equation of a plane given given three
  point of this point PQR

  The normal to the plane is given by
  n = PQ x PR

*/
void
vpPlane::init(vpPoint &P, vpPoint &Q, vpPoint &R)
{
  vpColVector a(3);
  vpColVector b(3);
  vpColVector n(3);

  //Calculate vector corresponding to PQ
  a[0]=P.get_X()-Q.get_X();
  a[1]=P.get_Y()-Q.get_Y();
  a[2]=P.get_Z()-Q.get_Z();

  //Calculate vector corresponding to PR
  b[0]=P.get_X()-R.get_X();
  b[1]=P.get_Y()-R.get_Y();
  b[2]=P.get_Z()-R.get_Z();

  //Calculate normal vector to plane PQ x PR
  n=vpColVector::cross(a,b);

  //Equation of the plane is given by:
  A = n[0];
  B = n[1];
  C = n[2];
  D=-(A*P.get_X()+B*P.get_Y()+C*P.get_Z());
}


/*!
  \brief constructor :
  Compute the equation of a plane given given three
  point of this point PQR

  The normal to the plane is given by
  n = PQ x PR

  sa init(vpPoint &P, vpPoint &Q, vpPoint &R)
*/
vpPlane::vpPlane(vpPoint &P, vpPoint &Q, vpPoint &R)
{
  init(P,Q,R) ;
}

/*!
  \brief Return the normal to the plan

  plane equation ax+by+cz+d = 0

  normal is given by [a,b,c]

  \sa getNormal(vpColVector &n)
*/
vpColVector  vpPlane::getNormal() const
{
  vpColVector n(3);
  n[0] = A ;
  n[1] = B ;
  n[2] = C ;

  return n ;
}

/*!
  \brief Return the normal to the plane

  plane equation ax+by+cz+d = 0

  normal is given by [a,b,c]

  \sa vpColVector getNormal()

*/
void  vpPlane::getNormal(vpColVector &n) const
{
  n.resize(3) ;
  n[0] = A ;
  n[1] = B ;
  n[2] = C ;
}

vpColVector
vpPlane::abcd()
{
  vpColVector n(4);
  n[0]=A;
  n[1]=B;
  n[2]=C;
  n[3]=D;

  return n;
}

/*!
  \brief Compute the coordinates of the projection of a point on the plane

  \param P : point to be projected on the plane
  \param pproj : result of the projection (pproj belongs to the plane)
*/
void
vpPlane::projectionPointOnPlan(const  vpPoint& P, vpPoint& pproj)
{
  double x0,y0,z0 ;
  double rho ;

  x0 = P.get_X()/P.get_W() ;
  y0 = P.get_Y()/P.get_W() ;
  z0 = P.get_Z()/P.get_W() ;

  rho = - (A*x0+B*y0+C*z0+D)/(A*A+B*B+C*C) ;

  pproj.set_X(x0+A*rho) ;
  pproj.set_Y(y0+B*rho) ;
  pproj.set_Z(z0+C*rho) ;
  pproj.set_W(1) ;

}


double
vpPlane::rayIntersection(const vpPoint &M0,
				const vpPoint &M1,
				vpColVector &H ) const
{

  double k,scal;
  double R[3];

  if(M0.get_X()!=0&&M0.get_Y()!=0&&M0.get_Z()!=0)
  {
    R[0]= M1.get_X() - M0.get_X();
    R[1]= M1.get_Y() - M0.get_Y();
    R[2]= M1.get_Z() - M0.get_Z();

    scal = getA()*R[0] + getB()*R[1] + getC()*R[2];
    if (scal != 0)
      k =  -( getA()*M0.get_X() + getB()*M0.get_Y() + getC()*M0.get_Z() + getD())/scal;
    else
      k = 0;

    H[0] = M0.get_X()+ k*R[0];
    H[1] = M0.get_Y()+ k*R[1];
    H[2] = M0.get_Z()+ k*R[2];
  }
  else
  {
    scal = getA()*M1.get_X() + getB()*M1.get_Y() + getC()*M1.get_Z();
    if (scal != 0)
      k = -getD()/scal;
    else
      k=0;
    H[0] = k*M1.get_X();
    H[1] = k*M1.get_Y();
    H[2] = k*M1.get_Z();
  }

  return k;

}

double vpPlane::getIntersection(const vpColVector &M1,vpColVector &H )const
{

  double k,scal;

  scal = A*M1[0] + B*M1[1] + C*M1[2];
  if (scal != 0)
    k = -getD()/scal;
  else
    k=0;
  H[0] = k*M1[0];
  H[1] = k*M1[1];
  H[2] = k*M1[2];

  return k;

}

#endif

