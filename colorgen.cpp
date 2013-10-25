/*--------------------------------------------------------------------------
GraphSlick (c) Elias Bachaalany
-------------------------------------

Color generation module

This module implements the color generator

History
--------

10/24/2013 - eliasb             - First version

--------------------------------------------------------------------------*/

#include "colorgen.h"

//----------------------------------------------------------------------
// Conversion between the HSL(Hue, Saturation, and Luminosity)
// and RBG color model.
//----------------------------------------------------------------------
// The conversion algorithms presented here come from the book by
// Fundamentals of Interactive Computer Graphics by Foley and van Dam.
// In the example code, HSL values are represented as floating point
// number in the range 0 to 1. RGB tridrants use the Windows convention
// of 0 to 255 of each element.
//----------------------------------------------------------------------

//--------------------------------------------------------------------------
/**
* @brief 
*/
inline unsigned int make_rgb(
  bool bRealRgb, 
  unsigned char r, 
  unsigned char g, 
  unsigned char b)
{
  if (bRealRgb)
  {
    return (r << 16) | (g << 8) | b;
  }
  else
  {
    return (b << 16) | (g << 8) | r;
  }
}

// ------------------------------------------------------------------------------
/**
* @brief 
*/
static unsigned char to_rgb(double rm1, double rm2, double rh) 
{
  if (rh > 360.0) 
    rh -= 360.0;
  else if (rh <   0.0) 
    rh += 360.0;

  if (rh <  60.0) 
    rm1 = rm1 + (rm2 - rm1) * rh / 60.0;
  else if (rh < 180.0) 
    rm1 = rm2;
  else if (rh < 240.0) 
    rm1 = rm1 + (rm2 - rm1) * (240.0 - rh) / 60.0; 

  return (unsigned char)(rm1 * 255.0);
}

//--------------------------------------------------------------------------
static unsigned int HSLtoRGB(
  bool bRealRgb,
  unsigned int H, 
  unsigned int S, 
  unsigned int L) 
{
  if (S == 0)
  {
    // achromatic
    return make_rgb(bRealRgb, L, L, L);
  }

  double h = (double)H*360/255;
  double s = (double)S / 255;
  double l = (double)L / 255;
  double rm1, rm2;

  if (l <= 0.5) 
    rm2 = l + l * s;
  else 
    rm2 = l + s - l * s;
  rm1 = 2.0 * l - rm2;

  return make_rgb(bRealRgb, 
             to_rgb(rm1, rm2, h + 120.0),
             to_rgb(rm1, rm2, h),
             to_rgb(rm1, rm2, h - 120.0) );
}

//--------------------------------------------------------------------------
unsigned int colorvargen_t::get_color()
{
  if (l <= L_END)
    return 0;

  unsigned int old_l = l;
  l += L_INT;

  return HSLtoRGB(bRealRgb, h, s, old_l);
}

//--------------------------------------------------------------------------
bool colorgen_t::get_colorvar(colorvargen_t &cv)
{
  unsigned int old_h = h, old_s = s;

  // Done with the Hue?
  if (h > H_END)
  {
    // Done with the Saturation?
    if (s < S_END)
    {
      // Nothing else to do
      return false;
    }
    else
    {
      // Advance Saturation so we get a whole new set of Hue
      s += S_INT;
      old_s = s;
    }
    // Start Hue all over again
    old_h = h = H_START;
  }
  else
  {
    // Advance to next Hue
    h += H_INT;
  }

  cv.bRealRgb = bRealRgb;
  cv.h        = old_h;
  cv.s        = old_s;
  cv.l        = L_START;
  cv.L_END    = L_END;
  cv.L_INT    = L_INT;

  return true;
}

//--------------------------------------------------------------------------
colorgen_t::colorgen_t(
    bool bRealRgb, 
    unsigned int h_start, unsigned int h_end, unsigned int h_int, 
    unsigned int s_start, unsigned int s_end, unsigned int s_int, 
    unsigned int l_start, unsigned int l_end, unsigned int l_int) :
          bRealRgb(bRealRgb),
          H_START(h_start), H_END(h_end), H_INT(h_int),
          S_START(s_start), S_END(s_end), S_INT(s_int),
          L_START(l_start), L_END(l_end), L_INT(l_int)
{
  h = H_START;
  s = S_START;
}

//--------------------------------------------------------------------------
void colorgen_t::rewind()
{
  h = H_START;
  s = S_START;
}
