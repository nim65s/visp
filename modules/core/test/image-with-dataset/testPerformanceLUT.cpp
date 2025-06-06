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
 * Test performance between iteration and LUT.
 */

#include <stdio.h>
#include <stdlib.h>
#include <visp3/core/vpImage.h>
#include <visp3/core/vpIoTools.h>
#include <visp3/core/vpMath.h>
#include <visp3/io/vpImageIo.h>
#include <visp3/io/vpParseArgv.h>

#ifdef ENABLE_VISP_NAMESPACE
using namespace VISP_NAMESPACE_NAME;
#endif

/*!
 * \example testPerformanceLUT.cpp
 *
 * \brief Test performance between iteration and LUT.
 */

// List of allowed command line options
#define GETOPTARGS "cdi:o:t:h"

/*
 * Print the program options.
 *
 * \param name : Program name.
 * \param badparam : Bad parameter name.
 * \param ipath : Input image path.
 * \param opath : Output image path.
 * \param user : Username.
 */
void usage(const char *name, const char *badparam, const std::string &ipath, const std::string &opath,
           const std::string &user)
{
  fprintf(stdout, "\n\
Test performance between methods to iterate over pixel image.\n\
\n\
SYNOPSIS\n\
  %s [-i <input image path>] [-o <output image path>] [-t <nb threads>]\n\
     [-h]\n\
",
name);

  fprintf(stdout, "\n\
OPTIONS:                                               Default\n\
  -i <input image path>                                %s\n\
     Set image input path.\n\
     From this path read \"Klimt/Klimt.pgm\"\n\
     image.\n\
     Setting the VISP_INPUT_IMAGE_PATH environment\n\
     variable produces the same behaviour than using\n\
     this option.\n\
\n\
  -o <output image path>                               %s\n\
     Set image output path.\n\
     From this directory, creates the \"%s\"\n\
     subdirectory depending on the username, where \n\
     Klimt_grey.pgm output image is written.\n\
\n\
  -t <nb threads>                               \n\
     Set the number of threads to use for the computation.\n\
\n\
  -h\n\
     Print the help.\n\n",
          ipath.c_str(), opath.c_str(), user.c_str());

  if (badparam)
    fprintf(stdout, "\nERROR: Bad parameter [%s]\n", badparam);
}

/*!
 * Set the program options.
 *
 * \param argc : Command line number of parameters.
 * \param argv : Array of command line parameters.
 * \param ipath : Input image path.
 * \param opath : Output image path.
 * \param user : Username.
 * \param nbThreads : Number of threads to use.
 * \return false if the program has to be stopped, true otherwise.
 */
bool getOptions(int argc, const char **argv, std::string &ipath, std::string &opath, const std::string &user,
                unsigned int &nbThreads)
{
  const char *optarg_;
  int c;
  while ((c = vpParseArgv::parse(argc, argv, GETOPTARGS, &optarg_)) > 1) {

    switch (c) {
    case 'i':
      ipath = optarg_;
      break;
    case 'o':
      opath = optarg_;
      break;
    case 't':
      nbThreads = static_cast<unsigned int>(atoi(optarg_));
      break;
    case 'h':
      usage(argv[0], nullptr, ipath, opath, user);
      return false;
      break;

    case 'c':
    case 'd':
      break;

    default:
      usage(argv[0], optarg_, ipath, opath, user);
      return false;
      break;
    }
  }

  if ((c == 1) || (c == -1)) {
    // standalone param or error
    usage(argv[0], nullptr, ipath, opath, user);
    std::cerr << "ERROR: " << std::endl;
    std::cerr << "  Bad argument " << optarg_ << std::endl << std::endl;
    return false;
  }

  return true;
}

unsigned char getRandomValues(unsigned char min, unsigned char max)
{
  return static_cast<unsigned char>((max - min) * static_cast<double>(rand()) / static_cast<double>(RAND_MAX) + min);
}

void generateRandomImage(vpImage<unsigned char> &I, unsigned int min = 0, unsigned int max = 255)
{
  for (unsigned int i = 0; i < I.getHeight(); ++i) {
    for (unsigned int j = 0; j < I.getWidth(); ++j) {
      I[i][j] = getRandomValues(min, max);
    }
  }
}

