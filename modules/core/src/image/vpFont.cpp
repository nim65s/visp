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
 * Draw text in an image.
 */
// Contains code from:
/*
 * Simd Library (http://ermig1979.github.io/Simd).
 *
 * Copyright (c) 2011-2017 Yermalayeu Ihar.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <visp3/core/vpFont.h>
#include <visp3/core/vpIoException.h>
#include <visp3/core/vpIoTools.h>
#include <visp3/core/vpMath.h>
#include <iterator>
#include "private/Font.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "private/stb_truetype.h"

BEGIN_VISP_NAMESPACE
#ifndef DOXYGEN_SHOULD_SKIP_THIS
class vpFont::Impl
{
private:
  typedef Font::Point<int> Point; /*!< Point type definition. */
  typedef std::string String;
  typedef Font::Rectangle<int> Rect;
  typedef std::vector<Rect> Rects;
  typedef std::vector<Point> Points;

public:
  /*!
    Creates a new Font class with given height.

    \note The font supports ASCII characters only. It was generated on the base of the generic monospace font from
    Gdiplus.

    \param [in] height - initial height value. By default it is equal to 16.
  */
  Impl(unsigned int height = 16, const vpFontFamily &fontFamily = TRUETYPE_FILE,
       const std::string &ttfFilename = std::string(VISP_RUBIK_REGULAR_FONT_RESOURCES))
  {
    _fontHeight = height;
    _ttfFamily = fontFamily;

    switch (_ttfFamily) {
    case GENERIC_MONOSPACE: {
      LoadDefault();
      Resize(height);
      break;
    }
    case TRUETYPE_FILE: {
      std::vector<std::string> ttfs = vpIoTools::splitChain(ttfFilename, ";");
      std::string ttf_file = "";
      for (size_t i = 0; i < ttfs.size(); i++) {
        if (vpIoTools::checkFilename(ttfs[i])) {
          ttf_file = ttfs[i];
          break;
        }
      }
      if (ttf_file.empty()) {
        throw(vpException(vpException::fatalError, "True type font file doesn't exist in %s", ttfFilename.c_str()));
      }

      LoadTTF(ttf_file);

      /* calculate font scaling */
      _fontScale = stbtt_ScaleForPixelHeight(&_info, static_cast<float>(_fontHeight));

      int lineGap = 0;
      stbtt_GetFontVMetrics(&_info, &_fontAscent, &_fontDescent, &lineGap);

      _fontAscent = vpMath::round(_fontAscent * _fontScale);
      _fontDescent = vpMath::round(_fontDescent * _fontScale);

      break;
    }
    default: {
      throw(vpException(vpException::fatalError, "Unsupported font family in vpFont::Impl constructor"));
    }
    }
  }

  /*!
    Sets a new height value to font.

    \param [in] height - a new height value.

    \return a result of the operation.
  */
  bool Resize(unsigned int height)
  {
    _fontHeight = height;

    switch (_ttfFamily) {
    case GENERIC_MONOSPACE: {
      if (height == static_cast<unsigned int>(_currentSize.y)) {
        return true;
      }

      if (height < 4u || height > static_cast<unsigned int>(_originalSize.y * 4)) {
        return false;
      }

      _currentSize.y = static_cast<int>(height);
      _currentSize.x = static_cast<int>(height) * _originalSize.x / _originalSize.y;
      _currentIndent.x = static_cast<int>(height) * _originalIndent.x / _originalSize.y;
      _currentIndent.y = static_cast<int>(height) * _originalIndent.y / _originalSize.y;

      size_t level = 0;
      for (; (height << (level + 1)) < static_cast<size_t>(_originalSize.y); level++)
        ;

      _currentSymbols.resize(_originalSymbols.size());
      for (size_t i = 0; i < _originalSymbols.size(); i++) {
        _currentSymbols[i].value = _originalSymbols[i].value;
        _currentSymbols[i].image.resize(static_cast<unsigned int>(_currentSize.y),
                                        static_cast<unsigned int>(_currentSize.x));
        vpImageTools::resize(_originalSymbols[i].image, _currentSymbols[i].image, vpImageTools::INTERPOLATION_LINEAR);
      }

      break;
    }

    case TRUETYPE_FILE:
      break;

    default: {
      throw(vpException(vpException::fatalError, "Unsupported font family in vpFont::Impl::Resize()"));
    }
    }

    return true;
  }

  /*!
    Gets height of the font.

    \return current height of the font.
  */
  int Height() const { return _fontHeight; }

  /*!
    Measures a size of region is need to draw given text.

    \param [in] text - a text to draw.

    \return measured size.
  */
  Point Measure(const String &text)
  {
    switch (_ttfFamily) {
    case GENERIC_MONOSPACE: {
      Point size, curr;
      for (size_t i = 0; i < text.size(); i++) {
        if (text[i] >= _symbolMin && text[i] <= _symbolMax) {
          curr.x += _currentSize.x;
          size.x = std::max<int>(size.x, curr.x);
          size.y = std::max<int>(size.y, curr.y + _currentSize.y);
        }
        else if (text[i] == '\n') {
          curr.x = 0;
          curr.y += _currentSize.y;
        }
      }
      return size.x ? size + 2 * _currentIndent : Point();
    }

    case TRUETYPE_FILE: {
      ToUTF32(text, _wordUTF32);

      _bb = vpRect(std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), -1, -1);
      _ax_vec.clear();
      _lsb_vec.clear();
      _bb_vec.clear();
      _y_vec.clear();

      int wordUTF32_size = static_cast<int>(_wordUTF32.size());

      int bottom = 0;
      // https://github.com/justinmeiners/stb-truetype-example/blob/master/main.c
      for (int i = 0, x = 0; i < wordUTF32_size; i++) {
        /* how wide is this character */
        int ax = 0;
        int lsb = 0;
        stbtt_GetCodepointHMetrics(&_info, _wordUTF32[i], &ax, &lsb);
        _ax_vec.push_back(ax);
        _lsb_vec.push_back(lsb);
        /* (Note that each Codepoint call has an alternative Glyph version which caches the work required to lookup the
         * character word[i].) */

        /* get bounding box for character (may be offset to account for chars that dip above or below the line */
        int c_x1 = 0, c_y1 = 0, c_x2 = 0, c_y2 = 0;
        stbtt_GetCodepointBitmapBox(&_info, _wordUTF32[i], _fontScale, _fontScale, &c_x1, &c_y1, &c_x2, &c_y2);
        _bb_vec.push_back(vpRect(c_x1, c_y1, c_x2 - c_x1, c_y2 - c_y1));

        /* compute y (different characters have different heights */
        int y = _fontAscent + c_y1;
        _y_vec.push_back(y);

        int d_x = x + vpMath::round(lsb * _fontScale);
        int d_y = y;
        int d_w = (c_x2 - c_x1);
        int d_h = (c_y2 - c_y1);
        _bb.setLeft(_bb.getLeft() > d_x ? d_x : _bb.getLeft());
        _bb.setTop(_bb.getTop() > d_y ? d_y : _bb.getTop());

        _bb.setWidth(d_x + d_w - _bb.getLeft());
        bottom = (bottom < d_y + d_h) ? (d_y + d_h) : bottom;

        /* advance x */
        x += vpMath::round(ax * _fontScale);

        /* add kerning */
        int kern = 0;
        if (i < wordUTF32_size - 1) {
          kern = stbtt_GetCodepointKernAdvance(&_info, _wordUTF32[i], _wordUTF32[i + 1]);
          x += vpMath::round(kern * _fontScale);
        }
      }

      _bb.setBottom(bottom);

      Point bb;
      bb.x = static_cast<int>(_bb.getWidth());
      bb.y = static_cast<int>(_bb.getHeight());

      return bb;
    }
    default: {
      throw(vpException(vpException::fatalError, "Unsupported font family in vpFont::Impl::Measure()"));
    }
    }
  }

  /*!
    (For ViSP type compatibility) Measures a size of region is need to draw given text.
  */
  vpImagePoint Measure2(const String &text)
  {
    Point mes = Measure(text);
    return vpImagePoint(mes.y, mes.x);
  }

  /*!
    Draws a text at the image.

    \param [out] canvas - a canvas (image where we draw text).
    \param [in] text - a text to draw.
    \param [in] position - a start position to draw text.
    \param [in] color - a color of the text.

    \return a result of the operation.
  */
  bool Draw(vpImage<unsigned char> &canvas, const String &text, const vpImagePoint &position, unsigned char color)
  {
    switch (_ttfFamily) {
    case GENERIC_MONOSPACE: {
      Rect canvasRect, alphaRect;
      CreateAlpha(text, Rect(0, 0, canvas.getWidth(), canvas.getHeight()), Point(position.get_u(), position.get_v()),
                  _alpha, canvasRect, alphaRect);

      if (_alpha.getSize()) {
        for (int i = 0; i < alphaRect.Height(); i++) {
          int dstY = (canvasRect.Top() + i) % canvas.getHeight();
          for (int j = 0; j < alphaRect.Width(); j++) {
            unsigned int dstX = static_cast<unsigned int>(canvasRect.Left() + j) % canvas.getWidth();

            // dst[x, y, c] = (pixel[c]*_alpha[x, y] + dst[x, y, c]*(255 - _alpha[x, y]))/255;
            int coeff = 255 - _alpha[i][j];
            unsigned char gray = static_cast<unsigned char>((color * _alpha[i][j] + coeff * canvas[dstY][dstX]) / 255);
            canvas[dstY][dstX] = gray;
          }
        }
      }

      break;
    }

    case TRUETYPE_FILE: {
      Measure(text);
      // Try to resize only if new size is bigger
      _fontBuffer.resize(std::max<unsigned int>(_fontBuffer.getHeight(), static_cast<unsigned int>(_bb.getBottom())),
                         std::max<unsigned int>(_fontBuffer.getWidth(), static_cast<unsigned int>(_bb.getRight())));

      int wordUTF32_size = static_cast<int>(_wordUTF32.size());

      for (int i = 0, x = 0; i < wordUTF32_size; i++) {
        /* render character (stride and offset is important here) */
        int byteOffset = x + vpMath::round(_lsb_vec[i] * _fontScale) + (_y_vec[i] * _fontBuffer.getWidth());
        stbtt_MakeCodepointBitmap(&_info, _fontBuffer.bitmap + byteOffset, static_cast<int>(_bb_vec[i].getWidth()),
                                  static_cast<int>(_bb_vec[i].getHeight()), _fontBuffer.getWidth(), _fontScale,
                                  _fontScale, _wordUTF32[i]);

        int d_x = x + vpMath::round(_lsb_vec[i] * _fontScale);
        int d_y = _y_vec[i];
        int d_w = static_cast<int>(_bb_vec[i].getWidth());
        int d_h = static_cast<int>(_bb_vec[i].getHeight());

        for (int y = d_y; y < d_y + d_h; y++) {
          int dstY = static_cast<int>(position.get_v() + y - _bb.getTop()) % canvas.getHeight();

          for (int x = d_x; x < d_x + d_w; x++) {
            unsigned int dstX = static_cast<unsigned int>(position.get_u() + x - _bb.getLeft()) % canvas.getWidth();

            int coeff = 255 - _fontBuffer[y][x];
            unsigned char gray =
              static_cast<unsigned char>((color * _fontBuffer[y][x] + coeff * canvas[dstY][dstX]) / 255);
            canvas[dstY][dstX] = gray;
          }
        }

        /* advance x */
        x += vpMath::round(_ax_vec[i] * _fontScale);

        /* add kerning */
        if (i < wordUTF32_size - 1) {
          int kern = stbtt_GetCodepointKernAdvance(&_info, _wordUTF32[i], _wordUTF32[i + 1]);
          x += vpMath::round(kern * _fontScale);
        }
      }

      break;
    }
    default: {
      throw(vpException(vpException::fatalError, "Unsupported font family in vpFont::Impl::Draw()"));
    }
    }

    return true;
  }

  /*!
    Draws a text at the image. Fills the text background by given color.

    \param [out] canvas - a canvas (image where we draw text).
    \param [in] text - a text to draw.
    \param [in] position - a position to draw text (see Simd::View::Position).
    \param [in] color - a color of the text.
    \param [in] background - background color.

    \return a result of the operation.
  */
  bool Draw(vpImage<unsigned char> &canvas, const String &text, const vpImagePoint &position, unsigned char color,
            unsigned char background)
  {
    Rect canvasRect = Rect(Measure(text)).Shifted(position.get_u(), position.get_v());
    for (int i = canvasRect.Top(); i < canvasRect.Bottom(); i++) {
      for (int j = canvasRect.Left(); j < canvasRect.Right(); j++) {
        canvas[i][j] = background;
      }
    }
    return Draw(canvas, text, position, color);
  }

  /*!
    Draws a text at the image.

    \param [out] canvas - a canvas (image where we draw text).
    \param [in] text - a text to draw.
    \param [in] position - a start position to draw text.
    \param [in] color - a color of the text.

    \return a result of the operation.
  */
  bool Draw(vpImage<vpRGBa> &canvas, const String &text, const vpImagePoint &position, const vpColor &color)
  {
    switch (_ttfFamily) {
    case GENERIC_MONOSPACE: {
      Rect canvasRect, alphaRect;
      CreateAlpha(text, Rect(0, 0, canvas.getWidth(), canvas.getHeight()), Point(position.get_u(), position.get_v()),
                  _alpha, canvasRect, alphaRect);

      if (_alpha.getSize()) {
        for (int i = 0; i < alphaRect.Height(); i++) {
          int dstY = (canvasRect.Top() + i) % canvas.getHeight();
          for (int j = 0; j < alphaRect.Width(); j++) {
            unsigned int dstX = static_cast<unsigned int>(canvasRect.Left() + j) % canvas.getWidth();

            // dst[x, y, c] = (pixel[c]*_alpha[x, y] + dst[x, y, c]*(255 - _alpha[x, y]))/255;
            int coeff = 255 - _alpha[i][j];
            int R = (color.R * _alpha[i][j] + coeff * canvas[dstY][dstX].R) / 255;
            int G = (color.G * _alpha[i][j] + coeff * canvas[dstY][dstX].G) / 255;
            int B = (color.B * _alpha[i][j] + coeff * canvas[dstY][dstX].B) / 255;
            canvas[dstY][dstX] =
              vpRGBa(static_cast<unsigned char>(R), static_cast<unsigned char>(G), static_cast<unsigned char>(B));
          }
        }
      }

      break;
    }

    case TRUETYPE_FILE: {
      Measure(text);
      // Try to resize only if new size is bigger
      _fontBuffer.resize(std::max<unsigned int>(_fontBuffer.getHeight(), static_cast<unsigned int>(_bb.getBottom())),
                         std::max<unsigned int>(_fontBuffer.getWidth(), static_cast<unsigned int>(_bb.getRight())));

      int wordUTF32_size = static_cast<int>(_wordUTF32.size());

      for (int i = 0, x = 0; i < wordUTF32_size; i++) {
        /* render character (stride and offset is important here) */
        int byteOffset = x + vpMath::round(_lsb_vec[i] * _fontScale) + (_y_vec[i] * _fontBuffer.getWidth());
        stbtt_MakeCodepointBitmap(&_info, _fontBuffer.bitmap + byteOffset, static_cast<int>(_bb_vec[i].getWidth()),
                                  static_cast<int>(_bb_vec[i].getHeight()), _fontBuffer.getWidth(), _fontScale,
                                  _fontScale, _wordUTF32[i]);

        int d_x = x + vpMath::round(_lsb_vec[i] * _fontScale);
        int d_y = _y_vec[i];
        int d_w = static_cast<int>(_bb_vec[i].getWidth());
        int d_h = static_cast<int>(_bb_vec[i].getHeight());

        for (int y = d_y; y < d_y + d_h; y++) {
          int dstY = static_cast<int>(position.get_v() + y - _bb.getTop()) % canvas.getHeight();

          for (int x = d_x; x < d_x + d_w; x++) {
            unsigned int dstX = static_cast<unsigned int>(position.get_u() + x - _bb.getLeft()) % canvas.getWidth();

            int coeff = 255 - _fontBuffer[y][x];
            int R = (color.R * _fontBuffer[y][x] + coeff * canvas[dstY][dstX].R) / 255;
            int G = (color.G * _fontBuffer[y][x] + coeff * canvas[dstY][dstX].G) / 255;
            int B = (color.B * _fontBuffer[y][x] + coeff * canvas[dstY][dstX].B) / 255;
            canvas[dstY][dstX] =
              vpRGBa(static_cast<unsigned char>(R), static_cast<unsigned char>(G), static_cast<unsigned char>(B));
          }
        }

        /* advance x */
        x += vpMath::round(_ax_vec[i] * _fontScale);

        /* add kerning */
        if (i < wordUTF32_size - 1) {
          int kern = stbtt_GetCodepointKernAdvance(&_info, _wordUTF32[i], _wordUTF32[i + 1]);
          x += vpMath::round(kern * _fontScale);
        }
      }

      break;
    }
    default: {
      throw(vpException(vpException::fatalError, "Unsupported font family in vpFont::Impl::Draw()"));
    }
    }

    return true;
  }

  /*!
    Draws a text at the image. Fills the text background by given color.

    \param [out] canvas - a canvas (image where we draw text).
    \param [in] text - a text to draw.
    \param [in] position - a position to draw text (see Simd::View::Position).
    \param [in] color - a color of the text.
    \param [in] background - background color.

    \return a result of the operation.
  */
  bool Draw(vpImage<vpRGBa> &canvas, const String &text, const vpImagePoint &position, const vpColor &color,
            const vpColor &background)
  {
    Rect canvasRect = Rect(Measure(text)).Shifted(position.get_u(), position.get_v());
    for (int i = canvasRect.Top(); i < canvasRect.Bottom(); i++) {
      for (int j = canvasRect.Left(); j < canvasRect.Right(); j++) {
        canvas[i][j] = background;
      }
    }
    return Draw(canvas, text, position, color);
  }

