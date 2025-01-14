#############################################################################
#
# ViSP, open source Visual Servoing Platform software.
# Copyright (C) 2005 - 2023 by Inria. All rights reserved.
#
# This software is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# See the file LICENSE.txt at the root directory of this source
# distribution for additional information about the GNU GPL.
#
# For using ViSP with software that can not be combined with the GNU
# GPL, please contact Inria about acquiring a ViSP Professional
# Edition License.
#
# See https://visp.inria.fr for more information.
#
# This software was developed at:
# Inria Rennes - Bretagne Atlantique
# Campus Universitaire de Beaulieu
# 35042 Rennes Cedex
# France
#
# If you have questions regarding the use of this file, please contact
# Inria at visp@inria.fr
#
# This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
# WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#
# Description:
# ViSP Python example to simulate an image-based visual servo on four 2D points
#
#############################################################################

""" @example ukf-nonlinear-example.py
 Example of a simple non-linear use-case of the Unscented Kalman Filter (UKF).

 The system we are interested in is an aircraft flying in the sky and
 observed by a radar station.

 We consider the plan perpendicular to the ground and passing by both the radar
 station and the aircraft. The x-axis corresponds to the position on the ground
 and the y-axis to the altitude.

 The state vector of the UKF is:
 \f{eqnarray*}{
      \textbf{x}[0] &=& x \\
      \textbf{x}[1] &=& \dot{x} \\
      \textbf{x}[1] &=& y \\
      \textbf{x}[2] &=& \dot{y}
 \f}

 The measurement \f$ \textbf{z} \f$ corresponds to the distance and angle between the ground and the aircraft
 observed by the radar station. Be \f$ p_x \f$ and \f$ p_y \f$ the position of the radar station
 along the x and y axis, the measurement vector can be written as:
 \f{eqnarray*}{
      \textbf{z}[0] &=& \sqrt{(p_x^i - x)^2 + (p_y^i - y)^2} \\
      \textbf{z}[1] &=& \tan^{-1}{\frac{y - p_y}{x - p_x}}
 \f}

 Some noise is added to the measurement vector to simulate a sensor which is
 not perfect.
"""

from visp.core import ColVector, Matrix, UnscentedKalman, UKSigmaDrawerMerwe, GaussRand, Math
import numpy as np
from typing import List
from math import cos, sin

# For the Graphical User Interface
try:
  from visp.gui import Plot
  has_gui = True
except:
  has_gui = False
import numpy as np

def normalizeAngle(angle: float) -> float:
  angleIn0to2pi = Math.modulo(angle, 2. * np.pi)
  angleInMinPiPi = angleIn0to2pi
  if angleInMinPiPi > np.pi:
    # Substract 2 PI to be in interval [-Pi; Pi]
    angleInMinPiPi = angleInMinPiPi - 2. * np.pi
  return angleInMinPiPi

def measurementMean(measurements: List[ColVector], wm: List[float]) -> ColVector:
  """
  @brief Compute the weighted mean of measurement vectors.

  @param[in] measurements The measurement vectors, such as measurements[i][0] = range and
   measurements[i][1] = elevation_angle.
  @param[in] wm The associated weights.

  @return vpColVector
  """
  nbPoints = len(measurements)
  sumCos = 0.
  sumSin = 0.
  meanRange = 0.
  for i in range(nbPoints):
    meanRange += wm[i] * measurements[i][0]
    sumCos += wm[i] * cos(measurements[i][1])
    sumSin += wm[i] * sin(measurements[i][1])

  meanAngle = np.arctan2(sumSin, sumCos)

  return ColVector([meanRange, meanAngle])

def measurementResidual(meas: ColVector, toSubstract: ColVector) -> ColVector:
  """
  @brief Compute the substraction between two vectors expressed in the measurement space,
   such as v[0] = range ; v[1] = elevation_angle

  @param[in] meas Measurement to which we must substract something.
  @param[in] toSubstract The something we must substract.

  @return vpColVector \b meas - \b toSubstract .
  """
  resTemp = meas - toSubstract
  return ColVector([resTemp[0], normalizeAngle(resTemp[1])])

def add_state_vectors(a, b) -> ColVector:
  """
  @brief Method that permits to add two state vectors.

  @param a The first state vector to which another state vector must be added.
  @param b The other state vector that must be added to a.

  @return ColVector The sum a + b.
  """
  return a + b

def residual_state_vectors(a, b) -> ColVector:
  """
  @brief Method that permits to substract a state vector to another.

  @param a The first state vector to which another state vector must be substracted.
  @param b The other state vector that must be substracted to a.

  @return ColVector The substraction a - b.
  """
  return a - b

