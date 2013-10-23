/*
GraphSlick (c) Elias Bachaalany
-------------------------------------

Plugin module

This module is responsible for handling IDA plugin code

History
--------

10/15/2013 - eliasb             - First version
10/21/2013 - eliasb             - Working chooser / graph renderer
10/22/2013 - eliasb             - Version with working selection of NodeDefLists and GroupDefs
                                - Now the graph-view closes when the panel closes
                                - Factored out functions from the grdata_t class
                                - Wrote initial combine nodes algorithm
10/23/2013 - eliasb             - Polished and completed the combine algorithm
*/

#pragma warning(disable: 4018 4800)

#include <ida.hpp>
#include <idp.hpp>
#include <graph.hpp>
#include <loader.hpp>
#include <kernwin.hpp>
#include "groupman.h"

//--------------------------------------------------------------------------
#define MY_TABSTR "    "

//--------------------------------------------------------------------------
static const char STR_CANNOT_BUILD_F_FC[] = "Cannot build function flowchart!";
static const char STR_GS_PANEL[]          = "Graph Slick - Panel";
static const char STR_GS_VIEW[]           = "Graph Slick - View";
static const char STR_OUTWIN_TITLE[]      = "Output window";
static const char STR_IDAVIEWA_TITLE[]    = "IDA View-A";