private:
  struct Symbol
  {
    char value;
    vpImage<unsigned char> image;
  };
  typedef std::vector<Symbol> Symbols;

  Symbols _originalSymbols, _currentSymbols;
  Point _originalSize, _currentSize, _originalIndent, _currentIndent;
  char _symbolMin, _symbolMax;
  vpImage<unsigned char> _alpha;

  // For font drawing using stb_truetype interface
  std::vector<unsigned char> fontBuffer;
  unsigned int _fontHeight;
  float _fontScale;
  int _fontAscent;
  int _fontDescent;
  vpFontFamily _ttfFamily;
  stbtt_fontinfo _info;
  vpImage<unsigned char> _fontBuffer;
  std::vector<unsigned int> _wordUTF32;
  vpRect _bb;
  std::vector<int> _ax_vec;
  std::vector<int> _lsb_vec;
  std::vector<vpRect> _bb_vec;
  std::vector<int> _y_vec;

  void CreateAlpha(const String &text, const Rect &canvas, const Point &shift, vpImage<unsigned char> &alpha,
                   Rect &canvasRect, Rect &alphaRect) const
  {
    Rects rects;
    rects.reserve(text.size());
    String symbols;
    symbols.reserve(text.size());
    Point curr;
    for (size_t i = 0; i < text.size(); i++) {
      char value = text[i];
      if (value >= _symbolMin && value <= _symbolMax) {
        Rect current(curr, curr + _currentSize);
        Rect shifted = current.Shifted(shift + _currentIndent);
        if (!canvas.Intersection(shifted).Empty()) {
          alphaRect |= current;
          canvasRect |= shifted;
          rects.push_back(current);
          symbols.push_back(value);
        }
        curr.x += _currentSize.x;
      }
      else if (value == '\n') {
        curr.x = 0;
        curr.y += _currentSize.y;
      }
    }

    alpha.resize(static_cast<unsigned int>(alphaRect.Size().y), static_cast<unsigned int>(alphaRect.Size().x), 0);
    for (size_t i = 0; i < symbols.size(); i++) {
      Rect r = rects[i].Shifted(-alphaRect.TopLeft());
      alpha.insert(_currentSymbols[static_cast<size_t>(symbols[i] - _symbolMin)].image,
                   vpImagePoint(r.Top(), r.Left()));
    }
    Rect old = canvasRect;
    canvasRect &= canvas;
    alphaRect.Shift(-alphaRect.TopLeft());
    alphaRect.left += canvasRect.left - old.left;
    alphaRect.top += canvasRect.top - old.top;
    alphaRect.right += canvasRect.right - old.right;
    alphaRect.bottom += canvasRect.bottom - old.bottom;
  }

  uint8_t LoadValue(const uint8_t *&data, size_t &size)
  {
    if (size == 0) {
      throw;
    }
    size--;
    return *data++;
  }

  bool Load(const uint8_t *data, size_t size)
  {
    try {
      _symbolMin = static_cast<char>(LoadValue(data, size));
      _symbolMax = static_cast<char>(LoadValue(data, size));
      _originalSize.x = LoadValue(data, size);
      _originalSize.y = LoadValue(data, size);
      _originalIndent.x = LoadValue(data, size);
      _originalIndent.y = LoadValue(data, size);
      _originalSymbols.resize(static_cast<size_t>(_symbolMax - _symbolMin));
      for (char s = _symbolMin; s < _symbolMax; s++) {
        Symbol &symbol = _originalSymbols[static_cast<size_t>(s - _symbolMin)];
        symbol.value = static_cast<char>(LoadValue(data, size));
        if (symbol.value != s) {
          throw;
        }

        symbol.image.resize(static_cast<unsigned int>(_originalSize.y), static_cast<unsigned int>(_originalSize.x), 0);
        size_t top = LoadValue(data, size);
        size_t bottom = LoadValue(data, size);
        for (size_t r = top; r < bottom; r++) {
          size_t count = LoadValue(data, size);
          for (size_t l = 0; l < count; l++) {
            size_t left = LoadValue(data, size);
            size_t right = LoadValue(data, size);
            assert(left < right);
            memset(symbol.image[static_cast<unsigned int>(r)] + left, 0xFF, right - left);
          }
        }
      }
    }
    catch (...) {
      _originalSize = Point();
      _originalSymbols.clear();
      return false;
    }
    return true;
  }

  bool LoadDefault()
  {
    static const uint8_t data[] = {
        32,  127, 127, 231, 33,  12,  32,  0,   0,   33,  26,  160, 1,   57,  66,  1,   54,  69,  1,   52,  70,  1,
        51,  72,  1,   50,  73,  1,   49,  74,  1,   48,  75,  1,   48,  75,  1,   47,  76,  1,   47,  76,  1,   46,
        77,  1,   46,  77,  1,   46,  77,  1,   45,  77,  1,   45,  77,  1,   45,  78,  1,   45,  78,  1,   45,  78,
        1,   45,  78,  1,   45,  78,  1,   45,  77,  1,   45,  77,  1,   46,  77,  1,   46,  77,  1,   46,  77,  1,
        46,  77,  1,   46,  77,  1,   46,  77,  1,   46,  77,  1,   46,  77,  1,   46,  77,  1,   46,  76,  1,   47,
        76,  1,   47,  76,  1,   47,  76,  1,   47,  76,  1,   47,  76,  1,   47,  76,  1,   47,  76,  1,   47,  76,
        1,   47,  76,  1,   47,  75,  1,   47,  75,  1,   48,  75,  1,   48,  75,  1,   48,  75,  1,   48,  75,  1,
        48,  75,  1,   48,  75,  1,   48,  75,  1,   48,  75,  1,   48,  75,  1,   48,  74,  1,   48,  74,  1,   49,
        74,  1,   49,  74,  1,   49,  74,  1,   49,  74,  1,   49,  74,  1,   49,  74,  1,   49,  74,  1,   49,  74,
        1,   49,  74,  1,   49,  73,  1,   50,  73,  1,   50,  73,  1,   50,  73,  1,   50,  73,  1,   50,  73,  1,
        50,  73,  1,   50,  73,  1,   50,  73,  1,   50,  73,  1,   50,  73,  1,   50,  72,  1,   51,  72,  1,   51,
        72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,
        1,   51,  72,  1,   52,  71,  1,   52,  71,  1,   52,  71,  1,   52,  71,  1,   53,  70,  1,   54,  69,  1,
        54,  68,  1,   56,  67,  1,   57,  65,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   1,   56,  67,  1,   54,  69,  1,   53,  71,  1,   52,  72,  1,   51,  72,  1,
        50,  73,  1,   50,  73,  1,   50,  74,  1,   49,  74,  1,   49,  74,  1,   49,  74,  1,   50,  74,  1,   50,
        74,  1,   50,  73,  1,   51,  73,  1,   51,  72,  1,   52,  71,  1,   53,  70,  1,   55,  68,  1,   56,  67,
        34,  35,  90,  2,   27,  53,  70,  96,  2,   28,  53,  70,  96,  2,   28,  53,  70,  95,  2,   28,  53,  70,
        95,  2,   28,  53,  70,  95,  2,   28,  53,  70,  95,  2,   28,  53,  71,  95,  2,   28,  52,  71,  95,  2,
        29,  52,  71,  95,  2,   29,  52,  71,  94,  2,   29,  52,  71,  94,  2,   29,  52,  71,  94,  2,   29,  52,
        71,  94,  2,   29,  52,  72,  94,  2,   30,  52,  72,  94,  2,   30,  51,  72,  94,  2,   30,  51,  72,  94,
        2,   30,  51,  72,  93,  2,   30,  51,  72,  93,  2,   30,  51,  72,  93,  2,   30,  51,  73,  93,  2,   31,
        51,  73,  93,  2,   31,  50,  73,  93,  2,   31,  50,  73,  93,  2,   31,  50,  73,  93,  2,   31,  50,  73,
        92,  2,   31,  50,  74,  92,  2,   31,  50,  74,  92,  2,   32,  50,  74,  92,  2,   32,  50,  74,  92,  2,
        32,  49,  74,  92,  2,   32,  49,  74,  92,  2,   32,  49,  74,  91,  2,   32,  49,  75,  91,  2,   32,  49,
        75,  91,  2,   33,  49,  75,  91,  2,   33,  49,  75,  91,  2,   33,  48,  75,  91,  2,   33,  48,  75,  91,
        2,   33,  48,  75,  91,  2,   33,  48,  76,  90,  2,   33,  48,  76,  90,  2,   34,  48,  76,  90,  2,   34,
        48,  76,  90,  2,   34,  48,  76,  90,  2,   34,  47,  76,  90,  2,   34,  47,  76,  90,  2,   34,  47,  77,
        89,  2,   34,  47,  77,  89,  2,   35,  47,  77,  89,  2,   35,  47,  77,  89,  2,   35,  46,  77,  89,  2,
        36,  46,  78,  88,  2,   36,  45,  79,  87,  2,   38,  43,  80,  86,  35,  18,  176, 2,   47,  51,  83,  87,
        2,   44,  54,  80,  89,  2,   43,  55,  78,  91,  2,   42,  56,  77,  92,  2,   41,  57,  76,  93,  2,   40,
        58,  76,  93,  2,   40,  58,  75,  94,  2,   39,  59,  75,  94,  2,   39,  59,  75,  94,  2,   39,  59,  74,
        94,  2,   39,  59,  74,  95,  2,   39,  59,  74,  95,  2,   39,  59,  74,  95,  2,   39,  59,  74,  94,  2,
        39,  59,  74,  94,  2,   39,  59,  74,  94,  2,   38,  59,  74,  94,  2,   38,  59,  74,  94,  2,   38,  59,
        74,  94,  2,   38,  59,  74,  94,  2,   38,  59,  74,  94,  2,   38,  58,  74,  94,  2,   38,  58,  73,  94,
        2,   38,  58,  73,  94,  2,   38,  58,  73,  94,  2,   38,  58,  73,  94,  2,   38,  58,  73,  93,  2,   38,
        58,  73,  93,  2,   38,  58,  73,  93,  2,   38,  58,  73,  93,  2,   37,  58,  73,  93,  2,   37,  58,  73,
        93,  2,   37,  58,  73,  93,  2,   37,  58,  73,  93,  2,   37,  58,  73,  93,  2,   37,  58,  73,  93,  2,
        37,  57,  73,  93,  2,   37,  57,  72,  93,  2,   37,  57,  72,  93,  2,   37,  57,  72,  93,  2,   37,  57,
        72,  93,  2,   37,  57,  72,  92,  2,   37,  57,  72,  92,  2,   37,  57,  72,  92,  2,   37,  57,  72,  92,
        2,   36,  57,  72,  92,  2,   36,  57,  72,  92,  2,   36,  57,  72,  92,  1,   26,  102, 1,   23,  105, 1,
        21,  107, 1,   20,  108, 1,   19,  109, 1,   19,  109, 1,   18,  110, 1,   18,  110, 1,   18,  110, 1,   18,
        110, 1,   18,  110, 1,   18,  110, 1,   18,  110, 1,   18,  110, 1,   19,  109, 1,   19,  109, 1,   20,  108,
        1,   21,  107, 1,   22,  106, 1,   25,  103, 2,   35,  55,  70,  91,  2,   35,  55,  70,  90,  2,   35,  55,
        70,  90,  2,   35,  55,  70,  90,  2,   35,  55,  70,  90,  2,   34,  55,  70,  90,  2,   34,  55,  70,  90,
        2,   34,  55,  70,  90,  2,   34,  55,  70,  90,  2,   34,  55,  70,  90,  2,   34,  55,  70,  90,  2,   34,
        54,  69,  90,  2,   34,  54,  69,  90,  2,   34,  54,  69,  90,  2,   34,  54,  69,  90,  2,   34,  54,  69,
        90,  2,   34,  54,  69,  89,  2,   34,  54,  69,  89,  2,   34,  54,  69,  89,  2,   34,  54,  69,  89,  2,
        33,  54,  69,  89,  2,   33,  54,  69,  89,  1,   25,  95,  1,   19,  100, 1,   17,  102, 1,   16,  103, 1,
        15,  104, 1,   15,  105, 1,   14,  105, 1,   14,  106, 1,   14,  106, 1,   13,  106, 1,   13,  106, 1,   13,
        106, 1,   14,  106, 1,   14,  106, 1,   14,  105, 1,   15,  105, 1,   15,  104, 1,   16,  103, 1,   17,  102,
        1,   19,  100, 1,   25,  95,  2,   32,  52,  67,  88,  2,   32,  52,  67,  87,  2,   32,  52,  67,  87,  2,
        32,  52,  67,  87,  2,   32,  52,  67,  87,  2,   31,  52,  67,  87,  2,   31,  52,  67,  87,  2,   31,  52,
        67,  87,  2,   31,  52,  67,  87,  2,   31,  52,  67,  87,  2,   31,  52,  67,  87,  2,   31,  51,  67,  87,
        2,   31,  51,  66,  87,  2,   31,  51,  66,  87,  2,   31,  51,  66,  87,  2,   31,  51,  66,  86,  2,   31,
        51,  66,  86,  2,   31,  51,  66,  86,  2,   31,  51,  66,  86,  2,   30,  51,  66,  86,  2,   30,  51,  66,
        86,  2,   30,  51,  66,  86,  2,   30,  51,  66,  86,  2,   30,  51,  66,  86,  2,   30,  51,  66,  86,  2,
        30,  50,  66,  86,  2,   30,  50,  65,  86,  2,   30,  50,  65,  86,  2,   30,  50,  65,  86,  2,   30,  50,
        65,  86,  2,   30,  50,  65,  85,  2,   30,  50,  65,  85,  2,   30,  50,  65,  85,  2,   30,  50,  65,  85,
        2,   29,  50,  65,  85,  2,   29,  50,  65,  85,  2,   29,  50,  65,  85,  2,   30,  50,  65,  85,  2,   30,
        50,  65,  84,  2,   30,  49,  65,  84,  2,   30,  49,  65,  84,  2,   31,  49,  66,  84,  2,   31,  48,  66,
        83,  2,   32,  47,  67,  82,  2,   33,  46,  68,  81,  2,   34,  45,  70,  80,  2,   36,  43,  71,  78,  36,
        16,  183, 1,   61,  63,  1,   58,  67,  1,   56,  68,  1,   55,  69,  1,   54,  70,  1,   53,  71,  1,   53,
        71,  1,   53,  71,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  73,  1,   48,  77,  1,
        44,  81,  2,   42,  84,  88,  96,  1,   40,  98,  1,   38,  99,  1,   37,  100, 1,   35,  100, 1,   34,  101,
        1,   33,  101, 1,   32,  101, 1,   31,  102, 1,   30,  102, 1,   29,  102, 1,   28,  102, 1,   28,  102, 1,
        27,  102, 1,   26,  102, 1,   26,  102, 1,   25,  102, 2,   25,  58,  69,  102, 2,   24,  53,  73,  102, 2,
        24,  50,  76,  102, 2,   24,  49,  78,  102, 2,   23,  47,  80,  102, 2,   23,  46,  81,  102, 2,   23,  45,
        82,  102, 2,   23,  44,  82,  102, 2,   22,  44,  82,  101, 2,   22,  43,  83,  101, 2,   22,  43,  83,  101,
        2,   22,  43,  84,  100, 2,   22,  42,  84,  100, 2,   22,  42,  85,  99,  2,   22,  42,  87,  98,  2,   22,
        43,  88,  96,  1,   22,  43,  1,   23,  44,  1,   23,  44,  1,   23,  45,  1,   23,  47,  1,   23,  48,  1,
        24,  51,  1,   24,  54,  1,   24,  58,  1,   25,  62,  1,   25,  67,  1,   26,  72,  1,   26,  76,  1,   27,
        80,  1,   28,  83,  1,   29,  85,  1,   29,  88,  1,   30,  90,  1,   31,  92,  1,   33,  93,  1,   34,  95,
        1,   35,  96,  1,   37,  97,  1,   39,  98,  1,   41,  99,  1,   43,  100, 1,   46,  101, 1,   50,  101, 1,
        54,  102, 1,   59,  102, 1,   64,  103, 1,   69,  104, 1,   73,  104, 1,   76,  104, 1,   79,  105, 1,   81,
        105, 1,   82,  105, 1,   83,  105, 1,   84,  106, 2,   26,  29,  85,  106, 2,   23,  32,  85,  106, 2,   22,
        34,  86,  106, 2,   20,  35,  86,  106, 2,   20,  36,  86,  106, 2,   19,  36,  86,  106, 2,   19,  37,  86,
        106, 2,   18,  37,  86,  106, 2,   18,  38,  86,  106, 2,   18,  38,  86,  106, 2,   18,  38,  85,  106, 2,
        18,  39,  85,  106, 2,   18,  39,  84,  106, 2,   18,  40,  84,  106, 2,   18,  41,  83,  105, 2,   18,  42,
        82,  105, 2,   18,  43,  81,  105, 2,   18,  44,  79,  104, 2,   18,  46,  77,  104, 2,   18,  49,  75,  104,
        2,   18,  52,  73,  103, 2,   18,  56,  68,  103, 1,   18,  102, 1,   18,  101, 1,   18,  101, 1,   18,  100,
        1,   18,  99,  1,   18,  98,  1,   18,  98,  1,   18,  97,  1,   18,  96,  1,   18,  95,  1,   18,  94,  1,
        19,  92,  1,   19,  91,  1,   19,  89,  1,   20,  88,  2,   21,  35,  39,  86,  2,   22,  33,  41,  83,  2,
        24,  31,  44,  81,  1,   47,  77,  1,   51,  73,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   53,  71,  1,   53,  71,  1,   53,  71,
        1,   54,  70,  1,   55,  69,  1,   56,  68,  1,   57,  67,  1,   61,  63,  37,  30,  160, 1,   44,  54,  1,
        40,  58,  1,   37,  61,  1,   36,  62,  1,   34,  64,  1,   32,  66,  1,   31,  67,  1,   30,  68,  1,   29,
        69,  1,   28,  70,  1,   27,  71,  1,   26,  72,  1,   25,  73,  2,   25,  44,  54,  73,  2,   24,  42,  56,
        74,  2,   24,  40,  58,  74,  2,   23,  39,  59,  75,  2,   23,  38,  61,  75,  2,   22,  37,  61,  76,  2,
        22,  36,  62,  76,  2,   22,  35,  63,  76,  2,   21,  35,  63,  77,  2,   21,  34,  64,  77,  2,   21,  34,
        64,  77,  2,   21,  34,  65,  77,  2,   21,  33,  65,  77,  2,   21,  33,  65,  77,  2,   20,  33,  65,  78,
        2,   20,  33,  65,  78,  2,   20,  33,  65,  78,  2,   21,  33,  65,  78,  2,   21,  33,  65,  77,  2,   21,
        33,  65,  77,  2,   21,  34,  64,  77,  2,   21,  34,  64,  77,  2,   21,  34,  64,  77,  2,   21,  35,  63,
        77,  2,   22,  36,  63,  76,  2,   22,  36,  62,  76,  2,   22,  37,  61,  76,  2,   23,  38,  60,  75,  2,
        23,  39,  59,  75,  2,   24,  41,  57,  74,  2,   25,  42,  56,  74,  2,   25,  45,  53,  73,  1,   26,  72,
        2,   27,  71,  100, 103, 2,   28,  71,  96,  105, 2,   28,  70,  93,  106, 2,   29,  69,  90,  107, 2,   31,
        68,  87,  107, 2,   32,  66,  84,  107, 2,   33,  65,  81,  108, 2,   35,  63,  78,  107, 2,   37,  62,  75,
        107, 2,   39,  59,  72,  107, 2,   42,  56,  68,  106, 1,   65,  105, 1,   62,  103, 1,   59,  100, 1,   56,
        97,  1,   53,  94,  1,   50,  91,  1,   47,  88,  1,   44,  85,  1,   41,  82,  1,   38,  78,  1,   35,  75,
        1,   31,  72,  1,   28,  69,  1,   25,  66,  1,   22,  63,  1,   20,  60,  2,   19,  57,  69,  81,  2,   18,
        54,  66,  84,  2,   17,  51,  63,  87,  2,   17,  47,  61,  88,  2,   17,  44,  59,  90,  2,   17,  41,  58,
        92,  2,   17,  38,  57,  93,  2,   18,  35,  55,  94,  2,   18,  32,  54,  95,  2,   19,  29,  53,  96,  2,
        20,  26,  53,  97,  1,   52,  98,  1,   51,  99,  2,   50,  69,  80,  99,  2,   50,  67,  82,  100, 2,   49,
        65,  84,  100, 2,   49,  64,  85,  101, 2,   48,  63,  86,  101, 2,   48,  62,  87,  102, 2,   48,  61,  88,
        102, 2,   47,  61,  89,  102, 2,   47,  60,  89,  102, 2,   47,  60,  90,  103, 2,   46,  59,  90,  103, 2,
        46,  59,  90,  103, 2,   46,  59,  91,  103, 2,   46,  59,  91,  103, 2,   46,  59,  91,  103, 2,   46,  59,
        91,  103, 2,   46,  59,  91,  103, 2,   46,  59,  91,  103, 2,   46,  59,  91,  103, 2,   46,  59,  90,  103,
        2,   46,  59,  90,  103, 2,   47,  60,  90,  103, 2,   47,  60,  89,  103, 2,   47,  61,  89,  102, 2,   47,
        61,  88,  102, 2,   48,  62,  87,  102, 2,   48,  63,  86,  101, 2,   49,  64,  85,  101, 2,   49,  65,  84,
        100, 2,   50,  67,  83,  100, 2,   50,  69,  81,  99,  2,   51,  73,  76,  99,  1,   52,  98,  1,   53,  97,
        1,   53,  96,  1,   54,  95,  1,   55,  94,  1,   56,  93,  1,   58,  92,  1,   59,  91,  1,   61,  89,  1,
        63,  87,  1,   65,  84,  1,   68,  81,  38,  44,  160, 1,   65,  72,  1,   59,  76,  2,   56,  79,  85,  89,
        1,   54,  92,  1,   52,  94,  1,   50,  95,  1,   49,  96,  1,   48,  96,  1,   47,  97,  1,   46,  97,  1,
        45,  97,  1,   44,  97,  1,   43,  97,  1,   42,  97,  1,   42,  97,  1,   41,  97,  1,   40,  97,  1,   40,
        96,  1,   39,  95,  1,   39,  94,  1,   38,  93,  2,   38,  63,  73,  92,  2,   38,  61,  74,  90,  2,   37,
        60,  76,  88,  2,   37,  59,  77,  86,  2,   37,  58,  78,  84,  2,   37,  58,  80,  82,  1,   37,  57,  1,
        37,  57,  1,   37,  57,  1,   36,  57,  1,   36,  57,  1,   37,  57,  1,   37,  57,  1,   37,  58,  1,   37,
        58,  1,   37,  59,  1,   37,  59,  1,   37,  60,  1,   38,  60,  1,   38,  61,  1,   39,  62,  1,   39,  62,
        1,   39,  63,  1,   40,  64,  1,   40,  64,  1,   41,  65,  1,   42,  66,  1,   42,  66,  1,   43,  67,  1,
        43,  68,  1,   42,  68,  1,   41,  69,  1,   39,  70,  1,   38,  71,  1,   37,  71,  2,   36,  72,  88,  105,
        2,   35,  73,  88,  108, 2,   34,  73,  87,  109, 2,   33,  74,  87,  110, 2,   32,  75,  87,  110, 2,   31,
        75,  87,  111, 2,   30,  76,  86,  111, 2,   30,  77,  86,  112, 2,   29,  77,  86,  112, 2,   28,  78,  85,
        112, 2,   28,  79,  85,  112, 2,   27,  79,  85,  112, 3,   27,  54,  56,  80,  84,  112, 3,   27,  52,  56,
        81,  84,  111, 3,   26,  51,  57,  81,  84,  111, 3,   26,  49,  58,  82,  84,  110, 2,   25,  48,  58,  110,
        2,   25,  48,  59,  109, 2,   25,  47,  60,  108, 2,   25,  46,  60,  106, 2,   24,  46,  61,  103, 2,   24,
        45,  62,  103, 2,   24,  45,  62,  103, 2,   24,  44,  63,  102, 2,   24,  44,  64,  102, 2,   24,  44,  65,
        102, 2,   24,  44,  65,  102, 2,   24,  44,  66,  101, 2,   24,  44,  67,  101, 2,   24,  44,  67,  101, 2,
        24,  44,  68,  100, 2,   24,  44,  69,  100, 2,   24,  45,  69,  99,  2,   24,  45,  70,  99,  2,   24,  46,
        71,  98,  2,   24,  47,  71,  98,  2,   25,  48,  72,  97,  2,   25,  49,  71,  105, 2,   25,  51,  69,  108,
        2,   26,  55,  65,  109, 1,   26,  110, 1,   26,  111, 1,   27,  111, 1,   28,  112, 1,   28,  112, 1,   29,
        112, 1,   29,  112, 1,   30,  112, 1,   31,  112, 1,   32,  112, 1,   33,  112, 1,   34,  111, 1,   35,  111,
        1,   37,  110, 2,   38,  84,  85,  109, 2,   40,  82,  85,  108, 2,   42,  80,  86,  105, 1,   44,  78,  1,
        48,  76,  1,   52,  71,  39,  33,  93,  1,   49,  75,  1,   49,  75,  1,   49,  75,  1,   49,  75,  1,   49,
        74,  1,   50,  74,  1,   50,  74,  1,   50,  74,  1,   50,  74,  1,   50,  74,  1,   50,  74,  1,   50,  74,
        1,   50,  74,  1,   51,  73,  1,   51,  73,  1,   51,  73,  1,   51,  73,  1,   51,  73,  1,   51,  73,  1,
        51,  73,  1,   52,  73,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   53,  72,  1,   53,  72,  1,   53,  71,  1,   53,  71,  1,   53,  71,  1,   53,  71,  1,   53,  71,
        1,   53,  71,  1,   54,  71,  1,   54,  71,  1,   54,  70,  1,   54,  70,  1,   54,  70,  1,   54,  70,  1,
        54,  70,  1,   55,  70,  1,   55,  70,  1,   55,  70,  1,   55,  69,  1,   55,  69,  1,   55,  69,  1,   55,
        69,  1,   55,  69,  1,   56,  69,  1,   56,  69,  1,   56,  69,  1,   56,  68,  1,   56,  68,  1,   57,  68,
        1,   57,  68,  1,   58,  67,  1,   58,  66,  1,   60,  64,  40,  27,  189, 1,   88,  92,  1,   85,  95,  1,
        84,  97,  1,   82,  98,  1,   82,  98,  1,   81,  99,  1,   80,  100, 1,   79,  100, 1,   79,  100, 1,   78,
        100, 1,   77,  100, 1,   77,  100, 1,   76,  100, 1,   76,  100, 1,   75,  99,  1,   74,  99,  1,   74,  98,
        1,   73,  98,  1,   73,  97,  1,   72,  97,  1,   72,  96,  1,   71,  95,  1,   71,  95,  1,   70,  94,  1,
        70,  94,  1,   69,  93,  1,   69,  93,  1,   68,  92,  1,   68,  92,  1,   67,  91,  1,   67,  90,  1,   66,
        90,  1,   66,  89,  1,   65,  89,  1,   65,  88,  1,   65,  88,  1,   64,  88,  1,   64,  87,  1,   63,  87,
        1,   63,  86,  1,   62,  86,  1,   62,  85,  1,   62,  85,  1,   61,  85,  1,   61,  84,  1,   61,  84,  1,
        60,  84,  1,   60,  83,  1,   60,  83,  1,   60,  83,  1,   59,  82,  1,   59,  82,  1,   59,  82,  1,   58,
        81,  1,   58,  81,  1,   58,  81,  1,   58,  81,  1,   57,  80,  1,   57,  80,  1,   57,  80,  1,   57,  80,
        1,   57,  79,  1,   56,  79,  1,   56,  79,  1,   56,  79,  1,   56,  79,  1,   56,  79,  1,   56,  78,  1,
        56,  78,  1,   56,  78,  1,   55,  78,  1,   55,  78,  1,   55,  78,  1,   55,  78,  1,   55,  78,  1,   55,
        78,  1,   55,  78,  1,   55,  78,  1,   55,  78,  1,   55,  78,  1,   55,  78,  1,   55,  78,  1,   55,  78,
        1,   55,  78,  1,   55,  78,  1,   55,  78,  1,   55,  78,  1,   55,  78,  1,   55,  78,  1,   55,  78,  1,
        55,  78,  1,   55,  78,  1,   56,  78,  1,   56,  78,  1,   56,  78,  1,   56,  79,  1,   56,  79,  1,   56,
        79,  1,   56,  79,  1,   57,  79,  1,   57,  79,  1,   57,  80,  1,   57,  80,  1,   57,  80,  1,   57,  80,
        1,   58,  81,  1,   58,  81,  1,   58,  81,  1,   58,  81,  1,   59,  82,  1,   59,  82,  1,   59,  82,  1,
        60,  83,  1,   60,  83,  1,   60,  83,  1,   60,  84,  1,   61,  84,  1,   61,  84,  1,   61,  85,  1,   62,
        85,  1,   62,  85,  1,   63,  86,  1,   63,  86,  1,   63,  87,  1,   64,  87,  1,   64,  88,  1,   65,  88,
        1,   65,  88,  1,   65,  89,  1,   66,  89,  1,   66,  90,  1,   67,  90,  1,   67,  91,  1,   68,  91,  1,
        68,  92,  1,   69,  92,  1,   69,  93,  1,   70,  94,  1,   70,  94,  1,   71,  95,  1,   71,  95,  1,   72,
        96,  1,   72,  96,  1,   73,  97,  1,   73,  98,  1,   74,  98,  1,   75,  99,  1,   75,  99,  1,   76,  100,
        1,   76,  100, 1,   77,  100, 1,   77,  100, 1,   78,  100, 1,   79,  100, 1,   79,  100, 1,   80,  100, 1,
        81,  99,  1,   82,  98,  1,   83,  98,  1,   84,  97,  1,   85,  95,  1,   88,  92,  41,  27,  189, 1,   33,
        38,  1,   31,  40,  1,   29,  42,  1,   28,  43,  1,   27,  44,  1,   26,  45,  1,   26,  45,  1,   26,  46,
        1,   26,  47,  1,   25,  47,  1,   25,  48,  1,   25,  49,  1,   26,  49,  1,   26,  50,  1,   26,  50,  1,
        27,  51,  1,   27,  52,  1,   28,  52,  1,   28,  53,  1,   29,  53,  1,   30,  54,  1,   30,  54,  1,   31,
        55,  1,   31,  55,  1,   32,  56,  1,   33,  56,  1,   33,  57,  1,   34,  57,  1,   34,  58,  1,   35,  58,
        1,   35,  59,  1,   36,  59,  1,   36,  60,  1,   37,  60,  1,   37,  61,  1,   37,  61,  1,   38,  61,  1,
        38,  62,  1,   39,  62,  1,   39,  63,  1,   40,  63,  1,   40,  63,  1,   41,  64,  1,   41,  64,  1,   41,
        64,  1,   42,  65,  1,   42,  65,  1,   42,  66,  1,   43,  66,  1,   43,  66,  1,   43,  66,  1,   44,  67,
        1,   44,  67,  1,   44,  67,  1,   44,  67,  1,   45,  68,  1,   45,  68,  1,   45,  68,  1,   45,  68,  1,
        46,  68,  1,   46,  69,  1,   46,  69,  1,   46,  69,  1,   46,  69,  1,   47,  69,  1,   47,  70,  1,   47,
        70,  1,   47,  70,  1,   47,  70,  1,   47,  70,  1,   47,  70,  1,   48,  70,  1,   48,  70,  1,   48,  70,
        1,   48,  71,  1,   48,  71,  1,   48,  71,  1,   48,  71,  1,   48,  71,  1,   48,  71,  1,   48,  71,  1,
        48,  71,  1,   48,  71,  1,   48,  71,  1,   48,  71,  1,   48,  71,  1,   48,  71,  1,   48,  70,  1,   48,
        70,  1,   48,  70,  1,   48,  70,  1,   47,  70,  1,   47,  70,  1,   47,  70,  1,   47,  70,  1,   47,  70,
        1,   47,  70,  1,   47,  69,  1,   47,  69,  1,   46,  69,  1,   46,  69,  1,   46,  69,  1,   46,  68,  1,
        46,  68,  1,   45,  68,  1,   45,  68,  1,   45,  68,  1,   44,  67,  1,   44,  67,  1,   44,  67,  1,   44,
        67,  1,   43,  66,  1,   43,  66,  1,   43,  66,  1,   42,  65,  1,   42,  65,  1,   42,  65,  1,   41,  64,
        1,   41,  64,  1,   41,  64,  1,   40,  63,  1,   40,  63,  1,   39,  63,  1,   39,  62,  1,   38,  62,  1,
        38,  61,  1,   38,  61,  1,   37,  61,  1,   37,  60,  1,   36,  60,  1,   36,  59,  1,   35,  59,  1,   35,
        58,  1,   34,  58,  1,   34,  57,  1,   33,  57,  1,   33,  56,  1,   32,  56,  1,   31,  55,  1,   31,  55,
        1,   30,  54,  1,   30,  54,  1,   29,  53,  1,   28,  53,  1,   28,  52,  1,   27,  52,  1,   27,  51,  1,
        26,  50,  1,   26,  50,  1,   26,  49,  1,   25,  49,  1,   25,  48,  1,   25,  47,  1,   26,  47,  1,   26,
        46,  1,   26,  45,  1,   26,  45,  1,   27,  44,  1,   28,  43,  1,   29,  42,  1,   30,  40,  1,   33,  38,
        42,  29,  114, 1,   59,  65,  1,   57,  67,  1,   55,  68,  1,   54,  69,  1,   54,  70,  1,   53,  71,  1,
        53,  71,  1,   52,  71,  1,   52,  71,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  3,   25,  31,  52,  72,  93,  99,  3,   23,  34,
        52,  72,  90,  101, 3,   21,  37,  52,  72,  86,  103, 3,   20,  40,  52,  72,  83,  104, 3,   20,  43,  52,
        72,  80,  104, 3,   19,  47,  52,  72,  77,  105, 3,   18,  50,  52,  72,  74,  105, 1,   18,  106, 1,   18,
        106, 1,   18,  106, 1,   18,  106, 1,   18,  106, 1,   18,  106, 1,   18,  106, 1,   19,  105, 1,   19,  104,
        1,   20,  104, 1,   21,  103, 1,   22,  101, 1,   24,  99,  1,   27,  97,  1,   30,  94,  1,   33,  91,  1,
        36,  88,  1,   39,  84,  1,   43,  81,  1,   45,  78,  1,   45,  79,  1,   44,  80,  1,   43,  81,  1,   43,
        81,  1,   42,  82,  1,   41,  83,  1,   40,  83,  1,   40,  84,  1,   39,  85,  1,   38,  86,  1,   37,  86,
        1,   37,  87,  2,   36,  61,  63,  88,  2,   35,  60,  63,  88,  2,   35,  60,  64,  89,  2,   34,  59,  65,
        90,  2,   33,  58,  66,  91,  2,   33,  57,  66,  91,  2,   32,  57,  67,  92,  2,   32,  56,  68,  92,  2,
        31,  55,  69,  93,  2,   31,  55,  69,  93,  2,   31,  54,  70,  93,  2,   31,  53,  71,  93,  2,   31,  52,
        71,  93,  2,   31,  52,  72,  93,  2,   31,  51,  73,  92,  2,   32,  50,  74,  92,  2,   32,  49,  74,  91,
        2,   33,  49,  75,  91,  2,   34,  48,  76,  90,  2,   35,  46,  77,  88,  2,   37,  45,  79,  87,  43,  42,
        157, 1,   59,  65,  1,   57,  67,  1,   56,  69,  1,   55,  70,  1,   54,  70,  1,   53,  71,  1,   53,  71,
        1,   53,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   21,  102, 1,   15,  109, 1,   13,  111, 1,   12,  112,
        1,   11,  113, 1,   11,  113, 1,   10,  114, 1,   10,  114, 1,   10,  115, 1,   9,   115, 1,   9,   115, 1,
        9,   115, 1,   10,  115, 1,   10,  114, 1,   10,  114, 1,   11,  114, 1,   11,  113, 1,   12,  112, 1,   13,
        111, 1,   15,  109, 1,   19,  105, 1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   53,  72,  1,   53,  71,  1,   53,
        71,  1,   54,  71,  1,   54,  70,  1,   55,  69,  1,   56,  68,  1,   58,  66,  44,  130, 190, 1,   57,  84,
        1,   57,  84,  1,   57,  83,  1,   56,  82,  1,   56,  82,  1,   56,  81,  1,   55,  81,  1,   55,  80,  1,
        55,  80,  1,   55,  79,  1,   54,  79,  1,   54,  78,  1,   54,  78,  1,   54,  77,  1,   53,  77,  1,   53,
        76,  1,   53,  76,  1,   52,  75,  1,   52,  75,  1,   52,  74,  1,   52,  73,  1,   51,  73,  1,   51,  72,
        1,   51,  72,  1,   50,  71,  1,   50,  71,  1,   50,  70,  1,   50,  70,  1,   49,  69,  1,   49,  69,  1,
        49,  68,  1,   49,  68,  1,   48,  67,  1,   48,  67,  1,   48,  66,  1,   47,  66,  1,   47,  65,  1,   47,
        64,  1,   47,  64,  1,   46,  63,  1,   46,  63,  1,   46,  62,  1,   45,  62,  1,   45,  61,  1,   45,  61,
        1,   45,  60,  1,   44,  60,  1,   44,  59,  1,   44,  59,  1,   43,  58,  1,   43,  58,  1,   43,  57,  1,
        43,  56,  1,   43,  56,  1,   43,  55,  1,   43,  55,  1,   43,  54,  1,   44,  53,  1,   45,  52,  1,   47,
        51,  45,  90,  109, 1,   20,  105, 1,   20,  105, 1,   20,  105, 1,   20,  105, 1,   20,  105, 1,   20,  105,
        1,   20,  105, 1,   20,  105, 1,   20,  105, 1,   20,  105, 1,   20,  105, 1,   20,  105, 1,   20,  105, 1,
        20,  105, 1,   20,  105, 1,   20,  105, 1,   20,  105, 1,   20,  105, 1,   20,  105, 46,  134, 160, 1,   56,
        68,  1,   54,  71,  1,   52,  72,  1,   51,  74,  1,   50,  74,  1,   49,  75,  1,   49,  76,  1,   48,  76,
        1,   48,  77,  1,   48,  77,  1,   47,  77,  1,   47,  77,  1,   47,  78,  1,   47,  78,  1,   47,  77,  1,
        47,  77,  1,   48,  77,  1,   48,  77,  1,   48,  76,  1,   49,  76,  1,   49,  75,  1,   50,  75,  1,   51,
        74,  1,   52,  72,  1,   54,  71,  1,   56,  69,  47,  14,  181, 1,   94,  98,  1,   91,  101, 1,   90,  102,
        1,   89,  104, 1,   88,  104, 1,   88,  105, 1,   87,  106, 1,   86,  106, 1,   86,  106, 1,   85,  106, 1,
        85,  106, 1,   84,  106, 1,   84,  106, 1,   83,  106, 1,   83,  105, 1,   83,  105, 1,   82,  104, 1,   82,
        104, 1,   81,  103, 1,   81,  103, 1,   80,  103, 1,   80,  102, 1,   79,  102, 1,   79,  101, 1,   78,  101,
        1,   78,  100, 1,   77,  100, 1,   77,  99,  1,   77,  99,  1,   76,  98,  1,   76,  98,  1,   75,  97,  1,
        75,  97,  1,   74,  96,  1,   74,  96,  1,   73,  96,  1,   73,  95,  1,   72,  95,  1,   72,  94,  1,   71,
        94,  1,   71,  93,  1,   70,  93,  1,   70,  92,  1,   70,  92,  1,   69,  91,  1,   69,  91,  1,   68,  90,
        1,   68,  90,  1,   67,  90,  1,   67,  89,  1,   66,  89,  1,   66,  88,  1,   65,  88,  1,   65,  87,  1,
        64,  87,  1,   64,  86,  1,   63,  86,  1,   63,  85,  1,   63,  85,  1,   62,  84,  1,   62,  84,  1,   61,
        83,  1,   61,  83,  1,   60,  83,  1,   60,  82,  1,   59,  82,  1,   59,  81,  1,   58,  81,  1,   58,  80,
        1,   57,  80,  1,   57,  79,  1,   56,  79,  1,   56,  78,  1,   56,  78,  1,   55,  77,  1,   55,  77,  1,
        54,  77,  1,   54,  76,  1,   53,  76,  1,   53,  75,  1,   52,  75,  1,   52,  74,  1,   51,  74,  1,   51,
        73,  1,   50,  73,  1,   50,  72,  1,   50,  72,  1,   49,  71,  1,   49,  71,  1,   48,  71,  1,   48,  70,
        1,   47,  70,  1,   47,  69,  1,   46,  69,  1,   46,  68,  1,   45,  68,  1,   45,  67,  1,   44,  67,  1,
        44,  66,  1,   43,  66,  1,   43,  65,  1,   43,  65,  1,   42,  64,  1,   42,  64,  1,   41,  64,  1,   41,
        63,  1,   40,  63,  1,   40,  62,  1,   39,  62,  1,   39,  61,  1,   38,  61,  1,   38,  60,  1,   37,  60,
        1,   37,  59,  1,   36,  59,  1,   36,  58,  1,   36,  58,  1,   35,  58,  1,   35,  57,  1,   34,  57,  1,
        34,  56,  1,   33,  56,  1,   33,  55,  1,   32,  55,  1,   32,  54,  1,   31,  54,  1,   31,  53,  1,   30,
        53,  1,   30,  52,  1,   29,  52,  1,   29,  51,  1,   29,  51,  1,   28,  51,  1,   28,  50,  1,   27,  50,
        1,   27,  49,  1,   26,  49,  1,   26,  48,  1,   25,  48,  1,   25,  47,  1,   24,  47,  1,   24,  46,  1,
        23,  46,  1,   23,  45,  1,   22,  45,  1,   22,  45,  1,   22,  44,  1,   21,  44,  1,   21,  43,  1,   20,
        43,  1,   20,  42,  1,   19,  42,  1,   19,  41,  1,   19,  41,  1,   18,  40,  1,   18,  40,  1,   18,  39,
        1,   18,  39,  1,   18,  38,  1,   19,  38,  1,   19,  37,  1,   19,  37,  1,   20,  36,  1,   21,  35,  1,
        22,  34,  1,   24,  33,  1,   28,  29,  48,  26,  160, 1,   55,  69,  1,   51,  74,  1,   47,  77,  1,   45,
        79,  1,   43,  81,  1,   42,  83,  1,   40,  84,  1,   39,  85,  1,   38,  87,  1,   36,  88,  1,   35,  89,
        1,   34,  90,  1,   33,  90,  1,   33,  91,  1,   32,  92,  1,   31,  93,  1,   30,  93,  1,   30,  94,  1,
        29,  95,  1,   29,  95,  2,   28,  60,  65,  96,  2,   27,  55,  69,  97,  2,   27,  53,  71,  97,  2,   26,
        52,  73,  98,  2,   26,  50,  74,  98,  2,   25,  49,  75,  99,  2,   25,  48,  76,  99,  2,   24,  47,  77,
        100, 2,   24,  47,  78,  100, 2,   23,  46,  78,  101, 2,   23,  45,  79,  101, 2,   23,  45,  80,  102, 2,
        22,  44,  80,  102, 2,   22,  43,  81,  102, 2,   22,  43,  81,  103, 2,   21,  42,  82,  103, 2,   21,  42,
        82,  103, 2,   21,  42,  82,  104, 2,   20,  41,  83,  104, 2,   20,  41,  83,  104, 2,   20,  40,  84,  105,
        2,   20,  40,  84,  105, 2,   19,  40,  84,  105, 2,   19,  40,  84,  105, 2,   19,  39,  85,  105, 2,   19,
        39,  85,  105, 2,   19,  39,  85,  106, 2,   18,  39,  85,  106, 2,   18,  39,  85,  106, 2,   18,  39,  86,
        106, 2,   18,  39,  86,  106, 2,   18,  38,  86,  106, 2,   18,  38,  86,  106, 2,   18,  38,  86,  106, 2,
        18,  38,  86,  106, 2,   18,  38,  86,  106, 2,   18,  38,  86,  106, 2,   18,  38,  86,  106, 2,   18,  38,
        86,  106, 2,   18,  38,  86,  106, 2,   18,  38,  86,  106, 2,   18,  38,  86,  106, 2,   18,  38,  86,  106,
        2,   18,  38,  86,  106, 2,   18,  38,  86,  106, 2,   18,  38,  86,  106, 2,   18,  38,  86,  106, 2,   18,
        38,  86,  106, 2,   18,  38,  86,  106, 2,   18,  38,  86,  106, 2,   18,  38,  86,  106, 2,   18,  38,  86,
        106, 2,   18,  38,  86,  106, 2,   18,  38,  86,  106, 2,   18,  38,  86,  106, 2,   18,  38,  86,  106, 2,
        18,  38,  86,  106, 2,   18,  38,  86,  106, 2,   18,  38,  86,  106, 2,   18,  38,  86,  106, 2,   18,  38,
        86,  106, 2,   18,  38,  86,  106, 2,   18,  38,  86,  106, 2,   18,  39,  86,  106, 2,   18,  39,  86,  106,
        2,   18,  39,  85,  106, 2,   18,  39,  85,  106, 2,   19,  39,  85,  106, 2,   19,  39,  85,  106, 2,   19,
        40,  85,  105, 2,   19,  40,  85,  105, 2,   19,  40,  84,  105, 2,   20,  40,  84,  105, 2,   20,  40,  84,
        104, 2,   20,  41,  83,  104, 2,   20,  41,  83,  104, 2,   21,  42,  83,  104, 2,   21,  42,  82,  104, 2,
        21,  43,  82,  103, 2,   22,  43,  81,  103, 2,   22,  43,  81,  102, 2,   22,  44,  80,  102, 2,   23,  45,
        80,  102, 2,   23,  45,  79,  101, 2,   24,  46,  78,  101, 2,   24,  46,  78,  100, 2,   24,  47,  77,  100,
        2,   25,  48,  76,  100, 2,   25,  49,  75,  99,  2,   26,  50,  74,  99,  2,   26,  51,  73,  98,  2,   27,
        53,  71,  97,  2,   28,  55,  69,  97,  2,   28,  58,  67,  96,  1,   29,  96,  1,   29,  95,  1,   30,  95,
        1,   31,  94,  1,   31,  93,  1,   32,  93,  1,   33,  92,  1,   34,  91,  1,   34,  90,  1,   35,  89,  1,
        36,  88,  1,   37,  87,  1,   39,  86,  1,   40,  85,  1,   41,  83,  1,   43,  81,  1,   45,  79,  1,   47,
        77,  1,   50,  74,  1,   54,  70,  49,  26,  157, 1,   70,  72,  1,   66,  72,  1,   63,  72,  1,   59,  72,
        1,   55,  72,  1,   51,  72,  1,   47,  72,  1,   43,  72,  1,   39,  72,  1,   35,  72,  1,   31,  72,  1,
        28,  72,  1,   25,  72,  1,   23,  72,  1,   21,  72,  1,   20,  72,  1,   19,  72,  1,   19,  72,  1,   18,
        72,  1,   18,  72,  1,   18,  72,  1,   18,  72,  1,   18,  72,  1,   18,  72,  1,   18,  72,  1,   19,  72,
        1,   19,  72,  2,   20,  48,  52,  72,  2,   20,  44,  52,  72,  2,   21,  40,  52,  72,  2,   22,  36,  52,
        72,  2,   24,  32,  52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   26,  98,  1,   23,  101, 1,   22,  103, 1,   21,  104, 1,   20,  105, 1,   19,  105, 1,   19,  106,
        1,   18,  106, 1,   18,  106, 1,   18,  106, 1,   18,  106, 1,   18,  106, 1,   18,  106, 1,   19,  106, 1,
        19,  106, 1,   20,  105, 1,   20,  104, 1,   21,  103, 1,   23,  102, 1,   25,  99,  50,  26,  157, 1,   53,
        68,  1,   48,  73,  1,   45,  77,  1,   42,  79,  1,   39,  81,  1,   37,  84,  1,   36,  86,  1,   34,  87,
        1,   32,  88,  1,   31,  89,  1,   30,  91,  1,   28,  92,  1,   27,  93,  1,   26,  94,  1,   25,  95,  1,
        25,  96,  1,   24,  97,  1,   23,  97,  1,   22,  98,  1,   22,  99,  2,   21,  57,  64,  99,  2,   21,  52,
        69,  100, 2,   20,  49,  72,  100, 2,   19,  47,  74,  101, 2,   19,  45,  76,  101, 2,   19,  44,  77,  102,
        2,   18,  42,  78,  102, 2,   18,  41,  79,  102, 2,   17,  40,  80,  103, 2,   17,  40,  81,  103, 2,   17,
        39,  82,  103, 2,   17,  38,  82,  104, 2,   16,  38,  83,  104, 2,   16,  37,  83,  104, 2,   16,  37,  83,
        104, 2,   16,  37,  84,  104, 2,   16,  36,  84,  104, 2,   17,  36,  84,  104, 2,   17,  35,  84,  104, 2,
        18,  35,  84,  104, 2,   19,  34,  84,  104, 2,   20,  33,  84,  104, 2,   21,  32,  84,  104, 2,   23,  30,
        83,  104, 1,   83,  104, 1,   82,  104, 1,   82,  104, 1,   81,  104, 1,   81,  103, 1,   80,  103, 1,   79,
        103, 1,   78,  102, 1,   77,  102, 1,   77,  102, 1,   76,  101, 1,   75,  101, 1,   74,  100, 1,   73,  100,
        1,   72,  99,  1,   71,  98,  1,   69,  98,  1,   68,  97,  1,   67,  96,  1,   66,  95,  1,   65,  94,  1,
        64,  93,  1,   63,  92,  1,   62,  92,  1,   61,  91,  1,   60,  90,  1,   59,  89,  1,   57,  88,  1,   56,
        87,  1,   55,  86,  1,   54,  85,  1,   53,  84,  1,   52,  83,  1,   51,  81,  1,   49,  80,  1,   48,  79,
        1,   47,  78,  1,   46,  77,  1,   45,  76,  1,   44,  75,  1,   42,  74,  1,   41,  73,  1,   40,  71,  1,
        39,  70,  1,   38,  69,  1,   37,  68,  1,   35,  67,  1,   34,  66,  1,   33,  65,  1,   32,  63,  1,   31,
        62,  1,   29,  61,  1,   28,  60,  1,   27,  59,  1,   26,  58,  1,   25,  57,  1,   23,  55,  1,   22,  54,
        1,   21,  53,  1,   20,  52,  1,   18,  51,  1,   17,  50,  1,   16,  48,  1,   15,  47,  2,   14,  46,  92,
        98,  2,   12,  45,  90,  100, 2,   12,  44,  88,  101, 1,   12,  102, 1,   12,  103, 1,   12,  103, 1,   12,
        104, 1,   12,  104, 1,   12,  104, 1,   12,  104, 1,   12,  104, 1,   12,  104, 1,   12,  105, 1,   12,  105,
        1,   12,  105, 1,   12,  105, 1,   12,  105, 1,   12,  105, 1,   12,  105, 1,   12,  105, 1,   12,  105, 1,
        12,  105, 1,   12,  105, 51,  26,  160, 1,   53,  70,  1,   49,  75,  1,   44,  79,  1,   41,  82,  1,   39,
        84,  1,   36,  86,  1,   35,  88,  1,   33,  89,  1,   31,  90,  1,   30,  92,  1,   28,  93,  1,   27,  94,
        1,   26,  95,  1,   25,  96,  1,   24,  97,  1,   23,  97,  1,   23,  98,  1,   22,  99,  1,   22,  99,  1,
        21,  100, 2,   21,  56,  67,  101, 2,   21,  51,  72,  101, 2,   21,  48,  75,  102, 2,   21,  46,  76,  102,
        2,   21,  44,  78,  102, 2,   21,  43,  79,  103, 2,   21,  42,  80,  103, 2,   22,  40,  81,  103, 2,   23,
        39,  82,  104, 2,   23,  38,  82,  104, 2,   24,  37,  83,  104, 2,   26,  36,  83,  104, 2,   29,  34,  84,
        104, 1,   84,  104, 1,   84,  104, 1,   84,  104, 1,   84,  104, 1,   84,  104, 1,   84,  104, 1,   84,  104,
        1,   83,  104, 1,   83,  104, 1,   82,  104, 1,   82,  104, 1,   81,  104, 1,   80,  103, 1,   80,  103, 1,
        79,  103, 1,   77,  102, 1,   76,  102, 1,   74,  101, 1,   72,  101, 1,   57,  100, 1,   52,  100, 1,   51,
        99,  1,   49,  98,  1,   48,  98,  1,   48,  97,  1,   47,  96,  1,   47,  95,  1,   47,  94,  1,   46,  93,
        1,   46,  92,  1,   46,  91,  1,   46,  92,  1,   47,  94,  1,   47,  95,  1,   47,  96,  1,   48,  97,  1,
        49,  98,  1,   50,  99,  1,   52,  100, 1,   55,  101, 1,   63,  102, 1,   69,  102, 1,   72,  103, 1,   74,
        104, 1,   76,  104, 1,   78,  105, 1,   79,  105, 1,   80,  106, 1,   81,  106, 1,   82,  107, 1,   83,  107,
        1,   84,  107, 1,   85,  108, 1,   86,  108, 1,   86,  108, 1,   87,  108, 1,   87,  108, 1,   88,  109, 1,
        88,  109, 1,   88,  109, 1,   88,  109, 1,   88,  109, 1,   89,  109, 1,   89,  109, 1,   88,  109, 1,   88,
        109, 1,   88,  109, 1,   88,  109, 1,   88,  108, 1,   87,  108, 1,   87,  108, 1,   86,  108, 1,   85,  108,
        1,   84,  107, 2,   21,  28,  83,  107, 2,   19,  30,  82,  107, 2,   18,  32,  80,  106, 2,   17,  34,  78,
        106, 2,   16,  36,  75,  105, 2,   16,  39,  71,  105, 2,   15,  45,  64,  104, 1,   15,  104, 1,   15,  103,
        1,   15,  102, 1,   15,  102, 1,   15,  101, 1,   15,  100, 1,   15,  99,  1,   16,  98,  1,   16,  97,  1,
        17,  96,  1,   18,  95,  1,   19,  93,  1,   21,  92,  1,   23,  90,  1,   25,  88,  1,   27,  86,  1,   29,
        83,  1,   33,  80,  1,   37,  76,  1,   44,  69,  52,  29,  157, 1,   65,  92,  1,   64,  92,  1,   64,  92,
        1,   63,  92,  1,   62,  92,  1,   62,  92,  1,   61,  92,  1,   61,  92,  1,   60,  92,  1,   60,  92,  1,
        59,  92,  1,   58,  92,  1,   58,  92,  1,   57,  92,  1,   57,  92,  1,   56,  92,  1,   55,  92,  1,   55,
        92,  1,   54,  92,  1,   54,  92,  1,   53,  92,  1,   52,  92,  1,   52,  92,  1,   51,  92,  1,   51,  92,
        1,   50,  92,  1,   50,  92,  1,   49,  92,  1,   48,  92,  1,   48,  92,  2,   47,  71,  72,  92,  2,   47,
        70,  72,  92,  2,   46,  70,  72,  92,  2,   45,  69,  72,  92,  2,   45,  69,  72,  92,  2,   44,  68,  72,
        92,  2,   44,  68,  72,  92,  2,   43,  67,  72,  92,  2,   43,  66,  72,  92,  2,   42,  66,  72,  92,  2,
        41,  65,  72,  92,  2,   41,  65,  72,  92,  2,   40,  64,  72,  92,  2,   40,  63,  72,  92,  2,   39,  63,
        72,  92,  2,   38,  62,  72,  92,  2,   38,  62,  72,  92,  2,   37,  61,  72,  92,  2,   37,  60,  72,  92,
        2,   36,  60,  72,  92,  2,   35,  59,  72,  92,  2,   35,  59,  72,  92,  2,   34,  58,  72,  92,  2,   34,
        58,  72,  92,  2,   33,  57,  72,  92,  2,   33,  56,  72,  92,  2,   32,  56,  72,  92,  2,   31,  55,  72,
        92,  2,   31,  55,  72,  92,  2,   30,  54,  72,  92,  2,   30,  53,  72,  92,  2,   29,  53,  72,  92,  2,
        28,  52,  72,  92,  2,   28,  52,  72,  92,  2,   27,  51,  72,  92,  2,   27,  50,  72,  92,  2,   26,  50,
        72,  92,  2,   25,  49,  72,  92,  2,   25,  49,  72,  92,  2,   24,  48,  72,  92,  2,   24,  47,  72,  92,
        2,   23,  47,  72,  92,  2,   23,  46,  72,  92,  2,   22,  46,  72,  92,  2,   21,  45,  72,  92,  2,   21,
        45,  72,  92,  2,   20,  44,  72,  92,  2,   20,  43,  72,  92,  2,   19,  43,  72,  92,  2,   18,  42,  72,
        92,  1,   18,  98,  1,   17,  100, 1,   17,  101, 1,   16,  102, 1,   16,  103, 1,   16,  104, 1,   16,  104,
        1,   16,  104, 1,   16,  104, 1,   16,  104, 1,   16,  104, 1,   16,  104, 1,   16,  104, 1,   16,  104, 1,
        16,  103, 1,   16,  103, 1,   16,  102, 1,   16,  101, 1,   16,  99,  1,   16,  96,  1,   72,  92,  1,   72,
        92,  1,   72,  92,  1,   72,  92,  1,   72,  92,  1,   72,  92,  1,   72,  92,  1,   72,  92,  1,   58,  97,
        1,   55,  99,  1,   54,  101, 1,   53,  102, 1,   52,  103, 1,   51,  103, 1,   51,  104, 1,   50,  104, 1,
        50,  104, 1,   50,  104, 1,   50,  104, 1,   50,  104, 1,   50,  104, 1,   51,  104, 1,   51,  103, 1,   52,
        103, 1,   52,  102, 1,   53,  101, 1,   55,  100, 1,   57,  97,  53,  29,  160, 1,   25,  91,  1,   25,  95,
        1,   25,  96,  1,   25,  98,  1,   25,  99,  1,   25,  99,  1,   25,  100, 1,   25,  100, 1,   25,  100, 1,
        25,  100, 1,   25,  100, 1,   25,  100, 1,   25,  100, 1,   25,  100, 1,   25,  100, 1,   25,  99,  1,   25,
        98,  1,   25,  97,  1,   25,  96,  1,   25,  94,  1,   25,  45,  1,   25,  45,  1,   25,  45,  1,   25,  45,
        1,   25,  45,  1,   25,  45,  1,   25,  45,  1,   25,  45,  1,   25,  45,  1,   25,  45,  1,   25,  45,  1,
        25,  45,  1,   25,  45,  1,   25,  45,  1,   25,  45,  1,   25,  45,  1,   25,  45,  1,   25,  45,  1,   25,
        45,  1,   25,  45,  1,   25,  45,  2,   25,  45,  59,  72,  2,   25,  45,  53,  77,  2,   25,  45,  49,  80,
        1,   25,  83,  1,   25,  85,  1,   25,  87,  1,   25,  89,  1,   25,  90,  1,   25,  92,  1,   25,  93,  1,
        25,  94,  1,   25,  95,  1,   25,  96,  1,   25,  97,  1,   25,  98,  1,   25,  99,  1,   25,  100, 1,   25,
        100, 1,   25,  101, 1,   25,  102, 2,   25,  64,  67,  102, 2,   25,  57,  73,  103, 2,   26,  53,  76,  104,
        2,   26,  50,  77,  104, 2,   27,  47,  79,  105, 2,   27,  45,  80,  105, 2,   28,  43,  81,  105, 2,   29,
        40,  82,  106, 2,   31,  38,  83,  106, 1,   84,  107, 1,   85,  107, 1,   85,  107, 1,   86,  107, 1,   86,
        108, 1,   87,  108, 1,   87,  108, 1,   87,  108, 1,   88,  108, 1,   88,  108, 1,   88,  109, 1,   88,  109,
        1,   88,  109, 1,   88,  109, 1,   88,  109, 1,   89,  109, 1,   89,  109, 1,   89,  109, 1,   89,  109, 1,
        88,  109, 1,   88,  109, 1,   88,  109, 1,   88,  109, 1,   88,  109, 1,   88,  108, 1,   88,  108, 1,   87,
        108, 1,   87,  108, 1,   87,  108, 1,   86,  108, 1,   86,  107, 2,   22,  27,  85,  107, 2,   20,  29,  84,
        107, 2,   18,  31,  83,  106, 2,   17,  32,  82,  106, 2,   16,  34,  81,  106, 2,   16,  35,  80,  105, 2,
        15,  37,  78,  105, 2,   15,  39,  75,  104, 2,   14,  42,  72,  104, 2,   14,  48,  66,  103, 1,   14,  103,
        1,   15,  102, 1,   15,  101, 1,   15,  101, 1,   15,  100, 1,   16,  99,  1,   16,  98,  1,   17,  97,  1,
        18,  96,  1,   19,  95,  1,   21,  94,  1,   22,  92,  1,   24,  91,  1,   26,  89,  1,   28,  87,  1,   30,
        85,  1,   33,  83,  1,   36,  80,  1,   40,  76,  1,   46,  70,  54,  26,  160, 1,   79,  94,  1,   74,  99,
        1,   71,  102, 1,   68,  104, 1,   65,  106, 1,   63,  107, 1,   61,  108, 1,   59,  109, 1,   57,  110, 1,
        56,  110, 1,   54,  110, 1,   53,  111, 1,   51,  111, 1,   50,  111, 1,   49,  111, 1,   48,  110, 1,   46,
        110, 1,   45,  110, 1,   44,  109, 1,   43,  109, 2,   42,  82,  92,  108, 2,   41,  78,  95,  107, 2,   40,
        75,  97,  105, 2,   39,  73,  100, 101, 1,   38,  70,  1,   38,  68,  1,   37,  67,  1,   36,  65,  1,   35,
        64,  1,   35,  62,  1,   34,  61,  1,   33,  60,  1,   33,  59,  1,   32,  58,  1,   31,  57,  1,   31,  56,
        1,   30,  55,  1,   30,  54,  1,   29,  53,  1,   29,  53,  1,   28,  52,  1,   28,  51,  1,   27,  50,  1,
        27,  50,  1,   27,  49,  1,   26,  49,  1,   26,  48,  1,   26,  48,  1,   25,  47,  1,   25,  47,  2,   25,
        46,  68,  76,  2,   25,  46,  62,  81,  2,   24,  46,  59,  84,  2,   24,  45,  56,  86,  2,   24,  45,  54,
        88,  2,   24,  45,  52,  90,  2,   24,  44,  50,  92,  2,   23,  44,  49,  93,  2,   23,  44,  48,  94,  2,
        23,  44,  46,  95,  2,   23,  43,  45,  97,  2,   23,  43,  44,  98,  1,   23,  99,  1,   23,  100, 1,   23,
        100, 1,   23,  101, 1,   23,  102, 1,   23,  103, 1,   23,  103, 1,   23,  104, 1,   23,  105, 2,   23,  66,
        76,  105, 2,   23,  63,  78,  106, 2,   23,  62,  80,  106, 2,   23,  60,  82,  107, 2,   23,  59,  83,  107,
        2,   23,  58,  84,  108, 2,   23,  57,  85,  108, 2,   23,  56,  85,  108, 2,   23,  55,  86,  109, 2,   23,
        54,  87,  109, 2,   23,  53,  88,  109, 2,   23,  52,  88,  109, 2,   23,  51,  88,  110, 2,   24,  51,  89,
        110, 2,   24,  50,  89,  110, 2,   24,  50,  90,  110, 2,   24,  49,  90,  110, 2,   24,  48,  90,  110, 2,
        24,  48,  90,  111, 2,   24,  47,  90,  111, 2,   25,  47,  91,  111, 2,   25,  46,  91,  111, 2,   25,  46,
        91,  111, 2,   25,  46,  91,  111, 2,   25,  47,  91,  111, 2,   26,  47,  91,  111, 2,   26,  47,  91,  111,
        2,   26,  48,  90,  111, 2,   27,  48,  90,  111, 2,   27,  48,  90,  111, 2,   27,  49,  90,  110, 2,   27,
        49,  89,  110, 2,   28,  50,  89,  110, 2,   28,  50,  88,  110, 2,   29,  51,  88,  110, 2,   29,  52,  87,
        109, 2,   29,  53,  86,  109, 2,   30,  54,  85,  109, 2,   30,  55,  84,  109, 2,   31,  56,  83,  108, 2,
        31,  58,  81,  108, 2,   32,  60,  79,  107, 2,   32,  64,  75,  107, 1,   33,  106, 1,   33,  106, 1,   34,
        105, 1,   34,  105, 1,   35,  104, 1,   36,  103, 1,   37,  102, 1,   38,  101, 1,   39,  101, 1,   40,  100,
        1,   41,  98,  1,   42,  97,  1,   43,  96,  1,   45,  95,  1,   46,  93,  1,   48,  91,  1,   50,  89,  1,
        53,  87,  1,   56,  83,  1,   61,  79,  55,  29,  157, 1,   16,  104, 1,   16,  104, 1,   16,  104, 1,   16,
        104, 1,   16,  104, 1,   16,  104, 1,   16,  104, 1,   16,  104, 1,   16,  104, 1,   16,  104, 1,   16,  104,
        1,   16,  104, 1,   16,  104, 1,   16,  104, 1,   16,  104, 1,   16,  104, 1,   16,  104, 1,   16,  104, 1,
        16,  104, 1,   16,  104, 2,   16,  37,  83,  104, 2,   16,  36,  83,  104, 2,   17,  36,  82,  104, 2,   17,
        36,  82,  103, 2,   17,  36,  82,  103, 2,   17,  36,  81,  103, 2,   18,  35,  81,  102, 2,   18,  35,  81,
        102, 2,   19,  34,  80,  102, 2,   20,  33,  80,  101, 2,   22,  31,  80,  101, 2,   24,  28,  79,  101, 1,
        79,  100, 1,   79,  100, 1,   78,  100, 1,   78,  99,  1,   78,  99,  1,   77,  99,  1,   77,  98,  1,   77,
        98,  1,   76,  98,  1,   76,  97,  1,   76,  97,  1,   75,  97,  1,   75,  96,  1,   75,  96,  1,   74,  96,
        1,   74,  95,  1,   74,  95,  1,   73,  95,  1,   73,  94,  1,   73,  94,  1,   72,  94,  1,   72,  93,  1,
        71,  93,  1,   71,  93,  1,   71,  92,  1,   70,  92,  1,   70,  92,  1,   70,  91,  1,   69,  91,  1,   69,
        91,  1,   69,  90,  1,   68,  90,  1,   68,  90,  1,   68,  89,  1,   67,  89,  1,   67,  89,  1,   67,  88,
        1,   66,  88,  1,   66,  88,  1,   66,  87,  1,   65,  87,  1,   65,  87,  1,   65,  86,  1,   64,  86,  1,
        64,  86,  1,   64,  85,  1,   63,  85,  1,   63,  85,  1,   63,  84,  1,   62,  84,  1,   62,  84,  1,   62,
        83,  1,   61,  83,  1,   61,  83,  1,   61,  82,  1,   60,  82,  1,   60,  82,  1,   60,  81,  1,   59,  81,
        1,   59,  81,  1,   59,  80,  1,   58,  80,  1,   58,  80,  1,   58,  79,  1,   57,  79,  1,   57,  79,  1,
        57,  78,  1,   56,  78,  1,   56,  78,  1,   56,  77,  1,   55,  77,  1,   55,  77,  1,   55,  76,  1,   54,
        76,  1,   54,  76,  1,   54,  75,  1,   53,  75,  1,   53,  74,  1,   53,  74,  1,   52,  74,  1,   52,  73,
        1,   52,  73,  1,   51,  73,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  71,  1,   51,  71,  1,
        51,  71,  1,   51,  70,  1,   51,  70,  1,   52,  69,  1,   53,  69,  1,   54,  68,  1,   55,  67,  1,   57,
        65,  56,  26,  160, 1,   55,  70,  1,   50,  74,  1,   46,  78,  1,   44,  81,  1,   42,  83,  1,   40,  85,
        1,   38,  87,  1,   37,  88,  1,   35,  89,  1,   34,  91,  1,   33,  92,  1,   31,  93,  1,   31,  94,  1,
        30,  95,  1,   29,  96,  1,   28,  96,  1,   27,  97,  1,   26,  98,  1,   26,  99,  1,   25,  99,  2,   25,
        58,  66,  100, 2,   24,  53,  71,  100, 2,   23,  51,  73,  101, 2,   23,  49,  75,  102, 2,   22,  47,  77,
        102, 2,   22,  46,  78,  102, 2,   22,  45,  79,  103, 2,   22,  44,  80,  103, 2,   21,  43,  81,  103, 2,
        21,  43,  81,  104, 2,   21,  42,  82,  104, 2,   21,  42,  82,  104, 2,   20,  41,  83,  104, 2,   20,  41,
        83,  104, 2,   20,  41,  83,  104, 2,   20,  40,  84,  104, 2,   20,  40,  84,  104, 2,   20,  40,  84,  105,
        2,   20,  40,  84,  105, 2,   20,  40,  84,  104, 2,   20,  40,  84,  104, 2,   20,  41,  83,  104, 2,   20,
        41,  83,  104, 2,   20,  41,  83,  104, 2,   21,  41,  83,  104, 2,   21,  42,  82,  104, 2,   21,  42,  82,
        104, 2,   21,  43,  81,  103, 2,   22,  44,  80,  103, 2,   22,  44,  80,  103, 2,   22,  45,  79,  102, 2,
        23,  46,  78,  102, 2,   23,  48,  76,  101, 2,   24,  49,  75,  101, 2,   24,  51,  73,  100, 2,   25,  54,
        70,  100, 2,   25,  61,  63,  99,  1,   26,  99,  1,   27,  98,  1,   27,  97,  1,   28,  96,  1,   29,  96,
        1,   30,  95,  1,   31,  94,  1,   32,  93,  1,   33,  92,  1,   34,  91,  1,   32,  92,  1,   31,  93,  1,
        30,  94,  1,   29,  95,  1,   28,  96,  1,   27,  97,  1,   26,  98,  1,   26,  99,  1,   25,  100, 1,   24,
        100, 2,   23,  57,  67,  101, 2,   23,  53,  71,  102, 2,   22,  51,  73,  102, 2,   22,  49,  75,  103, 2,
        21,  48,  77,  103, 2,   21,  46,  78,  104, 2,   20,  45,  79,  104, 2,   20,  44,  80,  104, 2,   20,  43,
        81,  105, 2,   19,  43,  82,  105, 2,   19,  42,  83,  105, 2,   19,  41,  83,  106, 2,   19,  41,  84,  106,
        2,   19,  40,  84,  106, 2,   18,  40,  85,  106, 2,   18,  39,  85,  106, 2,   18,  39,  86,  106, 2,   18,
        39,  86,  106, 2,   18,  39,  86,  106, 2,   18,  38,  86,  106, 2,   18,  38,  86,  106, 2,   18,  38,  86,
        106, 2,   18,  38,  86,  106, 2,   18,  39,  86,  106, 2,   18,  39,  86,  106, 2,   18,  39,  85,  106, 2,
        19,  40,  85,  106, 2,   19,  40,  84,  106, 2,   19,  41,  84,  105, 2,   19,  41,  83,  105, 2,   20,  42,
        82,  105, 2,   20,  43,  81,  105, 2,   20,  45,  80,  104, 2,   21,  46,  78,  104, 2,   21,  48,  76,  104,
        2,   21,  51,  74,  103, 2,   22,  56,  69,  103, 1,   22,  102, 1,   23,  102, 1,   24,  101, 1,   24,  100,
        1,   25,  100, 1,   26,  99,  1,   27,  98,  1,   28,  97,  1,   28,  96,  1,   29,  95,  1,   31,  94,  1,
        32,  92,  1,   33,  91,  1,   35,  89,  1,   37,  87,  1,   39,  85,  1,   42,  83,  1,   44,  80,  1,   48,
        76,  1,   53,  71,  57,  26,  160, 1,   56,  71,  1,   52,  75,  1,   49,  78,  1,   46,  80,  1,   44,  83,
        1,   43,  84,  1,   41,  86,  1,   40,  87,  1,   38,  89,  1,   37,  90,  1,   36,  91,  1,   35,  92,  1,
        34,  93,  1,   34,  94,  1,   33,  95,  1,   32,  96,  1,   31,  97,  1,   31,  97,  1,   30,  98,  1,   29,
        99,  2,   29,  61,  66,  99,  2,   28,  57,  71,  100, 2,   28,  55,  73,  101, 2,   27,  53,  75,  101, 2,
        27,  51,  76,  102, 2,   26,  50,  77,  102, 2,   26,  49,  78,  103, 2,   26,  48,  79,  103, 2,   25,  48,
        80,  104, 2,   25,  47,  81,  104, 2,   25,  46,  82,  104, 2,   24,  46,  82,  105, 2,   24,  45,  83,  105,
        2,   24,  45,  83,  105, 2,   24,  44,  84,  106, 2,   23,  44,  84,  106, 2,   23,  44,  85,  106, 2,   23,
        44,  85,  107, 2,   23,  43,  86,  107, 2,   23,  43,  86,  107, 2,   23,  43,  86,  107, 2,   23,  43,  86,
        108, 2,   23,  43,  87,  108, 2,   23,  43,  87,  108, 2,   23,  43,  87,  108, 2,   23,  43,  88,  109, 2,
        23,  43,  88,  109, 2,   23,  43,  87,  109, 2,   23,  43,  87,  109, 2,   23,  43,  86,  109, 2,   23,  43,
        86,  110, 2,   23,  44,  85,  110, 2,   23,  44,  84,  110, 2,   23,  44,  84,  110, 2,   23,  44,  83,  110,
        2,   24,  45,  83,  110, 2,   24,  45,  82,  110, 2,   24,  46,  81,  110, 2,   24,  46,  80,  111, 2,   25,
        47,  80,  111, 2,   25,  47,  79,  111, 2,   25,  48,  78,  111, 2,   26,  49,  77,  111, 2,   26,  50,  76,
        111, 2,   27,  51,  75,  111, 2,   27,  52,  74,  111, 2,   27,  53,  73,  111, 2,   28,  55,  71,  111, 2,
        29,  57,  69,  111, 2,   29,  61,  64,  111, 1,   30,  111, 1,   30,  111, 1,   31,  111, 1,   32,  111, 1,
        33,  111, 1,   34,  111, 1,   34,  111, 1,   35,  111, 1,   36,  111, 2,   37,  89,  90,  110, 2,   38,  88,
        90,  110, 2,   40,  86,  90,  110, 2,   41,  85,  89,  110, 2,   42,  83,  89,  110, 2,   44,  82,  89,  110,
        2,   46,  80,  88,  109, 2,   48,  78,  88,  109, 2,   50,  76,  87,  109, 2,   53,  73,  87,  109, 2,   57,
        68,  86,  108, 1,   86,  108, 1,   85,  108, 1,   85,  107, 1,   84,  107, 1,   83,  107, 1,   82,  106, 1,
        82,  106, 1,   81,  105, 1,   80,  105, 1,   79,  104, 1,   78,  104, 1,   77,  103, 1,   76,  103, 1,   75,
        102, 1,   73,  101, 1,   72,  101, 1,   70,  100, 1,   69,  99,  1,   67,  98,  1,   65,  98,  1,   63,  97,
        2,   28,  37,  60,  96,  2,   27,  40,  57,  95,  2,   26,  43,  52,  94,  1,   25,  93,  1,   24,  92,  1,
        24,  91,  1,   23,  90,  1,   23,  89,  1,   23,  87,  1,   23,  86,  1,   23,  85,  1,   23,  83,  1,   23,
        82,  1,   24,  80,  1,   24,  78,  1,   25,  77,  1,   25,  75,  1,   26,  73,  1,   28,  70,  1,   29,  68,
        1,   31,  65,  1,   34,  61,  1,   38,  56,  58,  68,  161, 1,   56,  68,  1,   54,  71,  1,   52,  72,  1,
        51,  73,  1,   50,  74,  1,   49,  75,  1,   49,  76,  1,   48,  76,  1,   48,  77,  1,   47,  77,  1,   47,
        77,  1,   47,  77,  1,   47,  77,  1,   47,  77,  1,   47,  77,  1,   47,  77,  1,   47,  77,  1,   48,  77,
        1,   48,  76,  1,   49,  76,  1,   49,  75,  1,   50,  74,  1,   51,  73,  1,   52,  72,  1,   54,  71,  1,
        56,  68,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   57,
        67,  1,   55,  70,  1,   53,  72,  1,   51,  73,  1,   50,  74,  1,   50,  75,  1,   49,  76,  1,   48,  76,
        1,   48,  77,  1,   48,  77,  1,   47,  77,  1,   47,  77,  1,   47,  77,  1,   47,  78,  1,   47,  77,  1,
        47,  77,  1,   47,  77,  1,   48,  77,  1,   48,  77,  1,   49,  76,  1,   49,  75,  1,   50,  75,  1,   51,
        74,  1,   52,  73,  1,   53,  71,  1,   55,  69,  1,   59,  66,  59,  68,  179, 1,   67,  76,  1,   64,  79,
        1,   62,  81,  1,   61,  82,  1,   60,  83,  1,   59,  84,  1,   58,  85,  1,   58,  85,  1,   57,  86,  1,
        57,  86,  1,   57,  86,  1,   57,  87,  1,   56,  87,  1,   56,  87,  1,   56,  87,  1,   57,  87,  1,   57,
        86,  1,   57,  86,  1,   57,  86,  1,   58,  85,  1,   58,  85,  1,   59,  84,  1,   60,  83,  1,   61,  82,
        1,   62,  81,  1,   64,  79,  1,   67,  76,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   55,
        81,  1,   55,  80,  1,   54,  80,  1,   54,  79,  1,   54,  78,  1,   54,  78,  1,   53,  77,  1,   53,  76,
        1,   53,  76,  1,   52,  75,  1,   52,  75,  1,   52,  74,  1,   51,  73,  1,   51,  73,  1,   51,  72,  1,
        50,  71,  1,   50,  71,  1,   50,  70,  1,   49,  70,  1,   49,  69,  1,   49,  68,  1,   49,  68,  1,   48,
        67,  1,   48,  67,  1,   48,  66,  1,   47,  65,  1,   47,  65,  1,   47,  64,  1,   46,  63,  1,   46,  63,
        1,   46,  62,  1,   45,  62,  1,   45,  61,  1,   45,  60,  1,   44,  60,  1,   44,  59,  1,   44,  59,  1,
        44,  58,  1,   43,  57,  1,   43,  57,  1,   43,  56,  1,   42,  55,  1,   42,  55,  1,   42,  54,  1,   41,
        54,  1,   41,  53,  1,   41,  52,  1,   42,  52,  1,   42,  51,  1,   43,  50,  1,   46,  48,  60,  42,  157,
        1,   103, 107, 1,   100, 109, 1,   98,  111, 1,   96,  112, 1,   94,  113, 1,   92,  113, 1,   90,  114, 1,
        89,  114, 1,   87,  115, 1,   85,  115, 1,   83,  115, 1,   81,  115, 1,   80,  115, 1,   78,  114, 1,   76,
        114, 1,   74,  113, 1,   72,  113, 1,   71,  112, 1,   69,  111, 1,   67,  109, 1,   65,  107, 1,   63,  106,
        1,   62,  104, 1,   60,  102, 1,   58,  100, 1,   56,  99,  1,   54,  97,  1,   53,  95,  1,   51,  93,  1,
        49,  91,  1,   47,  90,  1,   45,  88,  1,   44,  86,  1,   42,  84,  1,   40,  82,  1,   38,  80,  1,   36,
        79,  1,   35,  77,  1,   33,  75,  1,   31,  73,  1,   29,  71,  1,   27,  70,  1,   26,  68,  1,   24,  66,
        1,   22,  64,  1,   20,  62,  1,   18,  61,  1,   17,  59,  1,   15,  57,  1,   13,  55,  1,   11,  53,  1,
        9,   52,  1,   8,   50,  1,   6,   48,  1,   4,   46,  1,   2,   44,  1,   0,   43,  1,   0,   41,  1,   1,
        43,  1,   2,   45,  1,   4,   46,  1,   6,   48,  1,   8,   50,  1,   10,  52,  1,   11,  54,  1,   13,  55,
        1,   15,  57,  1,   17,  59,  1,   19,  61,  1,   20,  63,  1,   22,  64,  1,   24,  66,  1,   26,  68,  1,
        28,  70,  1,   29,  72,  1,   31,  73,  1,   33,  75,  1,   35,  77,  1,   37,  79,  1,   38,  81,  1,   40,
        82,  1,   42,  84,  1,   44,  86,  1,   46,  88,  1,   47,  90,  1,   49,  91,  1,   51,  93,  1,   53,  95,
        1,   55,  97,  1,   56,  99,  1,   58,  100, 1,   60,  102, 1,   62,  104, 1,   64,  106, 1,   65,  108, 1,
        67,  109, 1,   69,  111, 1,   71,  112, 1,   73,  113, 1,   74,  113, 1,   76,  114, 1,   78,  114, 1,   80,
        115, 1,   82,  115, 1,   83,  115, 1,   85,  115, 1,   87,  115, 1,   89,  114, 1,   90,  114, 1,   92,  113,
        1,   94,  113, 1,   96,  112, 1,   98,  111, 1,   100, 109, 1,   103, 107, 61,  70,  129, 1,   15,  109, 1,
        11,  113, 1,   9,   115, 1,   8,   116, 1,   7,   117, 1,   7,   118, 1,   6,   118, 1,   6,   119, 1,   5,
        119, 1,   5,   119, 1,   5,   119, 1,   5,   119, 1,   5,   119, 1,   6,   119, 1,   6,   118, 1,   7,   118,
        1,   7,   117, 1,   8,   116, 1,   10,  115, 1,   12,  113, 1,   18,  107, 0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   18,  107, 1,   12,  113, 1,   10,  115, 1,   8,   116,
        1,   7,   117, 1,   7,   118, 1,   6,   118, 1,   6,   119, 1,   5,   119, 1,   5,   119, 1,   5,   119, 1,
        5,   119, 1,   5,   119, 1,   6,   119, 1,   6,   118, 1,   7,   118, 1,   7,   117, 1,   8,   116, 1,   9,
        115, 1,   11,  113, 1,   15,  109, 62,  42,  157, 1,   17,  21,  1,   15,  24,  1,   14,  27,  1,   12,  29,
        1,   12,  30,  1,   11,  32,  1,   10,  34,  1,   10,  36,  1,   10,  38,  1,   10,  39,  1,   10,  41,  1,
        10,  43,  1,   10,  45,  1,   10,  47,  1,   10,  48,  1,   11,  50,  1,   12,  52,  1,   13,  54,  1,   14,
        56,  1,   15,  57,  1,   17,  59,  1,   19,  61,  1,   20,  63,  1,   22,  65,  1,   24,  66,  1,   26,  68,
        1,   28,  70,  1,   29,  72,  1,   31,  74,  1,   33,  75,  1,   35,  77,  1,   37,  79,  1,   38,  81,  1,
        40,  83,  1,   42,  84,  1,   44,  86,  1,   46,  88,  1,   47,  90,  1,   49,  92,  1,   51,  93,  1,   53,
        95,  1,   55,  97,  1,   57,  99,  1,   58,  101, 1,   60,  102, 1,   62,  104, 1,   64,  106, 1,   66,  108,
        1,   67,  110, 1,   69,  111, 1,   71,  113, 1,   73,  115, 1,   75,  117, 1,   76,  119, 1,   78,  120, 1,
        80,  122, 1,   82,  124, 1,   83,  126, 1,   82,  124, 1,   80,  122, 1,   78,  120, 1,   76,  118, 1,   74,
        117, 1,   73,  115, 1,   71,  113, 1,   69,  111, 1,   67,  109, 1,   65,  108, 1,   64,  106, 1,   62,  104,
        1,   60,  102, 1,   58,  100, 1,   56,  99,  1,   55,  97,  1,   53,  95,  1,   51,  93,  1,   49,  91,  1,
        47,  90,  1,   46,  88,  1,   44,  86,  1,   42,  84,  1,   40,  82,  1,   38,  81,  1,   37,  79,  1,   35,
        77,  1,   33,  75,  1,   31,  73,  1,   29,  72,  1,   28,  70,  1,   26,  68,  1,   24,  66,  1,   22,  64,
        1,   20,  63,  1,   18,  61,  1,   17,  59,  1,   15,  57,  1,   14,  55,  1,   12,  54,  1,   12,  52,  1,
        11,  50,  1,   10,  48,  1,   10,  46,  1,   10,  45,  1,   10,  43,  1,   10,  41,  1,   10,  39,  1,   10,
        37,  1,   10,  36,  1,   10,  34,  1,   11,  32,  1,   12,  30,  1,   12,  28,  1,   14,  27,  1,   15,  24,
        1,   17,  21,  63,  34,  160, 1,   60,  67,  1,   52,  75,  1,   47,  79,  1,   43,  82,  1,   40,  84,  1,
        38,  86,  1,   35,  88,  1,   33,  90,  1,   30,  91,  1,   28,  92,  1,   26,  94,  1,   24,  95,  1,   22,
        96,  1,   22,  97,  1,   22,  98,  1,   22,  98,  1,   22,  99,  1,   22,  100, 1,   22,  101, 1,   22,  101,
        1,   22,  102, 2,   22,  55,  72,  102, 2,   22,  51,  75,  103, 2,   22,  47,  77,  103, 2,   22,  45,  79,
        104, 2,   22,  43,  80,  104, 2,   22,  42,  81,  104, 2,   22,  42,  82,  105, 2,   22,  42,  83,  105, 2,
        22,  42,  83,  105, 2,   22,  42,  84,  105, 2,   22,  42,  85,  106, 2,   22,  42,  85,  106, 2,   23,  42,
        85,  106, 2,   23,  42,  86,  106, 2,   23,  41,  86,  106, 2,   23,  41,  86,  106, 2,   24,  41,  86,  106,
        2,   24,  40,  86,  106, 2,   25,  39,  86,  106, 2,   27,  38,  86,  106, 2,   29,  36,  85,  106, 1,   85,
        106, 1,   85,  106, 1,   84,  106, 1,   84,  106, 1,   83,  106, 1,   82,  105, 1,   81,  105, 1,   80,  105,
        1,   78,  104, 1,   77,  104, 1,   75,  104, 1,   73,  103, 1,   71,  103, 1,   69,  102, 1,   67,  101, 1,
        65,  101, 1,   62,  100, 1,   60,  99,  1,   57,  98,  1,   55,  97,  1,   52,  96,  1,   52,  95,  1,   52,
        94,  1,   52,  92,  1,   52,  91,  1,   52,  90,  1,   52,  88,  1,   52,  86,  1,   52,  84,  1,   52,  83,
        1,   52,  81,  1,   52,  79,  1,   52,  77,  1,   52,  74,  1,   52,  72,  1,   52,  72,  1,   53,  72,  1,
        53,  72,  1,   53,  72,  1,   53,  71,  1,   54,  71,  1,   54,  70,  1,   55,  69,  1,   56,  68,  1,   58,
        67,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   58,  67,
        1,   54,  71,  1,   52,  73,  1,   51,  74,  1,   50,  75,  1,   49,  75,  1,   49,  76,  1,   48,  76,  1,
        48,  77,  1,   48,  77,  1,   48,  77,  1,   48,  77,  1,   48,  77,  1,   48,  76,  1,   49,  76,  1,   49,
        75,  1,   50,  75,  1,   51,  74,  1,   52,  73,  1,   53,  71,  1,   56,  68,  64,  26,  174, 1,   56,  68,
        1,   51,  73,  1,   48,  76,  1,   46,  79,  1,   44,  80,  1,   42,  82,  1,   41,  83,  1,   39,  85,  1,
        38,  86,  1,   37,  87,  1,   36,  88,  1,   35,  89,  1,   34,  90,  1,   33,  90,  1,   33,  91,  2,   32,
        58,  68,  92,  2,   31,  55,  71,  92,  2,   30,  52,  73,  93,  2,   30,  51,  74,  93,  2,   29,  49,  76,
        94,  2,   29,  48,  77,  94,  2,   28,  47,  78,  95,  2,   27,  46,  78,  95,  2,   27,  45,  79,  95,  2,
        26,  44,  80,  96,  2,   26,  43,  80,  96,  2,   25,  43,  81,  96,  2,   25,  42,  81,  97,  2,   24,  41,
        82,  97,  2,   24,  41,  82,  97,  2,   24,  40,  82,  97,  2,   23,  40,  82,  97,  2,   23,  39,  82,  98,
        2,   23,  39,  83,  98,  2,   22,  39,  83,  98,  2,   22,  38,  83,  98,  2,   22,  38,  83,  98,  2,   21,
        37,  83,  98,  2,   21,  37,  83,  98,  2,   21,  37,  83,  98,  2,   21,  37,  83,  98,  2,   21,  36,  83,
        98,  2,   20,  36,  83,  98,  2,   20,  36,  78,  98,  2,   20,  36,  73,  98,  2,   20,  35,  69,  98,  2,
        20,  35,  67,  98,  2,   20,  35,  65,  98,  2,   20,  35,  63,  98,  2,   20,  35,  61,  98,  2,   19,  35,
        60,  98,  2,   19,  34,  59,  98,  2,   19,  34,  57,  98,  2,   19,  34,  56,  98,  2,   19,  34,  55,  98,
        2,   19,  34,  54,  98,  2,   19,  34,  54,  98,  2,   19,  34,  53,  98,  3,   19,  34,  52,  80,  83,  98,
        3,   19,  34,  51,  76,  83,  98,  3,   19,  34,  51,  73,  83,  98,  3,   19,  34,  50,  71,  83,  98,  3,
        19,  34,  50,  69,  83,  98,  3,   19,  34,  49,  68,  83,  98,  3,   19,  34,  49,  67,  83,  98,  3,   19,
        34,  48,  66,  83,  98,  3,   19,  34,  48,  65,  83,  98,  3,   19,  34,  48,  64,  83,  98,  3,   19,  34,
        47,  63,  83,  98,  3,   19,  34,  47,  63,  83,  98,  3,   19,  34,  47,  62,  83,  98,  3,   19,  34,  47,
        62,  83,  98,  3,   19,  34,  47,  62,  83,  98,  3,   19,  34,  47,  61,  83,  98,  3,   19,  34,  46,  61,
        83,  98,  3,   19,  34,  46,  61,  83,  98,  3,   19,  34,  46,  61,  83,  98,  3,   19,  34,  47,  61,  83,
        98,  3,   19,  34,  47,  62,  83,  98,  3,   19,  34,  47,  62,  83,  98,  3,   19,  34,  47,  62,  83,  98,
        3,   19,  34,  47,  63,  83,  98,  3,   19,  34,  47,  63,  83,  98,  3,   19,  34,  48,  64,  83,  98,  3,
        19,  34,  48,  65,  83,  98,  3,   19,  34,  48,  66,  83,  98,  3,   19,  34,  49,  67,  83,  98,  3,   19,
        34,  49,  69,  83,  98,  3,   19,  34,  50,  71,  83,  98,  3,   19,  34,  50,  74,  83,  98,  2,   19,  34,
        51,  98,  2,   19,  34,  51,  99,  2,   19,  34,  52,  100, 2,   19,  34,  53,  101, 2,   19,  34,  54,  102,
        2,   19,  34,  55,  102, 2,   19,  34,  55,  102, 2,   19,  35,  57,  102, 2,   20,  35,  58,  102, 2,   20,
        35,  59,  102, 2,   20,  35,  60,  101, 2,   20,  35,  62,  101, 2,   20,  35,  64,  100, 2,   20,  36,  67,
        99,  3,   20,  36,  70,  82,  83,  97,  1,   21,  36,  1,   21,  36,  1,   21,  36,  1,   21,  37,  1,   22,
        37,  1,   22,  37,  1,   22,  38,  1,   22,  38,  1,   23,  38,  1,   23,  39,  1,   23,  39,  1,   24,  40,
        1,   24,  40,  1,   24,  41,  1,   25,  41,  1,   25,  42,  1,   25,  43,  1,   26,  43,  1,   26,  44,  2,
        27,  44,  87,  92,  2,   27,  45,  85,  93,  2,   28,  46,  84,  94,  2,   28,  47,  82,  95,  2,   29,  48,
        81,  96,  2,   29,  50,  80,  96,  2,   30,  51,  77,  96,  2,   31,  54,  75,  96,  2,   31,  57,  71,  96,
        1,   32,  96,  1,   33,  96,  1,   34,  95,  1,   35,  95,  1,   36,  94,  1,   37,  93,  1,   38,  92,  1,
        39,  91,  1,   40,  89,  1,   42,  87,  1,   43,  86,  1,   45,  84,  1,   48,  80,  1,   50,  77,  1,   55,
        71,  65,  37,  157, 1,   25,  73,  1,   22,  74,  1,   20,  74,  1,   19,  74,  1,   18,  75,  1,   17,  75,
        1,   17,  76,  1,   16,  76,  1,   16,  77,  1,   16,  77,  1,   16,  77,  1,   16,  78,  1,   16,  78,  1,
        16,  79,  1,   17,  79,  1,   17,  79,  1,   18,  80,  1,   19,  80,  1,   20,  81,  1,   22,  81,  1,   27,
        81,  1,   42,  82,  1,   42,  82,  1,   41,  83,  1,   41,  83,  1,   41,  83,  1,   40,  84,  1,   40,  84,
        2,   39,  62,  63,  85,  2,   39,  61,  63,  85,  2,   39,  61,  63,  85,  2,   38,  60,  64,  86,  2,   38,
        60,  64,  86,  2,   37,  60,  65,  87,  2,   37,  59,  65,  87,  2,   37,  59,  66,  87,  2,   36,  58,  66,
        88,  2,   36,  58,  66,  88,  2,   35,  57,  67,  89,  2,   35,  57,  67,  89,  2,   35,  57,  68,  89,  2,
        34,  56,  68,  90,  2,   34,  56,  68,  90,  2,   33,  55,  69,  91,  2,   33,  55,  69,  91,  2,   33,  55,
        70,  92,  2,   32,  54,  70,  92,  2,   32,  54,  70,  92,  2,   31,  53,  71,  93,  2,   31,  53,  71,  93,
        2,   31,  53,  72,  94,  2,   30,  52,  72,  94,  2,   30,  52,  72,  94,  2,   29,  51,  73,  95,  2,   29,
        51,  73,  95,  2,   29,  51,  74,  96,  2,   28,  50,  74,  96,  2,   28,  50,  74,  96,  2,   27,  49,  75,
        97,  2,   27,  49,  75,  97,  2,   27,  48,  76,  98,  2,   26,  48,  76,  98,  2,   26,  48,  76,  98,  2,
        25,  47,  77,  99,  2,   25,  47,  77,  99,  2,   25,  46,  78,  100, 2,   24,  46,  78,  100, 1,   24,  100,
        1,   23,  101, 1,   23,  101, 1,   23,  102, 1,   22,  102, 1,   22,  102, 1,   21,  103, 1,   21,  103, 1,
        21,  104, 1,   20,  104, 1,   20,  104, 1,   19,  105, 1,   19,  105, 1,   19,  106, 1,   18,  106, 1,   18,
        106, 1,   17,  107, 1,   17,  107, 1,   17,  108, 1,   16,  108, 1,   16,  109, 2,   15,  37,  87,  109, 2,
        15,  37,  88,  109, 2,   15,  36,  88,  110, 2,   14,  36,  88,  110, 2,   14,  36,  89,  111, 2,   13,  35,
        89,  111, 2,   13,  35,  90,  111, 2,   13,  34,  90,  112, 2,   12,  34,  90,  112, 2,   12,  34,  91,  113,
        2,   11,  33,  91,  113, 2,   11,  33,  92,  113, 2,   5,   43,  81,  120, 2,   2,   46,  78,  123, 2,   0,
        48,  77,  124, 2,   0,   49,  76,  125, 2,   0,   50,  75,  126, 2,   0,   50,  74,  126, 2,   0,   51,  74,
        127, 2,   0,   51,  74,  127, 2,   0,   51,  73,  127, 2,   0,   51,  73,  127, 2,   0,   51,  73,  128, 2,
        0,   51,  73,  127, 2,   0,   51,  73,  127, 2,   0,   51,  74,  127, 2,   0,   50,  74,  127, 2,   0,   50,
        75,  126, 2,   0,   49,  75,  125, 2,   0,   48,  76,  124, 2,   1,   47,  78,  123, 2,   4,   44,  80,  120,
        66,  37,  157, 1,   13,  73,  1,   10,  80,  1,   8,   84,  1,   7,   86,  1,   6,   89,  1,   5,   91,  1,
        5,   92,  1,   4,   94,  1,   4,   95,  1,   4,   97,  1,   4,   98,  1,   4,   99,  1,   4,   100, 1,   4,
        101, 1,   5,   101, 1,   5,   102, 1,   6,   103, 1,   7,   104, 1,   8,   104, 1,   10,  105, 1,   15,  105,
        2,   21,  41,  76,  106, 2,   21,  41,  79,  106, 2,   21,  41,  82,  107, 2,   21,  41,  83,  107, 2,   21,
        41,  84,  107, 2,   21,  41,  85,  108, 2,   21,  41,  86,  108, 2,   21,  41,  87,  108, 2,   21,  41,  87,
        108, 2,   21,  41,  88,  108, 2,   21,  41,  88,  108, 2,   21,  41,  88,  109, 2,   21,  41,  88,  109, 2,
        21,  41,  88,  109, 2,   21,  41,  88,  109, 2,   21,  41,  88,  109, 2,   21,  41,  88,  108, 2,   21,  41,
        88,  108, 2,   21,  41,  87,  108, 2,   21,  41,  86,  108, 2,   21,  41,  86,  108, 2,   21,  41,  85,  108,
        2,   21,  41,  84,  107, 2,   21,  41,  82,  107, 2,   21,  41,  81,  107, 2,   21,  41,  79,  106, 2,   21,
        41,  77,  106, 2,   21,  41,  74,  105, 2,   21,  41,  70,  105, 1,   21,  104, 1,   21,  104, 1,   21,  103,
        1,   21,  102, 1,   21,  101, 1,   21,  100, 1,   21,  99,  1,   21,  99,  1,   21,  100, 1,   21,  101, 1,
        21,  103, 1,   21,  104, 1,   21,  105, 1,   21,  106, 1,   21,  107, 1,   21,  108, 1,   21,  109, 1,   21,
        110, 1,   21,  111, 1,   21,  112, 2,   21,  41,  70,  112, 2,   21,  41,  78,  113, 2,   21,  41,  82,  113,
        2,   21,  41,  85,  114, 2,   21,  41,  87,  115, 2,   21,  41,  88,  115, 2,   21,  41,  90,  115, 2,   21,
        41,  91,  116, 2,   21,  41,  92,  116, 2,   21,  41,  93,  116, 2,   21,  41,  94,  117, 2,   21,  41,  95,
        117, 2,   21,  41,  95,  117, 2,   21,  41,  96,  117, 2,   21,  41,  96,  117, 2,   21,  41,  97,  117, 2,
        21,  41,  97,  117, 2,   21,  41,  97,  117, 2,   21,  41,  97,  118, 2,   21,  41,  97,  117, 2,   21,  41,
        97,  117, 2,   21,  41,  97,  117, 2,   21,  41,  96,  117, 2,   21,  41,  96,  117, 2,   21,  41,  95,  117,
        2,   21,  41,  94,  117, 2,   21,  41,  93,  117, 2,   21,  41,  91,  116, 2,   21,  41,  88,  116, 2,   21,
        41,  83,  116, 1,   12,  115, 1,   9,   115, 1,   8,   114, 1,   6,   114, 1,   6,   113, 1,   5,   112, 1,
        5,   112, 1,   4,   111, 1,   4,   110, 1,   4,   109, 1,   4,   108, 1,   4,   107, 1,   4,   105, 1,   5,
        104, 1,   5,   102, 1,   5,   100, 1,   6,   98,  1,   7,   96,  1,   9,   92,  1,   11,  88,  67,  34,  160,
        1,   59,  67,  1,   52,  76,  1,   48,  80,  2,   45,  83,  101, 105, 2,   42,  86,  98,  108, 2,   40,  88,
        97,  109, 2,   38,  90,  96,  110, 2,   36,  92,  95,  111, 1,   34,  111, 1,   33,  112, 1,   31,  112, 1,
        30,  112, 1,   28,  113, 1,   27,  113, 1,   26,  113, 1,   25,  113, 1,   24,  113, 1,   23,  113, 1,   22,
        113, 1,   21,  113, 1,   21,  113, 2,   20,  56,  72,  113, 2,   19,  52,  76,  113, 2,   18,  49,  79,  113,
        2,   18,  47,  82,  113, 2,   17,  46,  84,  113, 2,   16,  44,  85,  113, 2,   16,  43,  87,  113, 2,   15,
        41,  88,  113, 2,   15,  40,  89,  113, 2,   14,  39,  90,  113, 2,   14,  38,  91,  113, 2,   13,  37,  91,
        113, 2,   13,  37,  92,  113, 2,   12,  36,  92,  113, 2,   12,  35,  92,  113, 2,   12,  34,  93,  113, 2,
        11,  34,  93,  113, 2,   11,  33,  93,  112, 2,   11,  33,  93,  112, 2,   10,  32,  94,  112, 2,   10,  31,
        94,  112, 2,   10,  31,  94,  111, 2,   9,   31,  95,  111, 2,   9,   30,  96,  110, 2,   9,   30,  97,  109,
        2,   9,   30,  98,  107, 2,   9,   29,  102, 104, 1,   8,   29,  1,   8,   29,  1,   8,   29,  1,   8,   28,
        1,   8,   28,  1,   8,   28,  1,   8,   28,  1,   8,   28,  1,   8,   28,  1,   8,   28,  1,   8,   28,  1,
        8,   28,  1,   8,   28,  1,   8,   28,  1,   8,   28,  1,   8,   28,  1,   8,   28,  1,   8,   28,  1,   8,
        28,  1,   8,   28,  1,   8,   28,  1,   8,   28,  1,   8,   28,  1,   8,   28,  1,   8,   28,  1,   8,   28,
        1,   8,   28,  1,   8,   28,  1,   8,   28,  1,   8,   28,  1,   8,   29,  1,   8,   29,  1,   8,   29,  1,
        9,   29,  1,   9,   30,  1,   9,   30,  1,   9,   30,  1,   9,   31,  1,   10,  31,  1,   10,  32,  1,   10,
        32,  1,   11,  33,  1,   11,  34,  2,   12,  34,  103, 109, 2,   12,  35,  101, 111, 2,   12,  36,  99,  113,
        2,   13,  37,  98,  114, 2,   13,  38,  97,  114, 2,   14,  39,  96,  115, 2,   14,  40,  95,  115, 2,   15,
        42,  94,  116, 2,   16,  43,  93,  116, 2,   16,  45,  91,  116, 2,   17,  47,  90,  116, 2,   18,  49,  88,
        116, 2,   18,  51,  85,  116, 2,   19,  56,  82,  115, 2,   20,  62,  76,  115, 1,   21,  114, 1,   22,  114,
        1,   22,  113, 1,   23,  112, 1,   24,  111, 1,   25,  110, 1,   27,  109, 1,   28,  108, 1,   29,  107, 1,
        30,  106, 1,   32,  104, 1,   34,  103, 1,   35,  101, 1,   37,  99,  1,   40,  97,  1,   42,  95,  1,   45,
        92,  1,   48,  89,  1,   52,  84,  1,   58,  79,  68,  37,  157, 1,   14,  64,  1,   10,  71,  1,   8,   75,
        1,   7,   78,  1,   6,   81,  1,   5,   83,  1,   5,   85,  1,   5,   87,  1,   4,   89,  1,   4,   90,  1,
        4,   92,  1,   4,   93,  1,   4,   94,  1,   5,   95,  1,   5,   96,  1,   6,   97,  1,   6,   98,  1,   7,
        98,  1,   8,   99,  1,   10,  100, 1,   15,  101, 2,   17,  37,  67,  101, 2,   17,  37,  71,  102, 2,   17,
        37,  74,  103, 2,   17,  37,  76,  103, 2,   17,  37,  77,  104, 2,   17,  37,  79,  105, 2,   17,  37,  80,
        105, 2,   17,  37,  81,  106, 2,   17,  37,  82,  106, 2,   17,  37,  83,  107, 2,   17,  37,  84,  107, 2,
        17,  37,  84,  108, 2,   17,  37,  85,  108, 2,   17,  37,  86,  109, 2,   17,  37,  87,  109, 2,   17,  37,
        87,  110, 2,   17,  37,  88,  110, 2,   17,  37,  88,  110, 2,   17,  37,  89,  111, 2,   17,  37,  89,  111,
        2,   17,  37,  90,  111, 2,   17,  37,  90,  112, 2,   17,  37,  91,  112, 2,   17,  37,  91,  112, 2,   17,
        37,  91,  112, 2,   17,  37,  92,  112, 2,   17,  37,  92,  113, 2,   17,  37,  92,  113, 2,   17,  37,  92,
        113, 2,   17,  37,  92,  113, 2,   17,  37,  93,  113, 2,   17,  37,  93,  113, 2,   17,  37,  93,  113, 2,
        17,  37,  93,  113, 2,   17,  37,  93,  113, 2,   17,  37,  93,  113, 2,   17,  37,  93,  113, 2,   17,  37,
        93,  113, 2,   17,  37,  93,  113, 2,   17,  37,  93,  113, 2,   17,  37,  93,  113, 2,   17,  37,  93,  113,
        2,   17,  37,  93,  113, 2,   17,  37,  93,  113, 2,   17,  37,  93,  113, 2,   17,  37,  93,  113, 2,   17,
        37,  93,  113, 2,   17,  37,  93,  113, 2,   17,  37,  93,  113, 2,   17,  37,  93,  113, 2,   17,  37,  93,
        113, 2,   17,  37,  93,  113, 2,   17,  37,  92,  113, 2,   17,  37,  92,  113, 2,   17,  37,  92,  113, 2,
        17,  37,  92,  113, 2,   17,  37,  92,  112, 2,   17,  37,  91,  112, 2,   17,  37,  91,  112, 2,   17,  37,
        91,  112, 2,   17,  37,  90,  112, 2,   17,  37,  90,  111, 2,   17,  37,  90,  111, 2,   17,  37,  89,  111,
        2,   17,  37,  88,  111, 2,   17,  37,  88,  110, 2,   17,  37,  87,  110, 2,   17,  37,  86,  109, 2,   17,
        37,  85,  109, 2,   17,  37,  85,  109, 2,   17,  37,  84,  108, 2,   17,  37,  82,  108, 2,   17,  37,  81,
        107, 2,   17,  37,  80,  107, 2,   17,  37,  78,  106, 2,   17,  37,  77,  105, 2,   17,  37,  74,  105, 2,
        17,  37,  71,  104, 2,   17,  37,  66,  104, 1,   12,  103, 1,   9,   102, 1,   8,   101, 1,   7,   100, 1,
        6,   99,  1,   5,   98,  1,   5,   97,  1,   4,   96,  1,   4,   95,  1,   4,   94,  1,   4,   92,  1,   4,
        91,  1,   4,   89,  1,   5,   88,  1,   5,   86,  1,   6,   84,  1,   6,   82,  1,   7,   79,  1,   9,   75,
        1,   11,  69,  69,  37,  157, 1,   14,  109, 1,   10,  109, 1,   8,   109, 1,   7,   109, 1,   6,   109, 1,
        5,   109, 1,   5,   109, 1,   5,   109, 1,   4,   109, 1,   4,   109, 1,   4,   109, 1,   4,   109, 1,   4,
        109, 1,   5,   109, 1,   5,   109, 1,   6,   109, 1,   6,   109, 1,   7,   109, 1,   8,   109, 1,   10,  109,
        1,   16,  109, 2,   21,  41,  89,  109, 2,   21,  41,  89,  109, 2,   21,  41,  89,  109, 2,   21,  41,  89,
        109, 2,   21,  41,  89,  109, 2,   21,  41,  89,  109, 2,   21,  41,  89,  109, 2,   21,  41,  89,  109, 2,
        21,  41,  89,  109, 2,   21,  41,  89,  109, 2,   21,  41,  89,  109, 2,   21,  41,  89,  109, 2,   21,  41,
        89,  109, 2,   21,  41,  89,  109, 3,   21,  41,  66,  73,  89,  108, 3,   21,  41,  64,  75,  89,  108, 3,
        21,  41,  62,  76,  89,  108, 3,   21,  41,  62,  77,  90,  108, 3,   21,  41,  61,  78,  90,  107, 3,   21,
        41,  60,  78,  91,  107, 3,   21,  41,  60,  78,  91,  106, 3,   21,  41,  60,  79,  92,  105, 3,   21,  41,
        59,  79,  94,  104, 3,   21,  41,  59,  79,  97,  100, 2,   21,  41,  59,  79,  2,   21,  41,  59,  79,  2,
        21,  41,  59,  79,  2,   21,  41,  59,  79,  2,   21,  41,  59,  79,  1,   21,  79,  1,   21,  79,  1,   21,
        79,  1,   21,  79,  1,   21,  79,  1,   21,  79,  1,   21,  79,  1,   21,  79,  1,   21,  79,  1,   21,  79,
        1,   21,  79,  1,   21,  79,  1,   21,  79,  1,   21,  79,  1,   21,  79,  1,   21,  79,  1,   21,  79,  1,
        21,  79,  1,   21,  79,  1,   21,  79,  2,   21,  41,  59,  79,  2,   21,  41,  59,  79,  2,   21,  41,  59,
        79,  2,   21,  41,  59,  79,  2,   21,  41,  59,  79,  3,   21,  41,  59,  79,  102, 104, 3,   21,  41,  59,
        79,  99,  108, 3,   21,  41,  60,  79,  97,  109, 3,   21,  41,  60,  79,  96,  110, 3,   21,  41,  60,  78,
        95,  111, 3,   21,  41,  61,  78,  95,  112, 3,   21,  41,  61,  77,  94,  112, 3,   21,  41,  62,  76,  94,
        113, 3,   21,  41,  63,  75,  94,  113, 3,   21,  41,  65,  73,  93,  113, 2,   21,  41,  93,  113, 2,   21,
        41,  93,  113, 2,   21,  41,  93,  113, 2,   21,  41,  93,  113, 2,   21,  41,  93,  113, 2,   21,  41,  93,
        113, 2,   21,  41,  93,  113, 2,   21,  41,  93,  113, 2,   21,  41,  93,  113, 2,   21,  41,  93,  113, 2,
        21,  41,  93,  113, 2,   21,  41,  93,  113, 2,   21,  41,  93,  113, 2,   21,  41,  93,  113, 2,   21,  41,
        93,  113, 1,   13,  113, 1,   9,   113, 1,   8,   113, 1,   7,   113, 1,   6,   113, 1,   5,   113, 1,   5,
        113, 1,   5,   113, 1,   4,   113, 1,   4,   113, 1,   4,   113, 1,   4,   113, 1,   5,   113, 1,   5,   113,
        1,   5,   113, 1,   6,   113, 1,   7,   113, 1,   7,   113, 1,   9,   113, 1,   11,  113, 70,  37,  157, 1,
        21,  120, 1,   17,  120, 1,   15,  120, 1,   14,  120, 1,   13,  120, 1,   12,  120, 1,   12,  120, 1,   12,
        120, 1,   11,  120, 1,   11,  120, 1,   11,  120, 1,   11,  120, 1,   11,  120, 1,   12,  120, 1,   12,  120,
        1,   13,  120, 1,   13,  120, 1,   14,  120, 1,   15,  120, 1,   17,  120, 1,   22,  120, 2,   28,  48,  100,
        120, 2,   28,  48,  100, 120, 2,   28,  48,  100, 120, 2,   28,  48,  100, 120, 2,   28,  48,  100, 120, 2,
        28,  48,  100, 120, 2,   28,  48,  100, 120, 2,   28,  48,  100, 120, 2,   28,  48,  100, 120, 2,   28,  48,
        100, 120, 2,   28,  48,  100, 120, 2,   28,  48,  100, 120, 2,   28,  48,  100, 120, 2,   28,  48,  100, 120,
        3,   28,  48,  73,  79,  100, 120, 3,   28,  48,  71,  82,  100, 120, 3,   28,  48,  69,  83,  101, 120, 3,
        28,  48,  69,  84,  101, 119, 3,   28,  48,  68,  84,  101, 119, 3,   28,  48,  67,  85,  102, 118, 3,   28,
        48,  67,  85,  103, 117, 3,   28,  48,  67,  86,  104, 116, 3,   28,  48,  66,  86,  105, 115, 3,   28,  48,
        66,  86,  108, 112, 2,   28,  48,  66,  86,  2,   28,  48,  66,  86,  2,   28,  48,  66,  86,  2,   28,  48,
        66,  86,  2,   28,  48,  66,  86,  1,   28,  86,  1,   28,  86,  1,   28,  86,  1,   28,  86,  1,   28,  86,
        1,   28,  86,  1,   28,  86,  1,   28,  86,  1,   28,  86,  1,   28,  86,  1,   28,  86,  1,   28,  86,  1,
        28,  86,  1,   28,  86,  1,   28,  86,  1,   28,  86,  1,   28,  86,  1,   28,  86,  1,   28,  86,  1,   28,
        86,  2,   28,  48,  66,  86,  2,   28,  48,  66,  86,  2,   28,  48,  66,  86,  2,   28,  48,  66,  86,  2,
        28,  48,  66,  86,  2,   28,  48,  66,  86,  2,   28,  48,  66,  86,  2,   28,  48,  67,  86,  2,   28,  48,
        67,  85,  2,   28,  48,  67,  85,  2,   28,  48,  67,  85,  2,   28,  48,  68,  84,  2,   28,  48,  69,  83,
        2,   28,  48,  70,  82,  2,   28,  48,  72,  80,  1,   28,  48,  1,   28,  48,  1,   28,  48,  1,   28,  48,
        1,   28,  48,  1,   28,  48,  1,   28,  48,  1,   28,  48,  1,   28,  48,  1,   28,  48,  1,   28,  48,  1,
        28,  48,  1,   28,  48,  1,   28,  48,  1,   28,  48,  1,   19,  74,  1,   16,  77,  1,   15,  79,  1,   14,
        80,  1,   13,  81,  1,   12,  81,  1,   12,  82,  1,   12,  82,  1,   11,  82,  1,   11,  82,  1,   11,  82,
        1,   11,  82,  1,   11,  82,  1,   12,  82,  1,   12,  82,  1,   13,  81,  1,   13,  80,  1,   14,  79,  1,
        16,  78,  1,   18,  75,  71,  34,  160, 1,   60,  70,  1,   53,  78,  1,   49,  83,  2,   45,  86,  102, 105,
        2,   43,  89,  99,  108, 2,   40,  92,  97,  109, 2,   38,  94,  96,  111, 1,   36,  111, 1,   35,  112, 1,
        33,  112, 1,   32,  112, 1,   30,  113, 1,   29,  113, 1,   28,  113, 1,   27,  113, 1,   26,  113, 1,   25,
        113, 1,   24,  113, 1,   23,  113, 1,   22,  113, 1,   21,  113, 2,   20,  56,  76,  113, 2,   20,  52,  80,
        113, 2,   19,  49,  83,  113, 2,   18,  47,  86,  113, 2,   17,  45,  88,  113, 2,   17,  44,  89,  113, 2,
        16,  42,  90,  113, 2,   16,  41,  91,  113, 2,   15,  40,  92,  113, 2,   14,  39,  92,  113, 2,   14,  38,
        93,  113, 2,   13,  37,  93,  113, 2,   13,  37,  93,  113, 2,   12,  36,  94,  113, 2,   12,  35,  94,  113,
        2,   12,  34,  94,  112, 2,   11,  34,  95,  112, 2,   11,  33,  95,  112, 2,   11,  33,  95,  111, 2,   10,
        32,  96,  110, 2,   10,  32,  97,  109, 2,   10,  31,  99,  107, 1,   9,   31,  1,   9,   30,  1,   9,   30,
        1,   9,   30,  1,   9,   29,  1,   8,   29,  1,   8,   29,  1,   8,   29,  1,   8,   29,  1,   8,   28,  1,
        8,   28,  1,   8,   28,  1,   8,   28,  1,   8,   28,  1,   8,   28,  1,   8,   28,  1,   8,   28,  1,   8,
        28,  1,   8,   28,  1,   8,   28,  1,   8,   28,  1,   8,   28,  1,   8,   28,  2,   8,   28,  66,  114, 2,
        8,   28,  63,  117, 2,   8,   28,  62,  119, 2,   8,   28,  61,  119, 2,   8,   28,  60,  120, 2,   8,   28,
        60,  121, 2,   8,   28,  59,  121, 2,   8,   28,  59,  122, 2,   8,   28,  59,  122, 2,   8,   28,  59,  122,
        2,   8,   28,  59,  122, 2,   8,   28,  59,  122, 2,   8,   28,  59,  121, 2,   8,   28,  59,  121, 2,   8,
        29,  60,  121, 2,   8,   29,  60,  120, 2,   9,   29,  61,  119, 2,   9,   29,  62,  118, 2,   9,   30,  64,
        117, 2,   9,   30,  66,  115, 2,   9,   30,  93,  113, 2,   10,  31,  93,  113, 2,   10,  31,  93,  113, 2,
        10,  32,  93,  113, 2,   10,  32,  93,  113, 2,   11,  33,  93,  113, 2,   11,  33,  93,  113, 2,   12,  34,
        93,  113, 2,   12,  35,  93,  113, 2,   12,  36,  93,  113, 2,   13,  37,  93,  113, 2,   13,  37,  93,  113,
        2,   14,  39,  93,  113, 2,   14,  40,  93,  113, 2,   15,  42,  93,  113, 2,   15,  44,  91,  113, 2,   16,
        46,  88,  113, 2,   17,  49,  85,  113, 2,   17,  53,  81,  113, 2,   18,  59,  75,  113, 1,   19,  113, 1,
        19,  113, 1,   20,  113, 1,   21,  113, 1,   22,  113, 1,   23,  113, 1,   24,  113, 1,   25,  113, 1,   26,
        111, 1,   28,  109, 1,   29,  107, 1,   31,  105, 1,   33,  103, 1,   35,  101, 1,   37,  98,  1,   40,  96,
        1,   43,  93,  1,   47,  90,  1,   51,  85,  1,   58,  80,  72,  37,  157, 2,   19,  47,  77,  105, 2,   16,
        52,  73,  109, 2,   14,  54,  72,  111, 2,   13,  55,  70,  113, 2,   12,  56,  69,  114, 2,   11,  56,  69,
        114, 2,   11,  57,  68,  115, 2,   10,  57,  68,  115, 2,   10,  58,  68,  115, 2,   10,  58,  68,  115, 2,
        10,  58,  68,  115, 2,   10,  58,  68,  115, 2,   10,  58,  68,  115, 2,   10,  57,  68,  115, 2,   11,  57,
        68,  115, 2,   11,  56,  69,  114, 2,   12,  56,  70,  113, 2,   13,  55,  70,  112, 2,   14,  54,  72,  111,
        2,   16,  52,  74,  109, 2,   20,  46,  79,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,
        41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,
        105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,
        21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,
        84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105,
        2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,
        41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,
        105, 1,   21,  105, 1,   21,  105, 1,   21,  105, 1,   21,  105, 1,   21,  105, 1,   21,  105, 1,   21,  105,
        1,   21,  105, 1,   21,  105, 1,   21,  105, 1,   21,  105, 1,   21,  105, 1,   21,  105, 1,   21,  105, 1,
        21,  105, 1,   21,  105, 1,   21,  105, 1,   21,  105, 1,   21,  105, 1,   21,  105, 1,   21,  105, 2,   21,
        41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,
        105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,
        21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,
        84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105,
        2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,
        41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,
        105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   13,  49,  76,  112, 2,   10,  53,  73,  115, 2,
        9,   54,  71,  116, 2,   8,   55,  70,  117, 2,   7,   56,  69,  118, 2,   7,   57,  69,  118, 2,   6,   57,
        68,  119, 2,   6,   57,  68,  119, 2,   6,   58,  68,  119, 2,   6,   58,  68,  119, 2,   6,   58,  68,  119,
        2,   6,   58,  68,  119, 2,   6,   57,  68,  119, 2,   6,   57,  68,  119, 2,   7,   57,  69,  119, 2,   7,
        56,  69,  118, 2,   8,   55,  70,  117, 2,   9,   54,  71,  116, 2,   10,  53,  72,  115, 2,   13,  51,  75,
        112, 73,  37,  157, 1,   27,  96,  1,   24,  100, 1,   22,  102, 1,   21,  104, 1,   20,  104, 1,   19,  105,
        1,   19,  106, 1,   18,  106, 1,   18,  106, 1,   18,  106, 1,   18,  106, 1,   18,  106, 1,   18,  106, 1,
        18,  106, 1,   19,  105, 1,   19,  105, 1,   20,  104, 1,   21,  103, 1,   22,  102, 1,   24,  100, 1,   29,
        95,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   26,  98,  1,
        23,  101, 1,   21,  103, 1,   20,  104, 1,   20,  105, 1,   19,  105, 1,   19,  106, 1,   18,  106, 1,   18,
        106, 1,   18,  106, 1,   18,  106, 1,   18,  106, 1,   18,  106, 1,   18,  106, 1,   19,  105, 1,   19,  105,
        1,   20,  104, 1,   21,  103, 1,   23,  102, 1,   25,  99,  74,  37,  160, 1,   51,  115, 1,   48,  120, 1,
        46,  122, 1,   44,  123, 1,   44,  124, 1,   43,  124, 1,   43,  125, 1,   42,  125, 1,   42,  126, 1,   42,
        126, 1,   42,  126, 1,   42,  126, 1,   42,  126, 1,   42,  125, 1,   43,  125, 1,   43,  124, 1,   44,  124,
        1,   45,  123, 1,   46,  122, 1,   48,  120, 1,   53,  114, 1,   80,  100, 1,   80,  100, 1,   80,  100, 1,
        80,  100, 1,   80,  100, 1,   80,  100, 1,   80,  100, 1,   80,  100, 1,   80,  100, 1,   80,  100, 1,   80,
        100, 1,   80,  100, 1,   80,  100, 1,   80,  100, 1,   80,  100, 1,   80,  100, 1,   80,  100, 1,   80,  100,
        1,   80,  100, 1,   80,  100, 1,   80,  100, 1,   80,  100, 1,   80,  100, 1,   80,  100, 1,   80,  100, 1,
        80,  100, 1,   80,  100, 1,   80,  100, 1,   80,  100, 1,   80,  100, 1,   80,  100, 1,   80,  100, 1,   80,
        100, 1,   80,  100, 1,   80,  100, 1,   80,  100, 1,   80,  100, 1,   80,  100, 1,   80,  100, 1,   80,  100,
        1,   80,  100, 1,   80,  100, 2,   19,  26,  80,  100, 2,   17,  28,  80,  100, 2,   16,  29,  80,  100, 2,
        15,  30,  80,  100, 2,   14,  31,  80,  100, 2,   13,  31,  80,  100, 2,   13,  31,  80,  100, 2,   13,  32,
        80,  100, 2,   13,  32,  80,  100, 2,   12,  32,  80,  100, 2,   12,  32,  80,  100, 2,   12,  32,  80,  100,
        2,   12,  32,  80,  100, 2,   12,  32,  80,  100, 2,   12,  32,  80,  100, 2,   12,  32,  80,  100, 2,   12,
        32,  80,  100, 2,   12,  32,  80,  100, 2,   12,  32,  80,  100, 2,   12,  32,  80,  100, 2,   12,  32,  80,
        100, 2,   12,  32,  80,  100, 2,   12,  32,  80,  100, 2,   12,  32,  80,  100, 2,   12,  32,  80,  100, 2,
        12,  32,  79,  100, 2,   12,  32,  79,  100, 2,   12,  32,  78,  100, 2,   12,  32,  78,  99,  2,   12,  32,
        77,  99,  2,   12,  32,  77,  99,  2,   12,  32,  76,  99,  2,   12,  32,  75,  98,  2,   12,  33,  74,  98,
        2,   12,  35,  72,  98,  2,   12,  37,  71,  97,  2,   12,  40,  69,  97,  2,   12,  43,  67,  96,  2,   12,
        46,  64,  96,  2,   12,  51,  60,  95,  1,   12,  94,  1,   12,  94,  1,   12,  93,  1,   12,  92,  1,   12,
        91,  1,   12,  90,  1,   12,  89,  1,   14,  88,  1,   16,  87,  1,   18,  86,  1,   20,  85,  1,   22,  83,
        1,   25,  82,  1,   27,  80,  1,   30,  78,  1,   33,  76,  1,   36,  74,  1,   39,  71,  1,   43,  68,  1,
        47,  64,  75,  37,  157, 2,   13,  52,  81,  109, 2,   10,  56,  77,  113, 2,   8,   58,  75,  115, 2,   7,
        59,  74,  117, 2,   6,   60,  73,  117, 2,   5,   61,  73,  118, 2,   5,   61,  72,  118, 2,   4,   61,  72,
        119, 2,   4,   62,  72,  119, 2,   4,   62,  72,  119, 2,   4,   62,  71,  119, 2,   4,   62,  72,  119, 2,
        4,   62,  72,  119, 2,   4,   61,  72,  119, 2,   5,   61,  72,  118, 2,   5,   61,  73,  118, 2,   6,   60,
        73,  117, 2,   7,   59,  74,  116, 2,   8,   58,  76,  115, 2,   10,  56,  77,  113, 2,   15,  51,  76,  108,
        2,   21,  41,  75,  105, 2,   21,  41,  74,  104, 2,   21,  41,  73,  103, 2,   21,  41,  71,  102, 2,   21,
        41,  70,  101, 2,   21,  41,  69,  100, 2,   21,  41,  68,  99,  2,   21,  41,  67,  97,  2,   21,  41,  66,
        96,  2,   21,  41,  64,  95,  2,   21,  41,  63,  94,  2,   21,  41,  62,  93,  2,   21,  41,  61,  92,  2,
        21,  41,  60,  90,  2,   21,  41,  59,  89,  2,   21,  41,  57,  88,  2,   21,  41,  56,  87,  2,   21,  41,
        55,  86,  2,   21,  41,  54,  85,  2,   21,  41,  53,  84,  2,   21,  41,  52,  82,  2,   21,  41,  50,  81,
        2,   21,  41,  49,  80,  2,   21,  41,  48,  79,  2,   21,  41,  47,  78,  2,   21,  41,  46,  77,  2,   21,
        41,  45,  76,  2,   21,  41,  43,  74,  2,   21,  41,  42,  73,  1,   21,  72,  1,   21,  71,  1,   21,  70,
        1,   21,  72,  1,   21,  73,  1,   21,  75,  1,   21,  76,  1,   21,  77,  1,   21,  78,  1,   21,  79,  1,
        21,  80,  1,   21,  81,  1,   21,  82,  1,   21,  83,  1,   21,  84,  1,   21,  85,  1,   21,  86,  2,   21,
        53,  55,  87,  2,   21,  52,  57,  88,  2,   21,  50,  59,  89,  2,   21,  49,  60,  89,  2,   21,  48,  62,
        90,  2,   21,  47,  63,  91,  2,   21,  46,  64,  91,  2,   21,  44,  66,  92,  2,   21,  43,  67,  93,  2,
        21,  42,  68,  93,  2,   21,  41,  69,  94,  2,   21,  41,  70,  95,  2,   21,  41,  70,  95,  2,   21,  41,
        71,  96,  2,   21,  41,  72,  96,  2,   21,  41,  73,  97,  2,   21,  41,  73,  97,  2,   21,  41,  74,  98,
        2,   21,  41,  75,  98,  2,   21,  41,  75,  99,  2,   21,  41,  76,  99,  2,   21,  41,  77,  100, 2,   21,
        41,  77,  100, 2,   21,  41,  78,  101, 2,   21,  41,  78,  101, 2,   21,  41,  79,  102, 2,   21,  41,  79,
        102, 2,   21,  41,  80,  103, 2,   21,  41,  80,  103, 2,   21,  41,  81,  104, 2,   21,  41,  81,  104, 2,
        21,  41,  82,  104, 2,   21,  41,  82,  105, 2,   12,  54,  83,  116, 2,   9,   57,  83,  119, 2,   8,   58,
        84,  120, 2,   6,   59,  84,  121, 2,   6,   60,  85,  122, 2,   5,   61,  85,  123, 2,   5,   61,  86,  123,
        2,   4,   61,  86,  124, 2,   4,   62,  87,  124, 2,   4,   62,  87,  124, 2,   4,   62,  88,  124, 2,   4,
        62,  88,  124, 2,   4,   62,  89,  124, 2,   5,   61,  89,  123, 2,   5,   61,  89,  123, 2,   5,   60,  90,
        122, 2,   6,   60,  90,  122, 2,   7,   59,  91,  121, 2,   9,   57,  91,  119, 2,   11,  55,  92,  117, 76,
        37,  157, 1,   18,  69,  1,   14,  73,  1,   12,  75,  1,   11,  76,  1,   10,  77,  1,   9,   78,  1,   9,
        78,  1,   8,   79,  1,   8,   79,  1,   8,   79,  1,   8,   79,  1,   8,   79,  1,   8,   79,  1,   8,   79,
        1,   9,   78,  1,   9,   78,  1,   10,  77,  1,   11,  76,  1,   12,  75,  1,   14,  73,  1,   19,  68,  1,
        33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,
        54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,
        1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,
        33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,
        54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,
        1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,   33,  54,  1,
        33,  54,  1,   33,  54,  2,   33,  54,  105, 110, 2,   33,  54,  103, 112, 2,   33,  54,  101, 114, 2,   33,
        54,  100, 115, 2,   33,  54,  99,  116, 2,   33,  54,  99,  116, 2,   33,  54,  98,  117, 2,   33,  54,  98,
        117, 2,   33,  54,  98,  117, 2,   33,  54,  98,  117, 2,   33,  54,  98,  117, 2,   33,  54,  97,  117, 2,
        33,  54,  97,  118, 2,   33,  54,  97,  118, 2,   33,  54,  97,  118, 2,   33,  54,  97,  118, 2,   33,  54,
        97,  118, 2,   33,  54,  97,  118, 2,   33,  54,  97,  118, 2,   33,  54,  97,  118, 2,   33,  54,  97,  118,
        2,   33,  54,  97,  118, 2,   33,  54,  97,  118, 2,   33,  54,  97,  118, 2,   33,  54,  97,  118, 2,   33,
        54,  97,  118, 2,   33,  54,  97,  118, 2,   33,  54,  97,  118, 2,   33,  54,  97,  118, 2,   33,  54,  97,
        118, 2,   33,  54,  97,  118, 2,   33,  54,  97,  118, 2,   33,  54,  97,  118, 1,   16,  118, 1,   13,  118,
        1,   12,  118, 1,   10,  118, 1,   10,  118, 1,   9,   118, 1,   9,   118, 1,   8,   118, 1,   8,   118, 1,
        8,   118, 1,   8,   118, 1,   8,   118, 1,   8,   118, 1,   9,   118, 1,   9,   118, 1,   10,  118, 1,   10,
        118, 1,   11,  118, 1,   13,  118, 1,   15,  118, 77,  37,  157, 2,   9,   35,  90,  116, 2,   5,   36,  89,
        120, 2,   3,   36,  89,  122, 2,   2,   37,  88,  123, 2,   1,   37,  88,  124, 2,   1,   37,  87,  125, 2,
        0,   38,  87,  125, 2,   0,   38,  87,  125, 2,   0,   39,  86,  126, 2,   0,   39,  86,  126, 2,   0,   40,
        85,  126, 2,   0,   40,  85,  126, 2,   0,   41,  84,  126, 2,   0,   41,  84,  125, 2,   0,   42,  83,  125,
        2,   1,   42,  83,  124, 2,   1,   42,  83,  124, 2,   2,   43,  82,  123, 2,   4,   43,  82,  121, 2,   5,
        44,  81,  120, 2,   10,  44,  81,  116, 2,   10,  45,  80,  115, 2,   10,  45,  80,  115, 2,   10,  46,  80,
        115, 2,   10,  46,  79,  115, 2,   10,  47,  79,  115, 2,   10,  47,  78,  115, 2,   10,  47,  78,  115, 2,
        10,  48,  77,  115, 2,   10,  48,  77,  115, 2,   10,  49,  76,  115, 2,   10,  49,  76,  115, 2,   10,  50,
        76,  115, 2,   10,  50,  75,  115, 2,   10,  51,  75,  115, 2,   10,  51,  74,  115, 2,   10,  51,  74,  115,
        2,   10,  52,  73,  115, 2,   10,  52,  73,  115, 2,   10,  53,  72,  115, 4,   10,  30,  31,  53,  72,  94,
        95,  115, 4,   10,  30,  31,  54,  72,  94,  95,  115, 4,   10,  30,  32,  54,  71,  93,  95,  115, 4,   10,
        30,  32,  55,  71,  93,  95,  115, 4,   10,  30,  33,  55,  70,  93,  95,  115, 4,   10,  30,  33,  56,  70,
        92,  95,  115, 4,   10,  30,  34,  56,  69,  92,  95,  115, 4,   10,  30,  34,  56,  69,  91,  95,  115, 4,
        10,  30,  34,  57,  68,  91,  95,  115, 4,   10,  30,  35,  57,  68,  90,  95,  115, 4,   10,  30,  35,  58,
        68,  90,  95,  115, 4,   10,  30,  36,  58,  67,  89,  95,  115, 4,   10,  30,  36,  59,  67,  89,  95,  115,
        4,   10,  30,  37,  59,  66,  89,  95,  115, 4,   10,  30,  37,  60,  66,  88,  95,  115, 4,   10,  30,  38,
        60,  65,  88,  95,  115, 4,   10,  30,  38,  61,  65,  87,  95,  115, 4,   10,  30,  38,  61,  64,  87,  95,
        115, 4,   10,  30,  39,  61,  64,  86,  95,  115, 4,   10,  30,  39,  62,  64,  86,  95,  115, 4,   10,  30,
        40,  62,  63,  85,  95,  115, 3,   10,  30,  40,  85,  95,  115, 3,   10,  30,  41,  85,  95,  115, 3,   10,
        30,  41,  84,  95,  115, 3,   10,  30,  42,  84,  95,  115, 3,   10,  30,  42,  83,  95,  115, 3,   10,  30,
        43,  83,  95,  115, 3,   10,  30,  43,  82,  95,  115, 3,   10,  30,  43,  82,  95,  115, 3,   10,  30,  44,
        81,  95,  115, 3,   10,  30,  44,  81,  95,  115, 3,   10,  30,  45,  81,  95,  115, 3,   10,  30,  45,  80,
        95,  115, 3,   10,  30,  46,  80,  95,  115, 3,   10,  30,  46,  79,  95,  115, 3,   10,  30,  47,  79,  95,
        115, 3,   10,  30,  47,  78,  95,  115, 3,   10,  30,  48,  78,  95,  115, 3,   10,  30,  48,  77,  95,  115,
        3,   10,  30,  48,  77,  95,  115, 3,   10,  30,  49,  77,  95,  115, 3,   10,  30,  49,  76,  95,  115, 3,
        10,  30,  50,  76,  95,  115, 3,   10,  30,  50,  75,  95,  115, 3,   10,  30,  51,  75,  95,  115, 3,   10,
        30,  51,  74,  95,  115, 3,   10,  30,  52,  74,  95,  115, 3,   10,  30,  52,  73,  95,  115, 3,   10,  30,
        52,  73,  95,  115, 3,   10,  30,  53,  73,  95,  115, 3,   10,  30,  53,  72,  95,  115, 3,   10,  30,  54,
        72,  95,  115, 2,   10,  30,  95,  115, 2,   10,  30,  95,  115, 2,   10,  30,  95,  115, 2,   10,  30,  95,
        115, 2,   10,  30,  95,  115, 2,   10,  30,  95,  115, 2,   10,  30,  95,  115, 2,   10,  30,  95,  115, 2,
        5,   43,  82,  120, 2,   2,   46,  79,  123, 2,   1,   48,  77,  124, 2,   0,   49,  76,  125, 2,   0,   50,
        76,  126, 2,   0,   50,  75,  127, 2,   0,   51,  75,  127, 2,   0,   51,  74,  127, 2,   0,   51,  74,  127,
        2,   0,   51,  74,  128, 2,   0,   51,  74,  128, 2,   0,   51,  74,  128, 2,   0,   51,  74,  127, 2,   0,
        51,  74,  127, 2,   0,   50,  75,  127, 2,   0,   50,  75,  126, 2,   0,   49,  76,  125, 2,   1,   48,  77,
        124, 2,   2,   47,  79,  123, 2,   5,   44,  81,  120, 78,  37,  157, 2,   9,   37,  77,  111, 2,   5,   37,
        73,  115, 2,   4,   38,  72,  117, 2,   2,   39,  70,  119, 2,   1,   39,  69,  120, 2,   1,   40,  69,  120,
        2,   0,   41,  68,  121, 2,   0,   41,  68,  121, 2,   0,   42,  68,  121, 2,   0,   43,  68,  121, 2,   0,
        43,  68,  121, 2,   0,   44,  68,  121, 2,   0,   44,  68,  121, 2,   0,   45,  68,  121, 2,   0,   46,  68,
        121, 2,   1,   46,  69,  120, 2,   2,   47,  70,  119, 2,   3,   48,  71,  119, 2,   4,   48,  72,  117, 2,
        6,   49,  74,  115, 2,   11,  50,  79,  111, 2,   16,  50,  89,  109, 2,   16,  51,  89,  109, 2,   16,  51,
        89,  109, 2,   16,  52,  89,  109, 2,   16,  53,  89,  109, 2,   16,  53,  89,  109, 2,   16,  54,  89,  109,
        2,   16,  55,  89,  109, 2,   16,  55,  89,  109, 2,   16,  56,  89,  109, 2,   16,  57,  89,  109, 2,   16,
        57,  89,  109, 2,   16,  58,  89,  109, 2,   16,  58,  89,  109, 2,   16,  59,  89,  109, 2,   16,  60,  89,
        109, 2,   16,  60,  89,  109, 2,   16,  61,  89,  109, 2,   16,  62,  89,  109, 3,   16,  37,  38,  62,  89,
        109, 3,   16,  37,  39,  63,  89,  109, 3,   16,  37,  39,  64,  89,  109, 3,   16,  37,  40,  64,  89,  109,
        3,   16,  37,  40,  65,  89,  109, 3,   16,  37,  41,  65,  89,  109, 3,   16,  37,  42,  66,  89,  109, 3,
        16,  37,  42,  67,  89,  109, 3,   16,  37,  43,  67,  89,  109, 3,   16,  37,  44,  68,  89,  109, 3,   16,
        37,  44,  69,  89,  109, 3,   16,  37,  45,  69,  89,  109, 3,   16,  37,  46,  70,  89,  109, 3,   16,  37,
        46,  71,  89,  109, 3,   16,  37,  47,  71,  89,  109, 3,   16,  37,  47,  72,  89,  109, 3,   16,  37,  48,
        73,  89,  109, 3,   16,  37,  49,  73,  89,  109, 3,   16,  37,  49,  74,  89,  109, 3,   16,  37,  50,  74,
        89,  109, 3,   16,  37,  51,  75,  89,  109, 3,   16,  37,  51,  76,  89,  109, 3,   16,  37,  52,  76,  89,
        109, 3,   16,  37,  53,  77,  89,  109, 3,   16,  37,  53,  78,  89,  109, 3,   16,  37,  54,  78,  89,  109,
        3,   16,  37,  55,  79,  89,  109, 3,   16,  37,  55,  80,  89,  109, 3,   16,  37,  56,  80,  89,  109, 3,
        16,  37,  56,  81,  89,  109, 3,   16,  37,  57,  81,  89,  109, 3,   16,  37,  58,  82,  89,  109, 3,   16,
        37,  58,  83,  89,  109, 3,   16,  37,  59,  83,  89,  109, 3,   16,  37,  60,  84,  89,  109, 3,   16,  37,
        60,  85,  89,  109, 3,   16,  37,  61,  85,  89,  109, 3,   16,  37,  62,  86,  89,  109, 3,   16,  37,  62,
        87,  89,  109, 3,   16,  37,  63,  87,  89,  109, 3,   16,  37,  63,  88,  89,  109, 3,   16,  37,  64,  88,
        89,  109, 2,   16,  37,  65,  109, 2,   16,  37,  65,  109, 2,   16,  37,  66,  109, 2,   16,  37,  67,  109,
        2,   16,  37,  67,  109, 2,   16,  37,  68,  109, 2,   16,  37,  69,  109, 2,   16,  37,  69,  109, 2,   16,
        37,  70,  109, 2,   16,  37,  70,  109, 2,   16,  37,  71,  109, 2,   16,  37,  72,  109, 2,   16,  37,  72,
        109, 2,   16,  37,  73,  109, 2,   16,  37,  74,  109, 2,   16,  37,  74,  109, 2,   16,  37,  75,  109, 2,
        16,  37,  76,  109, 2,   12,  49,  76,  109, 2,   9,   52,  77,  109, 2,   7,   54,  78,  109, 2,   6,   55,
        78,  109, 2,   5,   56,  79,  109, 2,   5,   56,  79,  109, 2,   4,   57,  80,  109, 2,   4,   57,  81,  109,
        2,   4,   57,  81,  109, 2,   4,   58,  82,  109, 2,   4,   58,  83,  109, 2,   4,   57,  83,  109, 2,   4,
        57,  84,  109, 2,   4,   57,  85,  109, 2,   5,   57,  85,  109, 2,   5,   56,  86,  109, 2,   6,   55,  86,
        109, 2,   7,   54,  87,  109, 2,   8,   53,  88,  109, 2,   11,  50,  88,  109, 79,  34,  160, 1,   57,  68,
        1,   51,  74,  1,   47,  77,  1,   44,  80,  1,   42,  83,  1,   40,  85,  1,   38,  87,  1,   36,  89,  1,
        34,  90,  1,   33,  92,  1,   31,  93,  1,   30,  94,  1,   29,  96,  1,   27,  97,  1,   26,  98,  1,   25,
        99,  1,   24,  100, 1,   23,  101, 1,   22,  102, 1,   21,  103, 1,   20,  104, 2,   20,  55,  70,  105, 2,
        19,  51,  73,  106, 2,   18,  49,  75,  106, 2,   17,  47,  77,  107, 2,   16,  45,  79,  108, 2,   16,  44,
        81,  108, 2,   15,  42,  82,  109, 2,   15,  41,  83,  110, 2,   14,  40,  84,  110, 2,   13,  39,  85,  111,
        2,   13,  38,  86,  111, 2,   12,  37,  87,  112, 2,   12,  36,  88,  113, 2,   11,  35,  89,  113, 2,   11,
        35,  90,  113, 2,   10,  34,  90,  114, 2,   10,  33,  91,  114, 2,   10,  32,  92,  115, 2,   9,   32,  93,
        115, 2,   9,   31,  93,  115, 2,   9,   31,  94,  116, 2,   8,   30,  94,  116, 2,   8,   30,  95,  116, 2,
        8,   29,  95,  117, 2,   7,   29,  96,  117, 2,   7,   28,  96,  117, 2,   7,   28,  96,  117, 2,   7,   28,
        96,  118, 2,   7,   28,  97,  118, 2,   6,   27,  97,  118, 2,   6,   27,  97,  118, 2,   6,   27,  98,  118,
        2,   6,   27,  98,  118, 2,   6,   26,  98,  119, 2,   6,   26,  98,  119, 2,   6,   26,  98,  119, 2,   6,
        26,  98,  119, 2,   6,   26,  98,  119, 2,   5,   26,  98,  119, 2,   5,   26,  99,  119, 2,   5,   26,  99,
        119, 2,   5,   26,  99,  119, 2,   5,   26,  99,  119, 2,   5,   26,  99,  119, 2,   5,   26,  99,  119, 2,
        5,   26,  99,  119, 2,   5,   26,  98,  119, 2,   6,   26,  98,  119, 2,   6,   26,  98,  119, 2,   6,   26,
        98,  119, 2,   6,   26,  98,  119, 2,   6,   26,  98,  118, 2,   6,   27,  98,  118, 2,   6,   27,  97,  118,
        2,   6,   27,  97,  118, 2,   6,   27,  97,  118, 2,   7,   28,  97,  118, 2,   7,   28,  96,  117, 2,   7,
        28,  96,  117, 2,   7,   29,  96,  117, 2,   8,   29,  95,  117, 2,   8,   30,  95,  117, 2,   8,   30,  94,
        116, 2,   8,   31,  94,  116, 2,   9,   31,  93,  116, 2,   9,   32,  93,  115, 2,   9,   32,  92,  115, 2,
        10,  33,  92,  115, 2,   10,  34,  91,  114, 2,   11,  34,  90,  114, 2,   11,  35,  89,  113, 2,   12,  36,
        89,  113, 2,   12,  37,  88,  112, 2,   13,  37,  87,  112, 2,   13,  38,  86,  111, 2,   14,  39,  85,  111,
        2,   14,  41,  84,  110, 2,   15,  42,  83,  110, 2,   16,  43,  81,  109, 2,   16,  45,  80,  108, 2,   17,
        46,  78,  107, 2,   18,  48,  76,  107, 2,   18,  50,  74,  106, 2,   19,  53,  71,  105, 2,   20,  57,  67,
        104, 1,   21,  103, 1,   22,  103, 1,   23,  102, 1,   24,  101, 1,   25,  100, 1,   26,  99,  1,   27,  97,
        1,   28,  96,  1,   29,  95,  1,   31,  94,  1,   32,  92,  1,   33,  91,  1,   35,  89,  1,   37,  88,  1,
        39,  86,  1,   41,  84,  1,   43,  81,  1,   46,  79,  1,   49,  75,  1,   54,  71,  80,  37,  157, 1,   23,
        76,  1,   19,  85,  1,   17,  90,  1,   16,  93,  1,   15,  95,  1,   15,  97,  1,   14,  99,  1,   14,  101,
        1,   14,  102, 1,   13,  104, 1,   13,  105, 1,   13,  106, 1,   14,  107, 1,   14,  108, 1,   14,  109, 1,
        15,  110, 1,   15,  111, 1,   16,  111, 1,   17,  112, 1,   19,  113, 1,   25,  113, 2,   30,  50,  82,  114,
        2,   30,  50,  85,  114, 2,   30,  50,  88,  115, 2,   30,  50,  90,  115, 2,   30,  50,  91,  116, 2,   30,
        50,  92,  116, 2,   30,  50,  93,  116, 2,   30,  50,  94,  117, 2,   30,  50,  95,  117, 2,   30,  50,  96,
        117, 2,   30,  50,  96,  118, 2,   30,  50,  97,  118, 2,   30,  50,  97,  118, 2,   30,  50,  97,  118, 2,
        30,  50,  98,  118, 2,   30,  50,  98,  118, 2,   30,  50,  98,  118, 2,   30,  50,  98,  118, 2,   30,  50,
        98,  118, 2,   30,  50,  98,  118, 2,   30,  50,  98,  118, 2,   30,  50,  97,  118, 2,   30,  50,  97,  118,
        2,   30,  50,  97,  118, 2,   30,  50,  96,  118, 2,   30,  50,  96,  117, 2,   30,  50,  95,  117, 2,   30,
        50,  94,  117, 2,   30,  50,  94,  117, 2,   30,  50,  93,  116, 2,   30,  50,  92,  116, 2,   30,  50,  91,
        116, 2,   30,  50,  89,  115, 2,   30,  50,  87,  115, 2,   30,  50,  86,  114, 2,   30,  50,  83,  114, 2,
        30,  50,  80,  113, 2,   30,  50,  74,  112, 1,   30,  112, 1,   30,  111, 1,   30,  110, 1,   30,  109, 1,
        30,  108, 1,   30,  107, 1,   30,  106, 1,   30,  105, 1,   30,  104, 1,   30,  102, 1,   30,  101, 1,   30,
        100, 1,   30,  98,  1,   30,  97,  1,   30,  95,  1,   30,  93,  1,   30,  91,  1,   30,  88,  1,   30,  85,
        1,   30,  79,  1,   30,  50,  1,   30,  50,  1,   30,  50,  1,   30,  50,  1,   30,  50,  1,   30,  50,  1,
        30,  50,  1,   30,  50,  1,   30,  50,  1,   30,  50,  1,   30,  50,  1,   30,  50,  1,   30,  50,  1,   30,
        50,  1,   30,  50,  1,   30,  50,  1,   30,  50,  1,   30,  50,  1,   30,  50,  1,   30,  50,  1,   30,  50,
        1,   22,  76,  1,   18,  79,  1,   17,  81,  1,   16,  82,  1,   15,  83,  1,   14,  83,  1,   14,  84,  1,
        14,  84,  1,   14,  84,  1,   13,  84,  1,   13,  84,  1,   13,  84,  1,   14,  84,  1,   14,  84,  1,   14,
        83,  1,   15,  83,  1,   16,  82,  1,   17,  81,  1,   18,  80,  1,   20,  77,  81,  34,  187, 1,   57,  67,
        1,   51,  73,  1,   47,  77,  1,   44,  80,  1,   42,  82,  1,   40,  85,  1,   38,  87,  1,   36,  88,  1,
        34,  90,  1,   33,  91,  1,   31,  93,  1,   30,  94,  1,   29,  95,  1,   27,  97,  1,   26,  98,  1,   25,
        99,  1,   24,  100, 1,   23,  101, 1,   22,  102, 1,   21,  103, 1,   20,  104, 2,   20,  55,  70,  105, 2,
        19,  51,  73,  105, 2,   18,  49,  75,  106, 2,   17,  47,  77,  107, 2,   16,  45,  79,  108, 2,   16,  44,
        80,  108, 2,   15,  42,  82,  109, 2,   15,  41,  83,  110, 2,   14,  40,  84,  110, 2,   13,  39,  85,  111,
        2,   13,  38,  86,  111, 2,   12,  37,  87,  112, 2,   12,  36,  88,  112, 2,   11,  35,  89,  113, 2,   11,
        35,  90,  113, 2,   10,  34,  90,  114, 2,   10,  33,  91,  114, 2,   10,  32,  92,  115, 2,   9,   32,  93,
        115, 2,   9,   31,  93,  115, 2,   9,   31,  94,  116, 2,   8,   30,  94,  116, 2,   8,   30,  94,  116, 2,
        8,   29,  95,  117, 2,   7,   29,  95,  117, 2,   7,   28,  96,  117, 2,   7,   28,  96,  117, 2,   7,   28,
        96,  118, 2,   6,   28,  97,  118, 2,   6,   27,  97,  118, 2,   6,   27,  97,  118, 2,   6,   27,  97,  118,
        2,   6,   26,  98,  118, 2,   6,   26,  98,  118, 2,   6,   26,  98,  119, 2,   6,   26,  98,  119, 2,   6,
        26,  98,  119, 2,   5,   26,  98,  119, 2,   5,   26,  98,  119, 2,   5,   26,  98,  119, 2,   5,   26,  98,
        119, 2,   5,   26,  99,  119, 2,   5,   26,  99,  119, 2,   5,   26,  99,  119, 2,   5,   26,  98,  119, 2,
        5,   26,  98,  119, 2,   6,   26,  98,  119, 2,   6,   26,  98,  119, 2,   6,   26,  98,  119, 2,   6,   26,
        98,  119, 2,   6,   26,  98,  118, 2,   6,   26,  98,  118, 2,   6,   27,  98,  118, 2,   6,   27,  97,  118,
        2,   6,   27,  97,  118, 2,   7,   27,  97,  118, 2,   7,   28,  97,  118, 2,   7,   28,  96,  117, 2,   7,
        28,  96,  117, 2,   7,   28,  96,  117, 2,   8,   29,  95,  117, 2,   8,   29,  95,  116, 2,   8,   30,  94,
        116, 2,   9,   30,  94,  116, 2,   9,   31,  93,  115, 2,   9,   31,  93,  115, 2,   10,  32,  92,  115, 2,
        10,  33,  92,  114, 2,   10,  33,  91,  114, 2,   11,  34,  90,  114, 2,   11,  35,  89,  113, 2,   12,  36,
        88,  113, 2,   12,  36,  88,  112, 2,   13,  37,  87,  112, 2,   13,  38,  86,  111, 2,   14,  39,  85,  111,
        2,   14,  41,  84,  110, 2,   15,  42,  82,  109, 2,   16,  43,  81,  109, 2,   16,  44,  80,  108, 2,   17,
        46,  78,  107, 2,   18,  48,  76,  107, 2,   19,  50,  74,  106, 2,   19,  53,  71,  105, 2,   20,  57,  67,
        104, 1,   21,  103, 1,   22,  103, 1,   23,  102, 1,   24,  101, 1,   25,  100, 1,   26,  99,  1,   27,  97,
        1,   28,  96,  1,   29,  95,  1,   31,  94,  1,   32,  92,  1,   33,  91,  1,   35,  89,  1,   37,  88,  1,
        39,  86,  1,   41,  84,  1,   41,  81,  1,   40,  79,  1,   39,  75,  1,   38,  70,  2,   37,  66,  107, 110,
        2,   36,  73,  105, 114, 2,   35,  78,  103, 115, 2,   34,  82,  101, 116, 2,   33,  84,  99,  117, 2,   32,
        87,  97,  118, 2,   31,  89,  95,  118, 1,   30,  119, 1,   29,  119, 1,   28,  119, 1,   27,  119, 1,   26,
        119, 1,   26,  119, 1,   25,  119, 1,   25,  118, 1,   25,  118, 1,   25,  117, 1,   25,  116, 1,   25,  115,
        1,   25,  114, 1,   25,  112, 1,   26,  110, 2,   27,  56,  73,  109, 2,   27,  51,  76,  107, 2,   29,  46,
        79,  105, 2,   30,  42,  82,  102, 2,   33,  37,  86,  98,  82,  37,  157, 1,   14,  66,  1,   10,  76,  1,
        8,   80,  1,   7,   84,  1,   6,   86,  1,   5,   89,  1,   5,   91,  1,   5,   92,  1,   4,   94,  1,   4,
        95,  1,   4,   96,  1,   4,   97,  1,   4,   98,  1,   5,   99,  1,   5,   100, 1,   5,   101, 1,   6,   102,
        1,   7,   103, 1,   8,   103, 1,   10,  104, 1,   15,  104, 2,   21,  41,  73,  105, 2,   21,  41,  77,  106,
        2,   21,  41,  79,  106, 2,   21,  41,  81,  106, 2,   21,  41,  82,  107, 2,   21,  41,  83,  107, 2,   21,
        41,  84,  107, 2,   21,  41,  85,  108, 2,   21,  41,  86,  108, 2,   21,  41,  87,  108, 2,   21,  41,  87,
        108, 2,   21,  41,  88,  108, 2,   21,  41,  88,  108, 2,   21,  41,  88,  109, 2,   21,  41,  88,  109, 2,
        21,  41,  88,  109, 2,   21,  41,  88,  109, 2,   21,  41,  88,  109, 2,   21,  41,  88,  109, 2,   21,  41,
        87,  108, 2,   21,  41,  87,  108, 2,   21,  41,  86,  108, 2,   21,  41,  86,  108, 2,   21,  41,  85,  108,
        2,   21,  41,  84,  107, 2,   21,  41,  82,  107, 2,   21,  41,  81,  107, 2,   21,  41,  79,  106, 2,   21,
        41,  77,  106, 2,   21,  41,  75,  105, 2,   21,  41,  73,  105, 2,   21,  41,  69,  104, 2,   21,  41,  65,
        104, 1,   21,  103, 1,   21,  102, 1,   21,  101, 1,   21,  100, 1,   21,  99,  1,   21,  98,  1,   21,  97,
        1,   21,  96,  1,   21,  95,  1,   21,  93,  1,   21,  92,  1,   21,  90,  1,   21,  89,  1,   21,  87,  1,
        21,  87,  1,   21,  88,  1,   21,  89,  1,   21,  90,  1,   21,  91,  1,   21,  92,  2,   21,  41,  61,  93,
        2,   21,  41,  63,  94,  2,   21,  41,  65,  95,  2,   21,  41,  66,  96,  2,   21,  41,  68,  97,  2,   21,
        41,  69,  98,  2,   21,  41,  70,  99,  2,   21,  41,  71,  99,  2,   21,  41,  72,  100, 2,   21,  41,  74,
        101, 2,   21,  41,  75,  102, 2,   21,  41,  76,  103, 2,   21,  41,  77,  103, 2,   21,  41,  77,  104, 2,
        21,  41,  78,  105, 2,   21,  41,  79,  106, 2,   21,  41,  80,  106, 2,   21,  41,  81,  107, 2,   21,  41,
        82,  108, 2,   21,  41,  83,  108, 2,   21,  41,  84,  109, 2,   21,  41,  84,  110, 2,   21,  41,  85,  110,
        2,   21,  41,  86,  111, 2,   21,  41,  87,  112, 2,   21,  41,  88,  112, 2,   12,  54,  88,  119, 2,   9,
        57,  89,  123, 2,   8,   58,  90,  124, 2,   7,   60,  91,  125, 2,   6,   60,  91,  126, 2,   5,   61,  92,
        126, 2,   5,   61,  93,  127, 2,   4,   62,  93,  127, 2,   4,   62,  94,  127, 2,   4,   62,  95,  127, 2,
        4,   62,  96,  127, 2,   4,   62,  96,  127, 2,   4,   62,  97,  127, 2,   5,   61,  98,  127, 2,   5,   61,
        98,  126, 2,   6,   61,  99,  126, 2,   6,   60,  100, 125, 2,   7,   59,  100, 124, 2,   9,   57,  101, 123,
        2,   11,  55,  102, 120, 83,  34,  160, 1,   58,  66,  1,   50,  74,  1,   46,  78,  2,   43,  81,  96,  97,
        2,   41,  83,  92,  101, 2,   39,  85,  90,  102, 2,   36,  87,  89,  103, 1,   35,  104, 1,   34,  105, 1,
        32,  105, 1,   31,  106, 1,   30,  106, 1,   29,  106, 1,   28,  106, 1,   27,  106, 1,   26,  106, 1,   25,
        106, 1,   24,  106, 1,   24,  106, 1,   23,  106, 1,   22,  106, 2,   22,  54,  70,  106, 2,   21,  50,  73,
        106, 2,   21,  48,  76,  106, 2,   20,  46,  78,  106, 2,   20,  45,  79,  106, 2,   19,  44,  81,  106, 2,
        19,  42,  82,  106, 2,   19,  41,  83,  106, 2,   19,  41,  84,  106, 2,   18,  40,  84,  106, 2,   18,  39,
        85,  106, 2,   18,  39,  85,  106, 2,   18,  38,  86,  106, 2,   18,  38,  86,  106, 2,   18,  38,  86,  106,
        2,   18,  38,  86,  106, 2,   18,  38,  87,  106, 2,   18,  38,  87,  106, 2,   18,  39,  87,  105, 2,   18,
        39,  88,  105, 2,   18,  40,  88,  104, 2,   18,  40,  89,  104, 2,   19,  41,  90,  103, 2,   19,  42,  92,
        101, 2,   19,  43,  95,  98,  1,   19,  44,  1,   20,  45,  1,   20,  47,  1,   21,  50,  1,   21,  53,  1,
        22,  58,  1,   22,  62,  1,   23,  67,  1,   23,  72,  1,   24,  76,  1,   25,  80,  1,   26,  84,  1,   27,
        87,  1,   27,  89,  1,   28,  92,  1,   30,  94,  1,   31,  96,  1,   32,  97,  1,   34,  98,  1,   35,  100,
        1,   37,  101, 1,   40,  102, 1,   42,  103, 1,   45,  103, 1,   49,  104, 1,   52,  105, 1,   57,  106, 1,
        61,  106, 1,   67,  107, 1,   71,  107, 1,   76,  108, 1,   79,  108, 1,   81,  109, 1,   83,  109, 1,   85,
        109, 1,   86,  110, 2,   23,  25,  87,  110, 2,   19,  29,  88,  110, 2,   18,  30,  89,  110, 2,   17,  31,
        89,  110, 2,   16,  32,  90,  110, 2,   15,  33,  90,  111, 2,   15,  33,  90,  111, 2,   15,  33,  90,  111,
        2,   14,  34,  91,  111, 2,   14,  34,  90,  111, 2,   14,  35,  90,  111, 2,   14,  35,  90,  110, 2,   14,
        35,  90,  110, 2,   14,  36,  89,  110, 2,   14,  36,  88,  110, 2,   14,  38,  87,  110, 2,   14,  39,  86,
        110, 2,   14,  40,  85,  109, 2,   14,  42,  84,  109, 2,   14,  44,  82,  109, 2,   14,  46,  80,  108, 2,
        14,  49,  77,  108, 2,   14,  52,  74,  108, 2,   14,  57,  69,  107, 1,   14,  106, 1,   14,  106, 1,   14,
        105, 1,   14,  105, 1,   14,  104, 1,   14,  103, 1,   14,  102, 1,   14,  101, 1,   14,  100, 1,   14,  98,
        1,   15,  97,  1,   15,  96,  2,   16,  32,  33,  94,  2,   16,  31,  35,  93,  2,   17,  30,  38,  90,  2,
        19,  29,  40,  88,  2,   21,  27,  43,  86,  1,   46,  83,  1,   50,  79,  1,   55,  75,  84,  37,  157, 1,
        9,   114, 1,   9,   114, 1,   9,   114, 1,   9,   114, 1,   9,   114, 1,   9,   114, 1,   9,   114, 1,   9,
        114, 1,   9,   114, 1,   9,   114, 1,   9,   114, 1,   9,   114, 1,   9,   114, 1,   9,   114, 1,   9,   114,
        1,   9,   114, 1,   9,   114, 1,   9,   114, 1,   9,   114, 1,   9,   114, 1,   9,   114, 3,   9,   30,  52,
        72,  94,  114, 3,   9,   30,  52,  72,  94,  114, 3,   9,   30,  52,  72,  94,  114, 3,   9,   30,  52,  72,
        94,  114, 3,   9,   30,  52,  72,  94,  114, 3,   9,   30,  52,  72,  94,  114, 3,   9,   30,  52,  72,  94,
        114, 3,   9,   30,  52,  72,  94,  114, 3,   9,   30,  52,  72,  94,  114, 3,   9,   30,  52,  72,  94,  114,
        3,   9,   30,  52,  72,  94,  114, 3,   9,   30,  52,  72,  94,  114, 3,   9,   30,  52,  72,  94,  114, 3,
        9,   30,  52,  72,  94,  114, 3,   9,   30,  52,  72,  94,  114, 3,   9,   30,  52,  72,  94,  114, 3,   9,
        30,  52,  72,  94,  114, 3,   9,   30,  52,  72,  94,  114, 3,   9,   30,  52,  72,  94,  114, 3,   9,   30,
        52,  72,  94,  114, 3,   9,   30,  52,  72,  94,  114, 3,   10,  30,  52,  72,  94,  114, 3,   10,  29,  52,
        72,  94,  114, 3,   10,  29,  52,  72,  95,  114, 3,   10,  29,  52,  72,  95,  114, 3,   10,  29,  52,  72,
        95,  114, 3,   11,  28,  52,  72,  95,  113, 3,   11,  28,  52,  72,  96,  113, 3,   12,  27,  52,  72,  96,
        112, 3,   12,  27,  52,  72,  97,  111, 3,   14,  26,  52,  72,  98,  110, 3,   15,  24,  52,  72,  100, 109,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   33,  91,  1,   30,  94,  1,   28,  96,  1,   27,  97,  1,
        26,  97,  1,   26,  98,  1,   25,  99,  1,   25,  99,  1,   25,  99,  1,   25,  99,  1,   25,  99,  1,   25,
        99,  1,   25,  99,  1,   25,  99,  1,   25,  98,  1,   26,  98,  1,   27,  97,  1,   28,  96,  1,   29,  94,
        1,   32,  92,  85,  37,  160, 2,   14,  48,  77,  111, 2,   9,   51,  73,  115, 2,   7,   53,  71,  117, 2,
        6,   54,  70,  118, 2,   6,   55,  69,  119, 2,   5,   56,  68,  120, 2,   4,   56,  68,  120, 2,   4,   57,
        68,  121, 2,   4,   57,  67,  121, 2,   4,   57,  67,  121, 2,   3,   57,  67,  121, 2,   4,   57,  67,  121,
        2,   4,   57,  67,  121, 2,   4,   57,  68,  121, 2,   4,   56,  68,  120, 2,   5,   56,  69,  120, 2,   6,
        55,  69,  119, 2,   6,   54,  70,  118, 2,   8,   53,  71,  117, 2,   9,   51,  73,  115, 2,   14,  46,  79,
        110, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,
        16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,
        88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108,
        2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,
        36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,
        108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,
        16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,
        88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108,
        2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,
        36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,
        108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,
        16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,
        88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108,
        2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,  36,  88,  108, 2,   16,
        37,  88,  108, 2,   16,  37,  88,  108, 2,   16,  37,  88,  108, 2,   17,  37,  87,  108, 2,   17,  38,  87,
        108, 2,   17,  38,  87,  108, 2,   17,  38,  86,  107, 2,   17,  39,  86,  107, 2,   17,  39,  85,  107, 2,
        18,  40,  84,  107, 2,   18,  41,  84,  106, 2,   18,  42,  83,  106, 2,   19,  43,  82,  105, 2,   19,  43,
        81,  105, 2,   20,  45,  80,  105, 2,   20,  46,  79,  104, 2,   21,  47,  77,  104, 2,   21,  49,  76,  103,
        2,   22,  51,  74,  103, 2,   23,  53,  71,  102, 2,   23,  57,  67,  101, 1,   24,  101, 1,   25,  100, 1,
        26,  99,  1,   27,  98,  1,   28,  97,  1,   28,  96,  1,   29,  95,  1,   30,  94,  1,   32,  93,  1,   33,
        92,  1,   34,  91,  1,   35,  90,  1,   37,  88,  1,   38,  87,  1,   40,  85,  1,   42,  83,  1,   44,  81,
        1,   46,  78,  1,   49,  75,  1,   54,  71,  86,  37,  157, 2,   6,   41,  83,  117, 2,   2,   45,  79,  122,
        2,   1,   47,  77,  124, 2,   0,   48,  76,  125, 2,   0,   49,  75,  126, 2,   0,   50,  74,  126, 2,   0,
        50,  74,  127, 2,   0,   51,  74,  127, 2,   0,   51,  73,  127, 2,   0,   51,  73,  127, 2,   0,   51,  73,
        128, 2,   0,   51,  73,  127, 2,   0,   51,  73,  127, 2,   0,   51,  74,  127, 2,   0,   50,  74,  127, 2,
        0,   50,  75,  126, 2,   0,   49,  75,  125, 2,   0,   48,  76,  125, 2,   1,   47,  77,  123, 2,   2,   45,
        79,  122, 2,   7,   40,  84,  117, 2,   11,  33,  91,  113, 2,   12,  34,  91,  113, 2,   12,  34,  90,  112,
        2,   12,  34,  90,  112, 2,   13,  35,  89,  111, 2,   13,  35,  89,  111, 2,   14,  36,  89,  111, 2,   14,
        36,  88,  110, 2,   14,  37,  88,  110, 2,   15,  37,  87,  109, 2,   15,  37,  87,  109, 2,   16,  38,  86,
        109, 2,   16,  38,  86,  108, 2,   17,  39,  86,  108, 2,   17,  39,  85,  107, 2,   17,  39,  85,  107, 2,
        18,  40,  84,  106, 2,   18,  40,  84,  106, 2,   19,  41,  84,  106, 2,   19,  41,  83,  105, 2,   19,  42,
        83,  105, 2,   20,  42,  82,  104, 2,   20,  42,  82,  104, 2,   21,  43,  81,  104, 2,   21,  43,  81,  103,
        2,   22,  44,  81,  103, 2,   22,  44,  80,  102, 2,   22,  44,  80,  102, 2,   23,  45,  79,  101, 2,   23,
        45,  79,  101, 2,   24,  46,  78,  101, 2,   24,  46,  78,  100, 2,   24,  47,  78,  100, 2,   25,  47,  77,
        99,  2,   25,  47,  77,  99,  2,   26,  48,  76,  98,  2,   26,  48,  76,  98,  2,   27,  49,  76,  98,  2,
        27,  49,  75,  97,  2,   27,  49,  75,  97,  2,   28,  50,  74,  96,  2,   28,  50,  74,  96,  2,   29,  51,
        73,  96,  2,   29,  51,  73,  95,  2,   29,  52,  73,  95,  2,   30,  52,  72,  94,  2,   30,  52,  72,  94,
        2,   31,  53,  71,  93,  2,   31,  53,  71,  93,  2,   32,  54,  71,  93,  2,   32,  54,  70,  92,  2,   32,
        54,  70,  92,  2,   33,  55,  69,  91,  2,   33,  55,  69,  91,  2,   34,  56,  68,  91,  2,   34,  56,  68,
        90,  2,   34,  57,  68,  90,  2,   35,  57,  67,  89,  2,   35,  57,  67,  89,  2,   36,  58,  66,  88,  2,
        36,  58,  66,  88,  2,   37,  59,  65,  88,  2,   37,  59,  65,  87,  2,   37,  59,  65,  87,  2,   38,  60,
        64,  86,  2,   38,  60,  64,  86,  2,   39,  61,  63,  86,  2,   39,  61,  63,  85,  2,   39,  62,  63,  85,
        1,   40,  84,  1,   40,  84,  1,   41,  83,  1,   41,  83,  1,   42,  83,  1,   42,  82,  1,   42,  82,  1,
        43,  81,  1,   43,  81,  1,   44,  80,  1,   44,  80,  1,   44,  80,  1,   45,  79,  1,   45,  79,  1,   46,
        78,  1,   46,  78,  1,   47,  78,  1,   47,  77,  1,   47,  77,  1,   48,  76,  1,   48,  76,  1,   49,  75,
        1,   49,  75,  1,   49,  75,  1,   50,  74,  1,   50,  74,  1,   51,  73,  1,   51,  73,  1,   52,  73,  1,
        52,  72,  87,  37,  157, 2,   9,   43,  81,  115, 2,   5,   47,  77,  119, 2,   3,   49,  75,  121, 2,   2,
        51,  74,  123, 2,   1,   51,  73,  123, 2,   0,   52,  72,  124, 2,   0,   52,  72,  125, 2,   0,   53,  72,
        125, 2,   0,   53,  71,  125, 2,   0,   53,  71,  125, 2,   0,   53,  71,  125, 2,   0,   53,  71,  125, 2,
        0,   53,  71,  125, 2,   0,   53,  72,  125, 2,   0,   52,  72,  124, 2,   0,   52,  73,  124, 2,   1,   51,
        73,  123, 2,   2,   50,  74,  122, 2,   3,   49,  75,  121, 2,   4,   47,  77,  119, 2,   8,   42,  83,  116,
        2,   9,   29,  95,  116, 2,   9,   29,  95,  115, 2,   9,   29,  95,  115, 2,   9,   30,  95,  115, 2,   9,
        30,  95,  115, 2,   9,   30,  95,  115, 2,   9,   30,  95,  115, 2,   9,   30,  94,  115, 2,   10,  30,  94,
        115, 3,   10,  30,  52,  73,  94,  115, 3,   10,  30,  51,  73,  94,  114, 3,   10,  30,  51,  74,  94,  114,
        3,   10,  31,  51,  74,  94,  114, 3,   10,  31,  50,  74,  94,  114, 3,   10,  31,  50,  75,  94,  114, 3,
        10,  31,  50,  75,  93,  114, 3,   11,  31,  49,  75,  93,  114, 3,   11,  31,  49,  76,  93,  114, 3,   11,
        31,  49,  76,  93,  113, 3,   11,  31,  48,  76,  93,  113, 3,   11,  32,  48,  77,  93,  113, 3,   11,  32,
        48,  77,  93,  113, 3,   11,  32,  47,  77,  93,  113, 3,   11,  32,  47,  78,  93,  113, 3,   12,  32,  47,
        78,  92,  113, 3,   12,  32,  46,  78,  92,  113, 3,   12,  32,  46,  79,  92,  113, 3,   12,  32,  46,  79,
        92,  112, 3,   12,  33,  45,  79,  92,  112, 3,   12,  33,  45,  80,  92,  112, 3,   12,  33,  45,  80,  92,
        112, 3,   12,  33,  44,  80,  92,  112, 3,   12,  33,  44,  81,  91,  112, 3,   13,  33,  44,  81,  91,  112,
        3,   13,  33,  43,  81,  91,  112, 3,   13,  33,  43,  82,  91,  111, 3,   13,  33,  43,  82,  91,  111, 3,
        13,  34,  42,  82,  91,  111, 3,   13,  34,  42,  83,  91,  111, 3,   13,  34,  41,  83,  91,  111, 3,   13,
        34,  41,  83,  91,  111, 3,   14,  34,  41,  84,  90,  111, 3,   14,  34,  40,  84,  90,  111, 4,   14,  34,
        40,  62,  63,  84,  90,  111, 4,   14,  34,  40,  61,  63,  85,  90,  110, 4,   14,  35,  39,  61,  63,  85,
        90,  110, 4,   14,  35,  39,  61,  64,  85,  90,  110, 4,   14,  35,  39,  60,  64,  86,  90,  110, 4,   14,
        35,  38,  60,  64,  86,  90,  110, 4,   15,  35,  38,  60,  65,  86,  89,  110, 4,   15,  35,  38,  59,  65,
        87,  89,  110, 4,   15,  35,  37,  59,  65,  87,  89,  110, 4,   15,  35,  37,  59,  66,  87,  89,  109, 4,
        15,  35,  37,  58,  66,  88,  89,  109, 3,   15,  58,  66,  88,  89,  109, 3,   15,  58,  67,  88,  89,  109,
        2,   15,  57,  67,  109, 2,   16,  57,  67,  109, 2,   16,  57,  68,  109, 2,   16,  56,  68,  109, 2,   16,
        56,  68,  109, 2,   16,  56,  69,  108, 2,   16,  55,  69,  108, 2,   16,  55,  70,  108, 2,   16,  55,  70,
        108, 2,   16,  54,  70,  108, 2,   17,  54,  71,  108, 2,   17,  54,  71,  108, 2,   17,  53,  71,  108, 2,
        17,  53,  72,  107, 2,   17,  53,  72,  107, 2,   17,  52,  72,  107, 2,   17,  52,  73,  107, 2,   17,  52,
        73,  107, 2,   18,  51,  73,  107, 2,   18,  51,  74,  107, 2,   18,  51,  74,  107, 2,   18,  50,  74,  107,
        2,   18,  50,  75,  106, 2,   18,  50,  75,  106, 2,   18,  49,  75,  106, 2,   18,  49,  76,  106, 2,   19,
        49,  76,  106, 2,   19,  48,  76,  106, 2,   19,  48,  77,  106, 2,   19,  48,  77,  106, 2,   19,  47,  77,
        105, 2,   19,  47,  78,  105, 2,   19,  47,  78,  105, 2,   19,  46,  78,  105, 2,   20,  46,  79,  105, 2,
        20,  46,  79,  105, 2,   20,  46,  79,  105, 2,   20,  45,  80,  105, 2,   20,  45,  80,  105, 2,   20,  45,
        80,  104, 2,   20,  44,  81,  104, 2,   20,  44,  81,  104, 2,   20,  44,  81,  104, 88,  37,  157, 2,   15,
        41,  83,  109, 2,   11,  45,  79,  113, 2,   9,   47,  77,  115, 2,   8,   48,  76,  116, 2,   7,   49,  75,
        117, 2,   7,   50,  74,  118, 2,   6,   50,  74,  118, 2,   6,   51,  74,  118, 2,   5,   51,  73,  119, 2,
        5,   51,  73,  119, 2,   5,   51,  73,  119, 2,   6,   51,  73,  119, 2,   6,   51,  73,  118, 2,   6,   51,
        74,  118, 2,   6,   50,  74,  118, 2,   7,   50,  75,  117, 2,   8,   49,  75,  117, 2,   8,   48,  76,  116,
        2,   9,   47,  77,  114, 2,   11,  45,  79,  113, 2,   16,  44,  81,  108, 2,   18,  44,  80,  106, 2,   19,
        45,  79,  106, 2,   20,  46,  78,  105, 2,   20,  47,  78,  104, 2,   21,  48,  77,  103, 2,   22,  48,  76,
        102, 2,   23,  49,  75,  101, 2,   24,  50,  74,  101, 2,   25,  51,  74,  100, 2,   25,  52,  73,  99,  2,
        26,  53,  72,  98,  2,   27,  53,  71,  97,  2,   28,  54,  70,  96,  2,   29,  55,  69,  96,  2,   30,  56,
        69,  95,  2,   30,  57,  68,  94,  2,   31,  58,  67,  93,  2,   32,  58,  66,  92,  2,   33,  59,  65,  92,
        2,   34,  60,  65,  91,  2,   35,  61,  64,  90,  2,   35,  62,  63,  89,  1,   36,  88,  1,   37,  87,  1,
        38,  87,  1,   39,  86,  1,   40,  85,  1,   40,  84,  1,   41,  83,  1,   42,  82,  1,   43,  82,  1,   44,
        81,  1,   45,  80,  1,   46,  79,  1,   46,  78,  1,   47,  78,  1,   48,  77,  1,   49,  76,  1,   49,  76,
        1,   48,  77,  1,   47,  77,  1,   46,  78,  1,   45,  79,  1,   45,  80,  1,   44,  81,  1,   43,  81,  1,
        42,  82,  1,   41,  83,  1,   40,  84,  1,   40,  85,  1,   39,  86,  1,   38,  86,  1,   37,  87,  1,   36,
        88,  2,   36,  62,  63,  89,  2,   35,  61,  64,  90,  2,   34,  60,  65,  91,  2,   33,  59,  65,  91,  2,
        32,  59,  66,  92,  2,   31,  58,  67,  93,  2,   31,  57,  68,  94,  2,   30,  56,  69,  95,  2,   29,  55,
        70,  96,  2,   28,  55,  70,  96,  2,   27,  54,  71,  97,  2,   27,  53,  72,  98,  2,   26,  52,  73,  99,
        2,   25,  51,  74,  100, 2,   24,  50,  74,  101, 2,   23,  50,  75,  101, 2,   22,  49,  76,  102, 2,   22,
        48,  77,  103, 2,   21,  47,  78,  104, 2,   20,  46,  79,  105, 2,   19,  46,  79,  106, 2,   18,  45,  80,
        106, 2,   17,  44,  81,  107, 2,   17,  43,  82,  108, 2,   16,  42,  83,  109, 2,   12,  45,  80,  113, 2,
        9,   48,  77,  116, 2,   7,   50,  75,  118, 2,   6,   51,  74,  119, 2,   5,   51,  73,  119, 2,   5,   52,
        73,  120, 2,   4,   53,  72,  121, 2,   4,   53,  72,  121, 2,   4,   53,  72,  121, 2,   3,   53,  72,  121,
        2,   3,   53,  71,  121, 2,   4,   53,  72,  121, 2,   4,   53,  72,  121, 2,   4,   53,  72,  121, 2,   4,
        52,  72,  120, 2,   5,   52,  73,  120, 2,   6,   51,  74,  119, 2,   7,   50,  75,  118, 2,   8,   49,  76,
        117, 2,   11,  46,  79,  114, 89,  37,  157, 2,   15,  41,  83,  109, 2,   11,  45,  79,  113, 2,   9,   47,
        77,  115, 2,   8,   48,  76,  116, 2,   7,   49,  75,  117, 2,   7,   50,  74,  118, 2,   6,   50,  74,  118,
        2,   6,   51,  73,  119, 2,   6,   51,  73,  119, 2,   6,   51,  73,  119, 2,   6,   51,  73,  119, 2,   6,
        51,  73,  119, 2,   6,   51,  73,  119, 2,   6,   51,  73,  119, 2,   6,   50,  74,  118, 2,   7,   50,  74,
        118, 2,   8,   49,  75,  117, 2,   8,   48,  76,  116, 2,   10,  47,  77,  115, 2,   12,  45,  79,  113, 2,
        16,  43,  81,  108, 2,   19,  44,  80,  105, 2,   20,  44,  80,  104, 2,   20,  45,  79,  104, 2,   21,  46,
        78,  103, 2,   22,  46,  78,  102, 2,   23,  47,  77,  102, 2,   23,  48,  76,  101, 2,   24,  48,  76,  100,
        2,   25,  49,  75,  100, 2,   25,  50,  74,  99,  2,   26,  50,  74,  98,  2,   27,  51,  73,  98,  2,   27,
        52,  72,  97,  2,   28,  52,  72,  96,  2,   29,  53,  71,  96,  2,   29,  54,  70,  95,  2,   30,  55,  70,
        94,  2,   31,  55,  69,  93,  2,   31,  56,  68,  93,  2,   32,  57,  68,  92,  2,   33,  57,  67,  91,  2,
        33,  58,  66,  91,  2,   34,  59,  66,  90,  2,   35,  59,  65,  89,  2,   35,  60,  64,  89,  2,   36,  61,
        64,  88,  2,   37,  61,  63,  87,  1,   37,  87,  1,   38,  86,  1,   39,  85,  1,   40,  85,  1,   40,  84,
        1,   41,  83,  1,   42,  83,  1,   42,  82,  1,   43,  81,  1,   44,  81,  1,   44,  80,  1,   45,  79,  1,
        46,  79,  1,   46,  78,  1,   47,  77,  1,   48,  77,  1,   48,  76,  1,   49,  75,  1,   50,  75,  1,   50,
        74,  1,   51,  73,  1,   52,  73,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   33,  91,  1,   30,  94,  1,   29,  96,  1,   27,  97,  1,
        27,  98,  1,   26,  98,  1,   26,  99,  1,   25,  99,  1,   25,  99,  1,   25,  100, 1,   25,  100, 1,   25,
        99,  1,   25,  99,  1,   25,  99,  1,   26,  99,  1,   26,  98,  1,   27,  97,  1,   28,  96,  1,   30,  95,
        1,   32,  92,  90,  37,  157, 1,   20,  104, 1,   20,  104, 1,   20,  104, 1,   20,  104, 1,   20,  104, 1,
        20,  104, 1,   20,  104, 1,   20,  104, 1,   20,  104, 1,   20,  104, 1,   20,  104, 1,   20,  104, 1,   20,
        104, 1,   20,  104, 1,   20,  104, 1,   20,  104, 1,   20,  104, 1,   20,  104, 1,   20,  104, 1,   20,  104,
        1,   20,  104, 2,   20,  41,  77,  103, 2,   20,  41,  77,  102, 2,   20,  41,  76,  102, 2,   20,  41,  75,
        101, 2,   20,  41,  74,  100, 2,   20,  41,  73,  99,  2,   20,  41,  73,  99,  2,   20,  41,  72,  98,  2,
        20,  41,  71,  97,  2,   20,  41,  70,  96,  2,   20,  41,  70,  95,  2,   20,  41,  69,  95,  2,   20,  41,
        68,  94,  2,   20,  41,  67,  93,  2,   20,  41,  66,  92,  2,   20,  41,  66,  92,  2,   21,  41,  65,  91,
        2,   21,  40,  64,  90,  2,   21,  40,  63,  89,  2,   21,  40,  63,  88,  2,   21,  40,  62,  88,  2,   21,
        40,  61,  87,  2,   22,  39,  60,  86,  2,   22,  39,  59,  85,  2,   23,  38,  59,  85,  2,   24,  37,  58,
        84,  2,   26,  35,  57,  83,  2,   28,  33,  56,  82,  1,   56,  81,  1,   55,  81,  1,   54,  80,  1,   53,
        79,  1,   52,  78,  1,   52,  78,  1,   51,  77,  1,   50,  76,  1,   49,  75,  1,   49,  74,  1,   48,  74,
        1,   47,  73,  1,   46,  72,  1,   45,  71,  1,   45,  71,  1,   44,  70,  1,   43,  69,  1,   42,  68,  1,
        41,  67,  1,   41,  67,  1,   40,  66,  1,   39,  65,  2,   38,  64,  97,  100, 2,   38,  64,  94,  103, 2,
        37,  63,  92,  105, 2,   36,  62,  91,  106, 2,   35,  61,  90,  106, 2,   34,  60,  90,  107, 2,   34,  60,
        89,  107, 2,   33,  59,  89,  108, 2,   32,  58,  89,  108, 2,   31,  57,  89,  108, 2,   31,  56,  88,  108,
        2,   30,  56,  88,  108, 2,   29,  55,  88,  108, 2,   28,  54,  88,  109, 2,   27,  53,  88,  109, 2,   27,
        53,  88,  109, 2,   26,  52,  88,  109, 2,   25,  51,  88,  109, 2,   24,  50,  88,  109, 2,   24,  49,  88,
        109, 2,   23,  49,  88,  109, 2,   22,  48,  88,  109, 2,   21,  47,  88,  109, 2,   20,  46,  88,  109, 2,
        20,  46,  88,  109, 2,   19,  45,  88,  109, 2,   18,  44,  88,  109, 2,   17,  43,  88,  109, 2,   17,  42,
        88,  109, 1,   16,  109, 1,   16,  109, 1,   16,  109, 1,   16,  108, 1,   16,  108, 1,   16,  108, 1,   16,
        108, 1,   16,  108, 1,   16,  108, 1,   16,  108, 1,   16,  108, 1,   16,  108, 1,   16,  108, 1,   16,  108,
        1,   16,  108, 1,   16,  108, 1,   16,  108, 1,   16,  108, 1,   16,  108, 1,   16,  108, 91,  29,  189, 1,
        52,  88,  1,   52,  92,  1,   52,  94,  1,   52,  95,  1,   52,  96,  1,   52,  97,  1,   52,  97,  1,   52,
        97,  1,   52,  98,  1,   52,  98,  1,   52,  98,  1,   52,  98,  1,   52,  98,  1,   52,  97,  1,   52,  97,
        1,   52,  96,  1,   52,  96,  1,   52,  95,  1,   52,  93,  1,   52,  91,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  91,  1,   52,  93,  1,   52,  95,  1,   52,  96,  1,   52,  96,  1,   52,  97,  1,   52,  97,
        1,   52,  98,  1,   52,  98,  1,   52,  98,  1,   52,  98,  1,   52,  98,  1,   52,  97,  1,   52,  97,  1,
        52,  97,  1,   52,  96,  1,   52,  95,  1,   52,  94,  1,   52,  93,  1,   52,  89,  92,  14,  180, 1,   27,
        31,  1,   23,  33,  1,   22,  35,  1,   21,  36,  1,   20,  36,  1,   19,  37,  1,   19,  38,  1,   19,  38,
        1,   18,  39,  1,   18,  39,  1,   18,  40,  1,   18,  40,  1,   19,  40,  1,   19,  41,  1,   19,  41,  1,
        20,  42,  1,   20,  42,  1,   20,  43,  1,   21,  43,  1,   21,  44,  1,   22,  44,  1,   22,  45,  1,   23,
        45,  1,   23,  46,  1,   24,  46,  1,   24,  47,  1,   25,  47,  1,   25,  47,  1,   26,  48,  1,   26,  48,
        1,   27,  49,  1,   27,  49,  1,   27,  50,  1,   28,  50,  1,   28,  51,  1,   29,  51,  1,   29,  52,  1,
        30,  52,  1,   30,  53,  1,   31,  53,  1,   31,  54,  1,   32,  54,  1,   32,  54,  1,   33,  55,  1,   33,
        55,  1,   34,  56,  1,   34,  56,  1,   34,  57,  1,   35,  57,  1,   35,  58,  1,   36,  58,  1,   36,  59,
        1,   37,  59,  1,   37,  60,  1,   38,  60,  1,   38,  61,  1,   39,  61,  1,   39,  61,  1,   40,  62,  1,
        40,  62,  1,   40,  63,  1,   41,  63,  1,   41,  64,  1,   42,  64,  1,   42,  65,  1,   43,  65,  1,   43,
        66,  1,   44,  66,  1,   44,  67,  1,   45,  67,  1,   45,  68,  1,   46,  68,  1,   46,  68,  1,   47,  69,
        1,   47,  69,  1,   47,  70,  1,   48,  70,  1,   48,  71,  1,   49,  71,  1,   49,  72,  1,   50,  72,  1,
        50,  73,  1,   51,  73,  1,   51,  74,  1,   52,  74,  1,   52,  75,  1,   53,  75,  1,   53,  75,  1,   54,
        76,  1,   54,  76,  1,   54,  77,  1,   55,  77,  1,   55,  78,  1,   56,  78,  1,   56,  79,  1,   57,  79,
        1,   57,  80,  1,   58,  80,  1,   58,  81,  1,   59,  81,  1,   59,  82,  1,   60,  82,  1,   60,  82,  1,
        61,  83,  1,   61,  83,  1,   61,  84,  1,   62,  84,  1,   62,  85,  1,   63,  85,  1,   63,  86,  1,   64,
        86,  1,   64,  87,  1,   65,  87,  1,   65,  88,  1,   66,  88,  1,   66,  89,  1,   67,  89,  1,   67,  89,
        1,   67,  90,  1,   68,  90,  1,   68,  91,  1,   69,  91,  1,   69,  92,  1,   70,  92,  1,   70,  93,  1,
        71,  93,  1,   71,  94,  1,   72,  94,  1,   72,  95,  1,   73,  95,  1,   73,  96,  1,   74,  96,  1,   74,
        96,  1,   74,  97,  1,   75,  97,  1,   75,  98,  1,   76,  98,  1,   76,  99,  1,   77,  99,  1,   77,  100,
        1,   78,  100, 1,   78,  101, 1,   79,  101, 1,   79,  102, 1,   80,  102, 1,   80,  103, 1,   81,  103, 1,
        81,  103, 1,   81,  104, 1,   82,  104, 1,   82,  105, 1,   83,  105, 1,   83,  105, 1,   84,  106, 1,   84,
        106, 1,   85,  106, 1,   85,  106, 1,   86,  106, 1,   86,  106, 1,   87,  106, 1,   87,  106, 1,   88,  105,
        1,   89,  104, 1,   90,  103, 1,   91,  102, 1,   93,  99,  93,  29,  189, 1,   36,  72,  1,   32,  72,  1,
        30,  72,  1,   29,  72,  1,   28,  72,  1,   28,  72,  1,   27,  72,  1,   27,  72,  1,   26,  72,  1,   26,
        72,  1,   26,  72,  1,   26,  72,  1,   26,  72,  1,   27,  72,  1,   27,  72,  1,   28,  72,  1,   28,  72,
        1,   29,  72,  1,   31,  72,  1,   33,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   33,  72,  1,   31,
        72,  1,   29,  72,  1,   29,  72,  1,   28,  72,  1,   27,  72,  1,   27,  72,  1,   27,  72,  1,   27,  72,
        1,   26,  72,  1,   27,  72,  1,   27,  72,  1,   27,  72,  1,   27,  72,  1,   28,  72,  1,   28,  72,  1,
        29,  72,  1,   30,  72,  1,   32,  72,  1,   35,  72,  94,  24,  90,  1,   61,  63,  1,   60,  64,  1,   59,
        65,  1,   58,  65,  1,   57,  66,  1,   57,  67,  1,   56,  68,  1,   55,  69,  1,   54,  70,  1,   53,  70,
        1,   52,  71,  1,   52,  72,  1,   51,  73,  1,   50,  74,  1,   49,  75,  1,   48,  75,  1,   47,  76,  1,
        47,  77,  1,   46,  78,  1,   45,  79,  1,   44,  80,  1,   43,  80,  1,   42,  81,  1,   42,  82,  1,   41,
        83,  1,   40,  84,  1,   39,  85,  1,   38,  85,  1,   38,  86,  1,   37,  87,  1,   36,  88,  1,   35,  89,
        2,   34,  61,  63,  90,  2,   33,  60,  64,  90,  2,   33,  59,  65,  91,  2,   32,  58,  65,  92,  2,   31,
        57,  66,  93,  2,   30,  57,  67,  94,  2,   29,  56,  68,  95,  2,   28,  55,  69,  95,  2,   28,  54,  70,
        96,  2,   27,  53,  70,  97,  2,   26,  52,  71,  98,  2,   25,  52,  72,  99,  2,   24,  51,  73,  100, 2,
        23,  50,  74,  100, 2,   23,  49,  75,  101, 2,   22,  48,  76,  102, 2,   21,  47,  76,  103, 2,   20,  47,
        77,  104, 2,   20,  46,  78,  104, 2,   19,  45,  79,  105, 2,   19,  44,  80,  106, 2,   18,  43,  81,  106,
        2,   18,  43,  81,  106, 2,   18,  42,  82,  106, 2,   18,  41,  83,  106, 2,   18,  40,  84,  106, 2,   18,
        39,  85,  106, 2,   18,  38,  86,  106, 2,   19,  37,  87,  105, 2,   20,  37,  87,  104, 2,   20,  36,  88,
        104, 2,   21,  35,  89,  103, 2,   23,  33,  91,  101, 2,   24,  32,  92,  100, 95,  198, 218, 1,   1,   124,
        1,   0,   127, 1,   0,   128, 1,   0,   129, 1,   0,   130, 1,   0,   131, 1,   0,   131, 1,   0,   131, 1,
        0,   132, 1,   0,   132, 1,   0,   132, 1,   0,   132, 1,   0,   131, 1,   0,   131, 1,   0,   131, 1,   0,
        130, 1,   0,   129, 1,   0,   128, 1,   0,   127, 1,   1,   124, 96,  22,  56,  1,   47,  51,  1,   45,  53,
        1,   44,  55,  1,   44,  56,  1,   43,  57,  1,   43,  59,  1,   43,  60,  1,   43,  61,  1,   43,  62,  1,
        44,  64,  1,   44,  65,  1,   45,  66,  1,   46,  67,  1,   48,  68,  1,   49,  70,  1,   50,  71,  1,   51,
        72,  1,   53,  73,  1,   54,  75,  1,   55,  76,  1,   56,  77,  1,   58,  78,  1,   59,  79,  1,   60,  80,
        1,   61,  81,  1,   62,  81,  1,   64,  81,  1,   65,  81,  1,   66,  81,  1,   67,  81,  1,   69,  80,  1,
        70,  80,  1,   72,  78,  1,   74,  76,  97,  65,  161, 1,   50,  71,  1,   44,  76,  1,   39,  80,  1,   35,
        82,  1,   31,  85,  1,   27,  86,  1,   25,  88,  1,   23,  90,  1,   22,  91,  1,   22,  92,  1,   21,  93,
        1,   21,  94,  1,   20,  95,  1,   20,  95,  1,   20,  96,  1,   20,  97,  1,   21,  97,  1,   21,  98,  1,
        21,  98,  1,   21,  99,  2,   22,  55,  69,  99,  2,   23,  48,  74,  99,  2,   24,  43,  76,  100, 2,   25,
        38,  77,  100, 2,   28,  34,  78,  100, 1,   79,  100, 1,   79,  100, 1,   80,  100, 1,   80,  100, 1,   80,
        101, 1,   80,  101, 1,   80,  101, 1,   80,  101, 2,   54,  65,  80,  101, 2,   45,  75,  80,  101, 1,   41,
        101, 1,   37,  101, 1,   35,  101, 1,   32,  101, 1,   30,  101, 1,   28,  101, 1,   27,  101, 1,   26,  101,
        1,   24,  101, 1,   23,  101, 1,   22,  101, 1,   20,  101, 1,   19,  101, 1,   19,  101, 1,   18,  101, 1,
        17,  101, 1,   16,  101, 1,   15,  101, 1,   15,  101, 2,   14,  49,  71,  101, 2,   13,  45,  77,  101, 2,
        13,  42,  80,  101, 2,   12,  40,  80,  101, 2,   12,  39,  80,  101, 2,   12,  37,  80,  101, 2,   11,  36,
        80,  101, 2,   11,  34,  80,  101, 2,   11,  33,  80,  101, 2,   10,  32,  80,  101, 2,   10,  32,  80,  101,
        2,   10,  31,  79,  101, 2,   10,  31,  77,  101, 2,   10,  31,  75,  101, 2,   10,  31,  74,  101, 2,   10,
        32,  71,  101, 2,   10,  32,  69,  101, 2,   11,  33,  66,  101, 2,   11,  35,  64,  109, 2,   11,  36,  60,
        113, 2,   11,  40,  56,  114, 1,   12,  115, 1,   12,  116, 1,   12,  117, 1,   13,  117, 1,   14,  117, 1,
        14,  117, 1,   15,  118, 1,   16,  118, 1,   17,  118, 1,   18,  117, 1,   19,  117, 1,   20,  117, 1,   21,
        116, 2,   23,  78,  80,  115, 2,   24,  76,  80,  114, 2,   25,  74,  80,  113, 2,   27,  71,  80,  111, 1,
        30,  67,  1,   32,  64,  1,   37,  58,  1,   45,  49,  98,  29,  160, 1,   7,   36,  1,   4,   36,  1,   3,
        36,  1,   2,   36,  1,   1,   36,  1,   0,   36,  1,   0,   36,  1,   0,   36,  1,   0,   36,  1,   0,   36,
        1,   0,   36,  1,   0,   36,  1,   0,   36,  1,   0,   36,  1,   0,   36,  1,   1,   36,  1,   2,   36,  1,
        2,   36,  1,   4,   36,  1,   6,   36,  1,   16,  36,  1,   16,  36,  1,   16,  36,  1,   16,  36,  1,   16,
        36,  1,   16,  36,  1,   16,  36,  1,   16,  36,  1,   16,  36,  1,   16,  36,  1,   16,  36,  1,   16,  36,
        1,   16,  36,  1,   16,  36,  1,   16,  36,  1,   16,  36,  2,   16,  36,  59,  76,  2,   16,  36,  54,  81,
        2,   16,  36,  51,  84,  2,   16,  36,  48,  87,  2,   16,  36,  46,  89,  2,   16,  36,  43,  91,  2,   16,
        36,  42,  93,  2,   16,  36,  40,  94,  2,   16,  36,  38,  96,  1,   16,  97,  1,   16,  99,  1,   16,  100,
        1,   16,  101, 1,   16,  102, 1,   16,  103, 1,   16,  104, 1,   16,  105, 1,   16,  106, 1,   16,  107, 1,
        16,  107, 2,   16,  61,  72,  108, 2,   16,  57,  76,  109, 2,   16,  54,  79,  110, 2,   16,  53,  81,  110,
        2,   16,  51,  83,  111, 2,   16,  49,  84,  111, 2,   16,  48,  86,  112, 2,   16,  47,  87,  112, 2,   16,
        46,  88,  113, 2,   16,  45,  89,  113, 2,   16,  44,  90,  114, 2,   16,  43,  91,  114, 2,   16,  42,  92,
        115, 2,   16,  41,  92,  115, 2,   16,  41,  93,  115, 2,   16,  40,  93,  116, 2,   16,  40,  94,  116, 2,
        16,  39,  95,  116, 2,   16,  39,  95,  116, 2,   16,  38,  95,  116, 2,   16,  38,  96,  117, 2,   16,  38,
        96,  117, 2,   16,  37,  96,  117, 2,   16,  37,  96,  117, 2,   16,  37,  97,  117, 2,   16,  37,  97,  117,
        2,   16,  37,  97,  117, 2,   16,  37,  97,  117, 2,   16,  37,  97,  117, 2,   16,  37,  97,  117, 2,   16,
        37,  97,  117, 2,   16,  37,  97,  117, 2,   16,  37,  97,  117, 2,   16,  37,  97,  117, 2,   16,  37,  97,
        117, 2,   16,  37,  97,  117, 2,   16,  37,  96,  117, 2,   16,  38,  96,  117, 2,   16,  38,  96,  117, 2,
        16,  38,  95,  116, 2,   16,  39,  95,  116, 2,   16,  39,  94,  116, 2,   16,  40,  94,  116, 2,   16,  41,
        93,  115, 2,   16,  41,  92,  115, 2,   16,  42,  92,  115, 2,   16,  43,  91,  114, 2,   16,  44,  90,  114,
        2,   16,  45,  89,  113, 2,   16,  46,  87,  113, 2,   16,  48,  86,  112, 2,   16,  50,  84,  112, 2,   7,
        52,  82,  111, 2,   4,   54,  79,  111, 2,   3,   59,  75,  110, 1,   2,   109, 1,   1,   109, 1,   0,   108,
        1,   0,   107, 1,   0,   106, 1,   0,   105, 1,   0,   104, 1,   0,   103, 1,   0,   102, 1,   0,   101, 1,
        0,   100, 1,   0,   98,  2,   1,   36,  37,  97,  2,   1,   36,  39,  95,  2,   2,   36,  41,  93,  2,   4,
        36,  43,  91,  2,   6,   36,  45,  89,  1,   48,  86,  1,   51,  83,  1,   56,  78,  99,  65,  161, 1,   55,
        75,  1,   50,  81,  1,   46,  84,  2,   43,  88,  97,  105, 2,   41,  90,  96,  107, 2,   39,  92,  94,  108,
        1,   37,  109, 1,   35,  110, 1,   34,  110, 1,   32,  110, 1,   31,  111, 1,   30,  111, 1,   29,  111, 1,
        27,  111, 1,   26,  111, 1,   26,  111, 1,   25,  111, 1,   24,  111, 1,   23,  111, 1,   22,  111, 2,   21,
        58,  72,  111, 2,   21,  54,  77,  111, 2,   20,  51,  80,  111, 2,   19,  49,  82,  111, 2,   19,  47,  84,
        111, 2,   18,  45,  86,  111, 2,   18,  44,  87,  111, 2,   17,  43,  88,  111, 2,   17,  41,  89,  111, 2,
        16,  41,  89,  111, 2,   16,  40,  90,  111, 2,   16,  39,  90,  111, 2,   15,  38,  91,  111, 2,   15,  37,
        91,  111, 2,   15,  37,  91,  111, 2,   14,  36,  91,  110, 2,   14,  36,  92,  110, 2,   14,  35,  92,  110,
        2,   14,  35,  93,  109, 2,   13,  35,  93,  108, 2,   13,  34,  95,  107, 2,   13,  34,  96,  106, 2,   13,
        34,  101, 102, 1,   13,  33,  1,   13,  33,  1,   13,  33,  1,   13,  33,  1,   13,  33,  1,   13,  33,  1,
        13,  33,  1,   13,  33,  1,   13,  33,  1,   13,  33,  1,   13,  33,  1,   13,  33,  1,   13,  33,  1,   13,
        34,  1,   13,  34,  1,   13,  34,  1,   13,  35,  1,   14,  35,  1,   14,  36,  1,   14,  36,  1,   14,  37,
        2,   15,  37,  102, 110, 2,   15,  38,  101, 112, 2,   15,  39,  100, 113, 2,   16,  40,  98,  114, 2,   16,
        41,  97,  115, 2,   17,  43,  96,  115, 2,   17,  44,  94,  116, 2,   18,  46,  92,  116, 2,   18,  49,  90,
        116, 2,   19,  52,  87,  116, 2,   19,  58,  81,  116, 1,   20,  116, 1,   21,  116, 1,   22,  116, 1,   22,
        115, 1,   23,  115, 1,   24,  114, 1,   25,  113, 1,   26,  112, 1,   28,  111, 1,   29,  110, 1,   30,  109,
        1,   32,  107, 1,   33,  106, 1,   35,  104, 1,   37,  102, 1,   39,  100, 1,   41,  97,  1,   44,  94,  1,
        48,  90,  1,   53,  84,  1,   65,  74,  100, 29,  160, 1,   79,  109, 1,   76,  109, 1,   75,  109, 1,   74,
        109, 1,   73,  109, 1,   72,  109, 1,   72,  109, 1,   72,  109, 1,   71,  109, 1,   71,  109, 1,   71,  109,
        1,   71,  109, 1,   72,  109, 1,   72,  109, 1,   72,  109, 1,   73,  109, 1,   74,  109, 1,   75,  109, 1,
        76,  109, 1,   79,  109, 1,   88,  109, 1,   88,  109, 1,   88,  109, 1,   88,  109, 1,   88,  109, 1,   88,
        109, 1,   88,  109, 1,   88,  109, 1,   88,  109, 1,   88,  109, 1,   88,  109, 1,   88,  109, 1,   88,  109,
        1,   88,  109, 1,   88,  109, 1,   88,  109, 2,   49,  66,  88,  109, 2,   44,  71,  88,  109, 2,   41,  74,
        88,  109, 2,   38,  77,  88,  109, 2,   36,  79,  88,  109, 2,   34,  81,  88,  109, 2,   32,  83,  88,  109,
        2,   30,  85,  88,  109, 2,   29,  87,  88,  109, 1,   28,  109, 1,   26,  109, 1,   25,  109, 1,   24,  109,
        1,   23,  109, 1,   22,  109, 1,   21,  109, 1,   20,  109, 1,   19,  109, 1,   18,  109, 1,   18,  109, 2,
        17,  53,  63,  109, 2,   16,  49,  67,  109, 2,   15,  46,  70,  109, 2,   15,  44,  72,  109, 2,   14,  42,
        74,  109, 2,   13,  40,  76,  109, 2,   13,  39,  77,  109, 2,   12,  38,  78,  109, 2,   12,  37,  79,  109,
        2,   12,  36,  80,  109, 2,   11,  35,  81,  109, 2,   11,  34,  82,  109, 2,   10,  33,  83,  109, 2,   10,
        33,  84,  109, 2,   10,  32,  84,  109, 2,   9,   31,  85,  109, 2,   9,   31,  85,  109, 2,   9,   30,  86,
        109, 2,   9,   30,  86,  109, 2,   8,   30,  87,  109, 2,   8,   29,  87,  109, 2,   8,   29,  87,  109, 2,
        8,   29,  87,  109, 2,   8,   28,  88,  109, 2,   8,   28,  88,  109, 2,   8,   28,  88,  109, 2,   8,   28,
        88,  109, 2,   8,   28,  88,  109, 2,   7,   28,  88,  109, 2,   7,   28,  88,  109, 2,   7,   28,  88,  109,
        2,   8,   28,  88,  109, 2,   8,   28,  88,  109, 2,   8,   28,  88,  109, 2,   8,   28,  88,  109, 2,   8,
        28,  88,  109, 2,   8,   29,  87,  109, 2,   8,   29,  87,  109, 2,   8,   29,  87,  109, 2,   8,   30,  87,
        109, 2,   9,   30,  86,  109, 2,   9,   31,  85,  109, 2,   9,   31,  85,  109, 2,   9,   32,  84,  109, 2,
        10,  32,  84,  109, 2,   10,  33,  83,  109, 2,   11,  34,  82,  109, 2,   11,  35,  81,  109, 2,   11,  36,
        80,  109, 2,   12,  38,  78,  109, 2,   12,  39,  77,  109, 2,   13,  41,  75,  109, 2,   13,  43,  73,  117,
        2,   14,  45,  70,  121, 2,   15,  50,  66,  122, 1,   15,  123, 1,   16,  124, 1,   17,  125, 1,   18,  125,
        1,   19,  125, 1,   20,  126, 1,   21,  126, 1,   22,  126, 1,   23,  126, 1,   24,  125, 1,   25,  125, 1,
        27,  125, 1,   28,  124, 2,   30,  86,  88,  123, 2,   32,  84,  88,  122, 2,   33,  82,  88,  121, 2,   36,
        79,  88,  119, 1,   39,  77,  1,   42,  74,  1,   46,  69,  101, 65,  160, 1,   50,  70,  1,   46,  75,  1,
        42,  78,  1,   40,  81,  1,   37,  84,  1,   35,  86,  1,   33,  88,  1,   31,  90,  1,   30,  91,  1,   28,
        93,  1,   27,  94,  1,   26,  96,  1,   24,  97,  1,   23,  98,  1,   22,  99,  1,   21,  100, 1,   20,  101,
        1,   19,  102, 1,   18,  102, 1,   17,  103, 2,   17,  54,  67,  104, 2,   16,  49,  71,  104, 2,   15,  46,
        75,  105, 2,   14,  44,  77,  106, 2,   14,  42,  79,  106, 2,   13,  41,  80,  107, 2,   13,  39,  82,  107,
        2,   12,  38,  83,  108, 2,   12,  37,  84,  108, 2,   11,  36,  85,  109, 2,   11,  35,  86,  109, 2,   11,
        34,  87,  110, 2,   10,  34,  87,  110, 2,   10,  33,  88,  110, 2,   10,  32,  89,  111, 2,   9,   32,  89,
        111, 2,   9,   31,  90,  111, 1,   9,   112, 1,   9,   112, 1,   9,   112, 1,   8,   112, 1,   8,   112, 1,
        8,   113, 1,   8,   113, 1,   8,   113, 1,   8,   113, 1,   8,   113, 1,   8,   113, 1,   8,   113, 1,   8,
        113, 1,   8,   113, 1,   8,   113, 1,   8,   113, 1,   8,   113, 1,   8,   113, 1,   8,   113, 1,   9,   113,
        1,   9,   113, 1,   9,   30,  1,   9,   31,  1,   9,   31,  1,   10,  32,  1,   10,  32,  1,   10,  33,  1,
        11,  34,  1,   11,  35,  1,   11,  36,  1,   12,  37,  2,   12,  38,  102, 104, 2,   13,  40,  96,  108, 2,
        13,  41,  92,  109, 2,   14,  43,  88,  110, 2,   15,  45,  84,  111, 2,   15,  49,  79,  112, 2,   16,  53,
        72,  112, 1,   17,  113, 1,   18,  113, 1,   18,  113, 1,   19,  113, 1,   20,  113, 1,   21,  113, 1,   22,
        113, 1,   23,  112, 1,   24,  111, 1,   26,  111, 1,   27,  110, 1,   28,  108, 1,   30,  106, 1,   32,  104,
        1,   34,  101, 1,   36,  98,  1,   38,  94,  1,   41,  89,  1,   44,  84,  1,   49,  77,  102, 29,  157, 1,
        71,  91,  1,   66,  100, 1,   63,  106, 1,   60,  110, 1,   58,  112, 1,   56,  114, 1,   55,  115, 1,   53,
        116, 1,   52,  116, 1,   51,  117, 1,   50,  117, 1,   49,  117, 1,   48,  117, 1,   47,  117, 1,   47,  117,
        1,   46,  117, 1,   45,  117, 1,   45,  116, 1,   44,  116, 1,   44,  115, 2,   43,  74,  85,  114, 2,   43,
        69,  94,  113, 2,   43,  67,  101, 111, 1,   43,  65,  1,   42,  64,  1,   42,  64,  1,   42,  63,  1,   42,
        63,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,
        1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   30,  95,  1,   25,  101, 1,   23,  103, 1,   21,  104, 1,
        20,  105, 1,   20,  105, 1,   19,  106, 1,   19,  106, 1,   19,  107, 1,   18,  107, 1,   18,  107, 1,   18,
        107, 1,   19,  107, 1,   19,  106, 1,   19,  106, 1,   20,  105, 1,   20,  105, 1,   21,  104, 1,   22,  103,
        1,   24,  101, 1,   29,  97,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,
        42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,
        62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,
        1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,
        42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,
        62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,
        1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,   42,  62,  1,
        24,  96,  1,   21,  99,  1,   20,  101, 1,   18,  102, 1,   18,  103, 1,   17,  103, 1,   17,  104, 1,   16,
        104, 1,   16,  104, 1,   16,  104, 1,   16,  104, 1,   16,  104, 1,   16,  104, 1,   17,  104, 1,   17,  103,
        1,   18,  103, 1,   18,  102, 1,   19,  101, 1,   21,  100, 1,   23,  97,  103, 64,  200, 1,   55,  57,  1,
        46,  66,  1,   41,  70,  2,   38,  73,  84,  110, 2,   36,  76,  84,  115, 2,   34,  78,  84,  117, 2,   32,
        80,  84,  119, 2,   30,  81,  84,  120, 2,   29,  83,  84,  120, 1,   27,  121, 1,   26,  121, 1,   25,  122,
        1,   24,  122, 1,   23,  122, 1,   22,  122, 1,   21,  122, 1,   20,  121, 1,   19,  121, 1,   18,  120, 1,
        17,  120, 1,   16,  119, 2,   16,  50,  63,  118, 2,   15,  46,  66,  116, 2,   14,  44,  68,  111, 2,   14,
        42,  70,  105, 2,   13,  40,  72,  105, 2,   13,  39,  73,  105, 2,   12,  38,  74,  105, 2,   12,  37,  76,
        105, 2,   11,  36,  77,  105, 2,   11,  35,  77,  105, 2,   11,  34,  78,  105, 2,   10,  33,  79,  105, 2,
        10,  33,  80,  105, 2,   10,  32,  80,  105, 2,   9,   31,  81,  105, 2,   9,   31,  81,  105, 2,   9,   30,
        82,  105, 2,   9,   30,  82,  105, 2,   9,   30,  83,  105, 2,   8,   29,  83,  105, 2,   8,   29,  83,  105,
        2,   8,   29,  84,  105, 2,   8,   29,  84,  105, 2,   8,   28,  84,  105, 2,   8,   28,  84,  105, 2,   8,
        28,  84,  105, 2,   8,   28,  84,  105, 2,   8,   28,  84,  105, 2,   8,   28,  84,  105, 2,   8,   28,  84,
        105, 2,   8,   28,  84,  105, 2,   8,   28,  84,  105, 2,   8,   29,  84,  105, 2,   8,   29,  84,  105, 2,
        8,   29,  83,  105, 2,   8,   29,  83,  105, 2,   9,   30,  83,  105, 2,   9,   30,  82,  105, 2,   9,   31,
        82,  105, 2,   9,   31,  81,  105, 2,   9,   32,  81,  105, 2,   10,  32,  80,  105, 2,   10,  33,  80,  105,
        2,   10,  34,  79,  105, 2,   11,  34,  78,  105, 2,   11,  35,  77,  105, 2,   12,  36,  76,  105, 2,   12,
        37,  75,  105, 2,   12,  38,  74,  105, 2,   13,  40,  73,  105, 2,   13,  41,  71,  105, 2,   14,  43,  69,
        105, 2,   15,  45,  68,  105, 2,   15,  48,  65,  105, 2,   16,  51,  61,  105, 1,   17,  105, 1,   17,  105,
        1,   18,  105, 1,   19,  105, 1,   20,  105, 1,   21,  105, 1,   22,  105, 1,   23,  105, 1,   24,  105, 1,
        25,  105, 1,   26,  105, 1,   28,  105, 2,   29,  83,  84,  105, 2,   31,  81,  84,  105, 2,   32,  79,  84,
        105, 2,   34,  77,  84,  105, 2,   37,  75,  84,  105, 2,   39,  72,  84,  105, 2,   42,  69,  84,  105, 2,
        47,  65,  84,  105, 1,   84,  105, 1,   84,  105, 1,   84,  105, 1,   84,  105, 1,   84,  105, 1,   84,  104,
        1,   84,  104, 1,   83,  104, 1,   83,  104, 1,   83,  104, 1,   82,  104, 1,   82,  104, 1,   81,  103, 1,
        80,  103, 1,   79,  103, 1,   78,  102, 1,   76,  102, 1,   74,  101, 1,   71,  101, 1,   39,  101, 1,   35,
        100, 1,   33,  99,  1,   32,  99,  1,   31,  98,  1,   30,  97,  1,   30,  96,  1,   29,  95,  1,   29,  95,
        1,   29,  93,  1,   29,  92,  1,   29,  91,  1,   29,  90,  1,   29,  88,  1,   30,  87,  1,   30,  85,  1,
        31,  83,  1,   32,  81,  1,   33,  78,  1,   35,  75,  1,   40,  67,  104, 29,  157, 1,   12,  41,  1,   9,
        41,  1,   7,   41,  1,   6,   41,  1,   5,   41,  1,   5,   41,  1,   4,   41,  1,   4,   41,  1,   4,   41,
        1,   4,   41,  1,   4,   41,  1,   4,   41,  1,   4,   41,  1,   4,   41,  1,   5,   41,  1,   5,   41,  1,
        6,   41,  1,   7,   41,  1,   9,   41,  1,   11,  41,  1,   21,  41,  1,   21,  41,  1,   21,  41,  1,   21,
        41,  1,   21,  41,  1,   21,  41,  1,   21,  41,  1,   21,  41,  1,   21,  41,  1,   21,  41,  1,   21,  41,
        1,   21,  41,  1,   21,  41,  1,   21,  41,  1,   21,  41,  1,   21,  41,  2,   21,  41,  59,  76,  2,   21,
        41,  54,  81,  2,   21,  41,  51,  84,  2,   21,  41,  49,  86,  2,   21,  41,  46,  88,  2,   21,  41,  44,
        90,  2,   21,  41,  42,  91,  1,   21,  93,  1,   21,  94,  1,   21,  95,  1,   21,  96,  1,   21,  97,  1,
        21,  98,  1,   21,  99,  1,   21,  99,  1,   21,  100, 1,   21,  101, 1,   21,  101, 1,   21,  102, 1,   21,
        102, 2,   21,  62,  72,  102, 2,   21,  58,  76,  103, 2,   21,  56,  78,  103, 2,   21,  54,  80,  104, 2,
        21,  52,  81,  104, 2,   21,  51,  82,  104, 2,   21,  49,  83,  104, 2,   21,  48,  83,  104, 2,   21,  47,
        83,  105, 2,   21,  46,  84,  105, 2,   21,  45,  84,  105, 2,   21,  44,  84,  105, 2,   21,  43,  84,  105,
        2,   21,  42,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,
        41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,
        105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,
        21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,
        84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105,
        2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,
        41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,
        105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,
        21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   21,  41,  84,  105, 2,   13,  48,
        77,  112, 2,   11,  51,  74,  115, 2,   9,   53,  73,  116, 2,   8,   54,  72,  117, 2,   8,   54,  71,  118,
        2,   7,   55,  71,  119, 2,   7,   55,  70,  119, 2,   6,   56,  70,  119, 2,   6,   56,  70,  119, 2,   6,
        56,  70,  120, 2,   6,   56,  70,  120, 2,   6,   56,  70,  120, 2,   6,   56,  70,  119, 2,   6,   56,  70,
        119, 2,   7,   55,  71,  119, 2,   7,   55,  71,  118, 2,   8,   54,  72,  117, 2,   9,   53,  73,  116, 2,
        11,  51,  74,  115, 2,   13,  49,  77,  113, 105, 29,  157, 1,   45,  69,  1,   45,  69,  1,   45,  69,  1,
        45,  69,  1,   45,  69,  1,   45,  69,  1,   45,  69,  1,   45,  69,  1,   45,  69,  1,   45,  69,  1,   45,
        69,  1,   45,  69,  1,   45,  69,  1,   45,  69,  1,   45,  69,  1,   45,  69,  1,   45,  69,  1,   45,  69,
        1,   45,  69,  1,   45,  69,  1,   45,  69,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   1,   33,  72,  1,   28,  72,  1,   26,  72,  1,   25,  72,  1,   24,  72,  1,   23,  72,
        1,   23,  72,  1,   22,  72,  1,   22,  72,  1,   22,  72,  1,   22,  72,  1,   22,  72,  1,   22,  72,  1,
        22,  72,  1,   23,  72,  1,   23,  72,  1,   24,  72,  1,   25,  72,  1,   26,  72,  1,   28,  72,  1,   32,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   22,  102, 1,   19,  105,
        1,   18,  106, 1,   16,  108, 1,   16,  108, 1,   15,  109, 1,   15,  109, 1,   14,  110, 1,   14,  110, 1,
        14,  110, 1,   14,  110, 1,   14,  110, 1,   14,  110, 1,   15,  109, 1,   15,  109, 1,   15,  109, 1,   16,
        108, 1,   17,  107, 1,   19,  105, 1,   21,  103, 106, 29,  200, 1,   58,  82,  1,   58,  82,  1,   58,  82,
        1,   58,  82,  1,   58,  82,  1,   58,  82,  1,   58,  82,  1,   58,  82,  1,   58,  82,  1,   58,  82,  1,
        58,  82,  1,   58,  82,  1,   58,  82,  1,   58,  82,  1,   58,  82,  1,   58,  82,  1,   58,  82,  1,   58,
        82,  1,   58,  82,  1,   58,  82,  1,   58,  82,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   1,   31,  94,  1,   25,  94,  1,   23,  94,  1,   22,  94,  1,   21,  94,  1,   21,
        94,  1,   20,  94,  1,   20,  94,  1,   19,  94,  1,   19,  94,  1,   19,  94,  1,   19,  94,  1,   19,  94,
        1,   20,  94,  1,   20,  94,  1,   21,  94,  1,   21,  94,  1,   22,  94,  1,   23,  94,  1,   25,  94,  1,
        29,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,
        94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,
        1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,
        74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,
        94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,
        1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,
        74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,
        94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,
        1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,
        74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,
        94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   74,  94,  1,   73,  94,
        1,   73,  94,  1,   72,  94,  1,   72,  94,  1,   71,  93,  1,   71,  93,  1,   70,  93,  1,   69,  92,  1,
        67,  92,  1,   66,  92,  1,   64,  91,  1,   61,  91,  1,   29,  90,  1,   25,  90,  1,   23,  89,  1,   21,
        88,  1,   21,  88,  1,   20,  87,  1,   19,  86,  1,   19,  85,  1,   19,  84,  1,   19,  83,  1,   19,  82,
        1,   19,  81,  1,   19,  80,  1,   19,  78,  1,   19,  77,  1,   20,  75,  1,   21,  73,  1,   22,  71,  1,
        23,  68,  1,   25,  65,  1,   30,  57,  107, 29,  157, 1,   16,  45,  1,   13,  45,  1,   12,  45,  1,   11,
        45,  1,   10,  45,  1,   9,   45,  1,   9,   45,  1,   8,   45,  1,   8,   45,  1,   8,   45,  1,   8,   45,
        1,   8,   45,  1,   8,   45,  1,   9,   45,  1,   9,   45,  1,   10,  45,  1,   10,  45,  1,   11,  45,  1,
        13,  45,  1,   15,  45,  1,   25,  45,  1,   25,  45,  1,   25,  45,  1,   25,  45,  1,   25,  45,  1,   25,
        45,  1,   25,  45,  1,   25,  45,  1,   25,  45,  1,   25,  45,  1,   25,  45,  1,   25,  45,  1,   25,  45,
        1,   25,  45,  1,   25,  45,  1,   25,  45,  1,   25,  45,  1,   25,  45,  2,   25,  45,  70,  98,  2,   25,
        45,  65,  103, 2,   25,  45,  63,  105, 2,   25,  45,  62,  106, 2,   25,  45,  61,  107, 2,   25,  45,  60,
        108, 2,   25,  45,  60,  108, 2,   25,  45,  59,  108, 2,   25,  45,  59,  109, 2,   25,  45,  59,  109, 2,
        25,  45,  59,  109, 2,   25,  45,  59,  109, 2,   25,  45,  59,  109, 2,   25,  45,  59,  108, 2,   25,  45,
        60,  108, 2,   25,  45,  60,  108, 2,   25,  45,  61,  107, 2,   25,  45,  61,  106, 2,   25,  45,  60,  105,
        2,   25,  45,  59,  103, 2,   25,  45,  58,  99,  2,   25,  45,  56,  89,  2,   25,  45,  55,  88,  2,   25,
        45,  54,  86,  2,   25,  45,  53,  85,  2,   25,  45,  51,  84,  2,   25,  45,  50,  83,  2,   25,  45,  49,
        81,  2,   25,  45,  48,  80,  2,   25,  45,  46,  79,  1,   25,  78,  1,   25,  76,  1,   25,  75,  1,   25,
        74,  1,   25,  73,  1,   25,  72,  1,   25,  70,  1,   25,  69,  1,   25,  68,  1,   25,  67,  1,   25,  65,
        1,   25,  66,  1,   25,  67,  1,   25,  68,  1,   25,  69,  1,   25,  71,  1,   25,  72,  1,   25,  73,  1,
        25,  74,  1,   25,  75,  1,   25,  76,  1,   25,  77,  1,   25,  79,  1,   25,  80,  2,   25,  48,  50,  81,
        2,   25,  47,  52,  82,  2,   25,  45,  53,  83,  2,   25,  45,  54,  84,  2,   25,  45,  55,  85,  2,   25,
        45,  56,  86,  2,   25,  45,  57,  88,  2,   25,  45,  58,  89,  2,   25,  45,  59,  90,  2,   25,  45,  61,
        91,  2,   25,  45,  62,  92,  2,   25,  45,  63,  93,  2,   25,  45,  64,  94,  2,   25,  45,  65,  95,  2,
        25,  45,  66,  97,  2,   25,  45,  67,  98,  2,   16,  45,  68,  109, 2,   13,  45,  70,  113, 2,   12,  45,
        71,  114, 2,   11,  45,  70,  115, 2,   10,  45,  69,  116, 2,   9,   45,  69,  117, 2,   9,   45,  68,  117,
        2,   8,   45,  68,  117, 2,   8,   45,  67,  118, 2,   8,   45,  67,  118, 2,   8,   45,  67,  118, 2,   8,
        45,  67,  118, 2,   8,   45,  68,  117, 2,   9,   45,  68,  117, 2,   9,   45,  68,  117, 2,   10,  45,  69,
        116, 2,   10,  45,  70,  115, 2,   11,  45,  70,  115, 2,   13,  45,  72,  113, 2,   15,  45,  74,  111, 108,
        29,  157, 1,   30,  72,  1,   27,  72,  1,   26,  72,  1,   24,  72,  1,   24,  72,  1,   23,  72,  1,   23,
        72,  1,   22,  72,  1,   22,  72,  1,   22,  72,  1,   22,  72,  1,   22,  72,  1,   22,  72,  1,   23,  72,
        1,   23,  72,  1,   24,  72,  1,   24,  72,  1,   25,  72,  1,   27,  72,  1,   29,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   22,  102, 1,
        19,  105, 1,   18,  107, 1,   17,  108, 1,   16,  108, 1,   15,  109, 1,   15,  109, 1,   14,  110, 1,   14,
        110, 1,   14,  110, 1,   14,  110, 1,   14,  110, 1,   14,  110, 1,   15,  110, 1,   15,  109, 1,   16,  109,
        1,   16,  108, 1,   17,  107, 1,   19,  105, 1,   21,  103, 109, 65,  157, 2,   43,  54,  82,  95,  2,   40,
        57,  78,  99,  3,   9,   31,  38,  59,  76,  101, 3,   4,   31,  36,  60,  74,  103, 3,   2,   31,  35,  62,
        72,  104, 3,   1,   31,  34,  63,  70,  106, 3,   0,   31,  33,  64,  69,  107, 2,   0,   65,  67,  108, 1,
        0,   109, 1,   0,   110, 1,   0,   111, 1,   0,   111, 1,   0,   112, 1,   0,   112, 1,   0,   113, 1,   0,
        113, 1,   0,   114, 1,   0,   114, 1,   0,   114, 1,   1,   115, 3,   2,   43,  49,  85,  92,  115, 3,   4,
        41,  51,  83,  93,  115, 3,   7,   39,  52,  81,  94,  115, 3,   10,  37,  52,  80,  94,  115, 3,   10,  36,
        53,  79,  95,  115, 3,   10,  35,  53,  78,  95,  115, 3,   10,  34,  53,  77,  95,  116, 3,   10,  34,  53,
        76,  95,  116, 3,   10,  33,  53,  75,  95,  116, 3,   10,  32,  53,  75,  95,  116, 3,   10,  31,  53,  74,
        95,  116, 3,   10,  31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,
        116, 3,   10,  31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116,
        3,   10,  31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116, 3,
        10,  31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116, 3,   10,
        31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116, 3,   10,  31,
        53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116, 3,   10,  31,  53,
        73,  95,  116, 3,   10,  31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116, 3,   10,  31,  53,  73,
        95,  116, 3,   10,  31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,
        116, 3,   10,  31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116,
        3,   10,  31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116, 3,
        10,  31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116, 3,   10,
        31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116, 3,   10,  31,
        53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116, 3,   10,  31,  53,  73,  95,  116, 3,   10,  31,  53,
        73,  95,  116, 3,   6,   35,  53,  78,  95,  120, 3,   3,   38,  53,  81,  95,  123, 3,   1,   40,  53,  82,
        95,  125, 3,   0,   41,  53,  83,  95,  125, 3,   0,   42,  53,  84,  95,  126, 3,   0,   42,  53,  85,  95,
        127, 3,   0,   43,  53,  85,  95,  127, 3,   0,   43,  53,  85,  95,  128, 3,   0,   43,  53,  86,  95,  128,
        3,   0,   43,  53,  86,  95,  128, 3,   0,   43,  53,  86,  95,  128, 3,   0,   43,  53,  86,  95,  128, 3,
        0,   43,  53,  86,  95,  128, 3,   0,   43,  53,  85,  95,  128, 3,   0,   42,  53,  85,  95,  127, 3,   0,
        42,  53,  84,  95,  126, 3,   0,   41,  53,  84,  95,  126, 3,   1,   40,  53,  83,  95,  125, 3,   3,   39,
        53,  81,  95,  123, 3,   5,   36,  53,  79,  95,  121, 110, 65,  157, 1,   60,  77,  1,   56,  81,  2,   20,
        42,  53,  84,  2,   15,  42,  51,  86,  2,   13,  42,  49,  88,  2,   12,  42,  47,  90,  2,   11,  42,  45,
        92,  2,   10,  42,  44,  93,  2,   9,   42,  43,  94,  1,   9,   95,  1,   9,   96,  1,   9,   97,  1,   9,
        98,  1,   9,   99,  1,   9,   99,  1,   9,   100, 1,   9,   101, 1,   10,  101, 1,   11,  102, 1,   11,  102,
        2,   13,  62,  73,  103, 2,   15,  58,  77,  103, 2,   18,  56,  79,  103, 2,   21,  54,  80,  104, 2,   21,
        52,  82,  104, 2,   21,  51,  83,  104, 2,   21,  50,  83,  104, 2,   21,  48,  84,  105, 2,   21,  47,  84,
        105, 2,   21,  46,  84,  105, 2,   21,  45,  85,  105, 2,   21,  44,  85,  105, 2,   21,  43,  85,  105, 2,
        21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,
        85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105,
        2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,
        42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,
        105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,
        21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,
        85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105,
        2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,
        42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   14,  49,  80,
        110, 2,   11,  52,  77,  113, 2,   10,  53,  76,  114, 2,   9,   54,  75,  115, 2,   8,   55,  74,  116, 2,
        7,   55,  73,  117, 2,   7,   56,  73,  117, 2,   7,   56,  73,  117, 2,   6,   56,  72,  117, 2,   6,   56,
        72,  118, 2,   6,   56,  72,  118, 2,   6,   56,  72,  118, 2,   7,   56,  73,  117, 2,   7,   56,  73,  117,
        2,   7,   56,  73,  117, 2,   8,   55,  74,  116, 2,   9,   54,  75,  115, 2,   10,  53,  75,  114, 2,   11,
        52,  77,  113, 2,   13,  49,  79,  111, 111, 65,  160, 1,   54,  71,  1,   49,  76,  1,   45,  79,  1,   42,
        82,  1,   40,  84,  1,   38,  86,  1,   36,  89,  1,   34,  90,  1,   33,  92,  1,   31,  93,  1,   29,  95,
        1,   28,  96,  1,   27,  97,  1,   26,  99,  1,   25,  100, 1,   24,  101, 1,   23,  102, 1,   22,  103, 1,
        21,  104, 1,   20,  104, 2,   19,  56,  68,  105, 2,   19,  52,  72,  106, 2,   18,  49,  76,  106, 2,   17,
        47,  78,  107, 2,   17,  45,  79,  108, 2,   16,  43,  81,  108, 2,   16,  42,  83,  109, 2,   15,  41,  84,
        109, 2,   15,  40,  85,  110, 2,   14,  39,  86,  110, 2,   14,  38,  87,  111, 2,   13,  37,  88,  111, 2,
        13,  36,  88,  112, 2,   13,  35,  89,  112, 2,   12,  35,  90,  112, 2,   12,  34,  91,  113, 2,   12,  33,
        91,  113, 2,   11,  33,  92,  113, 2,   11,  32,  92,  113, 2,   11,  32,  93,  114, 2,   11,  32,  93,  114,
        2,   11,  31,  93,  114, 2,   10,  31,  94,  114, 2,   10,  31,  94,  114, 2,   10,  31,  94,  114, 2,   10,
        30,  94,  115, 2,   10,  30,  94,  115, 2,   10,  30,  94,  115, 2,   10,  30,  94,  115, 2,   10,  30,  94,
        115, 2,   10,  30,  94,  115, 2,   10,  30,  94,  115, 2,   10,  30,  94,  115, 2,   10,  31,  94,  114, 2,
        10,  31,  94,  114, 2,   10,  31,  93,  114, 2,   11,  32,  93,  114, 2,   11,  32,  93,  114, 2,   11,  32,
        92,  114, 2,   11,  33,  91,  113, 2,   11,  34,  91,  113, 2,   12,  34,  90,  113, 2,   12,  35,  89,  112,
        2,   13,  36,  89,  112, 2,   13,  37,  88,  112, 2,   13,  38,  87,  111, 2,   14,  39,  86,  111, 2,   14,
        40,  85,  110, 2,   15,  41,  83,  110, 2,   15,  43,  82,  109, 2,   16,  44,  80,  109, 2,   16,  46,  78,
        108, 2,   17,  48,  76,  108, 2,   18,  51,  74,  107, 2,   18,  55,  70,  106, 1,   19,  106, 1,   20,  105,
        1,   21,  104, 1,   22,  103, 1,   23,  102, 1,   24,  101, 1,   25,  100, 1,   26,  99,  1,   27,  98,  1,
        28,  96,  1,   30,  95,  1,   31,  93,  1,   33,  92,  1,   35,  90,  1,   37,  88,  1,   39,  86,  1,   41,
        83,  1,   44,  80,  1,   47,  77,  1,   52,  73,  112, 65,  200, 1,   59,  76,  1,   54,  81,  2,   11,  37,
        51,  84,  2,   6,   37,  48,  87,  2,   4,   37,  46,  89,  2,   2,   37,  43,  91,  2,   2,   37,  42,  93,
        2,   1,   37,  40,  95,  2,   0,   37,  38,  96,  1,   0,   98,  1,   0,   99,  1,   0,   100, 1,   0,   101,
        1,   0,   102, 1,   0,   103, 1,   0,   104, 1,   0,   105, 1,   1,   106, 1,   1,   107, 1,   2,   108, 2,
        4,   62,  73,  109, 2,   6,   57,  77,  109, 2,   10,  54,  80,  110, 2,   17,  52,  82,  111, 2,   17,  51,
        83,  111, 2,   17,  49,  85,  112, 2,   17,  48,  86,  112, 2,   17,  47,  87,  113, 2,   17,  46,  88,  113,
        2,   17,  45,  90,  114, 2,   17,  44,  90,  114, 2,   17,  43,  91,  115, 2,   17,  42,  92,  115, 2,   17,
        41,  93,  115, 2,   17,  41,  93,  116, 2,   17,  40,  94,  116, 2,   17,  40,  94,  116, 2,   17,  39,  95,
        116, 2,   17,  39,  95,  117, 2,   17,  38,  96,  117, 2,   17,  38,  96,  117, 2,   17,  38,  96,  117, 2,
        17,  37,  97,  117, 2,   17,  37,  97,  117, 2,   17,  37,  97,  117, 2,   17,  37,  97,  117, 2,   17,  37,
        97,  117, 2,   17,  37,  97,  117, 2,   17,  37,  97,  117, 2,   17,  37,  97,  117, 2,   17,  37,  96,  117,
        2,   17,  38,  96,  117, 2,   17,  38,  96,  117, 2,   17,  39,  95,  117, 2,   17,  39,  95,  117, 2,   17,
        40,  94,  116, 2,   17,  40,  93,  116, 2,   17,  41,  93,  116, 2,   17,  42,  92,  115, 2,   17,  43,  91,
        115, 2,   17,  44,  90,  115, 2,   17,  45,  89,  114, 2,   17,  46,  88,  114, 2,   17,  47,  87,  113, 2,
        17,  49,  85,  113, 2,   17,  50,  84,  112, 2,   17,  52,  82,  112, 2,   17,  54,  80,  111, 2,   17,  57,
        77,  111, 2,   17,  62,  72,  110, 1,   17,  109, 1,   17,  108, 1,   17,  107, 1,   17,  106, 1,   17,  106,
        1,   17,  105, 1,   17,  103, 1,   17,  102, 1,   17,  101, 1,   17,  100, 1,   17,  98,  2,   17,  37,  38,
        97,  2,   17,  37,  40,  95,  2,   17,  37,  41,  93,  2,   17,  37,  43,  92,  2,   17,  37,  46,  89,  2,
        17,  37,  48,  87,  2,   17,  37,  51,  85,  2,   17,  37,  55,  80,  2,   17,  37,  59,  76,  1,   17,  37,
        1,   17,  37,  1,   17,  37,  1,   17,  37,  1,   17,  37,  1,   17,  37,  1,   17,  37,  1,   17,  37,  1,
        17,  37,  1,   17,  37,  1,   17,  37,  1,   17,  37,  1,   17,  37,  1,   17,  37,  1,   17,  37,  1,   17,
        37,  1,   17,  37,  1,   17,  37,  1,   17,  37,  1,   17,  37,  1,   17,  37,  1,   17,  37,  1,   17,  37,
        1,   17,  37,  1,   10,  52,  1,   6,   57,  1,   4,   59,  1,   2,   60,  1,   1,   61,  1,   1,   61,  1,
        0,   62,  1,   0,   62,  1,   0,   62,  1,   0,   63,  1,   0,   63,  1,   0,   63,  1,   0,   62,  1,   0,
        62,  1,   0,   62,  1,   1,   61,  1,   1,   61,  1,   2,   60,  1,   4,   59,  1,   6,   56,  1,   11,  51,
        113, 65,  200, 1,   49,  67,  1,   45,  72,  2,   41,  75,  89,  115, 2,   39,  78,  89,  120, 2,   37,  80,
        89,  122, 2,   34,  82,  89,  123, 2,   33,  84,  89,  124, 2,   31,  86,  89,  125, 2,   30,  87,  89,  125,
        1,   28,  126, 1,   27,  126, 1,   26,  126, 1,   25,  126, 1,   23,  126, 1,   22,  126, 1,   21,  126, 1,
        21,  125, 1,   20,  125, 1,   19,  124, 1,   18,  123, 2,   17,  54,  65,  122, 2,   16,  49,  69,  120, 2,
        16,  46,  71,  116, 2,   15,  44,  73,  109, 2,   14,  43,  75,  109, 2,   14,  41,  77,  109, 2,   13,  39,
        78,  109, 2,   13,  38,  79,  109, 2,   12,  38,  80,  109, 2,   12,  37,  81,  109, 2,   11,  36,  82,  109,
        2,   11,  35,  83,  109, 2,   11,  34,  84,  109, 2,   10,  33,  84,  109, 2,   10,  33,  85,  109, 2,   10,
        32,  86,  109, 2,   10,  31,  86,  109, 2,   9,   31,  86,  109, 2,   9,   30,  87,  109, 2,   9,   30,  87,
        109, 2,   9,   30,  88,  109, 2,   9,   29,  88,  109, 2,   9,   29,  88,  109, 2,   9,   29,  89,  109, 2,
        8,   29,  89,  109, 2,   8,   29,  89,  109, 2,   8,   29,  89,  109, 2,   8,   29,  89,  109, 2,   8,   29,
        89,  109, 2,   9,   29,  88,  109, 2,   9,   29,  88,  109, 2,   9,   30,  88,  109, 2,   9,   30,  88,  109,
        2,   9,   30,  87,  109, 2,   9,   31,  87,  109, 2,   9,   32,  86,  109, 2,   10,  32,  85,  109, 2,   10,
        33,  85,  109, 2,   10,  34,  84,  109, 2,   11,  35,  83,  109, 2,   11,  36,  82,  109, 2,   11,  37,  81,
        109, 2,   12,  38,  80,  109, 2,   12,  39,  79,  109, 2,   13,  40,  77,  109, 2,   13,  42,  76,  109, 2,
        14,  44,  74,  109, 2,   15,  46,  72,  109, 2,   15,  49,  69,  109, 2,   16,  54,  63,  109, 1,   17,  109,
        1,   17,  109, 1,   18,  109, 1,   19,  109, 1,   20,  109, 1,   21,  109, 1,   22,  109, 1,   24,  109, 1,
        25,  109, 1,   26,  109, 1,   28,  109, 2,   29,  88,  89,  109, 2,   31,  86,  89,  109, 2,   32,  84,  89,
        109, 2,   34,  82,  89,  109, 2,   36,  80,  89,  109, 2,   39,  77,  89,  109, 2,   41,  75,  89,  109, 2,
        45,  71,  89,  109, 2,   49,  67,  89,  109, 1,   89,  109, 1,   89,  109, 1,   89,  109, 1,   89,  109, 1,
        89,  109, 1,   89,  109, 1,   89,  109, 1,   89,  109, 1,   89,  109, 1,   89,  109, 1,   89,  109, 1,   89,
        109, 1,   89,  109, 1,   89,  109, 1,   89,  109, 1,   89,  109, 1,   89,  109, 1,   89,  109, 1,   89,  109,
        1,   89,  109, 1,   89,  109, 1,   89,  109, 1,   89,  109, 1,   89,  109, 1,   73,  116, 1,   69,  120, 1,
        67,  122, 1,   66,  123, 1,   65,  124, 1,   64,  125, 1,   64,  125, 1,   64,  126, 1,   63,  126, 1,   63,
        126, 1,   63,  126, 1,   63,  126, 1,   63,  126, 1,   64,  126, 1,   64,  125, 1,   65,  125, 1,   65,  124,
        1,   66,  123, 1,   67,  122, 1,   69,  120, 1,   74,  115, 114, 65,  157, 1,   85,  98,  1,   81,  101, 2,
        28,  58,  79,  103, 2,   23,  58,  77,  106, 2,   21,  58,  74,  107, 2,   20,  58,  73,  109, 2,   19,  58,
        71,  110, 2,   18,  58,  69,  111, 2,   17,  58,  68,  113, 2,   17,  58,  66,  114, 2,   17,  58,  65,  115,
        2,   17,  58,  63,  115, 2,   17,  58,  62,  116, 2,   17,  58,  60,  116, 2,   17,  58,  59,  117, 1,   17,
        117, 1,   17,  117, 1,   18,  117, 1,   19,  117, 1,   19,  117, 2,   21,  89,  94,  117, 2,   23,  86,  96,
        116, 2,   27,  84,  98,  116, 2,   38,  82,  99,  115, 2,   38,  80,  100, 114, 2,   38,  79,  101, 113, 2,
        38,  77,  103, 111, 2,   38,  76,  107, 108, 1,   38,  74,  1,   38,  73,  1,   38,  72,  1,   38,  70,  1,
        38,  69,  1,   38,  68,  1,   38,  66,  1,   38,  65,  1,   38,  64,  1,   38,  63,  1,   38,  61,  1,   38,
        60,  1,   38,  59,  1,   38,  58,  1,   38,  58,  1,   38,  58,  1,   38,  58,  1,   38,  58,  1,   38,  58,
        1,   38,  58,  1,   38,  58,  1,   38,  58,  1,   38,  58,  1,   38,  58,  1,   38,  58,  1,   38,  58,  1,
        38,  58,  1,   38,  58,  1,   38,  58,  1,   38,  58,  1,   38,  58,  1,   38,  58,  1,   38,  58,  1,   38,
        58,  1,   38,  58,  1,   38,  58,  1,   38,  58,  1,   38,  58,  1,   38,  58,  1,   38,  58,  1,   38,  58,
        1,   38,  58,  1,   38,  58,  1,   38,  58,  1,   20,  92,  1,   17,  95,  1,   16,  97,  1,   14,  98,  1,
        14,  99,  1,   13,  99,  1,   13,  100, 1,   12,  100, 1,   12,  100, 1,   12,  100, 1,   12,  100, 1,   12,
        100, 1,   12,  100, 1,   13,  100, 1,   13,  99,  1,   13,  99,  1,   14,  98,  1,   15,  97,  1,   17,  96,
        1,   19,  93,  115, 65,  160, 1,   52,  72,  1,   47,  77,  2,   43,  81,  94,  96,  2,   41,  84,  91,  99,
        2,   38,  86,  89,  100, 1,   36,  101, 1,   34,  102, 1,   33,  103, 1,   31,  103, 1,   30,  103, 1,   28,
        104, 1,   27,  104, 1,   26,  104, 1,   25,  104, 1,   25,  104, 1,   24,  104, 1,   23,  104, 1,   23,  104,
        1,   22,  104, 1,   22,  104, 2,   21,  55,  69,  104, 2,   21,  50,  73,  104, 2,   21,  47,  76,  104, 2,
        21,  44,  79,  104, 2,   21,  43,  81,  104, 2,   20,  41,  83,  104, 2,   20,  41,  84,  104, 2,   20,  40,
        84,  104, 2,   20,  41,  85,  103, 2,   20,  41,  85,  103, 2,   20,  42,  86,  102, 2,   21,  44,  87,  101,
        2,   21,  46,  88,  100, 2,   21,  49,  90,  98,  1,   21,  55,  1,   22,  62,  1,   22,  68,  1,   22,  74,
        1,   23,  79,  1,   23,  83,  1,   24,  86,  1,   25,  89,  1,   26,  91,  1,   26,  93,  1,   27,  95,  1,
        28,  96,  1,   30,  98,  1,   31,  99,  1,   33,  100, 1,   35,  101, 1,   37,  102, 1,   40,  103, 1,   44,
        104, 1,   48,  104, 1,   54,  105, 1,   62,  106, 1,   69,  106, 1,   75,  106, 2,   24,  29,  79,  107, 2,
        21,  31,  82,  107, 2,   20,  33,  84,  108, 2,   19,  34,  85,  108, 2,   18,  34,  86,  108, 2,   18,  35,
        87,  108, 2,   17,  35,  88,  108, 2,   17,  36,  88,  108, 2,   17,  36,  88,  109, 2,   17,  37,  88,  108,
        2,   16,  38,  87,  108, 2,   16,  40,  86,  108, 2,   16,  41,  85,  108, 2,   16,  43,  83,  108, 2,   16,
        46,  81,  108, 2,   16,  49,  77,  107, 2,   16,  53,  73,  107, 1,   16,  107, 1,   16,  106, 1,   16,  106,
        1,   16,  105, 1,   16,  104, 1,   16,  104, 1,   16,  103, 1,   16,  102, 1,   17,  101, 1,   17,  99,  1,
        17,  98,  1,   18,  96,  1,   18,  95,  1,   19,  93,  1,   20,  92,  2,   21,  32,  36,  89,  2,   23,  30,
        38,  87,  1,   41,  84,  1,   45,  80,  1,   50,  75,  116, 36,  160, 1,   43,  51,  1,   41,  53,  1,   40,
        54,  1,   39,  55,  1,   38,  56,  1,   38,  56,  1,   38,  56,  1,   37,  56,  1,   37,  57,  1,   37,  57,
        1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,
        37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,
        57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   27,  97,
        1,   22,  102, 1,   20,  104, 1,   19,  105, 1,   18,  106, 1,   17,  107, 1,   17,  107, 1,   16,  108, 1,
        16,  108, 1,   16,  108, 1,   16,  108, 1,   16,  108, 1,   16,  108, 1,   16,  108, 1,   17,  107, 1,   17,
        107, 1,   18,  106, 1,   19,  105, 1,   20,  104, 1,   22,  102, 1,   26,  98,  1,   37,  57,  1,   37,  57,
        1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,
        37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,
        57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,
        1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,
        37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,
        57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  1,   37,  57,  2,   37,  57,  109, 114, 2,   37,  57,  106,
        116, 2,   37,  58,  104, 117, 2,   37,  58,  101, 118, 2,   37,  59,  99,  119, 2,   37,  59,  97,  120, 2,
        37,  61,  93,  120, 2,   38,  63,  90,  121, 2,   38,  66,  84,  121, 1,   38,  121, 1,   39,  121, 1,   39,
        121, 1,   39,  121, 1,   40,  120, 1,   41,  120, 1,   41,  119, 1,   42,  118, 1,   43,  117, 1,   43,  116,
        1,   44,  115, 1,   45,  113, 1,   46,  111, 1,   48,  108, 1,   49,  106, 1,   51,  102, 1,   53,  99,  1,
        56,  95,  1,   58,  91,  1,   63,  86,  117, 67,  160, 2,   16,  42,  75,  105, 2,   10,  42,  70,  105, 2,
        8,   42,  68,  105, 2,   7,   42,  67,  105, 2,   6,   42,  66,  105, 2,   6,   42,  65,  105, 2,   5,   42,
        65,  105, 2,   5,   42,  64,  105, 2,   4,   42,  64,  105, 2,   4,   42,  64,  105, 2,   4,   42,  64,  105,
        2,   4,   42,  64,  105, 2,   4,   42,  64,  105, 2,   5,   42,  64,  105, 2,   5,   42,  65,  105, 2,   6,
        42,  65,  105, 2,   6,   42,  66,  105, 2,   7,   42,  67,  105, 2,   8,   42,  68,  105, 2,   10,  42,  70,
        105, 2,   14,  42,  74,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,
        21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,
        85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105,
        2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,
        42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,
        105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,
        21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,
        85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105,
        2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,
        42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  85,  105, 2,   21,  42,  84,  105, 2,   21,  42,  82,
        105, 2,   21,  42,  81,  105, 2,   21,  42,  79,  105, 2,   21,  42,  77,  105, 2,   21,  43,  75,  105, 2,
        21,  43,  73,  105, 2,   22,  44,  71,  105, 2,   22,  45,  68,  110, 2,   22,  46,  65,  113, 2,   22,  49,
        61,  114, 1,   23,  115, 1,   23,  116, 1,   23,  117, 1,   24,  117, 1,   24,  117, 1,   25,  118, 1,   26,
        118, 1,   26,  118, 1,   27,  118, 1,   28,  117, 1,   29,  117, 1,   30,  117, 1,   31,  116, 2,   32,  83,
        85,  115, 2,   33,  80,  85,  115, 2,   35,  78,  85,  113, 2,   36,  75,  85,  111, 1,   38,  72,  1,   41,
        68,  1,   44,  63,  118, 67,  157, 2,   13,  44,  80,  112, 2,   7,   49,  75,  117, 2,   5,   51,  73,  119,
        2,   4,   53,  72,  121, 2,   3,   54,  71,  122, 2,   3,   54,  70,  122, 2,   2,   55,  70,  123, 2,   2,
        55,  69,  123, 2,   1,   55,  69,  124, 2,   1,   55,  69,  124, 2,   1,   55,  69,  124, 2,   1,   55,  69,
        124, 2,   1,   55,  69,  124, 2,   2,   55,  69,  123, 2,   2,   55,  70,  123, 2,   3,   54,  70,  122, 2,
        3,   54,  71,  122, 2,   4,   53,  71,  121, 2,   5,   52,  73,  120, 2,   7,   50,  74,  118, 2,   11,  45,
        79,  113, 2,   18,  41,  84,  106, 2,   19,  41,  83,  106, 2,   19,  42,  83,  105, 2,   20,  42,  82,  105,
        2,   20,  43,  82,  104, 2,   21,  43,  81,  104, 2,   21,  44,  81,  103, 2,   21,  44,  80,  103, 2,   22,
        45,  80,  102, 2,   22,  45,  79,  102, 2,   23,  45,  79,  101, 2,   23,  46,  78,  101, 2,   24,  46,  78,
        100, 2,   24,  47,  77,  100, 2,   25,  47,  77,  100, 2,   25,  48,  76,  99,  2,   26,  48,  76,  99,  2,
        26,  49,  75,  98,  2,   27,  49,  75,  98,  2,   27,  50,  74,  97,  2,   28,  50,  74,  97,  2,   28,  51,
        74,  96,  2,   29,  51,  73,  96,  2,   29,  52,  73,  95,  2,   30,  52,  72,  95,  2,   30,  53,  72,  94,
        2,   31,  53,  71,  94,  2,   31,  54,  71,  93,  2,   32,  54,  70,  93,  2,   32,  55,  70,  92,  2,   33,
        55,  69,  92,  2,   33,  56,  69,  91,  2,   34,  56,  68,  91,  2,   34,  57,  68,  90,  2,   35,  57,  67,
        90,  2,   35,  58,  67,  89,  2,   36,  58,  66,  89,  2,   36,  59,  66,  88,  2,   37,  59,  65,  88,  2,
        37,  60,  65,  87,  2,   38,  60,  64,  87,  2,   38,  61,  64,  86,  2,   39,  61,  63,  86,  2,   39,  62,
        63,  85,  1,   40,  85,  1,   40,  84,  1,   41,  84,  1,   41,  83,  1,   42,  83,  1,   42,  82,  1,   42,
        82,  1,   43,  81,  1,   43,  81,  1,   44,  80,  1,   44,  80,  1,   45,  80,  1,   45,  79,  1,   46,  79,
        1,   46,  78,  1,   47,  78,  1,   47,  77,  1,   48,  77,  1,   48,  76,  1,   49,  76,  1,   49,  75,  1,
        50,  75,  1,   50,  74,  1,   51,  74,  1,   51,  73,  119, 67,  157, 2,   12,  36,  88,  112, 2,   7,   41,
        83,  117, 2,   5,   43,  81,  119, 2,   4,   44,  80,  120, 2,   3,   45,  79,  121, 2,   2,   46,  78,  122,
        2,   2,   46,  78,  122, 2,   1,   46,  78,  123, 2,   1,   47,  77,  123, 2,   1,   47,  77,  123, 2,   1,
        47,  77,  123, 2,   1,   47,  77,  123, 2,   1,   47,  77,  123, 2,   1,   46,  78,  123, 2,   2,   46,  78,
        123, 2,   2,   46,  78,  122, 2,   3,   45,  79,  121, 2,   4,   44,  80,  121, 2,   5,   43,  81,  119, 2,
        7,   41,  82,  117, 3,   11,  37,  53,  71,  87,  113, 3,   12,  33,  53,  72,  92,  112, 3,   13,  33,  52,
        72,  91,  112, 3,   13,  33,  52,  73,  91,  112, 3,   13,  34,  51,  73,  91,  112, 3,   13,  34,  51,  73,
        91,  111, 3,   13,  34,  51,  74,  90,  111, 3,   14,  34,  50,  74,  90,  111, 3,   14,  35,  50,  74,  90,
        111, 3,   14,  35,  49,  75,  90,  110, 3,   14,  35,  49,  75,  89,  110, 3,   15,  35,  49,  76,  89,  110,
        3,   15,  36,  48,  76,  89,  110, 3,   15,  36,  48,  76,  89,  109, 3,   15,  36,  48,  77,  88,  109, 3,
        16,  36,  47,  77,  88,  109, 3,   16,  37,  47,  78,  88,  109, 3,   16,  37,  46,  78,  88,  108, 3,   16,
        37,  46,  78,  87,  108, 3,   17,  37,  46,  79,  87,  108, 3,   17,  38,  45,  79,  87,  108, 3,   17,  38,
        45,  79,  87,  108, 3,   17,  38,  45,  80,  86,  107, 3,   18,  38,  44,  80,  86,  107, 3,   18,  39,  44,
        81,  86,  107, 3,   18,  39,  43,  81,  86,  107, 3,   18,  39,  43,  81,  85,  106, 3,   19,  39,  43,  82,
        85,  106, 3,   19,  40,  42,  82,  85,  106, 3,   19,  40,  42,  83,  85,  106, 3,   19,  40,  41,  83,  84,
        105, 3,   20,  40,  41,  83,  84,  105, 2,   20,  40,  41,  105, 1,   20,  105, 2,   20,  62,  63,  104, 2,
        21,  61,  63,  104, 2,   21,  61,  64,  104, 2,   21,  61,  64,  104, 2,   21,  60,  64,  103, 2,   22,  60,
        65,  103, 2,   22,  59,  65,  103, 2,   22,  59,  66,  103, 2,   22,  59,  66,  103, 2,   23,  58,  66,  102,
        2,   23,  58,  67,  102, 2,   23,  57,  67,  102, 2,   23,  57,  68,  102, 2,   24,  57,  68,  101, 2,   24,
        56,  68,  101, 2,   24,  56,  69,  101, 2,   24,  56,  69,  101, 2,   24,  55,  70,  100, 2,   25,  55,  70,
        100, 2,   25,  54,  70,  100, 2,   25,  54,  71,  100, 2,   25,  54,  71,  99,  2,   26,  53,  72,  99,  2,
        26,  53,  72,  99,  2,   26,  53,  72,  99,  2,   26,  52,  73,  98,  2,   27,  52,  73,  98,  2,   27,  51,
        74,  98,  2,   27,  51,  74,  98,  2,   27,  51,  74,  97,  2,   28,  50,  75,  97,  2,   28,  50,  75,  97,
        2,   28,  50,  76,  97,  2,   28,  49,  76,  97,  2,   29,  49,  76,  96,  2,   29,  48,  77,  96,  120, 67,
        157, 2,   21,  44,  80,  103, 2,   16,  49,  75,  109, 2,   14,  51,  73,  111, 2,   12,  53,  72,  112, 2,
        11,  54,  71,  113, 2,   11,  54,  70,  113, 2,   10,  55,  70,  114, 2,   10,  55,  69,  114, 2,   10,  55,
        69,  115, 2,   9,   55,  69,  115, 2,   9,   55,  69,  115, 2,   10,  55,  69,  115, 2,   10,  55,  69,  114,
        2,   10,  55,  69,  114, 2,   10,  55,  70,  114, 2,   11,  54,  70,  113, 2,   11,  53,  71,  113, 2,   12,
        53,  72,  112, 2,   14,  52,  73,  111, 2,   15,  50,  74,  109, 2,   19,  51,  73,  105, 2,   21,  52,  72,
        103, 2,   22,  53,  71,  102, 2,   23,  54,  70,  101, 2,   25,  56,  69,  100, 2,   26,  57,  68,  98,  2,
        27,  58,  66,  97,  2,   28,  59,  65,  96,  2,   29,  60,  64,  95,  2,   30,  62,  63,  94,  1,   32,  93,
        1,   33,  91,  1,   34,  90,  1,   35,  89,  1,   36,  88,  1,   37,  87,  1,   39,  86,  1,   40,  84,  1,
        41,  83,  1,   42,  82,  1,   43,  81,  1,   44,  80,  1,   46,  78,  1,   47,  77,  1,   45,  78,  1,   44,
        80,  1,   43,  81,  1,   42,  82,  1,   41,  83,  1,   40,  84,  1,   39,  85,  1,   38,  87,  1,   36,  88,
        1,   35,  89,  1,   34,  90,  1,   33,  91,  1,   32,  92,  2,   31,  61,  63,  94,  2,   30,  60,  64,  95,
        2,   28,  59,  65,  96,  2,   27,  58,  66,  97,  2,   26,  57,  67,  98,  2,   25,  55,  69,  99,  2,   24,
        54,  70,  101, 2,   23,  53,  71,  102, 2,   22,  52,  72,  103, 2,   21,  51,  73,  104, 2,   19,  50,  74,
        105, 2,   18,  48,  76,  106, 2,   17,  47,  77,  108, 2,   14,  48,  76,  111, 2,   11,  51,  74,  114, 2,
        9,   52,  73,  115, 2,   8,   53,  71,  117, 2,   7,   54,  71,  117, 2,   7,   54,  70,  118, 2,   6,   55,
        70,  118, 2,   6,   55,  69,  119, 2,   6,   55,  69,  119, 2,   6,   55,  69,  119, 2,   6,   55,  69,  119,
        2,   6,   55,  69,  119, 2,   6,   55,  69,  119, 2,   6,   55,  70,  119, 2,   6,   54,  70,  118, 2,   7,
        54,  70,  118, 2,   8,   53,  71,  117, 2,   9,   52,  72,  116, 2,   10,  51,  74,  114, 2,   13,  48,  76,
        112, 121, 67,  200, 2,   17,  40,  84,  108, 2,   12,  45,  79,  113, 2,   10,  47,  77,  115, 2,   8,   48,
        76,  116, 2,   7,   49,  75,  117, 2,   7,   50,  74,  118, 2,   6,   50,  74,  118, 2,   6,   51,  74,  119,
        2,   6,   51,  73,  119, 2,   6,   51,  73,  119, 2,   6,   51,  73,  119, 2,   6,   51,  73,  119, 2,   6,
        51,  73,  119, 2,   6,   51,  74,  119, 2,   6,   50,  74,  118, 2,   7,   50,  74,  118, 2,   8,   49,  75,
        117, 2,   8,   48,  76,  117, 2,   10,  47,  77,  115, 2,   11,  45,  79,  114, 2,   15,  41,  83,  109, 2,
        16,  39,  85,  108, 2,   17,  39,  85,  108, 2,   17,  40,  84,  107, 2,   18,  40,  84,  107, 2,   18,  41,
        83,  106, 2,   19,  41,  83,  106, 2,   19,  42,  82,  105, 2,   20,  42,  82,  105, 2,   20,  43,  81,  104,
        2,   21,  43,  81,  104, 2,   21,  44,  80,  103, 2,   22,  44,  80,  103, 2,   22,  45,  79,  102, 2,   23,
        45,  79,  102, 2,   23,  46,  78,  101, 2,   24,  46,  78,  101, 2,   24,  47,  77,  100, 2,   25,  47,  77,
        100, 2,   25,  48,  76,  99,  2,   26,  48,  76,  99,  2,   26,  49,  75,  98,  2,   27,  49,  75,  98,  2,
        27,  50,  74,  97,  2,   28,  50,  74,  97,  2,   28,  51,  73,  96,  2,   29,  51,  73,  96,  2,   29,  52,
        72,  95,  2,   30,  52,  72,  95,  2,   30,  53,  71,  94,  2,   31,  53,  71,  93,  2,   31,  54,  70,  93,
        2,   32,  55,  70,  92,  2,   32,  55,  69,  92,  2,   33,  56,  69,  91,  2,   33,  56,  68,  91,  2,   34,
        57,  68,  90,  2,   34,  57,  67,  90,  2,   35,  58,  67,  89,  2,   35,  58,  66,  89,  2,   36,  59,  66,
        88,  2,   36,  59,  65,  88,  2,   37,  60,  65,  87,  2,   37,  60,  64,  87,  2,   38,  61,  64,  86,  2,
        38,  61,  63,  86,  2,   39,  62,  63,  85,  1,   39,  85,  1,   40,  84,  1,   41,  84,  1,   41,  83,  1,
        42,  83,  1,   42,  82,  1,   43,  82,  1,   43,  81,  1,   44,  81,  1,   44,  80,  1,   45,  80,  1,   45,
        79,  1,   46,  79,  1,   46,  78,  1,   47,  78,  1,   47,  77,  1,   48,  77,  1,   48,  76,  1,   49,  76,
        1,   49,  75,  1,   50,  75,  1,   50,  74,  1,   51,  74,  1,   50,  73,  1,   50,  73,  1,   49,  72,  1,
        49,  72,  1,   48,  71,  1,   48,  71,  1,   47,  70,  1,   47,  70,  1,   46,  69,  1,   46,  69,  1,   45,
        68,  1,   45,  68,  1,   44,  67,  1,   44,  67,  1,   43,  66,  1,   43,  66,  1,   42,  65,  1,   42,  65,
        1,   41,  64,  1,   41,  64,  1,   40,  63,  1,   40,  63,  1,   16,  66,  1,   11,  71,  1,   10,  73,  1,
        8,   74,  1,   7,   75,  1,   7,   76,  1,   6,   76,  1,   6,   76,  1,   6,   77,  1,   6,   77,  1,   5,
        77,  1,   6,   77,  1,   6,   77,  1,   6,   76,  1,   6,   76,  1,   7,   76,  1,   7,   75,  1,   8,   74,
        1,   10,  73,  1,   12,  71,  1,   17,  66,  122, 67,  157, 1,   21,  105, 1,   21,  105, 1,   21,  105, 1,
        21,  105, 1,   21,  105, 1,   21,  105, 1,   21,  105, 1,   21,  105, 1,   21,  105, 1,   21,  105, 1,   21,
        105, 1,   21,  105, 1,   21,  105, 1,   21,  105, 1,   21,  105, 1,   21,  105, 1,   21,  105, 1,   21,  104,
        1,   21,  103, 1,   21,  102, 1,   21,  101, 2,   21,  41,  71,  100, 2,   21,  41,  70,  99,  2,   21,  41,
        69,  98,  2,   21,  41,  68,  97,  2,   21,  41,  67,  96,  2,   21,  41,  66,  95,  2,   21,  41,  65,  94,
        2,   21,  41,  64,  93,  2,   22,  40,  63,  92,  2,   22,  40,  62,  91,  2,   23,  39,  61,  90,  2,   23,
        39,  60,  89,  2,   24,  38,  59,  88,  2,   25,  37,  58,  87,  2,   27,  34,  57,  86,  1,   56,  85,  1,
        55,  84,  1,   54,  83,  1,   53,  82,  1,   52,  81,  1,   51,  80,  1,   50,  79,  1,   49,  78,  1,   48,
        77,  1,   47,  76,  1,   46,  75,  1,   45,  74,  1,   44,  73,  1,   43,  72,  1,   42,  71,  1,   41,  70,
        1,   40,  69,  1,   39,  68,  1,   38,  67,  1,   37,  66,  1,   36,  65,  1,   35,  64,  1,   34,  63,  2,
        33,  62,  93,  101, 2,   32,  61,  91,  103, 2,   31,  60,  90,  104, 2,   30,  59,  89,  105, 2,   29,  58,
        89,  105, 2,   28,  57,  88,  106, 2,   27,  56,  88,  106, 2,   26,  55,  87,  106, 2,   25,  54,  87,  107,
        2,   24,  53,  87,  107, 2,   23,  52,  87,  107, 1,   22,  107, 1,   21,  107, 1,   20,  107, 1,   19,  107,
        1,   19,  107, 1,   19,  107, 1,   19,  107, 1,   19,  107, 1,   19,  107, 1,   19,  107, 1,   19,  107, 1,
        19,  107, 1,   19,  107, 1,   19,  107, 1,   19,  107, 1,   19,  107, 1,   19,  107, 1,   19,  107, 1,   19,
        107, 1,   19,  107, 123, 29,  189, 1,   74,  80,  1,   70,  84,  1,   68,  85,  1,   66,  86,  1,   64,  87,
        1,   63,  88,  1,   62,  88,  1,   61,  89,  1,   60,  89,  1,   59,  89,  1,   58,  89,  1,   57,  89,  1,
        56,  89,  1,   56,  88,  1,   55,  88,  1,   55,  88,  1,   54,  87,  1,   54,  86,  1,   54,  85,  1,   53,
        83,  1,   53,  80,  1,   53,  75,  1,   52,  74,  1,   52,  74,  1,   52,  73,  1,   52,  73,  1,   52,  73,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   51,  72,  1,   51,  72,  1,   50,  72,  1,   46,  72,  1,   42,  71,  1,   40,  71,
        1,   38,  71,  1,   37,  70,  1,   37,  70,  1,   36,  69,  1,   36,  69,  1,   36,  68,  1,   36,  67,  1,
        35,  67,  1,   35,  67,  1,   36,  67,  1,   36,  68,  1,   36,  69,  1,   36,  69,  1,   37,  70,  1,   37,
        70,  1,   38,  71,  1,   40,  71,  1,   42,  71,  1,   46,  72,  1,   49,  72,  1,   51,  72,  1,   51,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  73,  1,   52,  73,  1,   52,  73,  1,   52,  73,
        1,   53,  74,  1,   53,  76,  1,   53,  80,  1,   53,  84,  1,   54,  85,  1,   54,  86,  1,   54,  87,  1,
        55,  88,  1,   55,  88,  1,   56,  88,  1,   57,  89,  1,   57,  89,  1,   58,  89,  1,   59,  89,  1,   60,
        89,  1,   61,  89,  1,   62,  88,  1,   63,  88,  1,   64,  87,  1,   66,  86,  1,   68,  85,  1,   70,  84,
        1,   74,  81,  124, 29,  189, 1,   60,  64,  1,   57,  67,  1,   56,  69,  1,   55,  69,  1,   54,  70,  1,
        53,  71,  1,   53,  71,  1,   53,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,
        72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,  1,   52,  72,
        1,   53,  72,  1,   53,  71,  1,   53,  71,  1,   54,  71,  1,   55,  70,  1,   56,  69,  1,   57,  67,  1,
        59,  65,  125, 29,  189, 1,   43,  49,  1,   40,  53,  1,   38,  56,  1,   37,  58,  1,   37,  59,  1,   36,
        61,  1,   35,  62,  1,   35,  63,  1,   35,  64,  1,   35,  65,  1,   35,  66,  1,   35,  66,  1,   35,  67,
        1,   35,  68,  1,   36,  68,  1,   36,  69,  1,   37,  69,  1,   38,  70,  1,   39,  70,  1,   40,  70,  1,
        44,  71,  1,   48,  71,  1,   50,  71,  1,   50,  71,  1,   51,  71,  1,   51,  71,  1,   51,  72,  1,   51,
        72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,
        1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,
        51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,
        72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,
        1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   52,  72,  1,
        52,  72,  1,   52,  72,  1,   52,  73,  1,   52,  74,  1,   52,  77,  1,   52,  82,  1,   53,  84,  1,   53,
        85,  1,   53,  86,  1,   54,  87,  1,   54,  87,  1,   55,  88,  1,   56,  88,  1,   56,  88,  1,   57,  88,
        1,   57,  88,  1,   56,  88,  1,   56,  88,  1,   55,  88,  1,   54,  87,  1,   54,  87,  1,   53,  86,  1,
        53,  85,  1,   52,  84,  1,   52,  82,  1,   52,  78,  1,   52,  74,  1,   52,  73,  1,   52,  73,  1,   52,
        72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,
        1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,
        51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,
        72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,
        1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,
        51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  72,  1,   51,  71,  1,   51,  71,  1,   50,  71,  1,   50,
        71,  1,   48,  71,  1,   44,  71,  1,   40,  70,  1,   39,  70,  1,   38,  70,  1,   37,  69,  1,   36,  69,
        1,   36,  68,  1,   35,  68,  1,   35,  67,  1,   35,  67,  1,   35,  66,  1,   35,  65,  1,   35,  64,  1,
        35,  63,  1,   35,  62,  1,   36,  61,  1,   37,  59,  1,   37,  58,  1,   38,  56,  1,   40,  53,  1,   43,
        50,  126, 79,  121, 1,   38,  50,  1,   36,  53,  1,   33,  56,  1,   32,  57,  1,   30,  59,  1,   28,  60,
        2,   27,  61,  98,  102, 2,   26,  63,  96,  105, 2,   25,  64,  94,  107, 2,   24,  65,  93,  108, 2,   23,
        66,  92,  109, 2,   22,  67,  92,  109, 2,   21,  68,  91,  110, 2,   20,  69,  90,  110, 2,   19,  70,  89,
        110, 2,   18,  71,  88,  110, 2,   17,  72,  87,  111, 2,   17,  74,  86,  110, 2,   16,  75,  85,  110, 2,
        15,  76,  84,  110, 3,   15,  42,  46,  77,  83,  110, 2,   14,  41,  48,  109, 2,   14,  40,  49,  108, 2,
        14,  39,  50,  108, 2,   14,  38,  51,  107, 2,   14,  37,  52,  106, 2,   14,  36,  53,  105, 2,   14,  35,
        55,  105, 2,   14,  34,  56,  104, 2,   15,  33,  57,  103, 2,   15,  32,  58,  102, 2,   16,  31,  59,  101,
        2,   17,  30,  60,  100, 2,   18,  29,  61,  99,  2,   20,  27,  63,  98,  1,   64,  97,  1,   65,  95,  1,
        67,  94,  1,   69,  92,  1,   71,  90,  1,   73,  88,  1,   79,  84,
    };

    return Load(data, sizeof(data));
  }

  void LoadTTF(const std::string &filename)
  {
    std::ifstream fontFile(filename.c_str(), std::ios::binary | std::ios::ate);
    fontFile >> std::noskipws;

    std::streampos fileSize = fontFile.tellg();
    fontFile.seekg(0, std::ios::beg);

    fontBuffer.clear();
    fontBuffer.reserve(fileSize);

    std::copy(std::istream_iterator<unsigned char>(fontFile), std::istream_iterator<unsigned char>(),
              std::back_inserter(fontBuffer));

    /* prepare font */
    if (!stbtt_InitFont(&_info, fontBuffer.data(), 0)) {
      throw vpIoException(vpIoException::ioError, "Error when initializing the font data.");
    }
  }

  void ToUTF32(const std::string &str, std::vector<unsigned int> &utf32)
  {
    // convert UTF8 to UTF32
    utf32.clear();
    for (size_t i = 0; i < str.size(); i++) {
      unsigned char ch = (unsigned char)str[i];
      unsigned int charcode;
      if (ch <= 127) {
        charcode = ch;
      }
      else if (ch <= 223 && i + 1 < str.size() && (str[i + 1] & 0xc0) == 0x80) {
        charcode = ((ch & 31) << 6) | (str[i + 1] & 63);
        i++;
      }
      else if (ch <= 239 && i + 2 < str.size() && (str[i + 1] & 0xc0) == 0x80 && (str[i + 2] & 0xc0) == 0x80) {
        charcode = ((ch & 15) << 12) | ((str[i + 1] & 63) << 6) | (str[i + 2] & 63);
        i += 2;
      }
      else if (ch <= 247 && i + 3 < str.size() && (str[i + 1] & 0xc0) == 0x80 && (str[i + 2] & 0xc0) == 0x80 &&
              (str[i + 3] & 0xc0) == 0x80) {
        int val = static_cast<int>(((ch & 15) << 18) | ((str[i + 1] & 63) << 12) | ((str[i + 2] & 63) << 6) | (str[i + 3] & 63));
        if (val > 1114111) {
          val = 65533;
        }
        charcode = val;
        i += 3;
      }
      else {
        charcode = 65533;
        while (i + 1 < str.size() && (str[i + 1] & 0xc0) == 0x80) {
          i++;
        }
      }
      utf32.push_back(charcode);
    }
  }
};
#endif // DOXYGEN_SHOULD_SKIP_THIS

