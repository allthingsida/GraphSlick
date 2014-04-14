#ifndef __PY_BBMATCHER_INC__
#define __PY_BBMATCHER_INC__

/*--------------------------------------------------------------------------
GraphSlick (c) Elias Bachaalany
-------------------------------------

BBMatcher Python to C wrapper class

--------------------------------------------------------------------------*/

#include <pro.h>
#ifdef snprintf
  #undef snprintf
#endif

#include <Python.h>
#include "types.hpp"

//--------------------------------------------------------------------------
class PyBBMatcher
{
  PyObject *py_matcher_module;
  PyObject *py_instref;
  PyObject *py_meth_save_state, *py_meth_load_state, *py_meth_analyze, *py_meth_find_similar;

  const char *init_script;

  /**
  * @brief Call the supporting init file once
  */
  const char *call_init_file();

public:
  /**
  * @brief ctor
  */
  PyBBMatcher(const char *init_script): py_matcher_module(NULL), py_instref(NULL),
                 py_meth_find_similar (NULL), py_meth_save_state(NULL),
                 py_meth_load_state (NULL), py_meth_analyze (NULL), init_script(init_script)
  {
  }

  ~PyBBMatcher()
  {
    deinit();
  }

  /**
  * @brief Initialize the needed python references
  */
  const char *init();

  /**
  * @brief Release references
  */
  void deinit();

  /**
  * @brief Analyze and return the non-overlapping wellformed function instances	
  */
  void Analyze(ea_t func_addr, int_3dvec_t &result);

  /**
  * @brief Load state
  */
  bool LoadState(const char *filename);

  /**
  * @brief Save state and return it as a string
  */
  bool SaveState(qstring &out);

  /**
  * @brief Analyze and set the internal state
  */
  bool FindSimilar(intvec_t &node_list, int_2dvec_t &similar);
};

#endif