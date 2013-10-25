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
                                - Factored out many code into various modules
10/24/2013 - eliasb             - Added proper coloring on selection (via colorgen class)
                                - Factored out many code into various modules
                 								- Fixed crash on re-opening the plugin chooser
10/25/2013 - eliasb             - Refactored the code further
								                - Renamed from grdata_t to a full graphview class (gsgraphview_t)
                                - Devised the graph context menu system
								                - Added clear/toggle selection mode
								                - Added graph view mode functionality
								                - Dock the gsgraphview next to IDA-View
*/

#pragma warning(disable: 4018 4800)

#include <ida.hpp>
#include <idp.hpp>
#include <graph.hpp>
#include <loader.hpp>
#include <kernwin.hpp>
#include "groupman.h"
#include "util.h"
#include "algo.hpp"
#include "colorgen.h"

//--------------------------------------------------------------------------
#define MY_TABSTR "    "

//--------------------------------------------------------------------------
static const char STR_CANNOT_BUILD_F_FC[] = "Cannot build function flowchart!";
static const char STR_GS_PANEL[]          = "Graph Slick - Panel";
static const char STR_GS_VIEW[]           = "Graph Slick - View";
static const char STR_OUTWIN_TITLE[]      = "Output window";
static const char STR_IDAVIEWA_TITLE[]    = "IDA View-A";

//--------------------------------------------------------------------------
typedef std::map<int, bgcolor_t> ncolormap_t;

const bgcolor_t NODE_SEL_COLOR = 0x7C75AD;

//--------------------------------------------------------------------------
enum gvrefresh_modes_e
{
  gvrfm_soft,
  gvrfm_single_mode,
  gvrfm_combined_mode,
};

//--------------------------------------------------------------------------
/**
* @brief Graph data/context
*/
struct gsgraphview_t
{
private:
  struct menucbctx_t
  {
    gsgraphview_t *gv;
    qstring name;
  };
  typedef std::map<int, menucbctx_t> idmenucbtx_t;
  static idmenucbtx_t menu_ids;

  gnodemap_t node_map;
  pndl2id_t ndl2id;
  ea_t func_ea;
  gvrefresh_modes_e refresh_mode, current_graph_mode;

  gsgraphview_t   **parent_ref;

  int idm_clear_sel;
  int idm_set_sel_mode;

  int idm_single_mode;
  int idm_combined_mode;

  bool in_sel_mode;

public:
  int             cur_node;
  graph_viewer_t  *gv;
  ncolormap_t     sel_nodes;
  TForm           *form;
  groupman_t      *gm;


private:
  /**
  * @brief Static menu item dispatcher
  */
  static bool idaapi s_menu_item_callback(void *ud)
  {
    int id = (int)ud;
    idmenucbtx_t::iterator it = menu_ids.find(id);
    if (it == menu_ids.end())
      return false;

    it->second.gv->on_menu(id);

    return true;
  }

  /**
  * @brief Menu items handler
  */
  void on_menu(int menu_id)
  {
    // Clear selection
    if (menu_id == idm_clear_sel)
    {
      clear_selection();
    }
    // Selection mode change
    else if (menu_id == idm_set_sel_mode)
    {
      // Toggle selection mode
      set_sel_mode(!in_sel_mode);
    }
    else if (menu_id == idm_single_mode)
    {
      refresh(gvrfm_single_mode);
    }
    else if (menu_id == idm_combined_mode)
    {
      refresh(gvrfm_combined_mode);
    }
  }

  /**
  * @brief Clear the selection
  */
  void clear_selection()
  {
    sel_nodes.clear();
    refresh(gvrfm_soft);
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
    return ((gsgraphview_t *)ud)->gr_callback(code, va);
  }

