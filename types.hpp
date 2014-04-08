#ifndef __TYPES_INC__
#define __TYPES_INC__

/*
GraphSlick (c) Elias Bachaalany
-------------------------------------

Plugin module

This header file contain some type definitions used by various modules
*/

//--------------------------------------------------------------------------
#include <pro.h>

//--------------------------------------------------------------------------
/**
* @brief Node data class. It will be served from the graph callback
*/
struct gnode_t
{
  int id;
  qstring text;
  qstring hint;
};

//--------------------------------------------------------------------------
typedef qvector<intvec_t> int_2dvec_t;

//--------------------------------------------------------------------------
typedef qvector<int_2dvec_t> int_3dvec_t;

//--------------------------------------------------------------------------
#endif