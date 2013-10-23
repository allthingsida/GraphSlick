#include "algo.hpp"

//--------------------------------------------------------------------------
/**
* @brief Build a mutable graph from a function address
*/
bool func_to_mgraph(
    ea_t ea,
    mutable_graph_t *mg,
    gnodemap_t &node_map)
{
  // Build function's flowchart
  qflow_chart_t qf;
  if (!get_func_flowchart(ea, qf))
    return false;

  // Resize the graph
  int nodes_count = qf.size();
  mg->resize(nodes_count);

  // Build the node cache and edges
  for (int n=0; n < nodes_count; n++)
  {
    qbasic_block_t &block = qf.blocks[n];
    gnode_t *nc = node_map.add(n);

    // Generate disassembly text
    nc->text.sprnt("ID(%d)\n", n);
    get_disasm_text(block.startEA, block.endEA, &nc->text);

    // Build edges
    for (int isucc=0, succ_sz=qf.nsucc(n); isucc < succ_sz; isucc++)
    {
      int nsucc = qf.succ(n, isucc);
      mg->add_edge(n, nsucc, NULL);
    }
  }
  return true;
}
