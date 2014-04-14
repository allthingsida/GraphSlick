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
                                - Factored out get_color_anyway() to the colorgen module
                                - Proper support for node selection/coloring in single and combined mode
                                - Change naming from chooser nodes to chooser line
10/28/2013 - eliasb             - Separated highlight and selection
                                - Added support for 'user hint' on the node graph
                                - Added 'Load bbgroup' support
                                - Added quick selection support (no UI for it though)
                                - Added reload input file functionality
10/29/2013 - eliasb             - Added 'Highlight similar nodes' placeholder code and UI
                                - Refactored chooser population code into 'populate_chooser_lines()'
                                - Speed optimization: generate flowchart once in the chooser and pass it to other functions								
                                - Added find_and_highlight_nodes() to the graph								
10/30/2013 - eliasb             - Adapted to new class names
                                - Added to-do placeholder for parameters that could be passed as options
10/31/2013 - eliasb             - Get rid of the 'additive' parameter in set_highlighted_nodes
                                - renamed set_highlighted_nodes() to highlight_nodes(). it also checks whether to highlight or not
                                  (thus the caller does not have to do redundant highlight/lazy checks)
                                - Introduced the options class and parameterized all relevant function calls
                                - Added 'refresh view' and 'show options' context menus								
                                - Introduced get_ng_id() helper function to resolve node ids automatically based on current view mode
                                - Fixed wrong node id resolution upon double click on chooser lines								
11/01/2013 - eliasb             - Introduced redo_layout() to actually refresh and make the layout
                                - refresh_view() is a placeholder for a hack that will cause a screen repaint
                                - renamed 'lazy_sh_mode' to 'manual_refresh_mode'
                                - added a second chooser column to show EA of first ND in the NG
                                - got rid of 'refresh view' context menu item -> redundant with IDA's layout graph
11/04/2013 - eliasb             -  added 'debug' option
                                - "Edit SG" is no more dynamic. It seems it is not possible to add menu item inside the grcallback / refresh cb
                                - Added "Combine nodes" functionality
                                - refactored graph mode view switch into functions
                                - Added get_ng_from_ngid(ngid) utility function to do reverse ng2nid lookup
                                - Added  ngid_to_sg(ngid) utility function to do ngid to containing SG lookup
                                - Added gsgv_actions_t to allow better interfacing between GSGV and GSCH
                                - Added Jump to next highlighted/selected nodes
                                - Added save bbgroup functionality
                                - Added chooser on_show() event -> show copyright message on start
11/06/2013 - eliasb             - Added select all
                                - Added graph layout selection
                                - Added focus_node after a group or split operation
                                - Added Promote nodes
                                - Added "Move nodes to their own NGs"
                                - Added "Merge highlight with selection"
                                - Added "Jump to next highlight" / "Selection"
                                - Added chooser menu -> show graph (in case the graph was closed)
                                - Added "Reset groupping"
11/07/2013 - eliasb             - Removed the Orthogonal layout was not implemented in IDA <=6.4, causing a "not yet" messages
                                - Added initial Python adapter code

TODO
-----------

[ ] Add chooser that shows only SGs so one can move an NG to the dest SG
[ ] Ungroup -> move NDs to own SGs
[ ] Add code to handle operations group/ungroup from chooser
[ ] Expose the py::analyze function to chooser context menu
[ ] Augment bbload to call py::load ; same for SAVE
[ ] py::load should return
[ ] bug: somehow ungroupping all nodes is not correct, when analyze is used -> i experienced a crash
*/

#pragma warning(disable: 4018 4800)

#include <ida.hpp>
#include <idp.hpp>
#include <graph.hpp>
#include <loader.hpp>
#include <kernwin.hpp>
#include <diskio.hpp>
#include "groupman.h"
#include "util.h"
#include "algo.hpp"
#include "colorgen.h"
#include "pybbmatcher.h"

//--------------------------------------------------------------------------
#define MY_TABSTR "    "

#define STR_GS_MSG "GS: "

//--------------------------------------------------------------------------
static const char STR_CANNOT_BUILD_F_FC[] = "Cannot build function flowchart!";
static const char STR_PLGNAME[]           = "GraphSlick";
static const char TITLE_GS_PANEL[]        = "Graph Slick - Panel";
static const char STR_GS_VIEW[]           = "Graph Slick - View";
static const char STR_OUTWIN_TITLE[]      = "Output window";
static const char STR_IDAVIEWA_TITLE[]    = "IDA View-A";
static const char STR_SEARCH_PROMPT[]     = "Please enter search string";
static const char STR_DUMMY_SG_NAME[]     = "No name";
static const char STR_GS_PY_PLGFILE[]     = "GraphSlick\\init.py";

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
#define DECL_CG \
  colorgen_t cg; \
  cg.L_INT = -15


//--------------------------------------------------------------------------
/**
* @brief GraphSlick options handling class
*/
struct gsoptions_t
{
  /**
  * @brief Append node id to the node text
  */
  bool append_node_id;

  /**
  * @brief Do not propose initial path information on Analyze()
  */
  bool no_initial_path_info;

  /**
  * @brief Manual refresh view on selection/highlight
  */
  bool manual_refresh_mode;

  /**
  * @brief Highlight synthetic nodes 
  */
  bool highlight_syntethic_nodes;

  /**
  * @brief Should the options dialog be shown the next time?
  */
  bool show_options_dialog_next_time;

  /**
  * @brief If the group name is one line then add a few more lines
  *        to make it look bigger
  */
  bool enlarge_group_name;

  /**
  * @brief Display debug messages
  */
  bool debug;

  /**
  * @brief Graph layout
  */
  layout_type_t graph_layout;

  /**
  * @brief GraphSlick start up view mode
  */
  gvrefresh_modes_e start_view_mode;

  /**
  * @brief Constructor
  */
  gsoptions_t()
  {
    manual_refresh_mode = true;
    append_node_id = false;
    highlight_syntethic_nodes = false;
    show_options_dialog_next_time = true;
    enlarge_group_name = true;
    start_view_mode = gvrfm_single_mode; //gvrfm_combined_mode;//gvrfm_single_mode;
    debug = true;
    graph_layout = layout_digraph;
    //;!
    no_initial_path_info = false;
  }

  /**
  * @brief Show the options dialog
  */
  void show_dialog()
  {
    // TODO: askusingform
  }

  /**
  * @brief Load options from the current database
  */
  void load_options()
  {
    //TODO: load from netnode
  }

  /**
  * @brief Save options to the current database
  */
  void save_options()
  {
    // TODO: save to netnode
  }
};

//--------------------------------------------------------------------------
/**
* @brief GSGraph actions. It allows parents to get notified on GSGV actions
*/
class gsgv_actions_t
{
public:
  /**
  * @brief The GSGV is closing
  */
  virtual void notify_close() = 0;
  /**
  * @brief The GSGV content is refreshing
  */
  virtual void notify_refresh(bool hard_refresh = false) = 0;

  /**
  * @brief Find nodes similar to the highlighted ones
  */
  virtual pnodegroup_list_t find_similar(intvec_t &sel_nodes) = 0;
};

//--------------------------------------------------------------------------
/**
* @brief Graph data/context
*/
class gsgraphview_t
{
public:
  /**
  * @brief Currently selected node
  */
  int cur_node;

  /**
  * @brief Node to focus on after a group/split
  */
  int focus_node;

  /**
  * @brief Pointer to the graph viewer
  */
  graph_viewer_t  *gv;

  /**
  * @brief Pointer to the form hosting the graph viewer
  */
  TForm *form;

  /**
  * @brief Pointer to the associated group manager class
  */
  groupman_t *gm;

