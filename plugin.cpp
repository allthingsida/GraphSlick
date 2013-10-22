/*
GraphSlick (c) Elias Bachaalany
-------------------------------------

Plugin module

This module is responsible for handling IDA plugin code

History
--------

10/15/2013 - eliasb             - First version
*/

#pragma warning(disable: 4018 4800)

#include <ida.hpp>
#include <idp.hpp>
#include <graph.hpp>
#include <loader.hpp>
#include <kernwin.hpp>
#include "groupman.h"

//--------------------------------------------------------------------------
static const char STR_CANNOT_BUILD_F_FC[] = "Cannot build function flowchart!";

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
  graph_viewer_t **ref_gv;

  /**
  * @brief Constructor
  */
  grdata_t(ea_t ea)
  {
    cur_node = 0;
    err = NULL;
    ref_gv = NULL;
    this->ea = ea;
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
        break;
      }

      // Redraw the graph
      case grcode_user_refresh:
      {
        mutable_graph_t *mg = va_arg(va, mutable_graph_t *);
        func_to_mgraph(mg);
        result = 1;
        break;
      }

      // retrieve text for user-defined graph node
      case grcode_user_text:    
      {
        va_arg(va, mutable_graph_t *);
        int node           = va_arg(va, int);
        const char **text  = va_arg(va, const char **);
        va_arg(va, bgcolor_t *);

        *text = get_node(node)->text.c_str();

        result = 1;
        break;
      }
      case grcode_destroyed:
      {
        delete this;
        result = 1;
        break;
      }
    }
    return result;
  }
};

//--------------------------------------------------------------------------
static void show_graph(
    ea_t ea = BADADDR, 
    graph_viewer_t **out_gv = NULL)
{
  if (ea == BADADDR)
    ea = get_screen_ea();

  func_t *f = get_func(ea);

  if (f == NULL)
  {
    msg("No function here!\n");
    return;
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
        viewer_fit_window(gv);

      ctx->ref_gv = out_gv;

      break;
    }
    else
    {
      close_tform(form, 0);
    }
  }
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
    chnt_ngl = 2,
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
          out->sprnt("  %s (%s)", gd->groupname.c_str(), gd->id.c_str());
          break;
        }
        case chnt_ngl:
        {
          out->sprnt("    NGL(%d)", ngl->size());
          break;
        }
        case chnt_nl:
        {
          *out = "      ";
          for (nodedef_list_t::iterator it=nl->begin();
               it != nl->end();
               ++it)
          {
            nodedef_t *nd = &*it;
            out->cat_sprnt("%d:%a:%a", nd->nid, nd->start, nd->end);
          }
          break;
        }
      }
    }
  };

  typedef qvector<chooser_node_t> chooser_node_vec_t;
  chooser_node_vec_t ch_nodes;

  chooser_info_t chi;


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

  uint32 sizer()
  {
    return ch_nodes.size();
  }

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
    if (!IS_SEL(n) || n >= ch_nodes.size())
      return;


    chooser_node_t &chn = ch_nodes[n-1];
    nodeloc_t *loc = chn.gm->find_nodeid_loc(0);
    if (loc == NULL || loc->nl->empty())
    {
      msg("Could not show graph from this selection!\n");
      return;
    }

    show_graph(loc->nl->begin()->start);
  }

  void destroyer()
  {
    delete_singleton();
  }

  void idaapi refresh()
  {
  }

  void idaapi initializer()
  {
    load_file("P:\\projects\\experiments\\bbgroup\\sample_c\\bin\\v1\\x86\\f1.bbgroup");
  }

public:
  mychooser_t()
  {
    memset(&chi, 0, sizeof(chi));
    chi.cb = sizeof(chi);
    chi.flags = 0;
    chi.width = -1;
    chi.height = -1;
    chi.title = "Graph Slick";
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
    chi.initializer = s_initializer;

    //chi.popup_names = NULL;   // first 5 menu item names (insert, delete, edit, refresh, copy)
    //static uint32 idaapi *s_update(void *obj, uint32 n);
    //void (idaapi *edit)(void *obj, uint32 n);
    //static int idaapi s_get_icon)(void *obj, uint32 n);
    //void (idaapi *get_attrs)(void *obj, uint32 n, chooser_item_attrs_t *attrs);
    //static void idaapi s_select(void *obj, const intvec_t & sel);
  }

  /**
  * @brief Load the file bbgroup file into the chooser
  */
  bool load_file(const char *filename)
  {
    // Load a file and parse it
    groupman_t *gm = new groupman_t();
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
      node->type = chnt_gd;
      node->gd   = &gd;
      node->gm   = gm;

      // Add the second-level node = a set of group defs
      nodegroup_listp_t &ngl = gd.nodegroups;
      node = &ch_nodes.push_back();
      node->type = chnt_ngl;
      node->ngl = &ngl;
      node->gm  = gm;
      node->gd  = &gd;

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