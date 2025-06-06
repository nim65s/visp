/****************************************************************************
 *
 * ViSP, open source Visual Servoing Platform software.
 * Copyright (C) 2005 - 2023 by Inria. All rights reserved.
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
 * Hybrid tracker based on edges (vpMbt) and points of interests (KLT)
 *
*****************************************************************************/

//#define VP_DEBUG_MODE 1 // Activate debug level 1

#include <visp3/core/vpDebug.h>
#include <visp3/core/vpTrackingException.h>
#include <visp3/core/vpVelocityTwistMatrix.h>
#include <visp3/mbt/vpMbEdgeKltTracker.h>
#include <visp3/mbt/vpMbtXmlGenericParser.h>

#if defined(VISP_HAVE_MODULE_KLT) && defined(VISP_HAVE_OPENCV) && defined(HAVE_OPENCV_IMGPROC) && defined(HAVE_OPENCV_VIDEO)

BEGIN_VISP_NAMESPACE
vpMbEdgeKltTracker::vpMbEdgeKltTracker()
  : m_thresholdKLT(2.), m_thresholdMBT(2.), m_maxIterKlt(30), m_w_mbt(), m_w_klt(), m_error_hybrid(), m_w_hybrid()
{
  computeCovariance = false;

#ifdef VISP_HAVE_OGRE
  faces.getOgreContext()->setWindowName("MBT Hybrid");
#endif

  m_lambda = 0.8;
  m_maxIter = 200;
}

/*!
  Basic constructor

*/
vpMbEdgeKltTracker::~vpMbEdgeKltTracker() { }

/*!
  Initialization of the tracker using a known initial pose.
  The 3D model must first have been loaded.

  \param I : Input image.
*/
void vpMbEdgeKltTracker::init(const vpImage<unsigned char> &I)
{
  vpMbKltTracker::init(I);

  initPyramid(I, Ipyramid);

  vpMbEdgeTracker::resetMovingEdge();

  unsigned int i = static_cast<unsigned int>(scales.size());
  do {
    i--;
    if (scales[i]) {
      downScale(i);
      initMovingEdge(*Ipyramid[i], m_cMo);
      upScale(i);
    }
  } while (i != 0);

  cleanPyramid(Ipyramid);
}

/*!
  Set the pose to be used in entry (as guess) of the next call to the track()
  function. This pose will be just used once.

  \warning This functionnality is not available when tracking cylinders.

  \param I : grayscale image corresponding to the desired pose.
  \param cdMo : Pose to affect.
*/
void vpMbEdgeKltTracker::setPose(const vpImage<unsigned char> &I, const vpHomogeneousMatrix &cdMo)
{
  vpMbKltTracker::setPose(I, cdMo);

  resetMovingEdge();

  if (useScanLine) {
    m_cam.computeFov(I.getWidth(), I.getHeight());
    faces.computeClippedPolygons(m_cMo, m_cam);
    faces.computeScanLineRender(m_cam, I.getWidth(), I.getHeight());
  }

  initPyramid(I, Ipyramid);

  unsigned int i = static_cast<unsigned int>(scales.size());
  do {
    i--;
    if (scales[i]) {
      downScale(i);
      initMovingEdge(*Ipyramid[i], m_cMo);
      upScale(i);
    }
  } while (i != 0);

  cleanPyramid(Ipyramid);
}

/*!
  Set the pose to be used in entry (as guess) of the next call to the track()
  function. This pose will be just used once.

  \warning This functionnality is not available when tracking cylinders.

  \param I_color : image corresponding to the desired pose.
  \param cdMo : Pose to affect.
*/
void vpMbEdgeKltTracker::setPose(const vpImage<vpRGBa> &I_color, const vpHomogeneousMatrix &cdMo)
{
  vpImageConvert::convert(I_color, m_I);
  vpMbKltTracker::setPose(m_I, cdMo);

  resetMovingEdge();

  if (useScanLine) {
    m_cam.computeFov(m_I.getWidth(), m_I.getHeight());
    faces.computeClippedPolygons(m_cMo, m_cam);
    faces.computeScanLineRender(m_cam, m_I.getWidth(), m_I.getHeight());
  }

  initPyramid(m_I, Ipyramid);

  unsigned int i = static_cast<unsigned int>(scales.size());
  do {
    i--;
    if (scales[i]) {
      downScale(i);
      initMovingEdge(*Ipyramid[i], m_cMo);
      upScale(i);
    }
  } while (i != 0);

  cleanPyramid(Ipyramid);
}

/*!
  Reset the tracker. The model is removed and the pose is set to identity.
  The tracker needs to be initialized with a new model and a new pose.
*/
void vpMbEdgeKltTracker::resetTracker()
{
  vpMbEdgeTracker::resetTracker();
  vpMbKltTracker::resetTracker();
}

unsigned int vpMbEdgeKltTracker::initMbtTracking(unsigned int lvl)
{
  if (lvl >= scales.size() || !scales[lvl]) {
    throw vpException(vpException::dimensionError, "lvl not used.");
  }

  unsigned int nbrow = 0;
  for (std::list<vpMbtDistanceLine *>::iterator it = lines[lvl].begin(); it != lines[lvl].end(); ++it) {
    vpMbtDistanceLine *l = *it;

    if (l->isTracked()) {
      l->initInteractionMatrixError();
      nbrow += l->nbFeatureTotal;
    }
  }

  for (std::list<vpMbtDistanceCylinder *>::const_iterator it = cylinders[lvl].begin(); it != cylinders[lvl].end();
    ++it) {
    vpMbtDistanceCylinder *cy = *it;

    if (cy->isTracked()) {
      cy->initInteractionMatrixError();
      nbrow += cy->nbFeature;
    }
  }

  for (std::list<vpMbtDistanceCircle *>::const_iterator it = circles[lvl].begin(); it != circles[lvl].end(); ++it) {
    vpMbtDistanceCircle *ci = *it;

    if (ci->isTracked()) {
      ci->initInteractionMatrixError();
      nbrow += ci->nbFeature;
    }
  }

  return nbrow;
}

