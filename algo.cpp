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
void build_groupman_from_fc(
    qflow_chart_t *fc,
    groupman_t *gm,
    bool sanitize)
{
  // Clear previous groupman contents
  gm->clear();

  gm->src_filename = "noname.bbgroup";

  // Resize the graph
  int nodes_count = fc->size();

  // Build groupman
  for (int nid=0; nid < nodes_count; nid++)
  {
    qbasic_block_t &block = fc->blocks[nid];

    psupergroup_t sg = gm->add_supergroup();
    sg->id.sprnt("ID_%d", nid);
    sg->name.sprnt("SG_%d", nid);
    sg->is_synthetic = false;

    pnodegroup_t ng = sg->add_nodegroup();
    pnodedef_t   nd = ng->add_node();

    nd->nid = nid;
    nd->start = block.startEA;
    nd->end = block.endEA;

    gm->map_nodedef(nid, nd);
  }

  if (sanitize)
  {
    if (sanitize_groupman(BADADDR, gm, fc))
      gm->initialize_lookups();
  }
}

//--------------------------------------------------------------------------
void build_groupman_from_3dvec(
  qflow_chart_t *fc,
  int_3dvec_t &path,
  groupman_t *gm,
  bool sanitize)
{
  // Clear previous groupman contents
  gm->clear();

  gm->src_filename = "noname.bbgroup";

  // Build groupman
  int sg_id = 0;
  for (int_3dvec_t::iterator it_sg=path.begin();
       it_sg != path.end();
       ++it_sg, ++sg_id)
  {
    // Build super group
    psupergroup_t sg = gm->add_supergroup();

    sg->id.sprnt("ID_%d", sg_id);
    sg->name.sprnt("SG_%d", sg_id);
    sg->is_synthetic = false;

    // Build SG
    int_2dvec_t &ng_vec = *it_sg;
    for (int_2dvec_t::iterator it_ng= ng_vec.begin();
         it_ng != ng_vec.end();
         ++it_ng)
    {
      // Build NG
      pnodegroup_t ng = sg->add_nodegroup();
      intvec_t &nodes_vec = *it_ng;

      // Build nodes
      for (intvec_t::iterator it_nd = nodes_vec.begin();
           it_nd != nodes_vec.end();
           ++it_nd)
      {
        int nid = *it_nd;
        qbasic_block_t &block = fc->blocks[nid];

        pnodedef_t nd = ng->add_node();
        nd->nid = nid;
        nd->start = block.startEA;
        nd->end = block.endEA;

        gm->map_nodedef(nid, nd);
      }
    }
  }

  if (sanitize)
  {
    if (sanitize_groupman(BADADDR, gm, fc))
      gm->initialize_lookups();
  }
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

  nid2ndef_t *nds = gm->get_nds();
  // Verify that all nodes are present
  for (int n=0; n < nodes_count; n++)
  {
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