//--------------------------------------------------------------------------
/**
* @brief Node data class. It will be served from the graph callback
*/
struct gnode_t
{
  int id;
  qstring text;
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
/**
* @brief Build a function flowchart
*/
static bool get_func_flowchart(
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
/**
* @brief Get the disassembly text into a qstring
*/
static void get_disasm_text(
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
* @brief Build a mutable graph from a function address
*/
static bool func_to_mgraph(
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

//--------------------------------------------------------------------------
/**
* @brief Creates a mutable graph that have the combined nodes per the groupmanager
*        A class was used to simulate nested functions needed by the combine algo
*/
class fc_to_combined_mg_t
{
  // Create a mapping between single node ids and the nodedef list they belong to
  typedef std::map<nodedef_list_t *, int> ndl2id_t;
  ndl2id_t ndl2id;

  gnodemap_t *node_map;
  groupman_t *gm;
  qflow_chart_t *fc;
  bool show_nids_only;

  /**
  * @brief Create and return a groupped node ID
  */
  int get_ndlid(int n)
  {
    int ndl_id;

    // Find how this single node is defined in the group manager
    nodeloc_t *loc = gm->find_nodeid_loc(n);
    
    // Does this node have a group yet? (ndl)
    ndl2id_t::iterator it = ndl2id.find(loc->nl);
    if (it == ndl2id.end())
    {
      // Assign an auto-incr id
      ndl_id = ndl2id.size();  
      ndl2id[loc->nl] = ndl_id;

      // Initial text for this ndl is the current single node id
      gnode_t gn;
      gn.id = ndl_id;
      size_t t = loc->nl->size();
      for (nodedef_list_t::iterator it=loc->nl->begin();
           it != loc->nl->end();
           ++it)
      {
        if (show_nids_only)
        {
          gn.text.cat_sprnt("%d", it->nid);
          if (--t > 0)
            gn.text.append(", ");
        }
        else
        {
          qbasic_block_t &block = fc->blocks[it->nid];
          qstring s;
          get_disasm_text(
            block.startEA, 
            block.endEA, 
            &s);
          gn.text.append(s);
        }
      }

      // Cache the node data
      (*node_map)[ndl_id] = gn;
    }
    else
    {
      // Grab the ndl id
      ndl_id = it->second;
    }

    return ndl_id;
  }

  /**
  * @brief 
  */
  void build(
    qflow_chart_t &fc,
    groupman_t *gm,
    gnodemap_t &node_map,
    mutable_graph_t *mg)
  {
    // Take a reference to the local variables so they are used
    // in the other helper functions
    this->gm = gm;
    this->node_map = &node_map;
    this->fc = &fc;

    // Compute the total size of nodes needed for the combined graph
    // The size is the total count of node def lists in each group def
    size_t node_count = 0;
    for (groupdef_listp_t::iterator it=gm->get_groups()->begin();
         it != gm->get_groups()->end();
         ++it)
    {
      groupdef_t &gd = **it;
      node_count += gd.nodegroups.size();
    }

    // Resize the graph
    mg->resize(node_count);

    // Build the combined graph
    int snodes_count = fc.size();
    for (int n=0; n < snodes_count; n++)
    {
      // Figure out the combined node ID
      int ndl_id = get_ndlid(n);

      // Build the edges
      for (int isucc=0, succ_sz=fc.nsucc(n); isucc < succ_sz; isucc++)
      {
        // Get the successor node
        int nsucc = fc.succ(n, isucc);

        // This node belongs to the same NDL?
        int succ_ndlid = get_ndlid(nsucc);
        if (succ_ndlid == ndl_id)
        {
          // Do nothing, consider as one node
          continue;
        }
        // Add an edge
        mg->add_edge(ndl_id, succ_ndlid, NULL);
      }
    }
  }
public:
  /**
  * @brief 
  */
  fc_to_combined_mg_t(): show_nids_only(false)
  {
  }

  /**
  * @brief 
  */
  bool operator()(
    ea_t ea,
    groupman_t *gm,
    gnodemap_t &node_map,
    mutable_graph_t *mg)
  {
    // Build function's flowchart
    qflow_chart_t fc;
    if (!get_func_flowchart(ea, fc))
      return false;

    build(fc, gm, node_map, mg);
    return true;
  }
};

//--------------------------------------------------------------------------
/**
* @brief Graph data/context
*/
struct grdata_t
{
private:
  gnodemap_t node_map;
  ea_t ea;

public:
  int             cur_node;
  graph_viewer_t  *gv;
  intseq_t        sel_nodes;
  TForm           *form;
  grdata_t        **parent_ref;
  groupman_t      *gm;

  enum refresh_modes_e
  {
    rfm_soft    = 0,
    rfm_rebuild = 1,
  };

  int refresh_mode;

  /**
  * @brief Constructor
  */
  grdata_t(ea_t ea)
  {
    cur_node = 0;
    gv = NULL;
    this->ea = ea;
    form = NULL;
    refresh_mode = rfm_soft;
    parent_ref = NULL;
  }

  /**
  * @brief Return node data
  */
  gnode_t *get_node(int nid)
  {
    return node_map.get(nid);
  }

  /**
  * @brief Static graph callback
  */
  static int idaapi _gr_callback(
      void *ud, 
      int code, va_list va)
  {
    return ((grdata_t *)ud)->gr_callback(code, va);
  }

  /**
  * @brief 
  */
  int idaapi gr_callback(
    int code,
    va_list va)
  {
    int result = 0;
    switch (code)
    {
      case grcode_changed_current:
      {
        va_arg(va, graph_viewer_t *);
        cur_node = va_argi(va, int);
        //TODO: select all nodes in its group (NDL)
        break;
      }

      // Redraw the graph
      case grcode_user_refresh:
      {
        mutable_graph_t *mg = va_arg(va, mutable_graph_t *);
        if (node_map.empty() || refresh_mode == rfm_rebuild)
        {
          //func_to_mgraph(this->ea, mg, node_map);
          //;!;!
          fc_to_combined_mg_t()(this->ea, gm, node_map, mg);
        }
        result = 1;
        break;
      }

      // retrieve text and background color for the user-defined graph node
      case grcode_user_text:    
      {
        va_arg(va, mutable_graph_t *);
        int node           = va_arg(va, int);
        const char **text  = va_arg(va, const char **);
        bgcolor_t *bgcolor = va_arg(va, bgcolor_t *);

        *text = get_node(node)->text.c_str();
        if (bgcolor != NULL && sel_nodes.contains(node))
          *bgcolor = 0xcaf4cb;
        result = 1;
        break;
      }
      case grcode_destroyed:
      {
        gv = NULL;
        form = NULL;
        if (parent_ref != NULL)
          *parent_ref = NULL;

        delete this;
        result = 1;
        break;
      }
    }
    return result;
  }
};

//--------------------------------------------------------------------------
static grdata_t *show_graph(
    ea_t ea = BADADDR, 
    groupman_t *gm = NULL)
{
  if (ea == BADADDR)
    ea = get_screen_ea();

  func_t *f = get_func(ea);

  if (f == NULL)
  {
    msg("No function here!\n");
    return NULL;
  }

  for (int i=0;i<2;i++)
  {

    HWND hwnd = NULL;
    TForm *form = create_tform(STR_GS_VIEW, &hwnd);
    if (hwnd != NULL)
    {
      // get a unique graph id
      netnode id;
      qstring title;
      title.sprnt("$ Combined Graph of %a()", f->startEA);
      id.create(title.c_str());
      grdata_t *ctx = new grdata_t(f->startEA);
      ctx->gm = gm;
      graph_viewer_t *gv = create_graph_viewer(
        form,  
        id, 
        ctx->_gr_callback, 
        ctx, 
        0);
      open_tform(form, FORM_TAB|FORM_MENU|FORM_QWIDGET);
      if (gv != NULL)
      {
        ctx->gv = gv;
        ctx->form = form;
        viewer_fit_window(gv);
      }
      return ctx;
    }
    else
    {
      close_tform(form, 0);
    }
  }
  return NULL;
}

//--------------------------------------------------------------------------
/**
* @brief 
*/
class mychooser_t
{
private:
  static mychooser_t *singleton;
  enum chooser_node_type_t
  {
    chnt_gm  = 0,
    chnt_gd  = 1,
    chnt_nl  = 3,
  };

  class chooser_node_t
  {
  public:
    int type;
    groupman_t *gm;
    groupdef_t *gd;
    nodegroup_listp_t *ngl;
    nodedef_list_t *nl;

    chooser_node_t()
    {
      gm = NULL;
      gd = NULL;
      ngl = NULL;
      nl = NULL;
    }

    /**
    * @brief 
    * @param 
    * @return
    */
    void get_desc(qstring *out)
    {
      switch (type)
      {
        case chnt_gm:
        {
          *out = qbasename(gm->get_source_file());
          break;
        }
        case chnt_gd:
        {
          out->sprnt(MY_TABSTR "%s (%s) NGL(%d)", 
            gd->groupname.c_str(), 
            gd->id.c_str(),
            ngl->size());
          break;
        }
        case chnt_nl:
        {
          size_t sz = nl->size();
          out->sprnt(MY_TABSTR MY_TABSTR "NDL(%d):(", sz);
          for (nodedef_list_t::iterator it=nl->begin();
               it != nl->end();
               ++it)
          {
            nodedef_t *nd = &*it;
            out->cat_sprnt("%d:%a:%a", nd->nid, nd->start, nd->end);
            if (--sz > 0)
              out->append(", ");
          }
          out->append(")");
          break;
        }
      }
    }
  };

  typedef qvector<chooser_node_t> chooser_node_vec_t;
  chooser_node_vec_t ch_nodes;

  chooser_info_t chi;
  grdata_t *gr;
  groupman_t *gm;

  static uint32 idaapi s_sizer(void *obj)
  {
    return ((mychooser_t *)obj)->sizer();
  }

  static void idaapi s_getl(void *obj, uint32 n, char *const *arrptr)
  {
    ((mychooser_t *)obj)->getl(n, arrptr);
  }

  static uint32 idaapi s_del(void *obj, uint32 n)
  {
    return ((mychooser_t *)obj)->del(n);
  }

  static void idaapi s_ins(void *obj)
  {
    ((mychooser_t *)obj)->ins();
  }

  static void idaapi s_enter(void *obj, uint32 n)
  {
    ((mychooser_t *)obj)->enter(n);
  }

  static void idaapi s_refresh(void *obj)
  {
    ((mychooser_t *)obj)->refresh();
  }

  static void idaapi s_initializer(void *obj)
  {
    ((mychooser_t *)obj)->initializer();
  }

  static void idaapi s_destroyer(void *obj)
  {
    ((mychooser_t *)obj)->destroyer();
  }

  static void idaapi s_select(void *obj, const intvec_t &sel)
  {
    ((mychooser_t *)obj)->select(sel);
  }

  /**
  * @brief Delete the singleton instance if applicable
  */
  void delete_singleton()
  {
    if ((chi.flags & CH_MODAL) != 0)
      return;

    delete singleton;
    singleton = NULL;
  }

  void select(const intvec_t &sel)
  {
    enter(sel[0]);
  }

  /**
  * @brief Return the items count
  */
  uint32 sizer()
  {
    return ch_nodes.size();
  }

  /**
  * @brief Get textual representation of a given line
  */
  void getl(uint32 n, char *const *arrptr)
  {
    // Return the column name
    if (n == 0)
    {
      qstrncpy(arrptr[0], "Node", MAXSTR);
      return;
    }
    // Return description about a node
    else if (n >= 1)
    {
      --n;
      if (n >= ch_nodes.size())
        return;

      chooser_node_t &cn = ch_nodes[n];
      qstring desc;
      cn.get_desc(&desc);
      qstrncpy(arrptr[0], desc.c_str(), MAXSTR);
    }
  }

  uint32 del(uint32 n)
  {
    // nop
    return n;
  }

  void ins()
  {
    //TODO: askfolder()
    //      load the bbgroup file
  }

  void enter(uint32 n)
  {
    if (!IS_SEL(n) || n > ch_nodes.size())
      return;

    chooser_node_t &chn = ch_nodes[n-1];

    switch (chn.type)
    {
      case chnt_gm:
      {
        //TODO: remove me or really handle multiple files
        //TODO: this method can simply switch the working graph data ...so it can still work w/ multiple bbgroup files
        break;
        nodeloc_t *loc = chn.gm->find_nodeid_loc(0);
        if (loc == NULL || loc->nl->empty())
        {
          msg("Could not show graph from this selection!\n");
          return;
        }

        gr = show_graph(loc->nl->begin()->start, gm);
        if (gr != NULL)
        {
          //TODO
          gr->parent_ref = &gr;
        }
        break;
      }
      // Handle double click
      case chnt_nl:
      case chnt_gd:
      {
        if (gr == NULL || gr->gv == NULL)
          break;

        gr->sel_nodes.clear();
        if (chn.type == chnt_nl)
        {
          for (nodedef_list_t::iterator it = chn.nl->begin();
               it != chn.nl->end();
               ++it)
          {
            gr->sel_nodes.add(it->nid);
          }
        }
        else for (nodegroup_listp_t::iterator it=chn.ngl->begin();
                  it != chn.ngl->end();
                  ++it)
        {
          nodedef_list_t *nl = *it;
          for (nodedef_list_t::iterator it = nl->begin();
               it != nl->end();
               ++it)
          {
            gr->sel_nodes.add(it->nid);
          }
        }

        refresh_viewer(gr->gv);
        break;
      }
    }
  }

  /**
  * @brief Close the graph view
  */
  void close_graph()
  {
    if (gr == NULL && gr->form != NULL)
      return;
    close_tform(gr->form, 0);
  }

  /**
  * @brief The chooser is closed
  */
  void destroyer()
  {
    close_graph();
    delete_singleton();
  }

  /**
  * @brief 
  * @param 
  * @return
  */
  void idaapi refresh()
  {
  }

  void idaapi initializer()
  {
    if (!load_file("P:\\projects\\experiments\\bbgroup\\sample_c\\bin\\v1\\x86\\f1.bbgroup"))
      return;

    // Show the graph
    nodedef_listp_t *nodes = gm->get_nodes();
    gr = show_graph((*nodes->begin())->start, gm);
    if (gr != NULL)
    {
      //TODO:
      gr->parent_ref = &gr;
    }
  }

public:
  mychooser_t()
  {
    memset(&chi, 0, sizeof(chi));
    chi.cb = sizeof(chi);
    chi.flags = 0;
    chi.width = -1;
    chi.height = -1;
    chi.title = STR_GS_PANEL;
    chi.obj = this;
    chi.columns = 1;

    static const int widths[] = {60};
    chi.widths = widths;

    chi.icon  = -1;
    chi.deflt = -1;

    chi.sizer       = s_sizer;
    chi.getl        = s_getl;
    chi.ins         = s_ins;
    chi.del         = s_del;
    chi.enter       = s_enter;
    chi.destroyer   = s_destroyer;
    chi.refresh     = s_refresh;
    chi.select      = s_select;
    chi.initializer = s_initializer;

    //chi.popup_names = NULL;   // first 5 menu item names (insert, delete, edit, refresh, copy)
    //static uint32 idaapi *s_update(void *obj, uint32 n);
    //void (idaapi *edit)(void *obj, uint32 n);
    //static int idaapi s_get_icon)(void *obj, uint32 n);
    //void (idaapi *get_attrs)(void *obj, uint32 n, chooser_item_attrs_t *attrs);

    gr = NULL;
    gm = NULL;
  }

  /**
  * @brief Load the file bbgroup file into the chooser
  */
  bool load_file(const char *filename)
  {
    // Load a file and parse it
    delete gm;
    gm = new groupman_t();
    if (!gm->parse(filename))
    {
      msg("error: failed to parse group file '%s'\n", filename);
      delete gm;
      return false;
    }

    // Add the first-level node = bbgroup file
    chooser_node_t *node = &ch_nodes.push_back();
    node->type = chnt_gm;
    node->gm = gm;

    for (groupdef_listp_t::iterator it=gm->get_groups()->begin();
         it != gm->get_groups()->end();
         ++it)
    {
      groupdef_t &gd = **it;

      // Add the second-level node = a set of group defs
      node = &ch_nodes.push_back();
      nodegroup_listp_t &ngl = gd.nodegroups;
      node->type = chnt_gd;
      node->gd   = &gd;
      node->gm   = gm;
      node->ngl  = &ngl;

      // Add each nodedef list within each node group
      for (nodegroup_listp_t::iterator it = ngl.begin();
           it != ngl.end();
           ++it)
      {
        nodedef_list_t *nl = *it;
        // Add the third-level node = nodedef
        node = &ch_nodes.push_back();
        node->type = chnt_nl;
        node->nl = nl;
        node->ngl = &ngl;
        node->gm  = gm;
        node->gd  = &gd;
      }
    }
    return true;
  }

  static void show()
  {
    if (singleton == NULL)
      singleton = new mychooser_t();
    choose3(&singleton->chi);
    set_dock_pos(STR_GS_PANEL, STR_OUTWIN_TITLE, DP_RIGHT);
  }

  ~mychooser_t()
  {
    //NOTE: IDA will close the chooser for us and thus the destroy callback will be called
  }
};
mychooser_t *mychooser_t::singleton = NULL;

//--------------------------------------------------------------------------
//
//      PLUGIN CALLBACKS
//
//--------------------------------------------------------------------------


//--------------------------------------------------------------------------
void idaapi run(int /*arg*/)
{
  mychooser_t::show();
}

//--------------------------------------------------------------------------
int idaapi init(void)
{
  return (callui(ui_get_hwnd).vptr != NULL || is_idaq()) ? PLUGIN_OK : PLUGIN_SKIP;
}

//--------------------------------------------------------------------------
void idaapi term(void)
{
}

//--------------------------------------------------------------------------
//
//      PLUGIN DESCRIPTION BLOCK
//
//--------------------------------------------------------------------------
plugin_t PLUGIN =
{
  IDP_INTERFACE_VERSION,
  0,                    // plugin flags
  init,                 // initialize

  term,                 // terminate. this pointer may be NULL.

  run,                  // invoke plugin

  "",                   // long comment about the plugin
                        // it could appear in the status line
                        // or as a hint

  "",                   // multiline help about the plugin

  "GraphSlick",         // the preferred short name of the plugin
  "Ctrl-4"              // the preferred hotkey to run the plugin
};
//P:\projects\experiments\bbgroup\sample_c\bin\v1\x86\main.bbgroup