/*!
  Load the xml configuration file.
  From the configuration file initialize the parameters corresponding to the
  objects: moving-edges, KLT, camera.

  \throw vpException::ioError if the file has not been properly parsed (file
  not found or wrong format for the data).

  \param configFile : full name of the xml file.
  \param verbose : Set true to activate the verbose mode, false otherwise.

  The XML configuration file has the following form:
  \code
<?xml version="1.0"?>
<conf>
  <ecm>
    <mask>
      <size>5</size>
      <nb_mask>180</nb_mask>
    </mask>
    <range>
      <tracking>10</tracking>
    </range>
    <contrast>
      <edge_threshold_type>1</edge_threshold_type>
      <edge_threshold>20</edge_threshold>
      <mu1>0.5</mu1>
      <mu2>0.5</mu2>
    </contrast>
    <sample>
      <step>4</step>
    </sample>
  </ecm>
  <camera>
    <width>640</width>
    <height>480</height>
    <u0>320</u0>
    <v0>240</v0>
    <px>686.24</px>
    <py>686.24</py>
  </camera>
  <face>
    <angle_appear>65</angle_appear>
    <angle_disappear>85</angle_disappear>
    <near_clipping>0.01</near_clipping>
    <far_clipping>0.90</far_clipping>
    <fov_clipping>1</fov_clipping>
  </face>
  <klt>
    <mask_border>10</mask_border>
    <max_features>10000</max_features>
    <window_size>5</window_size>
    <quality>0.02</quality>
    <min_distance>10</min_distance>
    <harris>0.02</harris>
    <size_block>3</size_block>
    <pyramid_lvl>3</pyramid_lvl>
  </klt>
</conf>
  \endcode
*/
void vpMbEdgeKltTracker::loadConfigFile(const std::string &configFile, bool verbose)
{
#if defined(VISP_HAVE_PUGIXML)
// Load projection error config
  vpMbTracker::loadConfigFile(configFile, verbose);

  vpMbtXmlGenericParser xmlp(vpMbtXmlGenericParser::EDGE_PARSER | vpMbtXmlGenericParser::KLT_PARSER);
  xmlp.setVerbose(verbose);
  xmlp.setCameraParameters(m_cam);
  xmlp.setAngleAppear(vpMath::deg(angleAppears));
  xmlp.setAngleDisappear(vpMath::deg(angleDisappears));

  xmlp.setEdgeMe(me);

  xmlp.setKltMaxFeatures(10000);
  xmlp.setKltWindowSize(5);
  xmlp.setKltQuality(0.01);
  xmlp.setKltMinDistance(5);
  xmlp.setKltHarrisParam(0.01);
  xmlp.setKltBlockSize(3);
  xmlp.setKltPyramidLevels(3);
  xmlp.setKltMaskBorder(maskBorder);

  try {
    if (verbose) {
      std::cout << " *********** Parsing XML for Mb Edge KLT Tracker ************ " << std::endl;
    }
    xmlp.parse(configFile.c_str());
  }
  catch (...) {
    vpERROR_TRACE("Can't open XML file \"%s\"\n ", configFile.c_str());
    throw vpException(vpException::ioError, "problem to parse configuration file.");
  }

  vpCameraParameters camera;
  xmlp.getCameraParameters(camera);
  setCameraParameters(camera);

  angleAppears = vpMath::rad(xmlp.getAngleAppear());
  angleDisappears = vpMath::rad(xmlp.getAngleDisappear());

  if (xmlp.hasNearClippingDistance())
    setNearClippingDistance(xmlp.getNearClippingDistance());

  if (xmlp.hasFarClippingDistance())
    setFarClippingDistance(xmlp.getFarClippingDistance());

  if (xmlp.getFovClipping()) {
    setClipping(vpMbEdgeTracker::clippingFlag | vpPolygon3D::FOV_CLIPPING);
  }

  useLodGeneral = xmlp.getLodState();
  minLineLengthThresholdGeneral = xmlp.getLodMinLineLengthThreshold();
  minPolygonAreaThresholdGeneral = xmlp.getLodMinPolygonAreaThreshold();

  applyLodSettingInConfig = false;
  if (this->getNbPolygon() > 0) {
    applyLodSettingInConfig = true;
    setLod(useLodGeneral);
    setMinLineLengthThresh(minLineLengthThresholdGeneral);
    setMinPolygonAreaThresh(minPolygonAreaThresholdGeneral);
  }

  vpMe meParser;
  xmlp.getEdgeMe(meParser);
  vpMbEdgeTracker::setMovingEdge(meParser);

  tracker.setMaxFeatures(static_cast<int>(xmlp.getKltMaxFeatures()));
  tracker.setWindowSize(static_cast<int>(xmlp.getKltWindowSize()));
  tracker.setQuality(xmlp.getKltQuality());
  tracker.setMinDistance(xmlp.getKltMinDistance());
  tracker.setHarrisFreeParameter(xmlp.getKltHarrisParam());
  tracker.setBlockSize(static_cast<int>(xmlp.getKltBlockSize()));
  tracker.setPyramidLevels(static_cast<int>(xmlp.getKltPyramidLevels()));
  maskBorder = xmlp.getKltMaskBorder();

  // if(useScanLine)
  faces.getMbScanLineRenderer().setMaskBorder(maskBorder);
#else
  (void)configFile;
  (void)verbose;
  throw(vpException(vpException::ioError, "vpMbEdgeKltTracker::loadConfigFile() needs pugixml built-in 3rdparty"));
#endif
}

/*!
  Realize the post tracking operations. Mostly visibility tests
*/
bool vpMbEdgeKltTracker::postTracking(const vpImage<unsigned char> &I, vpColVector &w_mbt, vpColVector &w_klt,
  unsigned int lvl)
{
  postTrackingMbt(w_mbt, lvl);

  if (displayFeatures) {
    if (lvl == 0) {
      for (std::list<vpMbtDistanceLine *>::const_iterator it = lines[lvl].begin(); it != lines[lvl].end(); ++it) {
        vpMbtDistanceLine *l = *it;
        if (l->isVisible() && l->isTracked()) {
          l->displayMovingEdges(I);
        }
      }

      for (std::list<vpMbtDistanceCylinder *>::const_iterator it = cylinders[lvl].begin(); it != cylinders[lvl].end();
        ++it) {
        vpMbtDistanceCylinder *cy = *it;
        // A cylinder is always visible: #FIXME AY: Still valid?
        if (cy->isTracked())
          cy->displayMovingEdges(I);
      }

      for (std::list<vpMbtDistanceCircle *>::const_iterator it = circles[lvl].begin(); it != circles[lvl].end(); ++it) {
        vpMbtDistanceCircle *ci = *it;
        if (ci->isVisible() && ci->isTracked()) {
          ci->displayMovingEdges(I);
        }
      }
    }
  }

  bool reInit = vpMbKltTracker::postTracking(I, w_klt);

  if (useScanLine) {
    m_cam.computeFov(I.getWidth(), I.getHeight());
    faces.computeClippedPolygons(m_cMo, m_cam);
    faces.computeScanLineRender(m_cam, I.getWidth(), I.getHeight());
  }

  vpMbEdgeTracker::updateMovingEdge(I);

  vpMbEdgeTracker::initMovingEdge(I, m_cMo);
  vpMbEdgeTracker::reinitMovingEdge(I, m_cMo);

  if (computeProjError)
    vpMbEdgeTracker::computeProjectionError(I);

  if (reInit)
    return true;

  return false;
}

