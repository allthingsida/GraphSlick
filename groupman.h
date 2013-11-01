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
* @brief A list of nodes making up a group
*/
class nodegroup_t: public qlist<pnodedef_t>
{
public:
  void free_nodes();
  pnodedef_t add_node(pnodedef_t nd = NULL);
  /**
  * @brief Return the first node definition from this group
  */
  pnodedef_t get_first_node();
};
typedef nodegroup_t *pnodegroup_t;

//--------------------------------------------------------------------------
/**
* @brief Maps a node group to a single node id 
*/
class ng2nid_t: public std::map<pnodegroup_t, int>
{
public:
  inline int get_ng_id(pnodegroup_t ng)
  {
    iterator it = find(ng);
    return it == end() ? -1 : it->second;
  }
};

//--------------------------------------------------------------------------
/**
* @brief Maps a node id to node definitions
*/
typedef std::map<int, pnodedef_t> nid2ndef_t;

//--------------------------------------------------------------------------
/**
* @brief nodegroups type is a list of nodegroup type
*/
class nodegroup_list_t: public qlist<pnodegroup_t>
{
public:
  void free_nodegroup(bool free_nodes);
  /**
  * @brief Return the first node definition from the first group in the group list
  */
  pnodedef_t get_first_node();

  /**
  * @brief Return the first nodegroup
  */
  pnodegroup_t get_first_ng();
};
typedef nodegroup_list_t *pnodegroup_list_t;

//--------------------------------------------------------------------------
/**
* @brief A super group is a groups container
*/
struct supergroup_t
{
  /**
  * @brief Super group ID
  */
  qstring id;

  /**
  * @brief Super group name
  */
  qstring name;

  /**
  * @brief A synthetic group that was not loaded but generated on the fly
  */
  bool is_synthetic;

  /**
  * @brief List of groups in the super group
  */
  nodegroup_list_t groups;

  supergroup_t();
  ~supergroup_t();

  /**
  * @brief Properly clear out all the contained groups
  */
  void clear();

  /**
  * @brief Add a new node group
  * @return Node group
  */
  pnodegroup_t add_nodegroup(pnodegroup_t ng = NULL);

  /**
  * @brief TODO: check me
  */
  bool remove_nodegroup(pnodegroup_t ng);

  /**
  * @brief Return the count of defined groups
  */
  inline size_t gcount() { return groups.size(); }

  /**
  * @brief Return the first node definition from the first group in the group list
  */
  pnodedef_t get_first_node();

  /**
  * @brief Return the first nodegroup
  */
  pnodegroup_t get_first_ng();
};

//--------------------------------------------------------------------------
typedef supergroup_t *psupergroup_t;

//--------------------------------------------------------------------------
typedef qlist<psupergroup_t> supergroup_listp_t;
typedef supergroup_listp_t *psupergroup_listp_t;

//--------------------------------------------------------------------------
/**
* @brief Node location class
*/
struct nodeloc_t
{
  psupergroup_t sg;
  pnodegroup_t  ng;
  pnodedef_t    nd;

  nodeloc_t(): sg(NULL), ng(NULL), nd(NULL)
  {
  }
  nodeloc_t(psupergroup_t sg, 
            pnodegroup_t ng,
            pnodedef_t nd): sg(sg), ng(ng), nd(nd)
  {
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
  typedef std::map<int, nodeloc_t> nid2nloc_map_t;
  nid2nloc_map_t nid2loc;

  /**
  * @brief File name that was last loaded
  */
  qstring filename;

  /**
  * @brief Super groups definition
  */
  supergroup_listp_t sgroups;

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
      psupergroup_t sg, 
      char *grpstr);

public:

  /**
  * @brief Method to initialize lookups
  */
  void initialize_lookups();

  /**
  * @brief Return the super groups
  */
  inline psupergroup_listp_t get_supergroups() { return &sgroups; }

  /**
  * @brief All the node defs
  */
  inline nid2ndef_t *get_nds() { return &all_nds; }

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
  * @brief Add a new super group
  */
  psupergroup_t add_supergroup(psupergroup_t sg = NULL);

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