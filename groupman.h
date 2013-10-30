#ifndef __GROUPMAN__
#define __GROUPMAN__

/*--------------------------------------------------------------------------
GraphSlick (c) Elias Bachaalany
-------------------------------------

GroupManager class

This module define groups, nodes and related data structures.
It also provides the group management class.

--------------------------------------------------------------------------*/


//--------------------------------------------------------------------------
#include <pro.h>
#include <set>

//--------------------------------------------------------------------------
struct nodedef_t
{
  int nid;
  ea_t start;
  ea_t end;

  nodedef_t(): nid(0), start(0), end(0)
  {
  }
};
typedef nodedef_t *pnodedef_t;

//--------------------------------------------------------------------------
/**
* @brief A list of nodes: makes a group
*/
class nodedef_list_t: public qlist<pnodedef_t>
{
public:
  void free_nodes();
  pnodedef_t add_nodedef(pnodedef_t nd);
};
typedef nodedef_list_t *pnodedef_list_t;

//--------------------------------------------------------------------------
/**
* @brief Used to map a pNDL to an id 
         Normally a node ID representing a group of nodes
*/
typedef std::map<pnodedef_list_t, int> pndl2id_t;

/**
* @brief Map node ids to node definitions
*/
typedef std::map<int, pnodedef_t> nid2ndef_t;

//--------------------------------------------------------------------------
/**
* @brief A list of node definition pointers
*/
typedef qlist<pnodedef_t> nodedef_listp_t;

//--------------------------------------------------------------------------
/**
* @brief A list of node groups
*/
class nodegroup_listp_t: public qlist<pnodedef_list_t>
{
public:
  void free_ndls(bool free_nodes);
};

//--------------------------------------------------------------------------
/**
* @brief Group definition
*/
struct groupdef_t
{
  qstring id;
  qstring groupname;
  bool selected;
  bool groupped;
  asize_t inst_count;
  asize_t match_count;
  nodegroup_listp_t nodegroups;

  groupdef_t();
  ~groupdef_t();

  /**
  * @brief Properly clear out all the nodeset in the group
  */
  void clear();

  /**
  * @brief Add a new node group
  * @return Node group
  */
  pnodedef_list_t add_node_group();

  /**
  * @brief TODO:
  */
  void remove_node_group(pnodedef_list_t ndl);

  /**
  * @brief Return the count of defined NDLs
  */
  inline size_t group_count() { return nodegroups.size(); }

};
typedef groupdef_t *pgroupdef_t;
typedef qlist<pgroupdef_t>    groupdef_listp_t;
typedef std::set<groupdef_t *> groupdef_setp_t;

//--------------------------------------------------------------------------
/**
* @brief 
*/
struct nodeloc_t
{
  pgroupdef_t gd;
  pnodedef_list_t ndl;
  pnodedef_t nd;

  nodeloc_t(): gd(NULL), ndl(NULL), nd(NULL)
  {
  }
  nodeloc_t(pgroupdef_t gd, 
            pnodedef_list_t nl,
            pnodedef_t nd): gd(gd), ndl(nl), nd(nd)
  {
  }
};

//--------------------------------------------------------------------------
/**
* @brief Node network class
*/
class groupnet_t
{
  typedef std::map<pgroupdef_t, groupdef_setp_t *> groupnet_map_t;
  groupnet_map_t network;
public:
  /**
  * @brief Clear the network
  */
  void clear();

  /**
  * @brief Return the successor group def set
  */
  groupdef_setp_t *get_succs(pgroupdef_t key);

  ~groupnet_t()
  {
    clear();
  }
};

//--------------------------------------------------------------------------
/**
* @brief Group management class
*/
class groupman_t
{
private:
  /**
  * @brief NodeId node location lookup map
  */
  typedef std::map<int, nodeloc_t> node_nodeloc_map_t;
  node_nodeloc_map_t node_to_loc;

  qstring filename;

  /**
  * @brief Groups definition
  */
  groupdef_listp_t groups;

  /**
  * @brief A lookup table for all node defintions
  */
  nid2ndef_t all_nds;

  /**
  * @brief Private copy constructor
  */
  groupman_t(const groupman_t &) { }

  /**
  * @brief Parse a nodeset string
  */
  bool parse_nodeset(
      pgroupdef_t gd, 
      char *grpstr);

public:

  /**
  * @brief Method to initialize lookups
  */
  void initialize_lookups();

  /**
  * @brief Groups definition
  */
  inline groupdef_listp_t *get_groups() { return &groups; }

  /**
  * @brief All the node defs
  */
  inline nid2ndef_t *get_nds() { return &all_nds; }

  /**
  * @brief Utility function to convert a string to the 'asize_t' type
           It works based on the EA64 define
  */
  asize_t str2asizet(const char *str);

  /**
  * @ctor Default constructor
  */
  groupman_t() { }

  /**
  * @dtor Destructor
  */
  ~groupman_t();

  /**
  * @brief Clears the defined groups
  */
  void clear();

  /**
  * @brief Add a new group definition
  */
  pgroupdef_t add_group(pgroupdef_t gd = NULL);

  /**
  * @brief Rewrites the structure from memory back to a file
  * @param filename - the output file name
  */
  bool emit(const char *filename);

  /**
  * @brief Parse groups definition file
  */
  bool parse(
    const char *filename, 
    bool init_cache = true);

  /**
  * @brief Find a node location by ID
  */
  nodeloc_t *find_nodeid_loc(int nid);

  /**
  * @brief Find a node by an address
  */
  nodeloc_t *find_node_loc(ea_t ea);

  /**
  * @brief Return the file name that was used to load this group manager
  */
  const char *get_source_file();

  /**
  * @brief Returns one node definition from the data structure
  */
  pnodedef_t get_first_nd();
};
#endif