/*!
  Realize the post tracking operations. Mostly visibility tests
*/
bool vpMbEdgeKltTracker::postTracking(const vpImage<vpRGBa> &I_color, vpColVector &w_mbt, vpColVector &w_klt,
  unsigned int lvl)
{
  postTrackingMbt(w_mbt, lvl);

  if (displayFeatures) {
    if (lvl == 0) {
      for (std::list<vpMbtDistanceLine *>::const_iterator it = lines[lvl].begin(); it != lines[lvl].end(); ++it) {
        vpMbtDistanceLine *l = *it;
        if (l->isVisible() && l->isTracked()) {
          l->displayMovingEdges(I_color);
        }
      }

      for (std::list<vpMbtDistanceCylinder *>::const_iterator it = cylinders[lvl].begin(); it != cylinders[lvl].end();
        ++it) {
        vpMbtDistanceCylinder *cy = *it;
        // A cylinder is always visible: #FIXME AY: Still valid?
        if (cy->isTracked())
          cy->displayMovingEdges(m_I);
      }

      for (std::list<vpMbtDistanceCircle *>::const_iterator it = circles[lvl].begin(); it != circles[lvl].end(); ++it) {
        vpMbtDistanceCircle *ci = *it;
        if (ci->isVisible() && ci->isTracked()) {
          ci->displayMovingEdges(m_I);
        }
      }
    }
  }

  bool reInit = vpMbKltTracker::postTracking(m_I, w_klt);

  if (useScanLine) {
    m_cam.computeFov(m_I.getWidth(), m_I.getHeight());
    faces.computeClippedPolygons(m_cMo, m_cam);
    faces.computeScanLineRender(m_cam, m_I.getWidth(), m_I.getHeight());
  }

  vpMbEdgeTracker::updateMovingEdge(m_I);

  vpMbEdgeTracker::initMovingEdge(m_I, m_cMo);
  vpMbEdgeTracker::reinitMovingEdge(m_I, m_cMo);

  if (computeProjError)
    vpMbEdgeTracker::computeProjectionError(m_I);

  if (reInit)
    return true;

  return false;
}

/*!
  Post tracking computation. Compute the mean weight of a line and, check the
  weight associated to a site (to eventually remove an outlier) and eventually
  set a flag to re-initialize the line.

  \warning level parameter not yet implemented.

  \param w : Vector of weight associated to the residual.
  \param lvl : Optional parameter to specify the level to track.
*/
void vpMbEdgeKltTracker::postTrackingMbt(vpColVector &w, unsigned int lvl)
{
  if (lvl >= scales.size() || !scales[lvl]) {
    throw vpException(vpException::dimensionError, "_lvl not used.");
  }

  unsigned int n = 0;
  vpMbtDistanceLine *l;
  for (std::list<vpMbtDistanceLine *>::const_iterator it = lines[lvl].begin(); it != lines[lvl].end(); ++it) {
    if ((*it)->isTracked()) {
      l = *it;
      unsigned int indexLine = 0;
      double wmean = 0;

      for (size_t a = 0; a < l->meline.size(); a++) {
        std::list<vpMeSite>::iterator itListLine;
        if (l->nbFeature[a] > 0)
          itListLine = l->meline[a]->getMeList().begin();

        for (unsigned int i = 0; i < l->nbFeature[a]; i++) {
          wmean += w[n + indexLine];
          vpMeSite p = *itListLine;
          if (w[n + indexLine] < 0.5) {
            p.setState(vpMeSite::M_ESTIMATOR);
            *itListLine = p;
          }

          ++itListLine;
          indexLine++;
        }
      }

      n += l->nbFeatureTotal;

      if (l->nbFeatureTotal != 0)
        wmean /= l->nbFeatureTotal;
      else
        wmean = 1;

      l->setMeanWeight(wmean);

      if (wmean < 0.8)
        l->Reinit = true;
    }
  }

  // Same thing with cylinders as with lines
  vpMbtDistanceCylinder *cy;
  for (std::list<vpMbtDistanceCylinder *>::const_iterator it = cylinders[lvl].begin(); it != cylinders[lvl].end();
    ++it) {
    if ((*it)->isTracked()) {
      cy = *it;
      double wmean = 0;
      std::list<vpMeSite>::iterator itListCyl1;
      std::list<vpMeSite>::iterator itListCyl2;
      if (cy->nbFeature > 0) {
        itListCyl1 = cy->meline1->getMeList().begin();
        itListCyl2 = cy->meline2->getMeList().begin();
      }

      wmean = 0;
      for (unsigned int i = 0; i < cy->nbFeaturel1; i++) {
        wmean += w[n + i];
        vpMeSite p = *itListCyl1;
        if (w[n + i] < 0.5) {
          p.setState(vpMeSite::M_ESTIMATOR);

          *itListCyl1 = p;
        }

        ++itListCyl1;
      }

      if (cy->nbFeaturel1 != 0)
        wmean /= cy->nbFeaturel1;
      else
        wmean = 1;

      cy->setMeanWeight1(wmean);

      if (wmean < 0.8) {
        cy->Reinit = true;
      }

      wmean = 0;
      for (unsigned int i = cy->nbFeaturel1; i < cy->nbFeature; i++) {
        wmean += w[n + i];
        vpMeSite p = *itListCyl2;
        if (w[n + i] < 0.5) {
          p.setState(vpMeSite::M_ESTIMATOR);

          *itListCyl2 = p;
        }

        ++itListCyl2;
      }

      if (cy->nbFeaturel2 != 0)
        wmean /= cy->nbFeaturel2;
      else
        wmean = 1;

      cy->setMeanWeight2(wmean);

      if (wmean < 0.8) {
        cy->Reinit = true;
      }

      n += cy->nbFeature;
    }
  }

  // Same thing with circles as with lines
  vpMbtDistanceCircle *ci;
  for (std::list<vpMbtDistanceCircle *>::const_iterator it = circles[lvl].begin(); it != circles[lvl].end(); ++it) {
    if ((*it)->isTracked()) {
      ci = *it;
      double wmean = 0;
      std::list<vpMeSite>::iterator itListCir;

      if (ci->nbFeature > 0) {
        itListCir = ci->meEllipse->getMeList().begin();
      }

      wmean = 0;
      for (unsigned int i = 0; i < ci->nbFeature; i++) {
        wmean += w[n + i];
        vpMeSite p = *itListCir;
        if (w[n + i] < 0.5) {
          p.setState(vpMeSite::M_ESTIMATOR);

          *itListCir = p;
        }

        ++itListCir;
      }

      if (ci->nbFeature != 0)
        wmean /= ci->nbFeature;
      else
        wmean = 1;

      ci->setMeanWeight(wmean);

      if (wmean < 0.8) {
        ci->Reinit = true;
      }

      n += ci->nbFeature;
    }
  }
}