  /**
  * @brief GraphSlick options
  */
  gsoptions_t *options;

private:
  struct menucbctx_t
  {
    gsgraphview_t *gsgv;
    qstring name;
  };
  typedef std::map<int, menucbctx_t> idmenucbtx_t;
  static idmenucbtx_t menu_ids;

  gnodemap_t node_map;
  ng2nid_t ng2id;
  qflow_chart_t *func_fc;
  gvrefresh_modes_e refresh_mode, cur_view_mode;

  gsgv_actions_t *actions;

  /**
  * @brief Menu item IDs
  */
  int idm_single_view_mode, idm_combined_view_mode;

  int idm_clear_sel, idm_clear_highlight, idm_select_all;
  int idm_merge_highlight_with_selection;
  int idm_jump_next_selected_node, idm_jump_next_highlighted_node;
  int idm_set_sel_mode;

  int idm_edit_sg_desc;
  int idm_change_graph_layout;

  int idm_remove_nodes_from_group;
  int idm_promote_node_groups;
  int idm_reset_groupping;

  int idm_test;
  int idm_highlight_similar, idm_find_highlight;

  int idm_combine_ngs;

  int idm_show_options;

  bool in_sel_mode;

  ncolormap_t     highlighted_nodes;
  ncolormap_t     selected_nodes;

  ncolormap_t::iterator it_selected_node, it_highlighted_node;

  /**
  * @brief Static menu item dispatcher
  */
  static bool idaapi s_menu_item_callback(void *ud)
  {
    int id = (int)ud;
    idmenucbtx_t::iterator it = menu_ids.find(id);
    if (it == menu_ids.end())
      return false;

    it->second.gsgv->on_menu(id);

    return true;
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
  * @brief Displays an error when a node is not found!
  */
  void msg_err_node_not_found(int nid=0)
  {
    if (options->debug)
      msg(STR_GS_MSG "Error, node(%d) not found!\n", nid);
  }

  void msg_unk_mode()
  {
    if (options->debug)
      msg(STR_GS_MSG "Unknown mode\n");
  }

  /**
  * @brief Menu items handler
  */
  void on_menu(int menu_id)
  {
    //
    // Clear selection
    //
    if (menu_id == idm_clear_sel)
    {
      clear_selection(options->manual_refresh_mode);
    }
    //
    // Clear highlighted nodes
    //
    else if (menu_id == idm_clear_highlight)
    {
      clear_highlighting(options->manual_refresh_mode);
    }
    //
    // Select all nodes
    //
    else if (menu_id == idm_select_all)
    {
      select_all_nodes();
    }
    //
    // Selection mode change
    //
    else if (menu_id == idm_set_sel_mode)
    {
      // Toggle selection mode
      set_sel_mode(!in_sel_mode);
    }
    //
    // Switch to single view mode
    //
    else if (menu_id == idm_single_view_mode)
    {
      redo_layout(gvrfm_single_mode);
    }
    //
    // Switch to combined view mode
    //
    else if (menu_id == idm_combined_view_mode)
    {
      redo_layout(gvrfm_combined_mode);
    }
    //
    // Show the options dialog
    //
    else if (menu_id == idm_show_options)
    {
      options->show_dialog();
    }
    //
    // Highlight similar node group
    //
    else if (menu_id == idm_highlight_similar)
    {
      highlight_similar_selection(options->manual_refresh_mode);
    }
    //
    // Find and highlight supergroup
    //
    else if (menu_id == idm_find_highlight)
    {
      find_and_highlight_nodes(options->manual_refresh_mode);
    }
    //
    // Change the current graph layout
    //
    else if (menu_id == idm_change_graph_layout)
    {
      int code = askbuttons_c(
        "Circle", // YES
        "Tree",   // NO
        "Digraph", // CANCEL
        ASKBTN_YES, 
        "Please select layout type");
      switch (code)
      {
        case ASKBTN_YES:
          options->graph_layout = layout_circle;
          break;
        case ASKBTN_NO:
          options->graph_layout = layout_tree;
          break;
        case ASKBTN_CANCEL:
          options->graph_layout = layout_digraph;
          break;
      }
      redo_layout(cur_view_mode);
    }
    //
    // Edit supergroup description
    //
    else if (menu_id == idm_edit_sg_desc)
    {
      // Check the view mode and selection
      if (   cur_view_mode != gvrfm_combined_mode 
          || cur_node == -1)
      {
        msg(STR_GS_MSG "Incorrect view mode or no nodes are selected\n");
        return;
      }

      psupergroup_t sg = get_sg_from_ngid(cur_node);
      if (sg == NULL)
        return;

      if (edit_sg_description(sg))
      {
        // Notify that a refresh is taking place
        actions->notify_refresh();
      }
    }
    //
    // Combine node groups
    //
    else if (menu_id == idm_combine_ngs)
    {
      if (selected_nodes.size() <= 1)
      {
        msg(STR_GS_MSG "Not enough selected nodes\n");
        return;
      }
      combine_node_groups();
    }
    //
    // Jump to next selected node
    //
    else if (menu_id == idm_jump_next_selected_node)
    {
      jump_to_next_node(it_selected_node, selected_nodes);
    }
    //
    // Jump to next highlighted node
    //
    else if (menu_id == idm_jump_next_highlighted_node)
    {
      jump_to_next_node(it_highlighted_node, highlighted_nodes);
    }
    //
    // Remove nodes from a node group into their own individual node groups
    //
    else if (menu_id == idm_remove_nodes_from_group)
    {
      move_nodes_to_own_ng();
    }
    //
    // Merge highlight with selection
    //
    else if (menu_id == idm_merge_highlight_with_selection)
    {
      merge_highlight_with_selection();
    }
    //
    // Merge highlight with selection
    //
    else if (menu_id == idm_promote_node_groups)
    {
      promote_node_groups_to_sgs();
    }
    //
    // Reset groupping
    //
    else if (menu_id == idm_reset_groupping)
    {
      gm->reset_groupping();

      // Refresh the chooser
      actions->notify_refresh(true);

      // Re-layout
      redo_current_layout();
    }
    //
    // Test: interactive groupping
    //
    else if (menu_id == idm_test)
    {
      selected_nodes.clear();
      int sel[] = {1,3,4};
      intvec_t sel_nodes;
      for (int i=0;i<qnumber(sel);i++)
      {
        int nid = sel[i];
        selected_nodes[nid] = NODE_SEL_COLOR;
        sel_nodes.push_back(nid);
      }

      pnodegroup_list_t ngl = actions->find_similar(sel_nodes);

      DECL_CG;
      highlight_nodes(ngl, cg, options->manual_refresh_mode);
      ngl->free_nodegroup(false);
      delete ngl;
    }
  }

#ifdef MY_DEBUG
  /**
  * @brief 
  * @param
  * @return
  */
  void _DUMP_NG(const char *str, pnodegroup_t ng)
  {
    for (nodegroup_t::iterator it=ng->begin();
         it != ng->end();
         ++it)
    {
      pnodedef_t nd = *it;
      msg("%s: p=%p id=%d s=%a e=%a\n", str, nd, nd->nid, nd->start, nd->end);
    }
  }
#endif

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
        {
          toggle_select_node(
            item1->node, 
            options->manual_refresh_mode);
        }

        // don't ignore the click
        result = 0;
        break;
      }

      //
      // a new graph node became the current node
      //
      case grcode_changed_current:
      {
        va_arg(va, graph_viewer_t *);

        // Remember the current node
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
          reset_states();

          mg->current_layout = options->graph_layout;
          mg->circle_center = point_t(200, 200);
          mg->circle_radius = 100;

          // Remember the current graph mode
          // NOTE: we remember the state only if not 'soft'. 
          //       Otherwise it will screw up all the logic that rely on its value
          cur_view_mode = refresh_mode;
		  
          // Switch to the desired mode
          if (refresh_mode == gvrfm_single_mode)
            switch_to_single_view_mode(mg);
          else if (refresh_mode == gvrfm_combined_mode)
            switch_to_combined_view_mode(mg);
          else
            msg_unk_mode();
        }
        mg->redo_layout();
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

