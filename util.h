#ifndef __UTIL__
#define __UTIL__

/*--------------------------------------------------------------------------
GraphSlick (c) Elias Bachaalany
-------------------------------------

Util module

This module implements various utility functions

--------------------------------------------------------------------------*/

#include <map>
#include <pro.h>
#include <funcs.hpp>
#include <gdl.hpp>
#include <graph.hpp>

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
/**
* @brief Utility map class to store gnode_t types
*/
class gnodemap_t: public std::map<int, gnode_t>
{
public:
  /**
  * @brief Add a node to the map
  */
  gnode_t *add(int nid)
  {
    gnode_t *node = &(insert(std::make_pair(nid, gnode_t())).first->second);
    return node;
  }

  /**
  * @brief Return node data
  */
  gnode_t *get(int nid)
  {
    gnodemap_t::iterator it = find(nid);
    if ( it == end() )
      return NULL;
    else
      return &it->second;
  }
};

//--------------------------------------------------------------------------
void get_disasm_text(
    ea_t start, 
    ea_t end, 
    qstring *out);

bool get_func_flowchart(
    ea_t ea, 
    qflow_chart_t &qf);

//--------------------------------------------------------------------------
/**
* @brief Focuses and jumps to the given node id in the graph viewer
*/
void jump_to_node(graph_viewer_t *gv, int nid);

//--------------------------------------------------------------------------
/**
* @brief Utility function to convert a string to the 'asize_t' type
          It works based on the EA64 define
*/
asize_t str2asizet(const char *str);

//--------------------------------------------------------------------------
/**
* @brief Skips white spaces
*/
char *skip_spaces(char *p);

//--------------------------------------------------------------------------
/**
* @brief Returns whether a graphical version of IDA is being used
*/
bool is_ida_gui();

#endif