  /**
  * @brief Graph callback
  */
  int idaapi gr_callback(
        int code,
        va_list va)
  {
    int result = 0;
    switch (code)
    {
      //
      // graph is being clicked
      //
      case grcode_clicked:
      {
        va_arg(va, graph_viewer_t *);
        selection_item_t *item1 = va_arg(va, selection_item_t *);
        va_arg(va, graph_item_t *);
        if (in_sel_mode && item1 != NULL && item1->is_node)
          toggle_select_node(item1->node);

        break;
      }

      //
      // a new graph node became the current node
      //
      case grcode_changed_current:
      {
        va_arg(va, graph_viewer_t *);
        cur_node = va_argi(va, int);

        break;
      }

      //
      // A group is being created
      //
      case grcode_creating_group:
      {
        //mutable_graph_t *mg = va_arg(va, mutable_graph_t *);
        //intset_t *nodes = va_arg(va, intset_t *);

        //msg("grcode_creating_group(%p):", mg);
        //for (intset_t::iterator it=nodes->begin();
        //     it != nodes->end();
        //     ++it)
        //{
        //  msg("%d,", *it);
        //}
        //msg("\n");
        //result = 1;
        // out: 0-ok, 1-forbid group creation
        break;
      }

      //
      // A group is being deleted
      //
      case grcode_deleting_group:
      {
        // in:  mutable_graph_t *g
        //      int old_group
        // out: 0-ok, 1-forbid group deletion
        break;
      }

      //
      // New graph has been set
      //
      case grcode_changed_graph:
      {
        // in: mutable_graph_t *g
        mutable_graph_t *mg = va_arg(va, mutable_graph_t *);
        // out: must return 0
        //msg("grcode_changed_graph: %p\n", mg);
        break;
      }

      //
      // Redraw the graph
      //
      case grcode_user_refresh:
      {
        mutable_graph_t *mg = va_arg(va, mutable_graph_t *);
        if (node_map.empty() || refresh_mode != gvrfm_soft)
        {
          // Clear previous graph node data
          mg->clear();
          node_map.clear();
          ndl2id.clear();
		  
    		  // Remember the current graph mode
          current_graph_mode = refresh_mode;
		  
		      // Switch to the desired mode
          if (refresh_mode == gvrfm_single_mode)
            func_to_mgraph(func_ea, mg, node_map);
          else if (refresh_mode == gvrfm_combined_mode)
            fc_to_combined_mg(func_ea, gm, node_map, ndl2id, mg);
        }
        result = 1;
        break;
      }

      //
      // Retrieve text and background color for the user-defined graph node
      //
      case grcode_user_text:    
      {
        va_arg(va, mutable_graph_t *);
        int node           = va_arg(va, int);
        const char **text  = va_arg(va, const char **);
        bgcolor_t *bgcolor = va_arg(va, bgcolor_t *);

        *text = get_node(node)->text.c_str();
        ncolormap_t::iterator psel = sel_nodes.find(node);
        if (bgcolor != NULL && psel != sel_nodes.end())
          *bgcolor = psel->second;

        result = 1;
        break;
      }
      //
      // The graph is being destroyed
      //
      case grcode_destroyed:
      {
        gv = NULL;
        form = NULL;

        // Parent ref set?
        if (parent_ref != NULL)
        {
          // Clear this variable in the parent
          *parent_ref = NULL;
        }

        delete this;
        break;
      }
    }
    return result;
  }

  /**
  * @brief Set the selection mode
  */
  void set_sel_mode(bool sel_mode)
  {
    // Previous mode was set?
    if (idm_set_sel_mode != -1)
    {
      // Delete previous menu item
      del_menu(idm_set_sel_mode);
    }

    idm_set_sel_mode = add_menu(
          sel_mode ? "End selection mode" : "Start selection mode", "S");

    in_sel_mode = sel_mode;
  }

public:

  /**
  * @brief Set refresh mode and issue a refresh
  */
  void refresh(gvrefresh_modes_e rm)
  {
    refresh_mode = rm;
    refresh_viewer(gv);
  }

  /**
  * @brief Set the parent ref variable
  */
  inline void set_parentref(gsgraphview_t **pr)
  {
    parent_ref = pr;
  }

