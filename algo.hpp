#ifndef __ALGO__
#define __ALGO__

/*--------------------------------------------------------------------------
GraphSlick (c) Elias Bachaalany
-------------------------------------

Algorithms module

This module implements various algorithms used by the plugin

History
--------

10/23/2013 - eliasb     - First version, it comes from refactored code from the plugin module
10/24/2013 - eliasb     - Renamed class to a function like name and dropped the () operator
10/25/2013 - eliasb     - Return the ndl2id map to the caller
                        - added 'append_node_id' param to func_to_mgraph()
10/28/2013 - eliasb     - add 'hint' value to the combined blocks
                        - show the 'groupname' or 'id' as text for combined nodes
10/29/2013 - eliasb     - added sanity checks to fc_to_combined_mg()
                        - functions don't compute flowchart each time now. user can pass a flowchart
                        - added sanitize_groupman() to allow loaded incomplete bbgroup file						
--------------------------------------------------------------------------*/


//--------------------------------------------------------------------------
#include <map>
#include <pro.h>
#include <funcs.hpp>
#include <gdl.hpp>
#include <graph.hpp>
#include "groupman.h"
#include "util.h"

//--------------------------------------------------------------------------
/**
* @brief Creates a mutable graph that have the combined nodes per the groupmanager
*        A class was used to simulate nested functions needed by the combine algo
*/
class fc_to_combined_mg
{
  // Create a mapping between single node ids and the nodedef list they belong to
  ng2nid_t *group2id;

  gnodemap_t *node_map;
  groupman_t *gm;
  qflow_chart_t *fc;
  bool show_nids_only;

  /**
  * @brief Create and return a groupped node ID
  */
  int get_groupid(int n)
  {
    int group_id;

    // Find how this single node is defined in the group manager
    nodeloc_t *loc = gm->find_nodeid_loc(n);
    if (loc == NULL)
      return -1;

    // Does this node have a group yet? (ndl)
    ng2nid_t::iterator it = group2id->find(loc->ng);
    if (it == group2id->end())
    {
      // Assign an auto-incr id
      group_id = group2id->size();  
      (*group2id)[loc->ng] = group_id;

      // Initialize this group's node id
      gnode_t gn;
      gn.id = group_id;
      size_t t = loc->ng->size();
      for (nodegroup_t::iterator it=loc->ng->begin();
           it != loc->ng->end();
           ++it)
      {
        if (show_nids_only)
        {
          gn.text.cat_sprnt("%d", (*it)->nid);
          if (--t > 0)
            gn.text.append(", ");
        }

        qbasic_block_t &block = fc->blocks[(*it)->nid];
        qstring s;
        get_disasm_text(
          block.startEA, 
          block.endEA, 
          &s);
        gn.hint.append(s);
      }

      if (!show_nids_only)
      {
        // Are there any groupped nodes?
        if (loc->ng->size() > 1)
        {
          // Display the group name or the group id
          if (!loc->sg->name.empty())
            gn.text = loc->sg->name;
          else
            gn.text = loc->sg->id;
        }
        else
        {
          gn.text = gn.hint;
        }
      }

      // Cache the node data
      (*node_map)[group_id] = gn;
    }
    else
    {
      // Grab the group id
      group_id = it->second;
    }

    return group_id;
  }

  /**
  * @brief Build the combined mutable graph from the groupman and a flowchart
  */
  bool build(
    qflow_chart_t *fc,
    groupman_t *gm,
    gnodemap_t &node_map,
    ng2nid_t &group2id,
    mutable_graph_t *mg)
  {
    // Take a reference to the local variables so they are used
    // in the other helper functions
    this->gm = gm;
    this->node_map = &node_map;
    this->fc = fc;
  	this->group2id = &group2id;

    // Compute the total size of nodes needed for the combined graph
    // The size is the total count of node def lists in each group def
    size_t node_count = 0;
    for (supergroup_listp_t::iterator it=gm->get_supergroups()->begin();
         it != gm->get_supergroups()->end();
         ++it)
    {
      psupergroup_t sg = *it;
      node_count += sg->groups.size();
    }

    // Resize the graph
    mg->resize(node_count);

    // Build the combined graph
    int snodes_count = fc->size();
    for (int n=0; n < snodes_count; n++)
    {
      // Figure out the combined node ID
      int group_id = get_groupid(n);
      if (group_id == -1)
        return false;

      // Build the edges
      for (int isucc=0, succ_sz=fc->nsucc(n); 
           isucc < succ_sz; 
           isucc++)
      {
        // Get the successor node
        int nsucc = fc->succ(n, isucc);

        // This node belongs to the same group?
        int succ_grid = get_groupid(nsucc);
        if (succ_grid == -1)
          return false;

        if (succ_grid == group_id)
        {
          // Do nothing, consider as one node
          continue;
        }
        // Add an edge
        mg->add_edge(group_id, succ_grid, NULL);
      }
    }
    return true;
  }

public:
  /**
  * @brief Operator to call the class as a function
  */
  fc_to_combined_mg(
      ea_t func_ea,
      groupman_t *gm,
      gnodemap_t &node_map,
      ng2nid_t &group2id,
      mutable_graph_t *mg,
      qflow_chart_t *fc = NULL): show_nids_only(false)
  {
    // Build function's flowchart (if needed)
    qflow_chart_t _fc;
    if (fc == NULL)
    {
      fc = &_fc;
      if (!get_func_flowchart(func_ea, *fc))
        return;
    }

    build(fc, gm, node_map, group2id, mg);
  }
};

//--------------------------------------------------------------------------
/**
* @brief Build a mutable graph from a function address
*/
bool func_to_mgraph(
    ea_t func_ea,
    mutable_graph_t *mg,
    gnodemap_t &node_map,
    qflow_chart_t *fc = NULL,
    bool append_node_id = false);

//--------------------------------------------------------------------------
/**
* @brief Sanitize the contents of the groupman versus the flowchart 
         of the function
*/
bool sanitize_groupman(
  ea_t func_ea,
  groupman_t *gm,
  qflow_chart_t *fc = NULL);

#endif