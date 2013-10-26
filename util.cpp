#include "util.h"
#include <kernwin.hpp>

/*--------------------------------------------------------------------------

History
--------

10/23/2013 - eliasb   - First version, it comes from refactored 
                        code from the plugin module
10/25/2013 - eliasb   - Added jump_to_node()
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

//--------------------------------------------------------------------------
void jump_to_node(graph_viewer_t *gv, int nid)
{
  viewer_center_on(gv, nid);

  int x, y;

  // will return a place only when a node was previously selected
  place_t *old_pl = get_custom_viewer_place(gv, false, &x, &y);
  if (old_pl == NULL)
    return;

  user_graph_place_t *new_pl = (user_graph_place_t *) old_pl->clone();
  new_pl->node = nid;
  jumpto(gv, new_pl, x, y);
  delete new_pl;
}