void generateRandomImage(vpImage<vpRGBa> &I, unsigned int min = 0, unsigned int max = 255)
{
  for (unsigned int i = 0; i < I.getHeight(); ++i) {
    for (unsigned int j = 0; j < I.getWidth(); ++j) {
      I[i][j].R = getRandomValues(min, max);
      I[i][j].G = getRandomValues(min, max);
      I[i][j].B = getRandomValues(min, max);
      I[i][j].A = getRandomValues(min, max);
    }
  }
}

/*!
 * Iterate over pixels using raw pointer and adjust the pixel intensities with
 * the formula: new_intensity = old_intensity * alpha + beta.
 *
 * \param I : Input color image.
 * \param alpha : Gain.
 * \param beta : Offset.
 */
void iterate_method1(vpImage<vpRGBa> &I, double alpha, double beta)
{
  unsigned int size = I.getWidth() * I.getHeight();
  unsigned char *ptrStart = (unsigned char *)I.bitmap;
  unsigned char *ptrEnd = ptrStart + size * 4;
  unsigned char *ptrCurrent = ptrStart;

  while (ptrCurrent != ptrEnd) {
    *ptrCurrent = vpMath::saturate<unsigned char>((*ptrCurrent) * alpha + beta);
    ++ptrCurrent;
  }
}

/*!
 * Iterate over pixels using raw pointer and adjust the pixel intensities with
 * the formula: new_intensity = old_intensity * alpha + beta.
 *
 * \param I : Input grayscale image.
 * \param alpha : Gain.
 * \param beta : Offset.
 */
void iterate_method1(vpImage<unsigned char> &I, double alpha, double beta)
{
  unsigned int size = I.getWidth() * I.getHeight();
  unsigned char *ptrStart = (unsigned char *)I.bitmap;
  unsigned char *ptrEnd = ptrStart + size;
  unsigned char *ptrCurrent = ptrStart;

  while (ptrCurrent != ptrEnd) {
    *ptrCurrent = vpMath::saturate<unsigned char>((*ptrCurrent) * alpha + beta);
    ++ptrCurrent;
  }
}

/*!
 * Iterate over pixels using a double for loop and adjust the pixel intensities
 * with the formula: new_intensity = old_intensity * alpha + beta.
 *
 * \param I : Input color image.
 * \param alpha : Gain.
 * \param beta : Offset.
 */
void iterate_method2(vpImage<vpRGBa> &I, double alpha, double beta)
{
  for (unsigned int i = 0; i < I.getHeight(); i++) {
    for (unsigned int j = 0; j < I.getWidth(); j++) {
      I[i][j].R = vpMath::saturate<unsigned char>(I[i][j].R * alpha + beta);
      I[i][j].G = vpMath::saturate<unsigned char>(I[i][j].G * alpha + beta);
      I[i][j].B = vpMath::saturate<unsigned char>(I[i][j].B * alpha + beta);
      I[i][j].A = vpMath::saturate<unsigned char>(I[i][j].A * alpha + beta);
    }
  }
}