/*!
  Achieve the VVS loop for the tracking using edges and KLT keypoints as visual features.

  \param[in] I : current image.
  \param[in] nbInfos : Size of the features (KLT).
  \param[out] nbrow : Size of the features (Edge).
  \param[in] lvl : Level of the pyramid.
  \param[out] edge_residual : Residual considering edges as visual features.
  \param[out] klt_residual : Residual considering KLT keypoints as visual features.
*/
void vpMbEdgeKltTracker::computeVVS(const vpImage<unsigned char> &I, const unsigned int &nbInfos, unsigned int &nbrow,
  unsigned int lvl, double *edge_residual, double *klt_residual)
{
  vpColVector factor;
  nbrow = trackFirstLoop(I, factor, lvl);

  if (nbrow < 4 && nbInfos < 4) {
    throw vpTrackingException(vpTrackingException::notEnoughPointError, "Error: not enough features");
  }
  else if (nbrow < 4)
    nbrow = 0;

  unsigned int totalNbRows = nbrow + 2 * nbInfos;
  double residu = 0;
  double residu_1 = -1;
  unsigned int iter = 0;

  vpMatrix L(totalNbRows, 6);
  vpMatrix L_mbt, L_klt; // interaction matrix
  vpColVector weighted_error(totalNbRows);
  vpColVector R_mbt, R_klt; // residu
  vpMatrix L_true;
  vpMatrix LVJ_true;

  if (nbrow != 0) {
    L_mbt.resize(nbrow, 6, false, false);
    R_mbt.resize(nbrow, false);
  }

  if (nbInfos != 0) {
    L_klt.resize(2 * nbInfos, 6, false, false);
    R_klt.resize(2 * nbInfos, false);
  }

  vpColVector v; // "speed" for VVS
  vpRobust robust_mbt, robust_klt;
  vpHomography H;

  vpMatrix LTL;
  vpColVector LTR;

  double factorMBT; // = 1.0;
  double factorKLT; // = 1.0;

  // More efficient weight repartition for hybrid tracker should come soon...
  //   factorMBT = 1.0 - (double)nbrow / (double)(nbrow + nbInfos);
  //   factorKLT = 1.0 - factorMBT;
  factorMBT = 0.35;
  factorKLT = 0.65;

  if (nbrow < 4)
    factorKLT = 1.;
  if (nbInfos < 4)
    factorMBT = 1.;

  if (edge_residual != nullptr)
    *edge_residual = 0;
  if (klt_residual != nullptr)
    *klt_residual = 0;

  vpHomogeneousMatrix cMoPrev;
  vpHomogeneousMatrix ctTc0_Prev;
  vpColVector m_error_prev;
  vpColVector m_w_prev;

  bool isoJoIdentity = m_isoJoIdentity; // Backup since it can be modified if L is not full rank
  if (isoJoIdentity)
    oJo.eye();

  // Init size
  m_error_hybrid.resize(totalNbRows, false);
  m_w_hybrid.resize(totalNbRows, false);

  if (nbrow != 0) {
    m_w_mbt.resize(nbrow, false);
    m_w_mbt = 1; // needed in vpRobust::psiTukey()
  }

  if (nbInfos != 0) {
    m_w_klt.resize(2 * nbInfos, false);
    m_w_klt = 1; // needed in vpRobust::psiTukey()
  }

  double mu = m_initialMu;

  while ((static_cast<int>((residu - residu_1) * 1e8) != 0) && (iter < m_maxIter)) {
    if (nbrow >= 4)
      trackSecondLoop(I, L_mbt, R_mbt, m_cMo, lvl);

    if (nbInfos >= 4) {
      unsigned int shift = 0;

      for (std::list<vpMbtDistanceKltPoints *>::const_iterator it = vpMbKltTracker::kltPolygons.begin();
        it != vpMbKltTracker::kltPolygons.end(); ++it) {
        vpMbtDistanceKltPoints *kltpoly = *it;
        if (kltpoly->polygon->isVisible() && kltpoly->isTracked() && kltpoly->hasEnoughPoints()) {
          vpSubColVector subR(R_klt, shift, 2 * kltpoly->getCurrentNumberPoints());
          vpSubMatrix subL(L_klt, shift, 0, 2 * kltpoly->getCurrentNumberPoints(), 6);
          kltpoly->computeHomography(ctTc0, H);
          kltpoly->computeInteractionMatrixAndResidu(subR, subL);
          shift += 2 * kltpoly->getCurrentNumberPoints();
        }
      }

      for (std::list<vpMbtDistanceKltCylinder *>::const_iterator it = kltCylinders.begin(); it != kltCylinders.end();
        ++it) {
        vpMbtDistanceKltCylinder *kltPolyCylinder = *it;

        if (kltPolyCylinder->isTracked() && kltPolyCylinder->hasEnoughPoints()) {
          vpSubColVector subR(R_klt, shift, 2 * kltPolyCylinder->getCurrentNumberPoints());
          vpSubMatrix subL(L_klt, shift, 0, 2 * kltPolyCylinder->getCurrentNumberPoints(), 6);
          try {
            kltPolyCylinder->computeInteractionMatrixAndResidu(ctTc0, subR, subL);
          }
          catch (...) {
            throw vpTrackingException(vpTrackingException::fatalError, "Cannot compute interaction matrix");
          }

          shift += 2 * kltPolyCylinder->getCurrentNumberPoints();
        }
      }
    }

    /* residuals */
    if (nbrow > 3) {
      m_error_hybrid.insert(0, R_mbt);
    }

    if (nbInfos > 3) {
      m_error_hybrid.insert(nbrow, R_klt);
    }

    unsigned int cpt = 0;
    while (cpt < (nbrow + 2 * nbInfos)) {
      if (cpt < static_cast<unsigned int>(nbrow)) {
        m_w_hybrid[cpt] = ((m_w_mbt[cpt] * factor[cpt]) * factorMBT);
      }
      else {
        m_w_hybrid[cpt] = (m_w_klt[cpt - nbrow] * factorKLT);
      }
      cpt++;
    }

    bool reStartFromLastIncrement = false;
    computeVVSCheckLevenbergMarquardt(iter, m_error_hybrid, m_error_prev, cMoPrev, mu, reStartFromLastIncrement,
      &m_w_prev);
    if (reStartFromLastIncrement) {
      ctTc0 = ctTc0_Prev;
    }

    if (!reStartFromLastIncrement) {
      /* robust */
      if (nbrow > 3) {
        if (edge_residual != nullptr) {
          *edge_residual = 0;
          for (unsigned int i = 0; i < R_mbt.getRows(); i++)
            *edge_residual += fabs(R_mbt[i]);
          *edge_residual /= R_mbt.getRows();
        }

        robust_mbt.setMinMedianAbsoluteDeviation(m_thresholdMBT / m_cam.get_px());
        robust_mbt.MEstimator(vpRobust::TUKEY, R_mbt, m_w_mbt);

        L.insert(L_mbt, 0, 0);
      }

      if (nbInfos > 3) {
        if (klt_residual != nullptr) {
          *klt_residual = 0;
          for (unsigned int i = 0; i < R_klt.getRows(); i++)
            *klt_residual += fabs(R_klt[i]);
          *klt_residual /= R_klt.getRows();
        }

        robust_klt.setMinMedianAbsoluteDeviation(m_thresholdKLT / m_cam.get_px());
        robust_klt.MEstimator(vpRobust::TUKEY, R_klt, m_w_klt);

        L.insert(L_klt, nbrow, 0);
      }

      cpt = 0;
      while (cpt < (nbrow + 2 * nbInfos)) {
        if (cpt < static_cast<unsigned int>(nbrow)) {
          m_w_hybrid[cpt] = ((m_w_mbt[cpt] * factor[cpt]) * factorMBT);
        }
        else {
          m_w_hybrid[cpt] = (m_w_klt[cpt - nbrow] * factorKLT);
        }
        cpt++;
      }

      if (computeCovariance) {
        L_true = L;
        if (!isoJoIdentity) {
          vpVelocityTwistMatrix cVo;
          cVo.buildFrom(m_cMo);
          LVJ_true = (L * cVo * oJo);
        }
      }

      residu_1 = residu;
      residu = 0;
      double num = 0;
      double den = 0;

      for (unsigned int i = 0; i < weighted_error.getRows(); i++) {
        num += m_w_hybrid[i] * vpMath::sqr(m_error_hybrid[i]);
        den += m_w_hybrid[i];

        weighted_error[i] = m_error_hybrid[i] * m_w_hybrid[i];
        if (m_computeInteraction) {
          for (unsigned int j = 0; j < 6; j += 1) {
            L[i][j] *= m_w_hybrid[i];
          }
        }
      }

      residu = sqrt(num / den);

      computeVVSPoseEstimation(isoJoIdentity, iter, L, LTL, weighted_error, m_error_hybrid, m_error_prev, LTR, mu, v,
        &m_w_hybrid, &m_w_prev);

      cMoPrev = m_cMo;
      ctTc0_Prev = ctTc0;
      ctTc0 = vpExponentialMap::direct(v).inverse() * ctTc0;
      m_cMo = ctTc0 * c0Mo;
    }

    iter++;
  }

  computeCovarianceMatrixVVS(isoJoIdentity, m_w_hybrid, cMoPrev, L_true, LVJ_true, m_error_hybrid);
}

