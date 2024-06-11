/*
 * ViSP, open source Visual Servoing Platform software.
 * Copyright (C) 2005 - 2024 by Inria. All rights reserved.
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
 * Display a point cloud using PCL library.
 */

#ifndef VPPARTICLEFILTER_H
#define VPPARTICLEFILTER_H

#include <visp3/core/vpConfig.h>

#if (VISP_CXX_STANDARD >= VISP_CXX_STANDARD_11)
#include <visp3/core/vpColVector.h>
#include <visp3/core/vpGaussRand.h>
#include <visp3/core/vpMatrix.h>

#include <functional> // std::function

BEGIN_VISP_NAMESPACE
/*!
  \class vpParticleFilter
  This class permits to use a Particle Filter.
*/
class VISP_EXPORT vpParticleFilter
{
public:
  /**
   * \brief Process model function, which projects a particle forward in time.
   * The first argument is a particle, the second is the period and the return is the
   * particle projected in the future.
   */
  typedef std::function<vpColVector(const vpColVector &, const double &)> vpProcessFunction;

  /**
   * \brief Command model function, which projects a particle forward in time according to
   * the command and its previous state.
   * The first argument is the command(s), the second is the particle and the third is the period.
   * The return is the updated particle after period seconds.
   */
  typedef std::function<vpColVector(const vpColVector &, const vpColVector &, const double &)> vpCommandStateFunction;


  /**
   * \brief Likelihood function, which evaluates the likehood of a particle with regard to the measurements.
   * The first argument is the particle that is evaluated.
   * The second argument is the measurements vector.
   * The return is the likelihood of the particle, which equals to 0 when the particle does not match
   * at all the measurements and to 1 when it matches completely.
   */
  typedef std::function<double(const vpColVector &, const vpColVector &)> vpLikelihoodFunction;

  /**
   * \brief Filter function, which computes the filtered state of the particle filter.
   * The first argument is the vector containing the particles
   * and the second argument is the associated vector of weights. The return is the
   * corresponding filtered state.
   */
  typedef std::function<vpColVector(const std::vector<vpColVector> &, const std::vector<double> &)> vpFilterFunction;

  /**
   * \brief Function that takes as argument the number of particles and the vector of weights
   * associated to each particle and returns true if the resampling must be performed and false
   * otherwise.
   */
  typedef std::function<bool(const unsigned int &, const std::vector<double> &)> vpResamplingConditionFunction;

  /**
   * \brief Function that takes as argument the vector of particles and the vector of
   * associated weights. It returns a pair < new_vector_particles, new_weights >.
   */
  typedef std::function<std::pair<std::vector<vpColVector>, std::vector<double>>(const std::vector<vpColVector> &, const std::vector<double> &)> vpResamplingFunction;

  /**
   * \brief Construct a new vpParticleFilter object.
   *
   * \param[in] N The number of particles.
   * \param[in] stdev The standard deviations of the noise, each item correspond to one component of the state.
   * \param[in] seed The seed to use to create the noise generators. A negative value makes the seed to
   * be based on the current time.
   */
  vpParticleFilter(const unsigned int &N, const std::vector<double> &stdev, const long &seed = -1);

  /**
   * \brief Set the guess of the initial state.
   *
   * \param[in] x0 Guess of the initial state.
   * \param[in] f Process model function, which projects the particles forward in time.
   * The first argument is a particle, the second is the period and the return is the
   * particle projected in the future.
   * \param[in] l Likelihood function, that evaluates how much a particle matches the measurements.
   * 0 means that the particle does not match the measurement at all and 1 that it maches completely.
   * \param[in] filterFunc The function to compute the filtered state from the particles and their weights.
   */
  void init(const vpColVector &x0, const vpProcessFunction &f,
            const vpLikelihoodFunction &l, const vpFilterFunction &filterFunc = simpleMean);

  /**
   * \brief Set the guess of the initial state.
   *
   * \param[in] x0 Guess of the initial state.
   * \param[in] bx Process model function, which projects the particles forward in time based on the previous
   * state and on the input commands. The first argument is the command vector, the second is a particle and
   * the last is the period. The return is the particle projected in the future.
   * \param[in] l Likelihood function, that evaluates how much a particle matches the measurements.
   * 0 means that the particle does not match the measurement at all and 1 that it maches completely.
   * \param[in] filterFunc The function to compute the filtered state from the particles and their weights.
   */
  void init(const vpColVector &x0, const vpCommandStateFunction &bx,
            const vpLikelihoodFunction &l, const vpFilterFunction &filterFunc = simpleMean);