/*!
  Creates a new font class with given height.

  \note The vpFontFamily::GENERIC_MONOSPACE font supports ASCII characters only. It was generated on the base of the
  generic monospace font from Gdiplus.

  \param [in] height : Initial font height value in pixels. By default it is equal to 16 pixels.
  \param [in] fontFamily : Font family in TTF format.
  \param [in] ttfFilename : Path to the TTF file if needed. Can contain multiple paths separated by `;`character. The
  first valid path that is found is used.
*/
vpFont::vpFont(unsigned int height, const vpFontFamily &fontFamily, const std::string &ttfFilename)
  : m_impl(new Impl(height, fontFamily, ttfFilename))
{ }

/*!
  Destructor.
 */
vpFont::~vpFont() { delete m_impl; }

/*!
  Draws a text at the image.

  \param [in,out] I : Image where we draw text.
  \param [in] text : A text to draw.
  \param [in] position : A position to draw text.
  \param [in] color : A color of the text.

  \return A result of the operation.
*/
bool vpFont::drawText(vpImage<unsigned char> &I, const std::string &text, const vpImagePoint &position,
                      unsigned char color) const
{
  return m_impl->Draw(I, text, position, color);
}

/*!
  Draws a text at the image. Fills the text background by given color.

  \param [in,out] I : Image where we draw text.
  \param [in] text : A text to draw.
  \param [in] position : A position to draw text.
  \param [in] color : A color of the text.
  \param [in] background : Background color.

  \return a result of the operation.
*/
bool vpFont::drawText(vpImage<unsigned char> &I, const std::string &text, const vpImagePoint &position,
                      unsigned char color, unsigned char background) const
{
  return m_impl->Draw(I, text, position, color, background);
}

