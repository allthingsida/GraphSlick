#include "groupman.h"
#define USE_STANDARD_FILE_FUNCTIONS
#include <fpro.h>
#include <string>
#include <fstream>
#include <iostream>

/*
History
--------

10/10/2013 - eliasb             - First version         
10/15/2013 - eliasb             - Added nodeloc and all_nodes lookups and corresponding lookup functions
10/21/2013 - eliasb             - Added groupnet_t class
                                - Change some type names
								- Moved history comment block to the cpp file
*/

//--------------------------------------------------------------------------
static const char STR_ID[]          = "ID";
static const char STR_MATCH_COUNT[] = "MC";
static const char STR_INST_COUNT[]  = "IC";
static const char STR_GROUPPED[]    = "GROUPPED";
static const char STR_SELECTED[]    = "SELECTED";
static const char STR_NODESET[]     = "NODESET";
static const char STR_GROUP_NAME[]  = "GROUPNAME";

//--------------------------------------------------------------------------
//--  GROUP NETWORK CLASS  -------------------------------------------------
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
void groupnet_t::clear()
{
  for (groupnet_map_t::iterator it=network.begin();
    it != network.end();
    ++it)
  {
    delete it->second;
  }
}

//--------------------------------------------------------------------------
pgroupdef_set_t * groupnet_t::get_succs(groupdef_t *key)
{
  groupnet_map_t::iterator it = network.find(key);
  if (it != network.end())
    return it->second;

  // Create a new groupdef set
  pgroupdef_set_t *succs = new pgroupdef_set_t();

  // Insert it into the network by key
  network[key] = succs;

  return succs;
}

//--------------------------------------------------------------------------
//--  GROUP DEFINITION CLASS  ----------------------------------------------
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
groupdef_t::~groupdef_t()
{
  clear();
}

//--------------------------------------------------------------------------
groupdef_t::groupdef_t() : selected(false), groupped(false), 
                           inst_count(0), match_count(0)
{
}

//--------------------------------------------------------------------------
//--  GROUP MANAGER CLASS  -------------------------------------------------
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
void groupman_t::parse_nodeset(groupdef_t *g, char *str)
{
  // Find node group bounds
  for ( /*init*/ char *p_group_start = NULL, *p_group_end = NULL;
        /* cond*/(p_group_start = strchr(str, '(')) != NULL
             && (++p_group_start, (p_group_end = strchr(p_group_start, ')')) != NULL);
        /*incr*/)
  {
    *p_group_end = '\0';

    // Advance to next group
    str = p_group_end + 1;

    // Add a new node group
    nodedef_list_t *ng = g->add_node_group();

    for (/*init*/ char *saved_ptr, 
                  *p = p_group_start, 
                  *token = qstrtok(p, ",", &saved_ptr);
         /*cond*/ p != NULL;
         /*incr*/ p = qstrtok(NULL, ",", &saved_ptr))
    {
      int nid;
      ea_t start = 0, end = 0;
      if (qsscanf(p, "%d : %a : %a", &nid, &start, &end) <= 0)
        continue;

      nodedef_t &node = ng->push_back();
      node.nid = nid;
      node.start = start;
      node.end = end;
    }
  }
}

//--------------------------------------------------------------------------
void groupman_t::initialize_lookups()
{
  // Clear previous cache structures
  node_to_loc.clear();
  all_nodes.clear();

  // Build new cache
  for (pgroupdef_list_t::iterator it=groups.begin();
       it != groups.end();
       ++it)
  {
    // Walk each group definition
    groupdef_t *gd = *it;
    for (nodegroup_listp_t::iterator it=gd->nodegroups.begin();
         it != gd->nodegroups.end();
         ++it)
    {
      // Walk each node list
      nodedef_list_t *nl = *it;
      for (nodedef_list_t::iterator it=nl->begin();
           it != nl->end();
           ++it)
      {
        // Grab each node def
        nodedef_t *nd = &*it;
        
        // Remember where this node is located
        node_to_loc[nd->nid] = nodeloc_t(gd, nl, nd);
        
        // Remember this nodedef
        all_nodes.push_back(nd);
      }
    }
  }
}

//--------------------------------------------------------------------------
asize_t groupman_t::str2asizet(const char *str)
{
  ea_t v;
  qsscanf(str, "%a", &v);
  return (asize_t)v;
}

//--------------------------------------------------------------------------
groupman_t::groupman_t()
{
}

//--------------------------------------------------------------------------
groupman_t::~groupman_t()
{
  clear();
}

//--------------------------------------------------------------------------
void groupman_t::clear()
{
  for (pgroupdef_list_t::iterator it=groups.begin(); 
       it != groups.end();
       ++it)
  {
    delete *it;
  }
  groups.clear();
}