        // Retrieve the node text
        gnode_t *gnode = get_node(node);
        if (gnode == NULL)
        {
          result = 0;
          break;
        }

        *text = gnode->text.c_str();

        // Caller requested a bgcolor?
        if (bgcolor != NULL) do
        {
          // Selection has priority over highlight
          ncolormap_t::iterator psel = selected_nodes.find(node);
          if (psel == selected_nodes.end())
          {
            psel = highlighted_nodes.find(node);
            if (psel == highlighted_nodes.end())
              break;
          }
          // Pass the color
          *bgcolor = psel->second;
        } while (false);

        result = 1;
        break;
      }

      //
      // retrieve hint for the user-defined graph
      //
      case grcode_user_hint:
      {
        va_arg(va, mutable_graph_t *);
        int mousenode = va_arg(va, int);
        va_arg(va, int); // mouseedge_src
        va_arg(va, int); // mouseedge_dst
        char **hint = va_arg(va, char **);

        // Get node data, aim for 'hint' field then 'text'
        gnode_t *node_data;
        if (     mousenode != -1 
             && (node_data = get_node(mousenode)) != NULL )
        {
          qstring *s = &node_data->hint;
          if (s->empty())
            s = &node_data->text;

          // 'hint' must be allocated by qalloc() or qstrdup()
          *hint = qstrdup(s->c_str());

          // out: 0-use default hint, 1-use proposed hint
          result = 1;
        }
        break;
      }

      //
      // The graph is being destroyed
      //
      case grcode_destroyed:
      {
        gv = NULL;
        form = NULL;

        actions->notify_close();

        delete this;
        break;
      }
    }
    return result;
  }

  /**
  * @brief Resets state variables upon view mode change
  */
  void reset_states()
  {
    // Clear node information
    node_map.clear();
    ng2id.clear();

    // Clear highlight / selected
    highlighted_nodes.clear();
    selected_nodes.clear();

    // Clear highlight / selection iterators
    it_selected_node = selected_nodes.end();
    it_highlighted_node = highlighted_nodes.end();

    // No node is selected
    cur_node = -1;
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

    const char *labels[] = 
    {
      "Start selection mode",
      "End selection mode"
    };
    const char *label = labels[sel_mode ? 1 : 0];

    idm_set_sel_mode = add_menu(label, "S");

    in_sel_mode = sel_mode;
    msg(STR_GS_MSG "Trigger again to '%s'\n", label);
  }

