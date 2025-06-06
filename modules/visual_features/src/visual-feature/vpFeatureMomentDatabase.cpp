/*
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
 * Pseudo-database used to handle dependencies between moment features.
 */

#include <iostream>
#include <typeinfo>
#include <visp3/core/vpConfig.h>
#include <visp3/visual_features/vpFeatureMoment.h>
#include <visp3/visual_features/vpFeatureMomentDatabase.h>

BEGIN_VISP_NAMESPACE
/*!
 * Add a moment and it's corresponding name to the database
 * \param featureMoment : database for moment features
 * \param name : the feature's name, usually the string naming it's class. Each
 * name must be unique
 */
  void vpFeatureMomentDatabase::add(vpFeatureMoment &featureMoment, const std::string &name)
{
  featureMomentsDataBase.insert(std::pair<const std::string, vpFeatureMoment *>(name, &featureMoment));
}

/*!
 * Retrieves a moment feature from the database
 * \param feature_name : The name of the feature, the one specified when using add
 * \param found : true if the type string is found inside the database, false
 * otherwise
 *
 * \return the moment feature corresponding to the type string
 */
vpFeatureMoment &vpFeatureMomentDatabase::get(const std::string &feature_name, bool &found)
{
  std::map<const std::string, vpFeatureMoment *, vpFeatureMomentDatabase::vpCmpStr_t>::const_iterator it =
    featureMomentsDataBase.find(feature_name);

  found = (it != featureMomentsDataBase.end());
  return *(it->second);
}

/*!
 * Update all moment features in the database with plane coefficients
 * \param A : first plane coefficient for a plane equation of the following type Ax+By+C=1/Z
 * \param B : second plane coefficient for a plane equation of the following type Ax+By+C=1/Z
 * \param C : third plane coefficient for a plane equation of the following type Ax+By+C=1/Z
 */
void vpFeatureMomentDatabase::updateAll(double A, double B, double C)
{
  std::map<const std::string, vpFeatureMoment *, vpFeatureMomentDatabase::vpCmpStr_t>::const_iterator itr;
#ifdef VISP_HAVE_OPENMP
  std::vector<vpFeatureMoment *> values;
  values.reserve(featureMomentsDataBase.size());
  for (itr = featureMomentsDataBase.begin(); itr != featureMomentsDataBase.end(); ++itr) {
    values.push_back((*itr).second);
  }
#pragma omp parallel for shared(A, B, C)
  for (int i = 0; i < static_cast<int>(values.size()); i++) {
    unsigned int i_ = static_cast<unsigned int>(i);
    values[i_]->update(A, B, C);
  }
#else
  for (itr = featureMomentsDataBase.begin(); itr != featureMomentsDataBase.end(); ++itr) {
    (*itr).second->update(A, B, C);
  }
#endif
}
END_VISP_NAMESPACE