int main(int argc, const char **argv)
{
  try {
    std::string env_ipath;
    std::string opt_ipath;
    std::string opt_opath;
    std::string ipath;
    std::string opath;
    std::string filename;
    std::string username;
    unsigned int nbThreads = 4;

    // Get the visp-images-data package path or VISP_INPUT_IMAGE_PATH
    // environment variable value
    env_ipath = vpIoTools::getViSPImagesDataPath();

    // Set the default input path
    if (!env_ipath.empty())
      ipath = env_ipath;

// Set the default output path
#if defined(_WIN32)
    opt_opath = "C:/temp";
#else
    opt_opath = "/tmp";
#endif

    // Get the user login name
    vpIoTools::getUserName(username);

    // Read the command line options
    if (getOptions(argc, argv, opt_ipath, opt_opath, username, nbThreads) == false) {
      return EXIT_FAILURE;
    }

    // Get the option values
    if (!opt_ipath.empty())
      ipath = opt_ipath;
    if (!opt_opath.empty())
      opath = opt_opath;

    // Append to the output path string, the login name of the user
    opath = vpIoTools::createFilePath(opath, username);

    // Test if the output path exist. If no try to create it
    if (vpIoTools::checkDirectory(opath) == false) {
      try {
        // Create the dirname
        vpIoTools::makeDirectory(opath);
      }
      catch (...) {
        usage(argv[0], nullptr, ipath, opt_opath, username);
        std::cerr << std::endl << "ERROR:" << std::endl;
        std::cerr << "  Cannot create " << opath << std::endl;
        std::cerr << "  Check your -o " << opt_opath << " option " << std::endl;
        return EXIT_FAILURE;
      }
    }

    // Compare ipath and env_ipath. If they differ, we take into account
    // the input path coming from the command line option
    if (!opt_ipath.empty() && !env_ipath.empty()) {
      if (ipath != env_ipath) {
        std::cout << std::endl << "WARNING: " << std::endl;
        std::cout << "  Since -i <visp image path=" << ipath << "> "
          << "  is different from VISP_IMAGE_PATH=" << env_ipath << std::endl
          << "  we skip the environment variable." << std::endl;
      }
    }

    // Test if an input path is set
    if (opt_ipath.empty() && env_ipath.empty()) {
      usage(argv[0], nullptr, ipath, opt_opath, username);
      std::cerr << std::endl << "ERROR:" << std::endl;
      std::cerr << "  Use -i <visp image path> option or set VISP_INPUT_IMAGE_PATH " << std::endl
        << "  environment variable to specify the location of the " << std::endl
        << "  image path where test images are located." << std::endl
        << std::endl;
      return EXIT_FAILURE;
    }

    double alpha = 1.5, beta = -30.0;
    unsigned int nbIterations = 10;

    //
    // Here starts really the test
    //
    if (1) {
      std::cout << "\n** Test LUT on color image" << std::endl;
      // Create a color image
      vpImage<vpRGBa> I_iterate1, I_iterate2, I_lut;

      // Load a color image from the disk
      filename = vpIoTools::createFilePath(ipath, "Klimt/Klimt.ppm");
      std::cout << "Read image: " << filename << std::endl;
      vpImageIo::read(I_iterate1, filename);
      vpImageIo::read(I_iterate2, filename);
      vpImageIo::read(I_lut, filename);

      std::cout << "Image size: " << I_iterate1.getWidth() << "x" << I_iterate1.getHeight() << std::endl;

      // Iterate method 1
      std::cout << "Run test n°1 " << nbIterations << " times" << std::endl;
      double t_iterate1 = vpTime::measureTimeMs();
      for (unsigned int cpt = 0; cpt < nbIterations; cpt++) {
        iterate_method1(I_iterate1, alpha, beta);
      }
      t_iterate1 = vpTime::measureTimeMs() - t_iterate1;
      std::cout << "  Total time: " << t_iterate1 << " ms ; Mean time: "
        << (t_iterate1 / nbIterations) << " ms" << std::endl;

      filename = vpIoTools::createFilePath(opath, "Klimt_performance_iterate1.ppm");
      std::cout << "  Save " << filename << std::endl;
      vpImageIo::write(I_iterate1, filename);

      // Iterate method 2
      std::cout << "Run test n°2 " << nbIterations << " times" << std::endl;
      double t_iterate2 = vpTime::measureTimeMs();
      for (unsigned int cpt = 0; cpt < nbIterations; cpt++) {
        iterate_method2(I_iterate2, alpha, beta);
      }
      t_iterate2 = vpTime::measureTimeMs() - t_iterate2;
      std::cout << "  Total time: " << t_iterate2 << " ms ; Mean time: " << (t_iterate2 / nbIterations) << " ms" << std::endl;

      filename = vpIoTools::createFilePath(opath, "Klimt_performance_iterate2.ppm");
      std::cout << "  Save " << filename << std::endl;
      vpImageIo::write(I_iterate2, filename);

      // Construct the LUT
      vpRGBa lut[256];
      for (unsigned int i = 0; i < 256; i++) {
        lut[i].R = vpMath::saturate<unsigned char>(alpha * i + beta);
        lut[i].G = vpMath::saturate<unsigned char>(alpha * i + beta);
        lut[i].B = vpMath::saturate<unsigned char>(alpha * i + beta);
        lut[i].A = vpMath::saturate<unsigned char>(alpha * i + beta);
      }

      // LUT method
      std::cout << "Run test n°3 " << nbIterations << " times" << std::endl;
      double t_lut = vpTime::measureTimeMs();
      for (unsigned int cpt = 0; cpt < nbIterations; cpt++) {
        I_lut.performLut(lut, nbThreads);
      }
      t_lut = vpTime::measureTimeMs() - t_lut;
      std::cout << "  Total time: " << t_lut << " ms ; Mean time: " << (t_lut / nbIterations) << " ms" << std::endl;

      filename = vpIoTools::createFilePath(opath, "Klimt_performance_lut.ppm");
      std::cout << "  Save " << filename << std::endl;
      vpImageIo::write(I_lut, filename);

      if ((I_iterate1 == I_iterate2) && (I_iterate1 == I_lut)) {
        std::cerr << "Color images are the same" << std::endl;
      }
      else {
        std::cerr << "Color images are different!" << std::endl;
        std::cout << "Test failed" << std::endl;
        return EXIT_FAILURE;
      }
    }
    {
      // Test LUT on grayscale image
      std::cout << "\n** Test LUT on grayscale image" << std::endl;
      vpImage<unsigned char> I_iterate_grayscale1, I_lut_grayscale;

      // Load a grayscale image from the disk
      filename = vpIoTools::createFilePath(ipath, "Klimt/Klimt.pgm");
      std::cout << "Read image: " << filename << std::endl;
      vpImageIo::read(I_iterate_grayscale1, filename);
      vpImageIo::read(I_lut_grayscale, filename);

      std::cout << "Image size: " << I_lut_grayscale.getWidth() << "x" << I_lut_grayscale.getHeight() << std::endl;

      // Iterate method 1 on grayscale
      std::cout << "Run test n°1 " << nbIterations << " times" << std::endl;
      double t_iterate_grayscale1 = vpTime::measureTimeMs();
      for (unsigned int cpt = 0; cpt < nbIterations; cpt++) {
        iterate_method1(I_iterate_grayscale1, alpha, beta);
      }
      t_iterate_grayscale1 = vpTime::measureTimeMs() - t_iterate_grayscale1;
      std::cout << "  Total time: " << t_iterate_grayscale1 << " ms ; Mean time: "
        << (t_iterate_grayscale1 / nbIterations) << " ms" << std::endl;

      filename = vpIoTools::createFilePath(opath, "Klimt_performance_iterate1_grayscale.pgm");
      std::cout << "  Save result in " << filename << std::endl;
      vpImageIo::write(I_iterate_grayscale1, filename);

      // Construct the LUT
      unsigned char lut[256];
      for (unsigned int i = 0; i < 256; i++) {
        lut[i] = vpMath::saturate<unsigned char>(alpha * i + beta);
      }

      // LUT method on grayscale
      std::cout << "Run test n°2 " << nbIterations << " times with " << nbThreads << " threads" << std::endl;
      double t_lut_grayscale = vpTime::measureTimeMs();
      for (unsigned int cpt = 0; cpt < nbIterations; cpt++) {
        I_lut_grayscale.performLut(lut, nbThreads);
      }
      t_lut_grayscale = vpTime::measureTimeMs() - t_lut_grayscale;
      std::cout << "  Total time: " << t_lut_grayscale << " ms ; Mean time: "
        << (t_lut_grayscale / nbIterations) << " ms" << std::endl;

      filename = vpIoTools::createFilePath(opath, "Klimt_performance_lut_grayscale.pgm");
      std::cout << "  Save result in " << filename << std::endl;
      vpImageIo::write(I_lut_grayscale, filename);

      // Check grayscale image
      if (I_lut_grayscale == I_iterate_grayscale1) {
        std::cout << "Grayscale images are same" << std::endl;
      }
      else {
        std::cerr << "Grayscale images are different!" << std::endl;
        std::cout << "Test failed" << std::endl;
        return EXIT_FAILURE;
      }
    }
    {
      std::cout << "\n** Test multi-threaded LUT on color image" << std::endl;
      vpImage<vpRGBa> I_lut_multi, I_lut_single;

      // Load a color image from the disk
      filename = vpIoTools::createFilePath(ipath, "Klimt/Klimt.ppm");
      std::cout << "Read image: " << filename << std::endl;
      vpImageIo::read(I_lut_multi, filename);

      // Construct the LUT
      vpRGBa lut[256];
      for (unsigned int i = 0; i < 256; i++) {
        lut[i].R = vpMath::saturate<unsigned char>(alpha * i + beta);
        lut[i].G = vpMath::saturate<unsigned char>(alpha * i + beta);
        lut[i].B = vpMath::saturate<unsigned char>(alpha * i + beta);
        lut[i].A = vpMath::saturate<unsigned char>(alpha * i + beta);
      }

      // Computation time on color image
      std::cout << "Run test n°1 " << nbIterations* 10 << " times with " << nbThreads << " threads" << std::endl;
      double t_lut_multithread = vpTime::measureTimeMs();
      for (unsigned int cpt = 0; cpt < nbIterations * 10; cpt++) {
        I_lut_multi.performLut(lut, nbThreads);
      }
      t_lut_multithread = vpTime::measureTimeMs() - t_lut_multithread;

      filename = vpIoTools::createFilePath(opath, "Klimt_performance_color_multi.ppm");
      std::cout << "  Save result in " << filename << std::endl;
      vpImageIo::write(I_lut_multi, filename);

      vpImageIo::read(I_lut_single, filename);

      std::cout << "Run test n°2 " << nbIterations* 10 << " times in a single thread" << std::endl;
      double t_lut_singlethread = vpTime::measureTimeMs();
      for (unsigned int cpt = 0; cpt < nbIterations * 10; cpt++) {
        I_lut_single.performLut(lut, 1);
      }
      t_lut_singlethread = vpTime::measureTimeMs() - t_lut_singlethread;

      filename = vpIoTools::createFilePath(opath, "Klimt_performance_color_single.ppm");
      std::cout << "  Save result in " << filename << std::endl;
      vpImageIo::write(I_lut_single, filename);

      // Check color image
      if (I_lut_multi == I_lut_single) {
        std::cout << "Color images are the same" << std::endl;
        std::cout << "Single-thread / multi-thread (color) gain: " << t_lut_singlethread / t_lut_multithread << std::endl;
      }
      else {
        std::cerr << "Color images are different!" << std::endl;
        std::cout << "Test failed" << std::endl;
        return EXIT_FAILURE;
      }
    }
    {
      std::cout << "\n** Test multi-threaded LUT on gray image" << std::endl;
      vpImage<unsigned char> I_lut_grayscale_multi, I_lut_grayscale_single;

      // Load a gray image from the disk
      filename = vpIoTools::createFilePath(ipath, "Klimt/Klimt.pgm");

      std::cout << "Read image: " << filename << std::endl;
      vpImageIo::read(I_lut_grayscale_multi, filename);

      // Construct the LUT
      unsigned char lut[256];
      for (unsigned int i = 0; i < 256; i++) {
        lut[i] = vpMath::saturate<unsigned char>(alpha * i + beta);
      }

      // Computation time on grayscale image
      std::cout << "Run test n°1 " << nbIterations* 10 << " times with " << nbThreads << " threads" << std::endl;
      double t_lut_multithread = vpTime::measureTimeMs();
      for (unsigned int cpt = 0; cpt < nbIterations * 10; cpt++) {
        I_lut_grayscale_multi.performLut(lut, nbThreads);
      }
      t_lut_multithread = vpTime::measureTimeMs() - t_lut_multithread;

      filename = vpIoTools::createFilePath(opath, "Klimt_performance_gray_multi.pgm");
      std::cout << "  Save result in " << filename << std::endl;
      vpImageIo::write(I_lut_grayscale_multi, filename);

      vpImageIo::read(I_lut_grayscale_single, filename);

      std::cout << "Run test n°2 " << nbIterations* 10 << " times in a single thread" << std::endl;
      double t_lut_singlethread = vpTime::measureTimeMs();
      for (unsigned int cpt = 0; cpt < nbIterations * 10; cpt++) {
        I_lut_grayscale_single.performLut(lut, 1);
      }
      t_lut_singlethread = vpTime::measureTimeMs() - t_lut_singlethread;

      filename = vpIoTools::createFilePath(opath, "Klimt_performance_gray_single.pgm");
      std::cout << "  Save result in " << filename << std::endl;
      vpImageIo::write(I_lut_grayscale_single, filename);

      // Check color image
      if (I_lut_grayscale_multi == I_lut_grayscale_single) {
        std::cout << "Gray images are the same" << std::endl;
        std::cout << "Single-thread / multi-thread (color) gain: " << t_lut_singlethread / t_lut_multithread << std::endl;
      }
      else {
        std::cerr << "Color images are different!" << std::endl;
        std::cout << "Test failed" << std::endl;
        return EXIT_FAILURE;
      }
    }
    {
      std::cout << "\n** Test multi-threaded LUT on gray image which size is not divisible by 8" << std::endl;

      // Check performLut with multithreading and image size not divisible by 8
      vpImage<unsigned char> I_test_grayscale_multi(49, 7), I_test_grayscale_single;
      generateRandomImage(I_test_grayscale_multi);
      I_test_grayscale_single = I_test_grayscale_multi;
      // Construct the LUT
      unsigned char lut_grayscale[256];
      for (unsigned int i = 0; i < 256; i++) {
        lut_grayscale[i] = vpMath::saturate<unsigned char>(alpha * i + beta);
      }
      std::cout << "Run test n°1 with " << nbThreads << " threads" << std::endl;
      I_test_grayscale_multi.performLut(lut_grayscale, nbThreads);
      std::cout << "Run test n°2 single threads" << std::endl;
      I_test_grayscale_single.performLut(lut_grayscale, 1);

      // Check gray images
      if (I_test_grayscale_multi == I_test_grayscale_single) {
        std::cout << "Gray images are the same" << std::endl;
      }
      else {
        std::cerr << "Gray images are different!" << std::endl;
        std::cout << "Test failed" << std::endl;
        return EXIT_FAILURE;
      }
    }
    {
      std::cout << "\n** Test multi-threaded LUT on color image which size is not divisible by 8" << std::endl;

      vpImage<vpRGBa> I_test_color_multi(49, 7), I_test_color_single;
      generateRandomImage(I_test_color_multi);
      I_test_color_single = I_test_color_multi;
      // Construct the LUT
      vpRGBa lut_color[256];
      for (unsigned int i = 0; i < 256; i++) {
        lut_color[i].R = vpMath::saturate<unsigned char>(alpha * i + beta);
        lut_color[i].G = vpMath::saturate<unsigned char>(alpha * i + beta);
        lut_color[i].B = vpMath::saturate<unsigned char>(alpha * i + beta);
        lut_color[i].A = vpMath::saturate<unsigned char>(alpha * i + beta);
      }
      std::cout << "Run test n°1 with " << nbThreads << " threads" << std::endl;
      I_test_color_multi.performLut(lut_color, nbThreads);
      std::cout << "Run test n°2 single threads" << std::endl;
      I_test_color_single.performLut(lut_color, 1);

      // Check color images
      if (I_test_color_multi == I_test_color_single) {
        std::cout << "Color images are the same" << std::endl;
      }
      else {
        std::cerr << "Color images are different!" << std::endl;
        std::cout << "Test failed" << std::endl;
        return EXIT_FAILURE;
      }
    }
    std::cout << "Test succeed" << std::endl;
    return EXIT_SUCCESS;
  }
  catch (const vpException &e) {
    std::cerr << "Catch an exception: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