public:

  /**
  * @brief Return graph view node id from an actual node id
  */
  int get_gvnid_from_nid(int nid)
  {
    if (cur_view_mode == gvrfm_single_mode)
      return nid;
    
    nodeloc_t *loc = gm->find_nodeid_loc(nid);
    // Get the other selected NG
    if (loc == NULL)
      return -1;
    else
      return ng2id.get_ng_id(loc->ng);
  }

  /**
  * @brief Return supergroup for which a nodegroup_id belongs to
  */
  psupergroup_t get_sg_from_ngid(int ngid)
  {
    // The current node is a node group id
    // Convert ngid to a node id
    pnodegroup_t ng = get_ng_from_ngid(cur_node);
    if (ng == NULL)
      return NULL;
    else
      return get_sg_from_ng(ng);
  }

  /**
  * @brief Convert a node group id to a nodegroup instance
  */
  pnodegroup_t get_ng_from_ngid(int ngid)
  {
    //TODO: PERFORMANCE: make a lookup table if needed
    for (ng2nid_t::iterator it=ng2id.begin();
         it != ng2id.end();
         ++it)
    {
      if (it->second == ngid)
        return it->first;
    }
    return NULL;
  }

  /**
  * @brief Return node data
  */
  inline gnode_t *get_node(int nid)
  {
    return node_map.get(nid);
  }

  /**
  * @brief Return a node id corresponding to the given node group. 
  *        The current view mode is respected
  */
  inline int get_ngid_from_ng(pnodegroup_t ng)
  {
    if (ng != NULL)
    {
      if (cur_view_mode == gvrfm_combined_mode)
      {
	    // Get the nodegroup id from the map
        return ng2id.get_ng_id(ng);
      }
      else if (cur_view_mode == gvrfm_single_mode)
      {
	    // Just get the node id of the first node definition in the node group
        pnodedef_t nd = ng->get_first_node();
        return nd == NULL ? -1 : nd->nid;
      }
    }
    if (options->debug)
      msg(STR_GS_MSG "Could not find gr_nid for %p\n", ng);
    return -1;
  }

  /**
  * @brief Return the super group that hosts the NG
  */
  psupergroup_t get_sg_from_ng(pnodegroup_t ng)
  {
    pnodedef_t nd = ng->get_first_node();
    if (nd == NULL)
      return NULL;

    nodeloc_t *loc = gm->find_nodeid_loc(nd->nid);
    if (loc == NULL)
      return NULL;
    else
      return loc->sg;
  }
  /**
  * @brief Clear the selection
  */
  void clear_selection(bool delay_refresh)
  {
    selected_nodes.clear();
    it_selected_node = selected_nodes.end();
    if (!delay_refresh)
      refresh_view();
  }

  /**
  * @brief Clear the highlighted nodes
  */
  void clear_highlighting(bool delay_refresh)
  {
    highlighted_nodes.clear();
    it_highlighted_node = highlighted_nodes.end();
    if (!delay_refresh)
      refresh_view();
  }

  /**
  * @brief Highlights a group
  */
  bool highlight_nodes(
      pnodegroup_t ng, 
      bgcolor_t clr,
      bool delay_refresh)
  {
    intset_t newly_colored;

    // Combined mode?
    if (cur_view_mode == gvrfm_combined_mode)
    {
      int gr_nid = get_ngid_from_ng(ng);
      if (gr_nid == -1)
        return false;

      if (delay_refresh)
        newly_colored.insert(gr_nid);

      highlighted_nodes[gr_nid] = clr;
    }
    // Single view mode?
    else if (cur_view_mode == gvrfm_single_mode)
    {
      // Add each node in the definition to the selection
      for (nodegroup_t::iterator it = ng->begin();
           it != ng->end();
           ++it)
      {
        int nid = (*it)->nid;

        if (delay_refresh)
          newly_colored.insert(nid);

        highlighted_nodes[nid] = clr;
      }
    }
    // Unknown mode
    else
    {
      msg_unk_mode();
      return false;
    }

    // In delayed refresh mode, just print what we plan to highlight
    if (delay_refresh)
    {
      if (options->debug)
      {
        // Just print
        msg(STR_GS_MSG "Lazy highlight( ");
        size_t t = newly_colored.size();
        for (intset_t::iterator it=newly_colored.begin();
              it != newly_colored.end();
              ++it)
        {
          int nid = *it;
          if (cur_view_mode == gvrfm_single_mode)
          {
            pnodedef_t nd = (*gm->get_nds())[nid];
            if (nd == NULL)
              continue;

            msg("%d : %a : %a ", nd->nid, nd->start, nd->end);
            if (--t > 0)
              msg(", ");
          }
          else
          {
            msg("%d ", nid);
          }
        }
        msg(")\n");
      }
    }
    // Refresh immediately
    else
    {
      refresh_view();
    }
    return true;
  }

  /**
  * @brief Highlight a nodegroup list
  */
  void highlight_nodes(
          pnodegroup_list_t ngl, 
          colorgen_t &cg,
          bool delay_refresh)
  {
    // Use one color for all the different group defs
    colorvargen_t cv;
    cg.get_colorvar(cv);

    for (nodegroup_list_t::iterator it=ngl->begin();
         it != ngl->end();
         ++it)
    {
      // Use a new color variant for each node group
      bgcolor_t clr = cg.get_color_anyway(cv);
      pnodegroup_t ng = *it;

      // Always call with delayed refresh mode in the inner loop
      highlight_nodes(
          ng, 
          clr,
          true);
    }

    // Since we called with delayed refresh mode, now see if refresh is needed
    if (!delay_refresh)
      refresh_view();
  }

  /**
  * @brief Selects all set of super groups
  */
  void highlight_nodes(
    psupergroup_listp_t groups,
    colorgen_t &cg,
    bool delay_refresh)
  {
    colorvargen_t cv;
    for (supergroup_listp_t::iterator it=groups->begin();
         it != groups->end();
         ++it)
    {
      // Get the super group
      psupergroup_t sg = *it;

      // - Super group is synthetic?
      // - User does not want us to color such sgs?
      if (    sg->is_synthetic 
           && !options->highlight_syntethic_nodes)
      {
        // Don't highlight syntethic super groups
        continue;
      }

      // Assign a new color variant for each group
      cg.get_colorvar(cv);
      for (nodegroup_list_t::iterator it=sg->groups.begin();
           it != sg->groups.end();
           ++it)
      {
        // Use a new color variant for each group
        bgcolor_t clr = cg.get_color_anyway(cv);
        pnodegroup_t ng = *it;

        // Always call with lazy mode in the inner loop
        highlight_nodes(
            ng, 
            clr, 
            true);
      }
    }

    // Since we were called with delayed refresh mode, now see if refresh is needed
    if (!delay_refresh)
      refresh_view();
  }

  /**
  * @brief Highlight nodes similar to the selection
  */
  void highlight_similar_selection(bool delay_refresh)
  {
    if (selected_nodes.empty())
      return;

    if (cur_view_mode != gvrfm_single_mode)
    {
      msg(STR_GS_MSG "Only the single view mode is supported\n");
      return;
    }

    // Convert selected nodes map to an intvec
    intvec_t sel_nodes;
    for (ncolormap_t::iterator it=selected_nodes.begin();
         it != selected_nodes.end();
         ++it)
    {
      sel_nodes.push_back(it->first);
    }

    pnodegroup_list_t ngl = actions->find_similar(sel_nodes);

    DECL_CG;
    highlight_nodes(ngl, cg, options->manual_refresh_mode);
    ngl->free_nodegroup(false);
    delete ngl;
  }

  /**
  * @brief Select all nodes
  */
  void select_all_nodes()
  {
    selected_nodes.clear();
    for (nid2ndef_t::iterator it = gm->get_nds()->begin();
         it != gm->get_nds()->end();
         ++it)
    {
      selected_nodes[it->second->nid] = NODE_SEL_COLOR;
    }
  }

  /**
  * @brief Toggle node selection
  */
  void toggle_select_node(
          int cur_node, 
          bool delay_refresh)
  {
    ncolormap_t::iterator p = selected_nodes.find(cur_node);
    if (p == selected_nodes.end())
      selected_nodes[cur_node] = NODE_SEL_COLOR;
    else
      selected_nodes.erase(p);

    // With quick selection mode, just display a message and don't force a refresh
    if (delay_refresh)
    {
      msg(STR_GS_MSG "Selected %d\n", cur_node);
    }
    else
    {
      // Refresh the graph to reflect selection
      refresh_view();
    }
  }

  /**
  * @brief Find and highlights nodes
  */
  void find_and_highlight_nodes(bool delay_refresh)
  {
    static char last_pattern[MAXSTR] = {0};

    const char *pattern = askstr(HIST_SRCH, last_pattern, STR_SEARCH_PROMPT);
    if (pattern == NULL)
      return;

    // Remember last search
    qstrncpy(
        last_pattern, 
        pattern, 
        sizeof(last_pattern));

    DECL_CG;

    clear_highlighting(true);

    pnodegroup_list_t groups = NULL;

    // Walk all the groups
    psupergroup_listp_t sgroups = gm->get_path_sgl();
    for (supergroup_listp_t::iterator it=sgroups->begin();
         it != sgroups->end();
         ++it)
    {
      psupergroup_t sg = *it;
      if (    stristr(sg->name.c_str(), pattern) != NULL
           || stristr(sg->id.c_str(), pattern) != NULL )
      {
        groups = &sg->groups;
        highlight_nodes(
          groups, 
          cg, 
          true);
      }
    }

    // Refresh graph if at least there is one match
    if (!delay_refresh)
	{
      refresh_view();
      if (groups == NULL)
        return;
	}

    int nid;
    pnodegroup_t ng = groups->get_first_ng();
    nid = ng == NULL ? -1 : get_ngid_from_ng(ng);

    if (nid != -1)
    {
      jump_to_node(
        gv, 
        nid);
    }
  }

  /**
  * @brief Merge the highlighted nodes with the selection. End result is more selection coming from highlight
  */
  void merge_highlight_with_selection()
  {
    for (ncolormap_t::iterator it=highlighted_nodes.begin();
         it != highlighted_nodes.end();
         ++it)
    {
      if (selected_nodes.find(it->first) != selected_nodes.end())
        continue;
      else
      {
        selected_nodes[it->first] = NODE_SEL_COLOR;
      }
    }
  }

  /**
  * @brief Jumps to next item in the container
  */
  void jump_to_next_node(ncolormap_t::iterator &it, ncolormap_t &cont)
  {
    if (cont.empty())
      return;

    // End? Rewind
    if (it == cont.end())
      it = cont.begin();

    jump_to_node(gv, it->first);

    // Advance to next match
    ++it;
  }

  /**
  * @brief Combine node groups
  */
  void combine_node_groups()
  {
    pnodegroup_t new_ng = NULL;
    if (cur_view_mode == gvrfm_combined_mode)
    {
      //
      // Make a nodegroup list from the selection
      //
      nodegroup_list_t ngl;
      for (ncolormap_t::iterator it = selected_nodes.begin();
           it != selected_nodes.end();
           ++it)
      {
        // Get the other selected NG
        pnodegroup_t ng = get_ng_from_ngid(it->first);
        ngl.push_back(ng);
      }

      // Combine the selected NGLs
      new_ng = gm->combine_ngl(&ngl);
    }
    else if (cur_view_mode == gvrfm_single_mode)
    {
      nodegroup_t ng;
      for (ncolormap_t::iterator it = selected_nodes.begin();
           it != selected_nodes.end();
           ++it)
      {
        // Find node structure
        nodeloc_t *loc = gm->find_nodeid_loc(it->first);
        if (loc == NULL)
        {
          msg_err_node_not_found();
          continue;
        }
        ng.add_node(loc->nd);
      }
      // Move the nodes from various NGs to a single NG
      new_ng = gm->move_nodes_to_ng(&ng);
    }

    // Edit the newly combined SG
    if (new_ng != NULL)
    {
      psupergroup_t sg = get_sg_from_ng(new_ng);
      edit_sg_description(sg);

      pnodedef_t nd = new_ng->get_first_node();;
      if (nd != NULL)
        focus_node = nd->nid;
    }

    // Refresh the chooser
    actions->notify_refresh(true);

    // Re-layout
    redo_current_layout();
  }

  /**
  * @brief Promote selected NGs to their own super groups
  */
  void promote_node_groups_to_sgs()
  {
    nodegroup_list_t ngs;
    std::map<pnodegroup_t, psupergroup_t> found_ng;
    pnodegroup_t  ng;
    psupergroup_t sg;
    for (ncolormap_t::iterator it=selected_nodes.begin();
         it != selected_nodes.end();
         ++it)
    {
      if (cur_view_mode == gvrfm_single_mode)
      {
        nodeloc_t *loc = gm->find_nodeid_loc(it->first);
        if (loc == NULL)
        {
          msg_err_node_not_found();
          continue;
        }
        ng = loc->ng;
        sg = loc->sg;
      }
      else if (cur_view_mode == gvrfm_combined_mode)
      {
        ng = get_ng_from_ngid(it->first);
        if (ng == NULL)
          continue;
        sg = get_sg_from_ng(ng);
      }
      else
      {
        msg_unk_mode();
        continue;
      }
      found_ng[ng] = sg;
    }

    // Now we have the NGs and their corresponding SGs
    while (!found_ng.empty())
    {
      // Take the first value set
      ng = found_ng.begin()->first;
      sg = found_ng.begin()->second;

      // Remove first element
      found_ng.erase(found_ng.begin());

      // SG has one NG? Most likely this is the same SG and NG, leave alone
      if (sg->gcount() == 1)
        continue;

      // Remove NG from the current SG
      sg->remove_nodegroup(ng, false);

      // Make a new SG
      psupergroup_t new_sg = gm->add_supergroup(gm->get_path_sgl());
      new_sg->copy_attr_from(sg);
      new_sg->add_nodegroup(ng);

      // Allow the user to edit the new SG
      edit_sg_description(new_sg);
    }

    // Reinitialize lookup tables
    gm->initialize_lookups();

    // Refresh the chooser; no need to re-do layout though
    actions->notify_refresh(true);

    // Re-layout
    redo_current_layout();
  }

  /**
  * @brief Move all nodes in selected node groups and put each ND into its own NG
  *        The SG of the node will remain the same
  */
  void move_nodes_to_own_ng()
  {
    if (selected_nodes.empty())
    {
      msg(STR_GS_MSG "No selection!\n");
      return;
    }
      
    //TODO: VERIFY: When find similar is applied, then this should work too

    if (cur_view_mode == gvrfm_single_mode)
    {
      // For each ND, directly take it out from its parent NG and put it in its own NG in the same SG
      // If the NG have one node and this node is self, then do nothing
      for (ncolormap_t::iterator it = selected_nodes.begin();
           it != selected_nodes.end();
           ++it)
      {
        nodeloc_t *loc = gm->find_nodeid_loc(it->first);
        if (loc == NULL)
        {
          msg_err_node_not_found();
          return;
        }
        // This node is the only one in the NG
        if (loc->ng->size() == 1)
          continue;

        // Now move the node out
        loc->ng->remove(loc->nd);

        // Create a new node group in the SG and add the node to it
        loc->sg->add_nodegroup()->add_node(loc->nd);

        // Remember a focus node
        focus_node = loc->nd->nid;
      }
    }
    else if (cur_view_mode == gvrfm_combined_mode)
    {
      // In combined mode, each node is an NG
      // Take each node out of the NG and put in the NG's SG
      for (ncolormap_t::iterator it = selected_nodes.begin();
           it != selected_nodes.end();
           ++it)
      {
        // Get the select NG
        pnodegroup_t ng = get_ng_from_ngid(it->first);
        if (ng == NULL || ng->size() == 1)
          continue;

        // Get any first node (to determine SG)
        pnodedef_t nd = ng->get_first_node();
        if (nd == NULL)
          continue;

        // Get the loc -> SG
        nodeloc_t *loc = gm->find_nodeid_loc(nd->nid);
        psupergroup_t sg = loc->sg;
        if (sg == NULL)
          continue;

        // Take out each ND in this NG
        while (ng->size() > 1)
        {
          nd = ng->back();
          ng->pop_back();

          sg->add_nodegroup()->add_node(nd);

          // Remember a focus node
          focus_node = nd->nid;
        }
      }
    }

    // Reinitialize lookup tables
    gm->initialize_lookups();

    // Refresh the chooser; no need to re-do layout though
    actions->notify_refresh(true);

    // Re-layout
    redo_current_layout();
  }

  /**
  * @brief Edit the description of a super group
  */
  bool edit_sg_description(psupergroup_t sg)
  {
    const char *desc;
    
    while (true)
    {
      desc = askstr(
        HIST_CMT,
        sg->get_display_name(STR_DUMMY_SG_NAME),
        "Please enter new description");

      if (desc == NULL)
        return false;

      if (   strchr(desc, ')') != NULL 
          || strchr(desc, '(') != NULL
          || strchr(desc, ':') != NULL
          || strchr(desc, ';') != NULL)
      {
        warning("The name cannot contain the following characters: '();:'");
        continue;
      }
      break;
    }

    // Adjust the name
    sg->name = desc;

    // From the super group, get all individual node groups
    for (nodegroup_list_t::iterator it=sg->groups.begin();
         it != sg->groups.end();
         ++it)
    {
      // Get the group node id from the node group reference
      pnodegroup_t ng = *it;
      int ngid = get_ngid_from_ng(ng);
      if (ngid == -1)
        continue;

      gnode_t *gnode = get_node(ngid);
      if (gnode == NULL)
        continue;;

      // Update the node display text
      // TODO: PERFORMANCE: can you have gnode link to a groupman related structure and pull its
      //                    text dynamically?
      gnode->text = sg->get_display_name();
    }

    if (!options->manual_refresh_mode)
      refresh_view();

    return true;
  }

  /**
  * @brief Switch to combined view mode
  */
  void switch_to_single_view_mode(mutable_graph_t *mg)
  {
    msg(STR_GS_MSG "Switching to single mode view...");
    func_to_mgraph(
      BADADDR, 
      mg, 
      node_map, 
      func_fc,
      options->append_node_id);
    msg("done\n");
  }

  /**
  * @brief Switch to combined view mode
  */
  void switch_to_combined_view_mode(mutable_graph_t *mg)
  {
    msg(STR_GS_MSG "Switching to combined mode view...");
    fc_to_combined_mg(
      BADADDR, 
      gm, 
      node_map, 
      ng2id, 
      mg, 
      func_fc);

    msg("done\n");
  }

  /**
  * @brief Add a context menu to the graphview
  */
  int add_menu(
    const char *name,
    const char *hotkey = NULL)
  {
    // Static ID for all menu item IDs
    static int id = 0;

    // Is this a separator menu item?
    bool is_sep = name[0] == '-' && name[1] == '\0';

    // Only remember this item if it was not a separator
    if (!is_sep)
    {
      ++id;

      // Create a menu context
      menucbctx_t ctx;
      ctx.gsgv = this;
      ctx.name = name;

      menu_ids[id] = ctx;
    }

    bool ok = viewer_add_menu_item(
      gv,
      name,
      is_sep ? NULL : s_menu_item_callback,
      (void *)id,
      hotkey,
      0);

    // Ignore return value for separator
    if (is_sep)
      return -1;

    if (!ok)
      menu_ids.erase(id);

    return ok ? id : -1;
  }

  /**
  * @brief Delete a context menu item
  */
  void del_menu(int menu_id)
  {
    if (menu_id == -1)
      return;

    idmenucbtx_t::iterator it = menu_ids.find(menu_id);
    if (it == menu_ids.end())
      return;

    viewer_del_menu_item(
      it->second.gsgv->gv, 
      it->second.name.c_str());

    menu_ids.erase(menu_id);
  }

  /**
  * @brief Refresh the current screen and the visible nodes (not the layout)
  */
  void refresh_view()
  {
    refresh_mode = gvrfm_single_mode;
    //TODO: HACK: trigger a window to show/hide so IDA repaints the nodes
  }

  /**
  * @brief Set refresh mode and issue a refresh
  */
  void redo_layout(gvrefresh_modes_e rm)
  {
    refresh_mode = rm;
    refresh_viewer(gv);
    if (focus_node != -1)
    {
      focus_node = get_gvnid_from_nid(focus_node);
      if (focus_node != -1)
        jump_to_node(gv, focus_node);
      focus_node = -1;
    }

  }

  /**
  * @brief Helper function to redo current layout
  */
  inline void redo_current_layout()
  {
    redo_layout(cur_view_mode);
  }

  /**
  * @brief Set the actions variable
  */
  inline void set_callback(gsgv_actions_t *actions)
  {
    this->actions = actions;
  }

  /**
  * @brief Creates and shows the graph
  */
  static gsgraphview_t *show_graph(
    qflow_chart_t *func_fc,
    groupman_t *gm,
    gsoptions_t *options)
  {
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
        title.sprnt("$ GS %s", func_fc->title.c_str());
        id.create(title.c_str());

        // Create a graph object
        gsgraphview_t *gsgv = new gsgraphview_t(func_fc, options);

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
  * @brief Initialize the graph view
  */
  void init(graph_viewer_t *gv, TForm *form)
  {
    this->gv = gv;
    this->form = form;
    viewer_fit_window(gv);
    viewer_center_on(gv, 0);

    //
    // Add the context menu items
    //

    add_menu("-");
    idm_show_options                  = add_menu("Show options",                    "O");

    // Highlighting / selection actions
    add_menu("-");
    idm_clear_sel                      = add_menu("Clear selection",                "D");
    idm_clear_highlight                = add_menu("Clear highlighting",             "H");
    idm_select_all                     = add_menu("Select all",                     "A");
    idm_merge_highlight_with_selection = add_menu("Merge highlight with selection", "V");

    // Cycling in selected/highlited nodes
    idm_jump_next_highlighted_node    = add_menu("Jump to next highlighted node",   "J");
    idm_jump_next_selected_node       = add_menu("Jump to next selected node",      "K");

    // Switch view mode actions
    add_menu("-");
    idm_change_graph_layout           = add_menu("Change graph layout");
    idm_single_view_mode              = add_menu("Switch to ungroupped view",       "U");
    idm_combined_view_mode            = add_menu("Switch to groupped view",         "G");

    // Experimental actions
    add_menu("-");
    idm_test                          = add_menu("Test",                            "Q");

    // Searching actions
    add_menu("-");
    idm_highlight_similar             = add_menu("Highlight similar nodes",         "M");
    idm_find_highlight                = add_menu("Find group",                      "F");

    //
    // Groupping actions

    idm_combine_ngs                   = add_menu("Combine nodes",                   "C");
    idm_remove_nodes_from_group       = add_menu("Move node(s) to their own group", "R");
    idm_promote_node_groups           = add_menu("Promote node group",              "P");
    idm_reset_groupping               = add_menu("Reset groupping",                 "T");

    // Edit group description menu
    idm_edit_sg_desc                  = add_menu("Edit group description",          "E");

    //
    // Dynamic menu items
    //
    
    // Set initial selection mode
    add_menu("-");
    set_sel_mode(in_sel_mode);
  }

  /**
  * @brief Constructor
  */
  gsgraphview_t(qflow_chart_t *func_fc, gsoptions_t *options): func_fc(func_fc), options(options)
  {
    gv = NULL;
    form = NULL;
    refresh_mode = options->start_view_mode;
    set_callback(NULL);

    focus_node = -1;
    in_sel_mode = false;
    cur_node = -1;
    idm_set_sel_mode = -1;
    idm_edit_sg_desc = -1;
  }

};
gsgraphview_t::idmenucbtx_t gsgraphview_t::menu_ids;