void vpMbEdgeKltTracker::computeVVSInit()
{
  throw vpException(vpException::fatalError, "vpMbEdgeKltTracker::computeVVSInit() should not be called!");
}

void vpMbEdgeKltTracker::computeVVSInteractionMatrixAndResidu()
{
  throw vpException(vpException::fatalError, "vpMbEdgeKltTracker::"
    "computeVVSInteractionMatrixAndR"
    "esidu() should not be called!");
}

/*!
  Realize the tracking of the object in the image.

  \throw vpException : if the tracking is supposed to have failed.

  \param I : the input grayscale image.
*/
void vpMbEdgeKltTracker::track(const vpImage<unsigned char> &I)
{
  try {
    vpMbKltTracker::preTracking(I);
  }
  catch (...) {
  }

  if (m_nbInfos >= 4) {
    unsigned int old_maxIter = m_maxIter;
    m_maxIter = m_maxIterKlt;
    vpMbKltTracker::computeVVS();
    m_maxIter = old_maxIter;
  }
  else {
    m_nbInfos = 0;
    // std::cout << "[Warning] Unable to init with KLT" << std::endl;
  }

  vpMbEdgeTracker::trackMovingEdge(I);

  unsigned int nbrow = 0;
  computeVVS(I, m_nbInfos, nbrow);

  if (postTracking(I, m_w_mbt, m_w_klt)) {
    vpMbKltTracker::reinit(I);

    // AY : Removed as edge tracked, if necessary, is reinitialized in
    // postTracking()

    //    initPyramid(I, Ipyramid);

    //    unsigned int i = (unsigned int)scales.size();
    //    do {
    //      i--;
    //      if(scales[i]){
    //        downScale(i);
    //        initMovingEdge(*Ipyramid[i], cMo);
    //        upScale(i);
    //      }
    //    } while(i != 0);

    //    cleanPyramid(Ipyramid);
  }

  if (displayFeatures) {
    m_featuresToBeDisplayedKlt = getFeaturesForDisplayKlt();
  }
}

