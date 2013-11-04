#include "algo.hpp"

//--------------------------------------------------------------------------
bool func_to_mgraph(
    ea_t func_ea,
    mutable_graph_t *mg,
    gnodemap_t &node_map,
    qflow_chart_t *fc,
    bool append_node_id)
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
  for (int nid=0; nid < nodes_count; nid++)
  {
    qbasic_block_t &block = fc->blocks[nid];
    gnode_t *nc = node_map.add(nid);

    // Append node ID to the output
    if (append_node_id)
      nc->text.sprnt("ID(%d)\n", nid);

    // Generate disassembly text
    get_disasm_text(
        block.startEA, 
        block.endEA, 
        &nc->text);

    // Build edges
    for (int nid_succ=0, succ_sz=fc->nsucc(nid); nid_succ < succ_sz; nid_succ++)
    {
      int nsucc = fc->succ(nid, nid_succ);
      mg->add_edge(nid, nsucc, NULL);
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
  psupergroup_t missing_sg = new supergroup_t();

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

    // Add the node to its own group
    pnodegroup_t ng = missing_sg->add_nodegroup();
    ng->add_node(nd);
  }

  if (missing_sg->gcount() == 0)
  {
    // No orphan nodes where found, get rid of the group
    delete missing_sg;
  }
  else
  {
    // Found at least one orphan node, add it to the groupman
    missing_sg->name = missing_sg->id = "orphan_nodes";
    // This is a synthetic group
    missing_sg->is_synthetic = true;
    gm->add_supergroup(
          gm->get_path_sgl(),
          missing_sg);
  }

  return true;
}