  /**
  * @brief Creates and shows the graph
  */
  static gsgraphview_t *show_graph(
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

    // Loop twice: 
    // - (1) Create the graph and exit or close it if it was there 
    // - (2) Re create graph due to last step
    for (int init=0; init<2; init++)
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

        // Create a graph object
        gsgraphview_t *gsgv = new gsgraphview_t(f->startEA);

        // Assign the groupmanager instance
        gsgv->gm = gm;

        // Create the graph control
        graph_viewer_t *gv = create_graph_viewer(
          form,  
          id, 
          gsgv->_gr_callback, 
          gsgv, 
          0);

        open_tform(form, FORM_TAB|FORM_MENU|FORM_QWIDGET);
        if (gv != NULL)

          gsgv->init(gv, form);

        return gsgv;
      }
      else
      {
        close_tform(form, 0);
      }
    }
    return NULL;
  }

  /**
  * @brief 
  */
  int add_menu(
    const char *name,
    const char *hotkey = NULL)
  {
    static int id = 0;
    ++id;

    // Create a menu context
    menucbctx_t ctx;
    ctx.gv = this;
    ctx.name = name;

    menu_ids[id] = ctx;

    bool ok = viewer_add_menu_item(
      gv,
      name,
      s_menu_item_callback,
      (void *)id,
      hotkey,
      0);

    if (!ok)
      menu_ids.erase(id);

    return ok ? id : -1;
  }

  /**
  * @brief Delete a menu item
  */
  void del_menu(int menu_id)
  {
    idmenucbtx_t::iterator it = menu_ids.find(menu_id);
    if (it == menu_ids.end())
      return;

    viewer_del_menu_item(
      it->second.gv->gv, 
      it->second.name.c_str());

    menu_ids.erase(menu_id);
  }

  /**
  * @brief Constructor
  */
  gsgraphview_t(ea_t func_ea): func_ea(func_ea)
  {
    cur_node = 0;
    gv = NULL;
    form = NULL;
    refresh_mode = gvrfm_single_mode;
    set_parentref(NULL);
    in_sel_mode = false;
    idm_set_sel_mode = -1;
  }

  /**
  * @brief Initialize the graph view
  */
  void init(graph_viewer_t *gv, TForm *form)
  {
    this->gv = gv;
    this->form = form;
    viewer_fit_window(gv);
    viewer_center_on(gv, 0);

    idm_clear_sel     = add_menu("Clear selection", "D");
    idm_single_mode   = add_menu("Switch to ungroupped view", "U");
    idm_combined_mode = add_menu("Switch to groupped view", "G");

    set_sel_mode(in_sel_mode);
  }

  void toggle_select_node(int cur_node)
  {
    ncolormap_t::iterator p = sel_nodes.find(cur_node);
    if (p == sel_nodes.end())
      sel_nodes[cur_node] = NODE_SEL_COLOR;
    else
      sel_nodes.erase(p);
    refresh(gvrfm_soft);
  }


};
gsgraphview_t::idmenucbtx_t gsgraphview_t::menu_ids;


//--------------------------------------------------------------------------
enum gsch_node_type_t
{
  chnt_gm  = 0,
  chnt_gd  = 1,
  chnt_nl  = 3,
};

//--------------------------------------------------------------------------
class gschooser_node_t
{
public:
  gsch_node_type_t type;
  groupman_t *gm;
  groupdef_t *gd;
  nodegroup_listp_t *ngl;
  pnodedef_list_t nl;

  gschooser_node_t()
  {
    gm = NULL;
    gd = NULL;
    ngl = NULL;
    nl = NULL;
  }
};

typedef qvector<gschooser_node_t> chooser_node_vec_t;

//--------------------------------------------------------------------------
/**
* @brief GraphSlick chooser class
*/
class gschooser_t
{
private:
  static gschooser_t *singleton;
  chooser_node_vec_t ch_nodes;

  chooser_info_t chi;
  gsgraphview_t *gsgv;
  groupman_t *gm;

  static uint32 idaapi s_sizer(void *obj)
  {
    return ((gschooser_t *)obj)->on_get_size();
  }

  static void idaapi s_getl(void *obj, uint32 n, char *const *arrptr)
  {
    ((gschooser_t *)obj)->on_get_line(n, arrptr);
  }

  static uint32 idaapi s_del(void *obj, uint32 n)
  {
    return ((gschooser_t *)obj)->on_delete(n);
  }

  static void idaapi s_ins(void *obj)
  {
    ((gschooser_t *)obj)->on_insert();
  }

  static void idaapi s_enter(void *obj, uint32 n)
  {
    ((gschooser_t *)obj)->on_enter(n);
  }

  static void idaapi s_refresh(void *obj)
  {
    ((gschooser_t *)obj)->on_refresh();
  }

  static void idaapi s_initializer(void *obj)
  {
    ((gschooser_t *)obj)->on_init();
  }

  static void idaapi s_destroyer(void *obj)
  {
    ((gschooser_t *)obj)->on_destroy();
  }