//--------------------------------------------------------------------------
/**
* @brief Types of lines in the chooser
*/
enum gsch_line_type_t
{
  chlt_gm  = 0,
  chlt_sg  = 1,
  chlt_ng  = 3,
};

//--------------------------------------------------------------------------
/**
* @brief Chooser line structure
*/
class gschooser_line_t
{
public:
  gsch_line_type_t type;
  groupman_t *gm;
  psupergroup_t sg;
  pnodegroup_t ng;
  pnodegroup_list_t ngl;

  /**
  * @brief Constructor
  */
  gschooser_line_t()
  {
    gm = NULL;
    sg = NULL;
    ng = NULL;
    ngl = NULL;
  }
};
typedef qvector<gschooser_line_t> chooser_lines_vec_t;

//--------------------------------------------------------------------------
/**
* @brief GraphSlick chooser class
*/
class gschooser_t: public gsgv_actions_t
{
private:
  static gschooser_t *singleton;
  chooser_lines_vec_t ch_nodes;

  chooser_info_t chi;
  gsgraphview_t *gsgv;
  groupman_t *gm;
  qstring last_loaded_file;

  qflow_chart_t func_fc;
  gsoptions_t options;

  PyBBMatcher *py_matcher;

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

  static void idaapi s_edit(void *obj, uint32 n)
  {
    ((gschooser_t *)obj)->on_edit_line(n);
  }