//--------------------------------------------------------------------------
groupdef_t *groupman_t::add_group()
{
  groupdef_t *g = new groupdef_t();
  groups.push_back(g);
  return g;
}

//--------------------------------------------------------------------------
bool groupman_t::emit(const char *filename)
{
  FILE *fp = qfopen(filename, "w");
  if (fp == NULL)
    return false;

  for (pgroupdef_list_t::iterator it=groups.begin();
    it != groups.end();
    ++it)
  {
    groupdef_t &gd = **it;

    // Write ID
    qfprintf(fp, "%s:%s;", STR_ID, gd.id.c_str());
    if (gd.inst_count != 0)
      qfprintf(fp, "%s:%a;", STR_INST_COUNT, gd.inst_count);

    // Write match count
    if (gd.match_count != 0)
      qfprintf(fp, "%s:%a;", STR_MATCH_COUNT, gd.match_count);

    // Write 'is-groupped'
    if (gd.groupped)
      qfprintf(fp, "%s:1;", STR_GROUPPED);

    // Write 'is-selected'
    if (gd.selected)
      qfprintf(fp, "%s:1;", STR_SELECTED);

    size_t group_count = gd.nodegroups.size();
    if (group_count > 0)
    {
      nodegroup_listp_t &ngl = gd.nodegroups;
      for (nodegroup_listp_t::iterator it = ngl.begin(); it != ngl.end(); ++it)
      {
        nodedef_list_t *nl = *it;

        qfprintf(fp, "(");
        size_t c = nl->size();
        for (nodedef_list_t::iterator it = nl->begin();it != nl->end(); ++it)
        {
          qfprintf(fp, "%d : %a : %a", it->nid, it->start, it->end);
          if (--c != 0)
            qfprintf(fp, ", ");
        }
        qfprintf(fp, ")");
        if (--group_count != 0)
          qfprintf(fp, ", ");
      }
    }
    qfprintf(fp, "\n");
  }
  qfclose(fp);

  return true;
}

//--------------------------------------------------------------------------
bool groupman_t::parse(const char *filename)
{
  std::ifstream in_file(filename);
  if (!in_file.is_open())
    return false;

  // Remember the opened file name
  this->filename = filename;

  // Clear previous group def
  clear();

  std::string line;
  while (in_file.good())
  {
    // Read the line
    std::getline(in_file, line);

    // Take a copy of the line so we tokenize it
    char *s = qstrdup(line.c_str());

    groupdef_t *g = add_group();

    for (char *saved_ptr, *token = qstrtok(s, ";", &saved_ptr); 
      token != NULL;
      token = qstrtok(NULL, ";", &saved_ptr))
    {
      char *val = strchr(token, ':');
      if (val == NULL)
        continue;

      // Kill separator and adjust value pointer
      *val++ = '\0';

      // Set key pointer
      char *key = token;


      if (stricmp(key, STR_ID) == 0)
      {
        g->id = val;  
      }
      else if (stricmp(key, STR_MATCH_COUNT) == 0)
      {
        g->match_count = str2asizet(val);
      }
      else if (stricmp(key, STR_INST_COUNT) == 0)
      {
        g->inst_count = str2asizet(val);
      }
      else if (stricmp(key, STR_GROUPPED) == 0)
      {
        g->groupped = atoi(val) == 1;
      }
      else if (stricmp(key, STR_SELECTED) == 0)
      {
        g->selected = atoi(val) == 1;
      }
      else if (stricmp(key, STR_GROUP_NAME) == 0)
      {
        g->groupname = val;  
      }
      else if (stricmp(key, STR_NODESET) == 0)
      {
        parse_nodeset(g, val);
      }
    }
    // Free this line
    qfree(s);
  }
  in_file.close();

  initialize_lookups();
  return true;
}

//--------------------------------------------------------------------------
void groupdef_t::clear()
{
  for (nodegroup_listp_t::iterator it=nodegroups.begin();
       it != nodegroups.end();
       ++it)
  {
    delete *it;
  }
  nodegroups.clear();
}

//--------------------------------------------------------------------------
nodeloc_t *groupman_t::find_nodeid_loc(int nid)
{
  node_nodeloc_map_t::iterator it = node_to_loc.find(nid);
  if (it == node_to_loc.end())
    return NULL;
  else
    return &it->second;
}

//--------------------------------------------------------------------------
nodeloc_t *groupman_t::find_node_loc(ea_t ea)
{
  //HINT: use a map with lower_bound() if this function is to be called
  //      frequently and speed is of importance
  for (nodedef_listp_t::iterator it=all_nodes.begin();
       it != all_nodes.end();
       ++it)
  {
    nodedef_t *nd = *it;
    if (nd->start >= ea && ea < nd->end)
      return find_nodeid_loc(nd->nid);
  }
  return NULL;
}

//--------------------------------------------------------------------------
const char *groupman_t::get_source_file()
{
  return filename.c_str();
}