/*!
  Realize the tracking of the object in the image.

  \throw vpException : if the tracking is supposed to have failed.

  \param I_color : the input color image.
*/
void vpMbEdgeKltTracker::track(const vpImage<vpRGBa> &I_color)
{
  vpImageConvert::convert(I_color, m_I);
  try {
    vpMbKltTracker::preTracking(m_I);
  }
  catch (...) {
  }

  if (m_nbInfos >= 4) {
    unsigned int old_maxIter = m_maxIter;
    m_maxIter = m_maxIterKlt;
    vpMbKltTracker::computeVVS();
    m_maxIter = old_maxIter;
  }
  else {
    m_nbInfos = 0;
    // std::cout << "[Warning] Unable to init with KLT" << std::endl;
  }

  vpMbEdgeTracker::trackMovingEdge(m_I);

  unsigned int nbrow = 0;
  computeVVS(m_I, m_nbInfos, nbrow);

  if (postTracking(I_color, m_w_mbt, m_w_klt)) {
    vpMbKltTracker::reinit(m_I);

    // AY : Removed as edge tracked, if necessary, is reinitialized in
    // postTracking()

    //    initPyramid(I, Ipyramid);

    //    unsigned int i = (unsigned int)scales.size();
    //    do {
    //      i--;
    //      if(scales[i]){
    //        downScale(i);
    //        initMovingEdge(*Ipyramid[i], cMo);
    //        upScale(i);
    //      }
    //    } while(i != 0);

    //    cleanPyramid(Ipyramid);
  }

  if (displayFeatures) {
    m_featuresToBeDisplayedKlt = getFeaturesForDisplayKlt();
  }
}

unsigned int vpMbEdgeKltTracker::trackFirstLoop(const vpImage<unsigned char> &I, vpColVector &factor, unsigned int lvl)
{
  vpMbtDistanceLine *l;
  vpMbtDistanceCylinder *cy;
  vpMbtDistanceCircle *ci;

  if (lvl >= scales.size() || !scales[lvl]) {
    throw vpException(vpException::dimensionError, "_lvl not used.");
  }

  unsigned int nbrow = initMbtTracking(lvl);

  if (nbrow == 0) {
    //     throw vpTrackingException(vpTrackingException::notEnoughPointError,
    //     "Error: not enough features in the interaction matrix...");
    return nbrow;
  }

  factor.resize(nbrow, false);
  factor = 1;

  unsigned int n = 0;
  for (std::list<vpMbtDistanceLine *>::const_iterator it = lines[lvl].begin(); it != lines[lvl].end(); ++it) {
    if ((*it)->isTracked()) {
      l = *it;
      l->computeInteractionMatrixError(m_cMo);

      double fac = 1;
      for (std::list<int>::const_iterator itindex = l->Lindex_polygon.begin(); itindex != l->Lindex_polygon.end();
        ++itindex) {
        int index = *itindex;
        if (l->hiddenface->isAppearing(static_cast<unsigned int>(index))) {
          fac = 0.2;
          break;
        }
        if (l->closeToImageBorder(I, 10)) {
          fac = 0.1;
          break;
        }
      }

      for (size_t a = 0; a < l->meline.size(); a++) {
        std::list<vpMeSite>::const_iterator itListLine;
        if (l->meline[a] != nullptr) {
          itListLine = l->meline[a]->getMeList().begin();

          for (unsigned int i = 0; i < l->nbFeature[a]; i++) {
            factor[n + i] = fac;
            vpMeSite site = *itListLine;
            if (site.getState() != vpMeSite::NO_SUPPRESSION)
              factor[n + i] = 0.2;
            ++itListLine;
          }
          n += l->nbFeature[a];
        }
      }
    }
  }

  for (std::list<vpMbtDistanceCylinder *>::const_iterator it = cylinders[lvl].begin(); it != cylinders[lvl].end();
    ++it) {
    if ((*it)->isTracked()) {
      cy = *it;
      cy->computeInteractionMatrixError(m_cMo, I);
      double fac = 1.0;

      std::list<vpMeSite>::const_iterator itCyl1;
      std::list<vpMeSite>::const_iterator itCyl2;
      if ((cy->meline1 != nullptr || cy->meline2 != nullptr)) {
        itCyl1 = cy->meline1->getMeList().begin();
        itCyl2 = cy->meline2->getMeList().begin();
      }

      for (unsigned int i = 0; i < cy->nbFeature; i++) {
        factor[n + i] = fac;
        vpMeSite site;
        if (i < cy->nbFeaturel1) {
          site = *itCyl1;
          ++itCyl1;
        }
        else {
          site = *itCyl2;
          ++itCyl2;
        }
        if (site.getState() != vpMeSite::NO_SUPPRESSION)
          factor[n + i] = 0.2;
      }

      n += cy->nbFeature;
    }
  }

  for (std::list<vpMbtDistanceCircle *>::const_iterator it = circles[lvl].begin(); it != circles[lvl].end(); ++it) {
    if ((*it)->isTracked()) {
      ci = *it;
      ci->computeInteractionMatrixError(m_cMo);
      double fac = 1.0;

      std::list<vpMeSite>::const_iterator itCir;
      if (ci->meEllipse != nullptr) {
        itCir = ci->meEllipse->getMeList().begin();
      }

      for (unsigned int i = 0; i < ci->nbFeature; i++) {
        factor[n + i] = fac;
        vpMeSite site = *itCir;
        if (site.getState() != vpMeSite::NO_SUPPRESSION)
          factor[n + i] = 0.2;
        ++itCir;
      }

      n += ci->nbFeature;
    }
  }

  return nbrow;
}

void vpMbEdgeKltTracker::trackSecondLoop(const vpImage<unsigned char> &I, vpMatrix &L, vpColVector &error,
  const vpHomogeneousMatrix &cMo, unsigned int lvl)
{
  vpMbtDistanceLine *l;
  vpMbtDistanceCylinder *cy;
  vpMbtDistanceCircle *ci;

  unsigned int n = 0;
  for (std::list<vpMbtDistanceLine *>::const_iterator it = lines[lvl].begin(); it != lines[lvl].end(); ++it) {
    if ((*it)->isTracked()) {
      l = *it;
      l->computeInteractionMatrixError(cMo);
      for (unsigned int i = 0; i < l->nbFeatureTotal; i++) {
        for (unsigned int j = 0; j < 6; j++) {
          L[n + i][j] = l->L[i][j];
          error[n + i] = l->error[i];
        }
      }
      n += l->nbFeatureTotal;
    }
  }

  for (std::list<vpMbtDistanceCylinder *>::const_iterator it = cylinders[lvl].begin(); it != cylinders[lvl].end();
    ++it) {
    if ((*it)->isTracked()) {
      cy = *it;
      cy->computeInteractionMatrixError(cMo, I);
      for (unsigned int i = 0; i < cy->nbFeature; i++) {
        for (unsigned int j = 0; j < 6; j++) {
          L[n + i][j] = cy->L[i][j];
          error[n + i] = cy->error[i];
        }
      }
      n += cy->nbFeature;
    }
  }
  for (std::list<vpMbtDistanceCircle *>::const_iterator it = circles[lvl].begin(); it != circles[lvl].end(); ++it) {
    if ((*it)->isTracked()) {
      ci = *it;
      ci->computeInteractionMatrixError(cMo);
      for (unsigned int i = 0; i < ci->nbFeature; i++) {
        for (unsigned int j = 0; j < 6; j++) {
          L[n + i][j] = ci->L[i][j];
          error[n + i] = ci->error[i];
        }
      }

      n += ci->nbFeature;
    }
  }
}

