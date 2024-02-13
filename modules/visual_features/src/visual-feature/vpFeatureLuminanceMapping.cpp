#include <visp3/visual_features/vpFeatureLuminanceMapping.h>

#ifdef VISP_HAVE_MODULE_IO
#include <visp3/io/vpImageIo.h>
#endif

vpLuminancePCA::vpLuminancePCA(std::shared_ptr<vpMatrix> basis, std::shared_ptr<vpColVector> mean) : vpLuminanceMapping(basis->getRows()), m_basis(basis), m_mean(mean)
{
  if (basis->getCols() != mean->getRows()) {
    throw vpException(vpException::dimensionError, "PCA mean and basis should have the same number of inputs");
  }
}

void vpLuminancePCA::map(const vpImage<unsigned char> &I, vpColVector &s)
{

}
void vpLuminancePCA::inverse(const vpColVector &s, vpImage<unsigned char> &I)
{

}

void vpLuminancePCA::interaction(const vpImage<unsigned char> &I, const vpMatrix &LI, const vpColVector &s, vpMatrix &L)
{
  L = (*m_basis) * LI;
}

vpLuminancePCA vpLuminancePCA::load(const std::string &basisFilename, const std::string &meanFilename)
{
  std::shared_ptr<vpMatrix> basis = std::make_shared<vpMatrix>();
  std::shared_ptr<vpColVector> mean = std::make_shared<vpColVector>();
  vpMatrix::loadMatrix(basisFilename, *basis, true);
  vpColVector::load(meanFilename, *mean);

  if (mean->getCols() > 1) {
    throw vpException(vpException::badValue,
    "Read something that was not a column vector when trying to load the PCA mean vector");
  }
  if (basis->getCols() != mean->getRows()) {
    std::stringstream ss;
    ss << "Error when loading PCA from binary files";
    ss << "The basis matrix had dimensions (" << basis->getRows() << ", " << basis->getCols() << ")";
    ss << " and the mean vector had size " << mean->getRows() << ".";
    ss << "You may be loading data from two different PCAs";
    throw vpException(vpException::dimensionError, ss.str());
  }

  return vpLuminancePCA(basis, mean);
}

void vpLuminancePCA::save(const std::string &basisFilename, const std::string &meanFilename) const
{
  if (m_basis.get() == nullptr || m_mean.get() == nullptr) {
    throw vpException(vpException::notInitialized,
    "Tried to save a PCA projection that was uninitialized");
  }
  if (m_basis->size() == 0 || m_mean->getCols() == 0 || m_basis->getCols() != m_mean->getRows()) {
    throw vpException(vpException::dimensionError,
    "Tried to save a PCA projection but there are issues with the basis and mean dimensions");
  }
  vpMatrix::saveMatrix(basisFilename, *m_basis, true);
  vpColVector::save(meanFilename, *m_mean, true);
}

vpLuminancePCA vpLuminancePCA::learn(const std::vector<vpImage<unsigned char>> &images, const unsigned int projectionSize, const unsigned int border)
{


  vpMatrix matrix;
  for (unsigned i = 0; i < images.size(); ++i) {
    const vpImage<unsigned char> &I = images[i];
    if (i == 0) {
      matrix.resize(images.size(), (I.getHeight() - 2 * border) * (I.getWidth() - 2 * border));
    }
    if ((I.getHeight() - 2 * border) * (I.getWidth() - 2 * border) != matrix.getCols()) {
      throw vpException(vpException::badValue, "Not all images have the same dimensions when learning pca");
    }
    for (unsigned j = border; j < I.getHeight() - border; ++j) {
      for (unsigned k = border; k < I.getWidth() - border; ++k) {
        matrix[i][(j - border) * (I.getWidth() - 2 * border) + k - border] = I[j][k];
      }
    }
  }

  return vpLuminancePCA::learn(matrix.transpose(), projectionSize);
}
#ifdef VISP_HAVE_MODULE_IO
vpLuminancePCA vpLuminancePCA::learn(const std::vector<std::string> &imageFiles, const unsigned int projectionSize, const unsigned int border)
{
  vpMatrix matrix;
  vpImage<unsigned char> I;
  for (unsigned i = 0; i < imageFiles.size(); ++i) {
    vpImageIo::read(I, imageFiles[i]);
    if (i == 0) {
      matrix.resize(imageFiles.size(), (I.getHeight() - 2 * border) * (I.getWidth() - 2 * border));
    }
    if ((I.getHeight() - 2 * border) * (I.getWidth() - 2 * border) != matrix.getCols()) {
      throw vpException(vpException::badValue, "Not all images have the same dimensions when learning pca");
    }
    for (unsigned j = border; j < I.getHeight() - border; ++j) {
      for (unsigned k = border; k < I.getWidth() - border; ++k) {
        matrix[i][(j - border) * (I.getWidth() - 2 * border) + k - border] = I[j][k];
      }
    }
  }
  return vpLuminancePCA::learn(matrix.transpose(), projectionSize);
}
#endif

