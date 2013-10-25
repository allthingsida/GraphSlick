/*--------------------------------------------------------------------------
GraphSlick (c) Elias Bachaalany
-------------------------------------

Color generation module

This module implements the color generator

History
--------

10/24/2013 - eliasb             - First version
                                - Added Rewind() method
--------------------------------------------------------------------------*/

//--------------------------------------------------------------------------
class colorgen_t;
class colorvargen_t
{
  friend class colorgen_t;

  unsigned int l, L_END, L_INT;
  unsigned int h, s;
  bool bRealRgb;
public:
  unsigned int get_color();
};

//--------------------------------------------------------------------------
class colorgen_t
{
private:
  unsigned int h, s;
  bool bRealRgb;
public:
  unsigned int S_START, S_END, S_INT;
  unsigned int H_START, H_END, H_INT;
  unsigned int L_START, L_END, L_INT;

  colorgen_t(bool bRealRgb = false,
             unsigned int h_start=0,   unsigned int h_end=255, unsigned int h_int=14,
             unsigned int s_start=255, unsigned int s_end=60,  unsigned int s_int=-8,
             unsigned int l_start=190, unsigned int l_end=100, unsigned int l_int=-3);

  bool get_colorvar(colorvargen_t &cv);
  void rewind();
};