/*!
  Set the camera parameters

  \param cam : the new camera parameters
*/
void vpMbEdgeKltTracker::setCameraParameters(const vpCameraParameters &cam)
{
  m_cam = cam;

  vpMbEdgeTracker::setCameraParameters(m_cam);
  vpMbKltTracker::setCameraParameters(m_cam);
}

/*!
  Initialise a new face from the coordinates given in parameter.

  \param polygon : The polygon describing the set of lines that has to be
  tracked.
*/
void vpMbEdgeKltTracker::initFaceFromCorners(vpMbtPolygon &polygon)
{
  vpMbEdgeTracker::initFaceFromCorners(polygon);
  vpMbKltTracker::initFaceFromCorners(polygon);
}
/*!
  Initialise a new face from the coordinates given in parameter.

  \param polygon : The polygon describing the set of lines that has to be
  tracked.
*/
void vpMbEdgeKltTracker::initFaceFromLines(vpMbtPolygon &polygon)
{
  vpMbEdgeTracker::initFaceFromLines(polygon);
  vpMbKltTracker::initFaceFromLines(polygon);
}

/*!
  Add a circle to track. With the center of the circle we have 3 points defining the plane that  contains the circle.
  To be visible, the plane defined by the 3 points p1, p2, p3 should have its normal going toward the camera.

  \param p1 : Center of the circle, considered as the first point on the plane containing the circle.
  \param p2 : Second point on the plane containing the circle.
  \param p3 : Third point on the plane containing the circle.
  \param radius : Radius of the circle.
  \param idFace : Index of the face associated to the circle to handle visibility test.
  \param name : The optional name of the circle.
*/
void vpMbEdgeKltTracker::initCircle(const vpPoint &p1, const vpPoint &p2, const vpPoint &p3, double radius, int idFace,
                                    const std::string &name)
{
  vpMbEdgeTracker::initCircle(p1, p2, p3, radius, idFace, name);
}

/*!
  Add a cylinder to track from tow points on the axis (defining the length of
  the cylinder) and its radius.

  \param p1 : First point on the axis.
  \param p2 : Second point on the axis.
  \param radius : Radius of the cylinder.
  \param idFace : Id of the face associated to the cylinder.
  \param name : The optional name of the cylinder.
*/
void vpMbEdgeKltTracker::initCylinder(const vpPoint &p1, const vpPoint &p2, double radius, int idFace,
                                      const std::string &name)
{
  vpMbEdgeTracker::initCylinder(p1, p2, radius, idFace, name);
  vpMbKltTracker::initCylinder(p1, p2, radius, idFace, name);
}

/*!
  Display the 3D model at a given position using the given camera parameters

  \param I : The image.
  \param cMo : Pose used to project the 3D model into the image.
  \param cam : The camera parameters.
  \param col : The desired color.
  \param thickness : The thickness of the lines.
  \param displayFullModel : boolean to say if all the model has to be
  displayed, even the faces that are not visible.
*/
void vpMbEdgeKltTracker::display(const vpImage<unsigned char> &I, const vpHomogeneousMatrix &cMo,
  const vpCameraParameters &cam, const vpColor &col, unsigned int thickness,
  bool displayFullModel)
{
  std::vector<std::vector<double> > models =
    vpMbEdgeKltTracker::getModelForDisplay(I.getWidth(), I.getHeight(), cMo, cam, displayFullModel);

  for (size_t i = 0; i < models.size(); i++) {
    if (vpMath::equal(models[i][0], 0)) {
      vpImagePoint ip1(models[i][1], models[i][2]);
      vpImagePoint ip2(models[i][3], models[i][4]);
      vpDisplay::displayLine(I, ip1, ip2, col, thickness);
    }
    else if (vpMath::equal(models[i][0], 1)) {
      vpImagePoint center(models[i][1], models[i][2]);
      double n20 = models[i][3];
      double n11 = models[i][4];
      double n02 = models[i][5];
      vpDisplay::displayEllipse(I, center, n20, n11, n02, true, col, thickness);
    }
  }

  if (displayFeatures) {
    for (size_t i = 0; i < m_featuresToBeDisplayedKlt.size(); i++) {
      if (vpMath::equal(m_featuresToBeDisplayedKlt[i][0], 1)) {
        vpImagePoint ip1(m_featuresToBeDisplayedKlt[i][1], m_featuresToBeDisplayedKlt[i][2]);
        vpDisplay::displayCross(I, ip1, 10, vpColor::red);

        vpImagePoint ip2(m_featuresToBeDisplayedKlt[i][3], m_featuresToBeDisplayedKlt[i][4]);
        double id = m_featuresToBeDisplayedKlt[i][5];
        std::stringstream ss;
        ss << id;
        vpDisplay::displayText(I, ip2, ss.str(), vpColor::red);
      }
    }
  }

#ifdef VISP_HAVE_OGRE
  if (useOgre)
    faces.displayOgre(cMo);
#endif
}