  static void idaapi s_select(void *obj, const intvec_t &sel)
  {
    ((gschooser_t *)obj)->on_select(sel);
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

  /**
  * @brief Handles instant node selection in the chooser
  */
  void on_select(const intvec_t &sel)
  {
    // Delegate this task to the 'enter' routine
    select_node(sel[0]-1);
  }

  /**
  * @brief Return the items count
  */
  uint32 on_get_size()
  {
    return ch_nodes.size();
  }

  /**
  * @brief Return chooser line description
  */
  void get_node_desc(gschooser_node_t *node, qstring *out)
  {
    switch (node->type)
    {
      // Handle a group file node
      case chnt_gm:
      {
        *out = qbasename(node->gm->get_source_file());
        break;
      }
      // Handle group definitions
      case chnt_gd:
      {
        out->sprnt(MY_TABSTR "%s (%s) NGL(%d)", 
          node->gd->groupname.c_str(), 
          node->gd->id.c_str(),
          node->ngl->size());
        break;
      }
      // Handle a node definition list
      case chnt_nl:
      {
        size_t sz = node->nl->size();
        out->sprnt(MY_TABSTR MY_TABSTR "NDL(%d):(", sz);
        for (nodedef_list_t::iterator it=node->nl->begin();
              it != node->nl->end();
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
    } // switch
  }

  /**
  * @brief Get textual representation of a given line
  */
  void on_get_line(uint32 n, char *const *arrptr)
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

      gschooser_node_t &cn = ch_nodes[n];
      qstring desc;
      get_node_desc(&cn, &desc);
      qstrncpy(arrptr[0], desc.c_str(), MAXSTR);
    }
  }

  /**
  * @brief 
  */
  uint32 on_delete(uint32 n)
  {
    // TODO: split a group
    return n;
  }

  /**
  * @brief 
  */
  void on_insert()
  {
    //TODO: askfolder()
    //      load the bbgroup file
  }

#define DECL_CG \
  colorgen_t cg; \
  cg.L_INT = -15; \
  colorvargen_t cv; \
  bgcolor_t clr

  /**
  * @brief Generates a color. Prefers first a color variant
  */
  bgcolor_t get_color_anyway(
      colorgen_t &cg, 
      colorvargen_t &cv)
  {
    bgcolor_t clr;

    while (true)
    {
      // Get a color variant
      clr = cv.get_color();
      if (clr != 0)
        break;

      // No variant? Pick a new color
      if (!cg.get_colorvar(cv))
      {
        // No more colors, just rewind
        cg.rewind();
        cg.get_colorvar(cv);
      }
    }
    return clr;
  }

    /**
  * @brief Callback that handles ENTER or double clicks on a chooser node
  */
  void on_enter(uint32 n)
  {
    if (gsgv == NULL || gsgv->gv == NULL || !IS_SEL(n) || n > ch_nodes.size())
      return;

    int nid;
    gschooser_node_t &chn = ch_nodes[n-1];
    if (chn.type == chnt_nl && !chn.nl->empty())
    {
      // Get first node in this nodedef list
      nid = chn.nl->begin()->nid;
    }
    else if (   chn.type == chnt_gd 
             && !chn.gd->nodegroups.empty() )
    {
      nodegroup_listp_t *ng = &chn.gd->nodegroups;
      pnodedef_list_t ndl0 = &(*(*ng->begin()));
      if (ndl0->empty())
        return;
      nid = ndl0->begin()->nid;
    }
    else
    {
      return;
    }
    // Select the current node
    viewer_center_on(gsgv->gv, nid);
  }

  /**
  * @brief Callback that handles node selection
  */
  void select_node(uint32 n)
  {
    gschooser_node_t &chn = ch_nodes[n];

    switch (chn.type)
    {
      case chnt_gm:
      {
        DECL_CG;
 
        // Walk all groups
        groupdef_listp_t *groups = gm->get_groups();
        for (groupdef_listp_t::iterator it=groups->begin();
             it != groups->end();
             ++it)
        {
          // Get the group definition -> node groups in this def
          groupdef_t *gd = *it;

          // Assign a new color variant for each groupdef
          cg.get_colorvar(cv);
          for (nodegroup_listp_t::iterator it=gd->nodegroups.begin();
               it != gd->nodegroups.end();
               ++it)
          {
            // Use a new color variant for each NDL
            clr = get_color_anyway(cg, cv);
            pnodedef_list_t nl = *it;
            for (nodedef_list_t::iterator it = nl->begin();
              it != nl->end();
              ++it)
            {
              gsgv->sel_nodes[it->nid] = clr;
            }
          }
        }
        break;
      }
      // Handle double click
      case chnt_nl:
      case chnt_gd:
      {
        if (gsgv == NULL || gsgv->gv == NULL)
          break;

        DECL_CG;

        gsgv->sel_nodes.clear();
        if (chn.type == chnt_nl)
        {
          cg.get_colorvar(cv);
          clr = get_color_anyway(cg, cv);
          for (nodedef_list_t::iterator it = chn.nl->begin();
               it != chn.nl->end();
               ++it)
          {
            gsgv->sel_nodes[it->nid] = clr;
          }
        }
        // chnt_gd
        else
        {
          // Use one color for all the different group defs
          cg.get_colorvar(cv);
          for (nodegroup_listp_t::iterator it=chn.ngl->begin();
               it != chn.ngl->end();
               ++it)
          {
            // Use a new color variant for each NDL
            clr = get_color_anyway(cg, cv);
            pnodedef_list_t nl = *it;
            for (nodedef_list_t::iterator it = nl->begin();
                 it != nl->end();
                 ++it)
            {
              gsgv->sel_nodes[it->nid] = clr;
            }
          }
        }
        gsgv->refresh(gvrfm_soft);
        break;
      }
    }
  }

  /**
  * @brief The chooser is closed
  */
  void on_destroy()
  {
    close_graph();
    delete_singleton();
  }

  /**
  * @brief Handles chooser refresh request
  */
  void on_refresh()
  {
  }

  /**
  * @brief Handles chooser initialization
  */
  void on_init()
  {
    const char *fn;
    fn = "P:\\projects\\experiments\\bbgroup\\sample_c\\bin\\v1\\x86\\f1.bbgroup";
    //fn = "P:\\projects\\experiments\\bbgroup\\sample_c\\bin\\v1\\x86\\main.bbgroup";
    if (!load_file(fn))
      return;

    // Show the graph
    nodedef_listp_t *nodes = gm->get_nodes();
    nodedef_t *nd = *(nodes->begin());
    
    gsgv = gsgraphview_t::show_graph(nd->start, gm);
    if (gsgv != NULL)
      gsgv->set_parentref(&gsgv);
  }

  /**
  * @brief Initialize the chooser3 structure
  */
  void init_chi()
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
  }

public:
  /**
  * @brief Constructor
  */
  gschooser_t()
  {
    init_chi();

    gsgv = NULL;
    gm = NULL;
  }

