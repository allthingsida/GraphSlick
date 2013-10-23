#include "util.h"
#include <kernwin.hpp>

/*--------------------------------------------------------------------------

History
--------

10/23/2013 - eliasb             - First version, it comes from refactored 
                                  code from the plugin module
--------------------------------------------------------------------------*/

//--------------------------------------------------------------------------
/**
* @brief Get the disassembly text into a qstring
*/
void get_disasm_text(
    ea_t start, 
    ea_t end, 
    qstring *out)
{
  // Generate disassembly text
  text_t txt;
  gen_disasm_text(
    start, 
    end, 
    txt, 
    false);

  // Append all disasm lines
  for (text_t::iterator it=txt.begin(); it != txt.end(); ++it)
  {
    out->append(it->line);
    out->append("\n");
  }
}

//--------------------------------------------------------------------------
/**
* @brief Build a function flowchart
*/
bool get_func_flowchart(
  ea_t ea, 
  qflow_chart_t &qf)
{
  func_t *f = get_func(ea);
  if (f == NULL)
    return false;

  qstring s;
  s.sprnt("$ flowchart of %a()", f->startEA);
  qf.create(
    s.c_str(), 
    f, 
    BADADDR, 
    BADADDR, 
    FC_PREDS);

  return true;
}