/*!
  Draws a text at the image.

  \param [in,out] I : Image where we draw text.
  \param [in] text : A text to draw.
  \param [in] position : A position to draw text.
  \param [in] color : A color of the text.

  \return A result of the operation.
*/
bool vpFont::drawText(vpImage<vpRGBa> &I, const std::string &text, const vpImagePoint &position,
                      const vpColor &color) const
{
  return m_impl->Draw(I, text, position, color);
}

/*!
  Draws a text at the image. Fills the text background by given color.

  \param [in,out] I : Image where we draw text.
  \param [in] text : A text to draw.
  \param [in] position : A position to draw text.
  \param [in] color : A color of the text.
  \param [in] background : Background color.

  \return a result of the operation.
*/
bool vpFont::drawText(vpImage<vpRGBa> &I, const std::string &text, const vpImagePoint &position, const vpColor &color,
                      const vpColor &background) const
{
  return m_impl->Draw(I, text, position, color, background);
}

/*!
  Gets height of the font in pixels.

  \return Current height of the font.
*/
unsigned int vpFont::getHeight() const { return static_cast<unsigned int>(m_impl->Height()); }

/*!
  Gets text bounding box size.

  \return The width (\e j) and height (\e i) of the text bounding box.
*/
vpImagePoint vpFont::getMeasure(const std::string &text) const { return m_impl->Measure2(text); }

/*!
  Sets a new height value to font.

  \param [in] height : A new height value in pixels.

  \return A result of the operation.
*/
bool vpFont::setHeight(unsigned int height) { return m_impl->Resize(height); }
END_VISP_NAMESPACE