  /**
  * @brief Destructor
  */
  ~gschooser_t()
  {
    //NOTE: IDA will close the chooser for us and thus the destroy callback will be called
  }

  /**
  * @brief Close the graph view
  */
  void close_graph()
  {
    // Make sure the gv was not closed independently
    if (gsgv == NULL || gsgv->form == NULL)
      return;

    // Close the graph-view hosting form
    close_tform(gsgv->form, 0);
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
    gschooser_node_t *node = &ch_nodes.push_back();
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
        pnodedef_list_t nl = *it;
        // Add the third-level node = nodedef
        node = &ch_nodes.push_back();
        node->type = chnt_nl;
        node->nl  = nl;
        node->ngl = &ngl;
        node->gm  = gm;
        node->gd  = &gd;
      }
    }
    return true;
  }

  /**
  * @brief Show the chooser
  */
  static void show()
  {
    if (singleton == NULL)
      singleton = new gschooser_t();

    choose3(&singleton->chi);
    set_dock_pos(STR_GS_PANEL, STR_OUTWIN_TITLE, DP_RIGHT);
    set_dock_pos(STR_GS_VIEW, STR_IDAVIEWA_TITLE, DP_INSIDE);
  }

};
gschooser_t *gschooser_t::singleton = NULL;

//--------------------------------------------------------------------------
//
//      PLUGIN CALLBACKS
//
//--------------------------------------------------------------------------


//--------------------------------------------------------------------------
void idaapi run(int /*arg*/)
{
  gschooser_t::show();
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