  /**
   * \brief Set the process function to use when projecting the particles in the future.
   *
   * \param f The process function to use.
   *
   * \warning It will deactive the command function.
   */
  inline void setProcessFunction(const vpProcessFunction &f)
  {
    m_f = f;
    m_useCommandStateFunction = false;
    m_useProcessFunction = true;
  }

  /**
   * \brief Set the command function to use when projecting the particles in the future.
   *
   * \param bx The command function to use.
   *
   * \warning It will deactive the process function.
   */
  inline void setCommandStateFunction(const vpCommandStateFunction &bx)
  {
    m_bx = bx;
    m_useCommandStateFunction = true;
    m_useProcessFunction = false;
  }

  /**
   * \brief Set the filter function that compute the filtered state from the particles
   * and their associated weights.
   *
   * \param filterFunc The filtering function to use.
   */
  inline void setFilterFunction(const vpFilterFunction &filterFunc)
  {
    m_stateFilterFunc = filterFunc;
  }

  /**
   * \brief Perform first the prediction step and then the update step.
   * If needed, resampling will also be performed.
   *
   * \param[in] z The new measurement.
   * \param[in] dt The time in the future we must predict.
   * \param[in] u The command(s) given to the system, if the impact of the system is known.
   *
   * \warning To use the commands, use the dedicated constructor or call
   * vpUnscentedKalman::setCommandStateFunction beforehand. In the second case, the process
   * function will be ignored.
   */
  void filter(const vpColVector &z, const double &dt, const vpColVector &u = vpColVector());

  /**
   * \brief Predict the new state based on the last state and how far in time we want to predict.
   *
   * \param[in] dt The time in the future we must predict.
   * \param[in] u The command(s) given to the system, if the impact of the system is known.
   *
   * \warning To use the commands, use the dedicated constructor or call
   * vpUnscentedKalman::setCommandStateFunction beforehand. In the second case, the process
   * function will be ignored.
   */
  void predict(const double &dt, const vpColVector &u = vpColVector());

  /**
   * \brief Update the weights of the particles based on a new measurement.
   *
   * \param[in] z The measurements at the current timestep.
   */
  void update(const vpColVector &z);

  /**
   * \brief Simple function to compute a weighted mean, which just does
   * \f$ \textbf{res} = \sum^{N-1}_{i=0} weights[i] \textbf{particles}[i] \f$
   *
   * \param[in] particles Vector that contains all the particles.
   * \param[in] weights Vector that contains the weights associated to the particles.
   * \return vpColVector \f$ \textbf{res} = \sum^{N-1}_{i=0} weights[i] \textbf{particles}[i] \f$
   */
  inline static vpColVector simpleMean(const std::vector<vpColVector> &particles, const std::vector<double> &weights)
  {
    size_t nbParticles = particles.size();
    if (nbParticles == 0) {
      throw(vpException(vpException::dimensionError, "No particles to add when computing the mean"));
    }
    vpColVector res = particles[0] * weights[0];
    for (size_t i = 1; i < nbParticles; ++i) {
      res += particles[i] * weights[i];
    }
    return res;
  }

private:
  unsigned int m_N; /*!< Number of particles.*/
  std::vector<vpGaussRand> m_noiseGenerators; /*!< The noise generator adding noise to the particles at each time step.*/
  std::vector<vpColVector> m_particles; /*!< The particles.*/
  std::vector<double> m_w; /*!< The weights associated to each particles.*/

  vpColVector m_Xest; /*!< The estimated (i.e. filtered) state variables.*/

  vpProcessFunction m_f; /*!< Process model function, which projects the sigma points forward in time.*/
  vpLikelihoodFunction m_likelihood; /*!< Likelihood function, which evaluates how much a particle matches the measurements.*/
  vpCommandStateFunction m_bx; /*!< Function that permits to compute the effect of the commands on the prior, with knowledge of the state.*/
  vpFilterFunction m_stateFilterFunc; /*!< Function to compute a weighted mean in the state space.*/
  vpResamplingConditionFunction m_checkIfResample; /*!< Return true if resampling must be performed, false otherwise.*/
  vpResamplingFunction m_resampling; /*!< Performs resampling, i.e. samples particles and weights when the particle filter degenerates.*/

  bool m_useProcessFunction; /*!< Set to true when the Particle filter should use the process function.*/
  bool m_useCommandStateFunction; /*!< Set to true when the Particle filter should use the command function.*/
};
END_VISP_NAMESPACE
#endif
#endif