vpLuminancePCA vpLuminancePCA::learn(const vpMatrix &images, const unsigned int projectionSize)
{
  if (projectionSize > images.getRows() && projectionSize > images.getCols()) {
    throw vpException(vpException::badValue, "Cannot use a subspace greater than the data dimensions (number of pixels or images)");
  }
  // Mean computation
  vpColVector mean(images.getRows(), 0.0);
  for (unsigned i = 0; i < images.getCols(); ++i) {
    mean += images.getCol(i);
  }
  mean /= images.getCols();

  // Before SVD, center data
  vpMatrix centered(images.getRows(), images.getCols());
  for (unsigned i = 0; i < centered.getRows(); ++i) {
    for (unsigned j = 0; j < centered.getCols(); ++j) {
      centered[i][j] = images[i][j] - mean[i];
    }
  }

  vpColVector eigenValues;
  vpMatrix V;
  centered.svd(eigenValues, V);

  std::shared_ptr<vpMatrix> basis = std::make_shared<vpMatrix>(projectionSize, centered.getRows());
  for (unsigned i = 0; i < projectionSize; ++i) {
    for (unsigned j = 0; j < centered.getRows(); ++j) {
      (*basis)[i][j] = centered[j][i];
    }
  }
  std::shared_ptr<vpColVector> meanPtr = std::make_shared<vpColVector>(mean);
  std::cout << centered.getRows() << " " << centered.getCols() << std::endl;
  return vpLuminancePCA(basis, meanPtr);
}


vpFeatureLuminanceMapping::vpFeatureLuminanceMapping(const vpCameraParameters &cam,
unsigned int h, unsigned int w, double Z, std::shared_ptr<vpLuminanceMapping> mapping)
{
  init(cam, h, w, Z, mapping);
}

vpFeatureLuminanceMapping::vpFeatureLuminanceMapping(const vpFeatureLuminance &luminance, std::shared_ptr<vpLuminanceMapping> mapping)
{
  init(luminance, mapping);
}
vpFeatureLuminanceMapping::vpFeatureLuminanceMapping(const vpFeatureLuminanceMapping &f)
{
  *this = f;
}

void vpFeatureLuminanceMapping::init()
{
  dim_s = 0;
  m_featI.init(0, 0, 0.0);
}

void vpFeatureLuminanceMapping::init(
  const vpCameraParameters &cam, unsigned int h, unsigned int w, double Z,
  std::shared_ptr<vpLuminanceMapping> mapping)
{
  m_featI.init(h, w, Z);
  m_featI.setCameraParameters(cam);
  m_mapping = mapping;
  dim_s = m_mapping->getProjectionSize();
  s.resize(dim_s, true);
}
void vpFeatureLuminanceMapping::init(const vpFeatureLuminance &luminance, std::shared_ptr<vpLuminanceMapping> mapping)
{
  m_featI = luminance;
  m_mapping = mapping;
  dim_s = m_mapping->getProjectionSize();
  s.resize(dim_s, true);
}


vpFeatureLuminanceMapping &vpFeatureLuminanceMapping::operator=(const vpFeatureLuminanceMapping &f)
{
  dim_s = f.dim_s;
  s = f.s;
  m_mapping = f.m_mapping;
  m_featI = f.m_featI;
  return *this;
}
vpFeatureLuminanceMapping *vpFeatureLuminanceMapping::duplicate() const
{
  return new vpFeatureLuminanceMapping(*this);
}


void vpFeatureLuminanceMapping::buildFrom(vpImage<unsigned char> &I) { }

void vpFeatureLuminanceMapping::display(const vpCameraParameters &cam, const vpImage<unsigned char> &I, const vpColor &color,
              unsigned int thickness) const
{ }
void vpFeatureLuminanceMapping::display(const vpCameraParameters &cam, const vpImage<vpRGBa> &I, const vpColor &color,
              unsigned int thickness) const
{ }


vpColVector vpFeatureLuminanceMapping::error(const vpBasicFeature &s_star, unsigned int select)
{
  if (select != FEATURE_ALL) {
    throw vpException(vpException::notImplementedError, "cannot compute error on subset of PCA features");
  }
  vpColVector e(dim_s);
  error(s_star, e);
  return e;
}

void vpFeatureLuminanceMapping::error(const vpBasicFeature &s_star, vpColVector &e)
{
  // if (dim_s != s_star.dimension_s()) {
  //   throw vpException(vpException::dimensionError, "Wrong dimensions when computing error in PCA features");
  // }
  e.resize(dim_s, false);
  for (unsigned int i = 0; i < dim_s; i++) {
    e[i] = s[i] - s_star[i];
  }
}

vpMatrix vpFeatureLuminanceMapping::interaction(unsigned int select)
{
  if (select != FEATURE_ALL) {
    throw vpException(vpException::notImplementedError, "cannot compute interaction matrix for a subset of PCA features");
  }
  vpMatrix dWdr(dim_s, 6);
  interaction(dWdr);
  return dWdr;
}
void vpFeatureLuminanceMapping::interaction(vpMatrix &L)
{
  L.resize(dim_s, 6, false, false);
  m_featI.interaction(m_LI);
}


void vpFeatureLuminanceMapping::print(unsigned int select) const
{
  if (select != FEATURE_ALL) {
    throw vpException(vpException::notImplementedError, "cannot print subset of PCA features");
  }

}
