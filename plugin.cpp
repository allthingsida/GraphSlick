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
* @brief 
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
* @brief Graph data/context
*/
struct grdata_t
{
private:
  gnodemap_t node_map;
  ea_t ea;

public:
  int cur_node;
  const char *err;
  graph_viewer_t *gv;
  intseq_t sel_nodes;
  TForm *form;
  grdata_t **parent_ref;

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
    err = NULL;
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
  * @brief Build a mutable graph from a function address
  */
  bool func_to_mgraph(
    mutable_graph_t *mg, 
    ea_t ea = BADADDR)
  {
    // No address passed anew? Used the one passed in the ctor
    if (ea == BADADDR)
      ea = this->ea;

    // Build function's flowchart
    qflow_chart_t qf;
    if (!get_func_flowchart(ea, qf))
    {
      this->err = STR_CANNOT_BUILD_F_FC;
      return false;
    }

    // Resize the graph
    size_t nodes_count = qf.size();
    mg->resize(nodes_count);

    // Build the node cache and edges
    for (size_t n=0; n < nodes_count; n++)
    {
      qbasic_block_t &block = qf.blocks[n];
      gnode_t *nc = node_map.add(n);

      // Generate disassembly text
      text_t txt;
      gen_disasm_text(
        block.startEA, 
        block.endEA, 
        txt, 
        false);

      // Append all lines to this node's text
      for (text_t::iterator it=txt.begin(); it != txt.end(); ++it)
      {
        nc->text.append(it->line);
        nc->text.append("\n");
      }

      // Build edges
      for (size_t isucc=0, succ_sz=qf.nsucc(n); isucc < succ_sz; isucc++)
      {
        int nsucc = qf.succ(n, isucc);
        mg->add_edge(n, nsucc, NULL);
      }
    }
    return true;
  }

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

  /**
  * @brief 
  * @param 
  * @return
  */
  static int idaapi _gr_callback(
      void *ud, 
      int code, va_list va)
  {
    return ((grdata_t *)ud)->gr_callback(code, va);
  }

  /**
  * @brief 
  * @param 
  * @return
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
          func_to_mgraph(mg);
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
static grdata_t *show_graph(ea_t ea = BADADDR)
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
    qstring title;

    HWND hwnd = NULL;
    title.sprnt("Combined Graph of %a()", f->startEA);
    TForm *form = create_tform(title.c_str(), &hwnd);
    if (hwnd != NULL)
    {
      // get a unique graph id
      netnode id;
      title.insert(0, "$ ");
      id.create(title.c_str());
      grdata_t *ctx = new grdata_t(f->startEA);
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
        nodeloc_t *loc = chn.gm->find_nodeid_loc(0);
        if (loc == NULL || loc->nl->empty())
        {
          msg("Could not show graph from this selection!\n");
          return;
        }

        gr = show_graph(loc->nl->begin()->start);
        if (gr != NULL)
          gr->parent_ref = &gr;
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
    gr = show_graph((*nodes->begin())->start);
    if (gr != NULL)
      gr->parent_ref = &gr;
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

    for (pgroupdef_list_t::iterator it=gm->get_groups()->begin();
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