  static void idaapi s_select(void *obj, const intvec_t &sel)
  {
    ((gschooser_t *)obj)->on_select(sel);
  }

  static uint32 idaapi s_onmenu_save_bbfile(void *obj, uint32 n)
  {
    ((gschooser_t *)obj)->onmenu_save_bbfile();
    return n;
  }

  static uint32 idaapi s_onmenu_show_graph(void *obj, uint32 n)
  {
    ((gschooser_t *)obj)->onmenu_show_graph();
    return n;
  }

  static uint32 idaapi s_onmenu_analyze(void *obj, uint32 n)
  {
    ((gschooser_t *)obj)->onmenu_analyze();
    return n;
  }

  static uint32 idaapi s_onmenu_auto_find_path(void *obj, uint32 n)
  {
    ((gschooser_t *)obj)->onmenu_analyze();
    return n;
  }

  /**
  * @brief Handle the save bbgroup menu command
  */
  void onmenu_save_bbfile()
  {
    const char *filename = askfile_c(
        1, 
        last_loaded_file.empty() ? "*.bbgroup" : last_loaded_file.c_str(), 
        "Please select BBGROUP file to save to");

    if (filename == NULL)
      return;

    return;
  }

  /**
  * @brief TODO
  */
  void onmenu_analyze()
  {
    func_t *f = get_func(get_screen_ea());
    if (f == NULL)
    {
      msg(STR_GS_MSG "No function at the cursor location!");
      return;
    }
    
    // Call Analyzer
    int_3dvec_t result;
    py_matcher->Analyze(f->startEA, result);
    if (result.empty())
    {
      msg(STR_GS_MSG "Failed to analyze function at %a\n", f->startEA);
      return;
    }

    if (!get_flowchart(f->startEA))
      return;

    /*RESET GROUPPING*/
    if (options.no_initial_path_info)
    {
      // Retrieve initial groupping information
      build_groupman_from_fc(&func_fc, gm, true);
    }
    else
    {
      // Build the groupping information from the analyze() result
      build_groupman_from_3dvec(&func_fc, result, gm, true);
    }

    // Refresh the chooser
    refresh(true);

    if (gsgv == NULL)
      show_graph();
    else
      gsgv->redo_current_layout();
  }

  /**
  * @brief TODO
  */
  void onmenu_auto_find_path()
  {
    if (gsgv == NULL)
      show_graph();
  }

  void onmenu_show_graph()
  {
    if (gsgv == NULL)
      show_graph();
  }