def fx(x: ColVector, dt: float) -> ColVector:
  """
  @brief Process function that projects in time the internal state of the UKF.

  @param x The internal state of the UKF.
  @param dt The sampling time: how far in the future are we projecting x.

  @return ColVector The updated internal state, projected in time, also known as the prior.
  """
  return ColVector([
  	x[0] + dt * x[1],
  	x[1],
  	x[2] + dt * x[3],
  	x[3]
  ])

class vpRadarStation:
  """
  @brief Class that permits to convert the position of the aircraft into
  range and elevation angle measurements.
  """
  def __init__(self, x, y, range_std, elev_angle_std):
    """
    @brief Construct a new vpRadarStation

    @param x The position on the ground of the radar.
    @param y The altitude of the radar.
    @param range_std The standard deviation of the range measurements.
    @param elev_angle_std The standard deviation of the elevation angle measurements.
    """
    self.m_x = x
    self.m_y = y
    self.m_stdevRange = range_std
    self.m_stdevElevAngle = elev_angle_std

  def state_to_measurement(self, x) -> ColVector:
    """
    @brief Measurement function that expresses the internal state of the UKF in the measurement space.

    @param x The internal state of the UKF.

    @return ColVector The internal state, expressed in the measurement space.
    """
    dx = x[0] - self.m_x
    dy = x[2] - self.m_y
    range = np.sqrt(dx * dx + dy * dy)
    elev_angle = np.arctan2(dy, dx)
    return ColVector([range, elev_angle])

  def measureGT(self, pos) -> ColVector:
    """
    @brief Perfect measurement of the range and elevation angle that
    correspond to pos.

    @param pos The actual position of the aircraft (pos[0]: projection of the position
    on the ground, pos[1]: altitude).

    @return ColVector [0] the range [1] the elevation angle.
    """
    dx = pos[0] - self.m_x
    dy = pos[1] - self.m_y
    range = np.sqrt(dx * dx + dy * dy)
    elev_angle = np.arctan2(dy, dx)
    return ColVector([range, elev_angle])

  def measureWithNoise(self, pos: ColVector) -> ColVector:
    """
    @brief Noisy measurement of the range and elevation angle that
    correspond to pos.

    @param pos The actual position of the aircraft (pos[0]: projection of the position
     on the ground, pos[1]: altitude).
    @return vpColVector [0] the range [1] the elevation angle.
    """
    measurementsGT = self.measureGT(pos)
    measurementsNoisy = ColVector([measurementsGT[0] + np.random.normal(0., self.m_stdevRange), measurementsGT[1] + np.random.normal(0., self.m_stdevElevAngle)])
    return measurementsNoisy

class vpACSimulator:
  """
  @brief Class to simulate a flying aircraft.
  """

  def __init__(self, X0: ColVector, vel: ColVector, vel_std: float):
    """
    @brief Construct a new vpACSimulator object.

    @param X0 Initial position of the aircraft.
    @param vel Velocity of the aircraft.
    @param vel_std Standard deviation of the variation of the velocity.
    """
    self.m_pos = X0 # Position of the simulated aircraft
    self.m_vel = vel # Velocity of the simulated aircraft
    np.random.seed(4224)
    self.m_stdevVel = vel_std # Standard deviation of the random generator for slight variations of the velocity of the aircraft


  def update(self, dt: float) -> ColVector:
    """
    \brief Compute the new position of the aircraft after dt seconds have passed
    since the last update.

    \param[in] dt Period since the last update.
    \return vpColVector The new position of the aircraft.
    """
    dx_temp = self.m_vel * dt
    dx = ColVector([dx_temp[0] + np.random.normal(0., self.m_stdevVel) * dt, dx_temp[1] + np.random.normal(0., self.m_stdevVel) * dt])
    self.m_pos += dx
    return self.m_pos

def generate_Q_matrix(dt: float) -> Matrix:
  """
  @brief Method that generates the process covariance matrix for a process for which the
  state vector can be written as (x, dx/dt)^T

  @param dt The sampling period.

  @return Matrix The corresponding process covariance matrix.
  """
  return Matrix(
  	[[dt**3/3, dt**2/2, 0, 0],
  	[dt**2/2, dt, 0, 0],
  	[0, 0, dt**3/3, dt**2/2],
  	[0, 0, dt**2/2, dt]])

