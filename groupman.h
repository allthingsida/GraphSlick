#ifndef __GROUPMAN__
#define __GROUPMAN__

/*
GroupManager class (c) Elias Bachaalany

This module define groups, nodes and related data structures.
It also provides the group management class.

History
--------

10/10/2013 - eliasb             - First version         
*/


//--------------------------------------------------------------------------
#include <pro.h>

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

//--------------------------------------------------------------------------
/**
* @brief A list of nodes: make a group
*/
typedef qlist<nodedef_t> nodedef_list_t;

//--------------------------------------------------------------------------
/**
* @brief A list of groups
*/
typedef qlist<nodedef_list_t *> pnodegroup_list_t;

//--------------------------------------------------------------------------
/**
* @brief 
*/
struct groupdef_t
{
  qstring id;
  qstring groupname;
  bool selected;
  bool groupped;
  asize_t inst_count;
  asize_t match_count;
  pnodegroup_list_t nodegroups;

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
  inline nodedef_list_t *add_node_group()
  {
    nodedef_list_t *ndl = new nodedef_list_t();
    nodegroups.push_back(ndl);
    return ndl;
  }
};
typedef qlist<groupdef_t *> pgroupdef_list_t;

//--------------------------------------------------------------------------
// Group management class
class groupman_t
{
private:
  /**
  * @brief Node to groupdeflist lookup type
  */
  typedef std::map<int, pgroupdef_list_t> node_groupdeflist_map_t;

  node_groupdeflist_map_t node_to_groups_lookup;

  groupman_t(const groupman_t &) { }

  /**
  * @brief Parse a nodeset string
  */
  void parse_nodeset(groupdef_t *g, char *str);

  /**
  * @brief Method to initialize lookups
  */
  void initialize_lookups();

public:
  /**
  * @brief Groups definition
  */
  pgroupdef_list_t groups;

  /**
  * @brief Utility function to convert a string to the 'asize_t' type
           It works based on the EA64 define
  */
  asize_t str2asizet(const char *str);

  /**
  * @ctor Constructor
  */
  groupman_t();

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
  groupdef_t *add_group();

  /**
  * @brief Rewrites the structure from memory back to a file
  * @param filename - the output file name
  */
  bool emit(const char *filename);

  /**
  * @brief Parse groups definition file
  */
  bool parse(const char *filename);
};
#endif