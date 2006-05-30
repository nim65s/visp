/****************************************************************************
 *
 * $Id: vpDisplayX.h,v 1.8 2006-05-30 08:40:47 fspindle Exp $
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
 * Image display.
 *
 * Authors:
 * Fabien Spindler
 *
 *****************************************************************************/

#ifndef vpDisplayX_h
#define vpDisplayX_h

#include <visp/vpConfig.h>
#ifdef VISP_HAVE_X11

//namespace X11name
//{
#include <X11/Xlib.h>
#include <X11/Xutil.h>
//#include <X11/Xatom.h>
//#include <X11/cursorfont.h>
//} ;

//using namespace X11name ;

#include <visp/vpImage.h>
#include <visp/vpDisplay.h>



/*!
  \file vpDisplayX.h
  \brief Define the X11 console to display images
*/


/*!
  \class vpDisplayX
  \brief Define the X11 console to display images2323

  \date June 1999

  \author  Fabien Spindler, Irisa / Inria Rennes

  \note Ready to use with a Unix System (tested for Linux and SunOS)

  This class define the X11 console to display  images
  It also define method to display some geometric feature (point, line, circle)
  in the image.
*/

class VISP_EXPORT vpDisplayX: public vpDisplay
{
private:
  // true if X11 display is ready to use
  bool Xinitialise ;


  int num_Xdisplay ;
  Display 	*display ;
  Window   	window ;
  XImage   	*Ximage ;
  Colormap	lut ;
  GC		context ;
  int      	screen ;
  int		planes;
  XEvent	event;
  Pixmap	pixmap;
  unsigned char *bitmap_bis ;
  unsigned long	x_color[vpColor::none];
  int screen_depth  ;
  unsigned short  colortable[256];
  XColor        color;
  XGCValues     values;
  int size ;
  bool ximage_data_init;


protected:
  void setWindowPosition(int /*_winx*/, int /*_winy*/) { ; }
  inline  int getRows() { return nrows ; }
  inline  int getCols() { return ncols ; }

public:


  void init(vpImage<unsigned char> &I,
	    int winx=-1, int winy=-1,
	    char *_title=NULL)  ;
  void init(vpImage<vpRGBa> &I,
	   int winx=-1, int winy=-1,
	   char *_title=NULL)  ;

  void init(int cols, int rows,
	   int winx=-1, int winy=-1 ,
	   char *_title=NULL) ;
  // only the constructor/destructor are public
public:
  vpDisplayX(vpImage<unsigned char> &I, int _winx=-1, int _winy=-1, char *title=NULL) ;
  vpDisplayX(vpImage<vpRGBa> &I, int _winx=-1, int _winy=-1, char *title=NULL) ;

  vpDisplayX(int _winx, int _winy, char *title=NULL) ;

  vpDisplayX() ;
  ~vpDisplayX() ;

protected:
  void displayImage(vpImage<vpRGBa> &I) ;
  void displayImage(vpImage<unsigned char> &I) ;
  void displayImage(unsigned char *I) ;

  void closeDisplay() ;
  void flushDisplay() ;
  void flushTitle(const char *string) ;

  void clearDisplay(int c=vpColor::white) ;

  void displayPoint(int x,int y,int col) ;
  void displayCross(int x,int y, int size,int col) ;
  void displayCrossLarge(int x,int y, int size,int col) ;
  void displayCircle(int i, int j, int r, int c);
  void displayLine(int x1, int y1, int x2, int y2, int col, int e=1) ;
  void displayDotLine(int x1, int y1, int x2, int y2, int col, int e=1) ;


  void displayArrow(int i1,int j1, int i2, int j2,
		    int col=1, int L=4,int l=2) ;

  void displayRectangle(int i, int j, int width, int height, int col);
  void displayCharString(int i,int j,char *s, int c=vpColor::green) ;

  bool  getClick(int& i, int& j) ;
  bool  getClick(int& i, int& j, int& button) ;
  void  getClick() ;
  bool  getClickUp(int& i, int& j, int& button) ;

public:

  int  getScreenDepth();
  void getScreenSize(int *xsize, int *ysize);
  void getImage(vpImage<vpRGBa> &I) ;
} ;

#endif
#endif