/*!
  Display the 3D model at a given position using the given camera parameters

  \param I : The color image.
  \param cMo : Pose used to project the 3D model into the image.
  \param cam : The camera parameters.
  \param col : The desired color.
  \param thickness : The thickness of the lines.
  \param displayFullModel : boolean to say if all the model has to be
  displayed, even the faces that are not visible.
*/
void vpMbEdgeKltTracker::display(const vpImage<vpRGBa> &I, const vpHomogeneousMatrix &cMo,
  const vpCameraParameters &cam, const vpColor &col, unsigned int thickness,
  bool displayFullModel)
{
  std::vector<std::vector<double> > models =
    getModelForDisplay(I.getWidth(), I.getHeight(), cMo, cam, displayFullModel);

  for (size_t i = 0; i < models.size(); i++) {
    if (vpMath::equal(models[i][0], 0)) {
      vpImagePoint ip1(models[i][1], models[i][2]);
      vpImagePoint ip2(models[i][3], models[i][4]);
      vpDisplay::displayLine(I, ip1, ip2, col, thickness);
    }
    else if (vpMath::equal(models[i][0], 1)) {
      vpImagePoint center(models[i][1], models[i][2]);
      double n20 = models[i][3];
      double n11 = models[i][4];
      double n02 = models[i][5];
      vpDisplay::displayEllipse(I, center, n20, n11, n02, true, col, thickness);
    }
  }

  if (displayFeatures) {
    for (size_t i = 0; i < m_featuresToBeDisplayedKlt.size(); i++) {
      if (vpMath::equal(m_featuresToBeDisplayedKlt[i][0], 1)) {
        vpImagePoint ip1(m_featuresToBeDisplayedKlt[i][1], m_featuresToBeDisplayedKlt[i][2]);
        vpDisplay::displayCross(I, ip1, 10, vpColor::red);

        vpImagePoint ip2(m_featuresToBeDisplayedKlt[i][3], m_featuresToBeDisplayedKlt[i][4]);
        double id = m_featuresToBeDisplayedKlt[i][5];
        std::stringstream ss;
        ss << id;
        vpDisplay::displayText(I, ip2, ss.str(), vpColor::red);
      }
    }
  }

#ifdef VISP_HAVE_OGRE
  if (useOgre)
    faces.displayOgre(cMo);
#endif
}

std::vector<std::vector<double> > vpMbEdgeKltTracker::getModelForDisplay(unsigned int width, unsigned int height,
  const vpHomogeneousMatrix &cMo,
  const vpCameraParameters &cam,
  bool displayFullModel)
{
  std::vector<std::vector<double> > models;

  for (unsigned int i = 0; i < scales.size(); i += 1) {
    if (scales[i]) {
      for (std::list<vpMbtDistanceLine *>::const_iterator it = lines[scaleLevel].begin(); it != lines[scaleLevel].end();
        ++it) {
        std::vector<std::vector<double> > currentModel =
          (*it)->getModelForDisplay(width, height, cMo, cam, displayFullModel);
        models.insert(models.end(), currentModel.begin(), currentModel.end());
      }

      for (std::list<vpMbtDistanceCylinder *>::const_iterator it = cylinders[scaleLevel].begin();
        it != cylinders[scaleLevel].end(); ++it) {
        std::vector<std::vector<double> > currentModel =
          (*it)->getModelForDisplay(width, height, cMo, cam, displayFullModel);
        models.insert(models.end(), currentModel.begin(), currentModel.end());
      }

      for (std::list<vpMbtDistanceCircle *>::const_iterator it = circles[scaleLevel].begin();
        it != circles[scaleLevel].end(); ++it) {
        std::vector<double> paramsCircle = (*it)->getModelForDisplay(cMo, cam, displayFullModel);
        models.push_back(paramsCircle);
      }

      break; // displaying model on one scale only
    }
  }

#ifdef VISP_HAVE_OGRE
  if (useOgre)
    faces.displayOgre(cMo);
#endif

  return models;
}

/*!
  Re-initialize the model used by the tracker.

  \param I : The image containing the object to initialize.
  \param cad_name : Path to the file containing the 3D model description.
  \param cMo : The new vpHomogeneousMatrix between the camera and the new
  model
  \param verbose : verbose option to print additional information when
  loading CAO model files which include other CAO model files.
  \param T : optional transformation matrix (currently only for .cao) to transform
  3D points expressed in the original object frame to the desired object frame.
*/
void vpMbEdgeKltTracker::reInitModel(const vpImage<unsigned char> &I, const std::string &cad_name,
  const vpHomogeneousMatrix &cMo, bool verbose, const vpHomogeneousMatrix &T)
{
  // Reinit klt

  // delete the Klt Polygon features
  for (std::list<vpMbtDistanceKltPoints *>::const_iterator it = kltPolygons.begin(); it != kltPolygons.end(); ++it) {
    vpMbtDistanceKltPoints *kltpoly = *it;
    if (kltpoly != nullptr) {
      delete kltpoly;
    }
    kltpoly = nullptr;
  }
  kltPolygons.clear();

  for (std::list<vpMbtDistanceKltCylinder *>::const_iterator it = kltCylinders.begin(); it != kltCylinders.end();
    ++it) {
    vpMbtDistanceKltCylinder *kltPolyCylinder = *it;
    if (kltPolyCylinder != nullptr) {
      delete kltPolyCylinder;
    }
    kltPolyCylinder = nullptr;
  }
  kltCylinders.clear();

  // delete the structures used to display circles
  vpMbtDistanceCircle *ci;
  for (std::list<vpMbtDistanceCircle *>::const_iterator it = circles_disp.begin(); it != circles_disp.end(); ++it) {
    ci = *it;
    if (ci != nullptr) {
      delete ci;
    }
    ci = nullptr;
  }

  circles_disp.clear();

  firstInitialisation = true;

  // Reinit edge
  vpMbtDistanceLine *l;
  vpMbtDistanceCylinder *cy;

  for (unsigned int i = 0; i < scales.size(); i += 1) {
    if (scales[i]) {
      for (std::list<vpMbtDistanceLine *>::const_iterator it = lines[i].begin(); it != lines[i].end(); ++it) {
        l = *it;
        if (l != nullptr)
          delete l;
        l = nullptr;
      }

      for (std::list<vpMbtDistanceCylinder *>::const_iterator it = cylinders[i].begin(); it != cylinders[i].end();
        ++it) {
        cy = *it;
        if (cy != nullptr)
          delete cy;
        cy = nullptr;
      }

      for (std::list<vpMbtDistanceCircle *>::const_iterator it = circles[i].begin(); it != circles[i].end(); ++it) {
        ci = *it;
        if (ci != nullptr)
          delete ci;
        ci = nullptr;
      }

      lines[i].clear();
      cylinders[i].clear();
      circles[i].clear();
    }
  }

  // compute_interaction=1;
  nline = 0;
  ncylinder = 0;
  ncircle = 0;
  // lambda = 1;
  nbvisiblepolygone = 0;

  // Reinit common parts
  faces.reset();

  loadModel(cad_name, verbose, T);

  m_cMo = cMo;
  init(I);
}
END_VISP_NAMESPACE
#elif !defined(VISP_BUILD_SHARED_LIBS)
// Work around to avoid warning: libvisp_mbt.a(vpMbEdgeKltTracker.cpp.o) has
// no symbols
void dummy_vpMbEdgeKltTracker() { }
#endif // VISP_HAVE_OPENCV
