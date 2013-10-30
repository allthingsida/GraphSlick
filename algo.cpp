#include "algo.hpp"

//--------------------------------------------------------------------------
bool func_to_mgraph(
    ea_t func_ea,
    mutable_graph_t *mg,
    gnodemap_t &node_map,
    bool append_node_id,
    qflow_chart_t *fc)
{
  // Build function's flowchart (if needed)
  qflow_chart_t _fc;
  if (fc == NULL)
  {
    fc = &_fc;
    if (!get_func_flowchart(func_ea, *fc))
      return false;
  }

  // Resize the graph
  int nodes_count = fc->size();
  mg->resize(nodes_count);

  // Build the node cache and edges
  for (int n=0; n < nodes_count; n++)
  {
    qbasic_block_t &block = fc->blocks[n];
    gnode_t *nc = node_map.add(n);

    // Generate disassembly text
    if (append_node_id)
      nc->text.sprnt("ID(%d)\n", n);
    get_disasm_text(block.startEA, block.endEA, &nc->text);

    // Build edges
    for (int isucc=0, succ_sz=fc->nsucc(n); isucc < succ_sz; isucc++)
    {
      int nsucc = fc->succ(n, isucc);
      mg->add_edge(n, nsucc, NULL);
    }
  }
  return true;
}

//--------------------------------------------------------------------------
bool sanitize_groupman(
  ea_t func_ea,
  groupman_t *gm,
  qflow_chart_t *fc)
{
  // Build function's flowchart (if needed)
  qflow_chart_t _fc;
  if (fc == NULL)
  {
    fc = &_fc;
    if (!get_func_flowchart(func_ea, *fc))
      return false;
  }

  // Create a group for all potentially missing nodes
  pgroupdef_t missing_gd = new groupdef_t();

  int nodes_count = fc->size();

  // Get a reference to all the nodedefs
  nid2ndef_t *nds = gm->get_nds();

  // Verify that all nodes are present
  for (int n=0; n < nodes_count; n++)
  {
    // Is this node already parsed/defined?
    nid2ndef_t::iterator it = nds->find(n);
    if (it != nds->end())
      continue;

    // Convert basic block to an ND
    qbasic_block_t &block = fc->blocks[n];
    pnodedef_t nd = new nodedef_t();
    nd->nid = n;
    nd->start = block.startEA;
    nd->end = block.endEA;

    // Add the ND to its own NDL
    pnodedef_list_t ndl = missing_gd->add_node_group();
    ndl->add_nodedef(nd);
  }

  if (missing_gd->group_count() == 0)
  {
    // No orphan nodes where found, get rid of the group
    delete missing_gd;
  }
  else
  {
    // Found at least one orphan node, add it to the groupman
    missing_gd->groupname = missing_gd->id = "orphan_nodes";
    gm->add_group(missing_gd);
  }

  return true;
}