def generate_P0_matrix() -> Matrix:
  """
  @brief Method that generates the intial guess of the state covariance matrix.

  @return Matrix The corresponding state covariance matrix.
  """
  return Matrix(
  	[[300*300, 0, 0, 0],
  	[0, 30*30, 0, 0],
  	[0, 0, 150*150, 0],
  	[0, 0, 0, 30*30]])

if __name__ == '__main__':
  dt = 3. # The sampling period
  gt_X_init = -500. # Ground truth initial position along the X-axis, in meters
  gt_Y_init = 1000. # Ground truth initial position along the Y-axis, in meters
  gt_vX_init = 100. # The velocity along the x-axis
  gt_vY_init = 5. # The velocity along the y-axis
  procVar = 0.1 # The variance of the process function
  sigmaRange = 5 # Standard deviation of the range measurement: 5m
  sigmaElevAngle = Math.rad(0.5) # Standard deviation of the elevation angle measurent: 0.5deg
  stdevAircraftVelocity = 0.2; # Standard deviation of the velocity of the simulated aircraft,
                               # to make it deviate a bit from the constant velocity model

  # The object that draws the sigma points used by the UKF
  drawer = UKSigmaDrawerMerwe(n=4, alpha=0.3, beta=2, kappa=-1, resFunc=residual_state_vectors, addFunc=add_state_vectors)

  # The object that performs radar measurements
  radar = vpRadarStation(0., 0., sigmaRange, sigmaElevAngle)

  P0 = generate_P0_matrix() # The initial estimate of the state covariance matrix
  R = Matrix([[sigmaRange * sigmaRange, 0], [0, sigmaElevAngle * sigmaElevAngle]]) # The measurement covariance matrix
  Q = generate_Q_matrix(dt) * procVar # The process covariance matrix
  ukf = UnscentedKalman(Q, R, drawer, fx, radar.state_to_measurement) # The Unscented Kalman Filter instance

  # Initializing the state vector and state covariance matrix estimates
  ukf.init(ColVector([0.9 * gt_X_init, 0.9 * gt_vX_init, 0.9 * gt_Y_init, 0.9 * gt_vY_init]), P0)
  ukf.setMeasurementMeanFunction(measurementMean)
  ukf.setMeasurementResidualFunction(measurementResidual)

  # Initializing the Graphical User Interface if the needed libraries are available
  if has_gui:
    num_plots = 4
    plot = Plot(num_plots)
    plot_titles = [
      'Position along X-axis', 'Velocity along X-axis',
      'Position along Y-axis', 'Velocity along Y-axis'
    ]
    plot_y_units = [
      'Position (m)', 'Velocity (m/s)',
      'Position (m)', 'Velocity (m/s)'
    ]
    plot_legends = ['GT', 'Filtered']

    # Initializing the subplots
    for plot_index in range(num_plots):
      plot.initGraph(plot_index, len(plot_legends))
      plot.setTitle(plot_index, plot_titles[plot_index])
      plot.setUnitY(plot_index, plot_y_units[plot_index])
      plot.setUnitX(plot_index, 'Time (s)')
      for legend_index in range(len(plot_legends)):
        plot.setLegend(plot_index, legend_index, plot_legends[legend_index])

  ac_pos = ColVector([gt_X_init, gt_Y_init]) # Ground truth position
  ac_vel = ColVector([gt_vX_init, gt_vY_init]) # Ground truth velocity
  ac = vpACSimulator(ac_pos, ac_vel, stdevAircraftVelocity)
  gt_Xprec = ColVector([ac_pos[0], ac_pos[1]])
  for i in range(500):
    # Creating noisy measurements
    gt_X = ac.update(dt)
    gt_V = (gt_X - gt_Xprec) / dt
    z = radar.measureWithNoise(gt_X)

    # Filtering using the UKF
    ukf.filter(z, dt)

    # Getting the filtered state vector
    Xest = ukf.getXest()

    # Update the GUI if available
    if has_gui:
      plot.plot(0, 0, i, gt_X[0])
      plot.plot(0, 1, i, Xest[0])

      plot.plot(1, 0, i, gt_V[0])
      plot.plot(1, 1, i, Xest[1])

      plot.plot(2, 0, i, gt_X[1])
      plot.plot(2, 1, i, Xest[2])

      plot.plot(3, 0, i, gt_V[1])
      plot.plot(3, 1, i, Xest[3])

    # Updating last measurement for future computation of the noisy velocity
    gt_Xprec = ColVector([gt_X[0], gt_X[1]])

  print('Finished')
  input('Press enter to quit')