  /**
  * @brief Delete the singleton instance if applicable
  */
  void delete_singleton()
  {
    //TODO: I don't like this singleton thing
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
    highlight_node(sel[0]-1);
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
  void get_node_desc(
        gschooser_line_t *node, 
        qstring *out,
        int col = 1)
  {
    switch (node->type)
    {
      // Handle a group file node
      case chlt_gm:
      {
        if (col == 1)
          *out = qbasename(node->gm->src_filename.c_str());
        break;
      }
      // Handle super groups
      case chlt_sg:
      {
        if (col == 1)
        {
          out->sprnt(MY_TABSTR "%s (%s) C(%d)", 
            node->sg->name.c_str(), 
            node->sg->id.c_str(),
            node->sg->gcount());
        }
        break;
      }
      // Handle a node definition list
      case chlt_ng:
      {
        pnodegroup_t groups = node->ng;

        if (col == 1)
        {
          size_t sz = groups->size();
          out->sprnt(MY_TABSTR MY_TABSTR "C(%d):(", sz);
          for (nodegroup_t::iterator it=groups->begin();
                it != groups->end();
                ++it)
          {
            pnodedef_t nd = *it;
            out->cat_sprnt("%d:%a:%a", nd->nid, nd->start, nd->end);
            if (--sz > 0)
              out->append(", ");
          }
          out->append(")");
        }
        else if (col == 2)
        {
          // Show the EA of the first node in this node group
          pnodedef_t nd = groups->get_first_node();
          if (nd != NULL)
            out->sprnt("%a", nd->start);
        }
        break;
      }
    }
  }

  /**
  * @brief On edit line
  */
  void on_edit_line(uint32 n)
  {
    if (   gsgv == NULL  
        || n > ch_nodes.size() 
        || !IS_SEL(n) )
    {
      return;
    }

    gschooser_line_t &chn = ch_nodes[n-1];
    if (chn.type != chlt_sg)
      return;

    gsgv->edit_sg_description(chn.sg);
  }

  /**
  * @brief Get textual representation of a given line
  */
  void on_get_line(
      uint32 n, 
      char *const *arrptr)
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

      gschooser_line_t &cn = ch_nodes[n];

      qstring desc;
      get_node_desc(&cn, &desc, 1);
      qstrncpy(arrptr[0], desc.c_str(), MAXSTR);

      desc.qclear();
      get_node_desc(&cn, &desc, 2);
      qstrncpy(arrptr[1], desc.c_str(), MAXSTR);
    }
  }

  /**
  * @brief Reload the last opened file
  */
  bool reload_input_file()
  {
    if (!last_loaded_file.empty())
      return load_file_show_graph(last_loaded_file.c_str());
    else
      return false;
  }
  /**
  * @brief Handle chooser line deletion. In fact we trigger a reload here
  */
  uint32 on_delete(uint32 n)
  {
    reload_input_file();
    return n;
  }

  /**
  * @brief Handle line insertion event. In fact we load a file here
  */
  void on_insert()
  {
    const char *filename = askfile_c(0, "*.bbgroup", "Please select BBGROUP file to load");
    if (filename == NULL)
      return;

    load_file_show_graph(filename);
  }

  /**
  * @brief Callback that handles ENTER or double clicks on a chooser node
  */
  void on_enter(uint32 n)
  {
    if (  gsgv == NULL 
       || gsgv->gv == NULL 
       || !IS_SEL(n) || 
       n > ch_nodes.size())
    {
      return;
    }

    gschooser_line_t &chn = ch_nodes[n-1];

    // Get the selected node group or first node group in the super group
    pnodegroup_t ng;
    if (chn.type == chlt_ng)
      ng = chn.ng;
    else if (chn.type == chlt_sg)
      ng = chn.sg->get_first_ng();
    else
      ng = NULL;

    if (ng != NULL)
    {
      // Select the current node
      int nid = gsgv->get_ngid_from_ng(ng);
      if (nid != -1)
        jump_to_node(gsgv->gv, nid);
    }
  }

  /**
  * @brief Callback that handles node selection
  */
  void highlight_node(uint32 n)
  {
    if (   gsgv == NULL 
        || gsgv->gv == NULL)
    {
      return;
    }

    gschooser_line_t &chn = ch_nodes[n];

    // Clear previous highlight
    gsgv->clear_highlighting(true);

    DECL_CG;

    switch (chn.type)
    {
      //
      // Group management
      //
      case chlt_gm:
      {
        // Get all super groups
        psupergroup_listp_t sgroups = gm->get_path_sgl();

        // Mark them for selection
        gsgv->highlight_nodes(
            sgroups, 
            cg, 
            true);

        break;
      }
      //
      // Node groups and supergroups
      //
      case chlt_ng:
      case chlt_sg:
      {
        colorvargen_t cv;
        bgcolor_t clr;

        if (chn.type == chlt_ng)
        {
          // Pick a color
          cg.get_colorvar(cv);
          clr = cg.get_color_anyway(cv);

          gsgv->highlight_nodes(
              chn.ng, 
              clr, 
              true);
        }
        // super groups - chnt_sg
        else
        {
          // Use one color for all the different node group list
          gsgv->highlight_nodes(
            chn.ngl, 
            cg, 
            true);
        }
        break;
      }
      default:
        return;
    }
    if (!options.manual_refresh_mode)
      gsgv->refresh_view();
  }

  /**
  * @brief The chooser is closed
  */
  void on_destroy()
  {
    if (chi.popup_names != NULL)
      qfree((void *)chi.popup_names);

    // Delete the group manager
    delete gm;
    gm = NULL;

    // Close the associated graph
    close_graph();
    delete_singleton();
  }

  /**
  * @brief Refresh the chooser lines
  */
  void refresh(bool populate_lines)
  {
    if (populate_lines)
      populate_chooser_lines();

    refresh_chooser(TITLE_GS_PANEL);
  }

  /**
  * @brief Handles chooser refresh request
  */
  void on_refresh()
  {
  }

  /**
  * @brief Show the GraphView (if it was closed)
  */
  bool show_graph()
  {
    if (gm->empty())
      return true;

    // Show the graph
    gsgv = gsgraphview_t::show_graph(
      &func_fc, 
      gm, 
      &options);
    if (gsgv == NULL)
      return false;

    gsgv->set_callback(this);
    return true;
  }

  /**
  * @brief Load and display a bbgroup file
  */
  bool load_file_show_graph(const char *filename)
  {
    // Retrieve the options
    options.load_options();

    // Show we show the options dialog again?
    if (options.show_options_dialog_next_time)
      options.show_dialog();

    // Load the input file
    if (!load_file(filename))
      return false;

    show_graph();

    // Remember last loaded file
    last_loaded_file = filename;

    return true;
  }

  /**
  * @brief Notification that the GV is closing
  */
  void gsgv_actions_t::notify_close()
  {
    // Tell the chooser not to rely on the GSGV anymore
    gsgv = NULL;
  }

  /**
  * @brief The GV requested a refresh
  */
  void gsgv_actions_t::notify_refresh(bool hard_refresh = false)
  {
    refresh(hard_refresh);
  }

  /**
  * @brief Find similar nodes to the selected one
  */
  pnodegroup_list_t gsgv_actions_t::find_similar(intvec_t &sel_nodes)
  {
    //TODO:
    int_2dvec_t ng_vec;
    if (!py_matcher->FindSimilar(sel_nodes, ng_vec) || ng_vec.empty())
      return NULL;

    // Build NG
    pnodegroup_list_t ngl = new nodegroup_list_t();
    for (int_2dvec_t::iterator it_ng= ng_vec.begin();
         it_ng != ng_vec.end();
         ++it_ng)
    {
      // Build NG
      pnodegroup_t ng = ngl->add_nodegroup();

      intvec_t &nodes_vec = *it_ng;

      // Build nodes
      for (intvec_t::iterator it_nd = nodes_vec.begin();
           it_nd != nodes_vec.end();
           ++it_nd)
      {
        int nid = *it_nd;
        nodeloc_t *loc = gm->find_nodeid_loc(nid);
        if (loc == NULL)
          continue;
        else
          ng->add_node(loc->nd);
      }
    }
    return ngl;
  }

  /**
  * @brief Populate chooser lines
  */
  void populate_chooser_lines()
  {
    // TODO: do not delete previous lines
	// TODO: add option to show similar_sgs
    ch_nodes.clear();

    // Add the first-level node = bbgroup file
    gschooser_line_t *line = &ch_nodes.push_back();
    line->type = chlt_gm;
    line->gm = gm;

    psupergroup_listp_t sgroups = gm->get_path_sgl();
    for (supergroup_listp_t::iterator it=sgroups->begin();
         it != sgroups->end();
         ++it)
    {
      supergroup_t &sg = **it;

      // Add the second-level node = a set of group defs
      line = &ch_nodes.push_back();
      nodegroup_list_t &ngl = sg.groups;
      line->type = chlt_sg;
      line->gm   = gm;
      line->sg   = &sg;
      line->ngl  = &ngl;

      // Add each nodedef list within each node group
      for (nodegroup_list_t::iterator it = ngl.begin();
           it != ngl.end();
           ++it)
      {
        pnodegroup_t ng = *it;
        // Add the third-level node = nodedef
        line = &ch_nodes.push_back();
        line->type = chlt_ng;
        line->gm   = gm;
        line->sg   = &sg;
        line->ngl  = &ngl;
        line->ng   = ng;
      }
    }
  }

  /**
  * @brief Get the flowchart at the given EA and displays an error message on failure
  */
  bool get_flowchart(ea_t startEA)
  {
    // Build the flowchart once
    if (!get_func_flowchart(startEA, func_fc))
    {
      msg(STR_GS_MSG "Could not build function flow chart at %a\n", startEA);
      return false;
    }
    return true;
  }

  /**
  * @brief Handles chooser initialization
  */
  void on_init()
  {
    // Chooser was shown, now create a menu item
    add_menu("Save bbgroup file",         s_onmenu_save_bbfile, "Ctrl-S");
    add_menu("Show graph",                s_onmenu_show_graph);
    add_menu("Analyze",                   s_onmenu_analyze);
    add_menu("Automatically find path",   s_onmenu_auto_find_path);
#ifdef MY_DEBUG
    const char *fn;
    
    //TODO: fix me; file not found -> crash on exit
    //fn = "P:\\projects\\experiments\\bbgroup\\sample_c\\bin\\v1\\x86\\f1.bbgroup";

    //fn = "P:\\projects\\experiments\\bbgroup\\sample_c\\bin\\v1\\x86\\main.bbgroup";
    //fn = "P:\\Tools\\idadev\\plugins\\workfile-1.bbgroup";
    //fn = "P:\\projects\\experiments\\bbgroup\\sample_c\\InlineTest\\f1.bbgroup";
    //fn = "P:\\projects\\experiments\\bbgroup\\sample_c\\InlineTest\\doit.bbgroup";
    //fn = "c:\\temp\\x.bbgroup";
    //fn = "P:\\projects\\experiments\\bbgroup\\sample_c\\InlineTest\\f2.bbgroup";
    //load_file_show_graph(fn);
    onmenu_analyze();
#endif
  }

  /**
  * @brief Fired when the chooser has been shown
  */
  void on_show()
  {
    msg("********************************************************************************\n"
        "%s (built on " __DATE__ " " __TIME__ ")\n"
        "********************************************************************************\n", 
        STR_PLGNAME);

    set_dock_pos(TITLE_GS_PANEL, STR_OUTWIN_TITLE, DP_RIGHT);
    set_dock_pos(STR_GS_VIEW, STR_IDAVIEWA_TITLE, DP_INSIDE);
  }

  /**
  * @brief Initialize the chooser3 structure
  */
  void init_chi()
  {
    static const int widths[] = {60, 16};

    memset(&chi, 0, sizeof(chi));
    chi.cb = sizeof(chi);
    chi.flags = 0;
    chi.width = -1;
    chi.height = -1;
    chi.title = TITLE_GS_PANEL;
    chi.obj = this;
    chi.columns = qnumber(widths);
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
    chi.edit        = s_edit;
    chi.initializer = s_initializer;

    chi.popup_names = (const char **)qalloc(sizeof(char *) * 5);
    *(((char **)chi.popup_names)+0) = "Load bbgroup file"; // Insert
    *(((char **)chi.popup_names)+1) = "Reload bbgroup file"; // Delete
    *(((char **)chi.popup_names)+2) = "Edit description"; // Edit
    *(((char **)chi.popup_names)+3) = NULL; // Refresh
    *(((char **)chi.popup_names)+4) = NULL; // Copy

    //static uint32 idaapi *s_update(void *obj, uint32 n);
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
    py_matcher = NULL;
    gm = new groupman_t();
  }

  /**
  * @brief Destructor
  */
  ~gschooser_t()
  {
    //NOTE: IDA will close the chooser for us and thus the destroy callback will be called
    delete py_matcher;
  }

  /**
  * @brief Add a chooser menu
  */
  inline bool add_menu(
      const char *name, 
      chooser_cb_t cb,
      const char *hotkey = NULL)
  {
    return add_chooser_command(
              TITLE_GS_PANEL,
              name,
              cb,
              hotkey,
              -1,
              -1,
              CHOOSER_POPUP_MENU);
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
    // Delete the previous group manager
    delete gm;
    gm = new groupman_t();

    // Load a file and parse it
    // (don't init cache yet because file may be optimized)
    if (!gm->parse(filename, false))
    {
      msg(STR_GS_MSG "Error: failed to parse group file '%s'\n", filename);
      delete gm;
      return false;
    }

    // Get an address from the parsed file
    nodedef_t *nd = gm->get_first_nd();
    if (nd == NULL)
    {
      msg(STR_GS_MSG "Invalid input file! No addresses defined\n");
      return false;
    }

    // Get related function
    func_t *f = get_func(nd->start);
    if (f == NULL)
    {
      msg(STR_GS_MSG "Input file does not related to a defined function!\n");
      return false;
    }

    if (!get_flowchart(f->startEA))
      return false;

    // De-optimize the input file
    if (sanitize_groupman(BADADDR, gm, &func_fc))
    {
      // Now initialize the cache
      gm->initialize_lookups();
    }

    populate_chooser_lines();
    return true;
  }

  /**
  * @brief Save BB group file
  */
  bool save_file(const char *filename)
  {
    return gm->emit(filename);
  }

  /**
  * @brief Initialize the Python matcher
  */
  bool init_python()
  {
    char init_script[MAXSTR];

    qmakepath(init_script, sizeof(init_script), idadir(PLG_SUBDIR), STR_GS_PY_PLGFILE, NULL);

    py_matcher = new PyBBMatcher(init_script);
    const char *err = py_matcher->init();
    if (err != NULL)
    {
      msg(STR_GS_MSG "Error: %s\n", err);
      delete py_matcher;
      py_matcher = NULL;
      return false;
    }
    return true;
  }

  /**
  * @brief Show the chooser
  */
  static bool show()
  {
    if (singleton == NULL)
    {
      singleton = new gschooser_t();
      if (!singleton->init_python())
      {
        delete singleton;
        singleton = NULL;
        return false;
      }
    }

    choose3(&singleton->chi);
    singleton->on_show();

    return true;
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
  return is_ida_gui() ? PLUGIN_OK : PLUGIN_SKIP;
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

  STR_PLGNAME,          // the preferred short name of the plugin
  "Ctrl-4"              // the preferred hotkey to run the plugin
};