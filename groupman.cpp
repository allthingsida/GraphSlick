/*--------------------------------------------------------------------------
History
--------

10/10/2013 - eliasb             - First version         
10/15/2013 - eliasb             - Added nodeloc and all_nodes lookups and corresponding lookup functions
10/21/2013 - eliasb             - Added groupnet_t class
                                - Change some type names
                                - Moved history comment block to the cpp file
10/22/2013 - eliasb             - Added gm.get_nodes()
                                - Change group def list type name
10/25/2013 - eliasb             - typedef nodedef_list_t * as pnodedef_list_t
10/28/2013 - eliasb             - made nodedef_t as a pointer in the nodedef_list class
10/29/2013 - eliasb             - skip trailing spaces while parsing
                                - added get_first_nd()
                                - got rid of the 'all_ndls' structure and added nid2nd structure
--------------------------------------------------------------------------*/

#include "groupman.h"
#define USE_STANDARD_FILE_FUNCTIONS
#include <fpro.h>
#include <string>
#include <fstream>
#include <iostream>

//--------------------------------------------------------------------------
static const char STR_ID[]          = "ID";
static const char STR_MATCH_COUNT[] = "MC";
static const char STR_INST_COUNT[]  = "IC";
static const char STR_GROUPPED[]    = "GROUPPED";
static const char STR_SELECTED[]    = "SELECTED";
static const char STR_NODESET[]     = "NODESET";
static const char STR_GROUP_NAME[]  = "GROUPNAME";

//--------------------------------------------------------------------------
inline char *skip_spaces(char *p)
{
  while (*p != '\0' && qisspace(*p))
    ++p;

  return p;
}

//--------------------------------------------------------------------------
//--  NODEGROUP_LIST CLASS  ------------------------------------------------
//--------------------------------------------------------------------------
void nodegroup_listp_t::free_ndls(bool free_nodes)
{
  for (iterator it=begin(); it != end(); ++it)
  {
    pnodedef_list_t ndl = *it;
    if (free_nodes)
      ndl->free_nodes();

    delete ndl;
  }
}

//--------------------------------------------------------------------------
//--  NODEDEF_LIST CLASS  --------------------------------------------------
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
void nodedef_list_t::free_nodes()
{
  for (iterator it=begin(); it != end(); ++it)
  {
    pnodedef_t nd = *it;
    delete nd;
  }
}

//--------------------------------------------------------------------------
pnodedef_t nodedef_list_t::add_nodedef(pnodedef_t nd)
{
  push_back(nd);
  return nd;
}

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
groupdef_setp_t *groupnet_t::get_succs(groupdef_t *key)
{
  groupnet_map_t::iterator it = network.find(key);
  if (it != network.end())
    return it->second;

  // Create a new groupdef set
  groupdef_setp_t *succs = new groupdef_setp_t();

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
pnodedef_list_t groupdef_t::add_node_group()
{
  pnodedef_list_t ndl = new nodedef_list_t();
  nodegroups.push_back(ndl);
  return ndl;
}

//--------------------------------------------------------------------------
//--  GROUP MANAGER CLASS  -------------------------------------------------
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
bool groupman_t::parse_nodeset(groupdef_t *gd, char *grpstr)
{
  // Find node group bounds
  for ( /*init*/ char *p_group_start = NULL, *p_group_end = NULL;
        /* cond*/(p_group_start = strchr(grpstr, '(')) != NULL
             && (p_group_start = skip_spaces(p_group_start+1), (p_group_end = strchr(p_group_start, ')')) != NULL);
        /*incr*/)
  {
    // Terminate the string with the closing parenthesis
    *p_group_end = '\0';

    // Advance to next group
    grpstr = skip_spaces(p_group_end + 1);

    // Add a new node group
    pnodedef_list_t ndl = gd->add_node_group();

    for (/*init*/ char *saved_ptr, 
                  *p = p_group_start, 
                  *token = qstrtok(p, ",", &saved_ptr);
         /*cond*/ p != NULL;
         /*incr*/ p = qstrtok(NULL, ",", &saved_ptr))
    {
      p = skip_spaces(p);

      int nid;
      ea_t start = 0, end = 0;
      if (qsscanf(p, "%d : %a : %a", &nid, &start, &end) <= 0)
        continue;

      if (nid == 0)
        continue;

      // Create an ND
      nodedef_t *nd = new nodedef_t();
      nd->nid = nid;
      nd->start = start;
      nd->end = end;

      ndl->push_back(nd);

      // Map this node
      all_nds[nid] = nd;
    }
  }
  return true;
}

//--------------------------------------------------------------------------
void groupman_t::initialize_lookups()
{
  // Clear previous cache structures
  node_to_loc.clear();

  // Build new cache
  for (groupdef_listp_t::iterator it=groups.begin();
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
      pnodedef_list_t nl = *it;
      for (nodedef_list_t::iterator it=nl->begin();
           it != nl->end();
           ++it)
      {
        // Grab each node def
        nodedef_t *nd = *it;
        
        // Remember where this node is located
        node_to_loc[nd->nid] = nodeloc_t(gd, nl, nd);
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
groupman_t::~groupman_t()
{
  clear();
}

//--------------------------------------------------------------------------
void groupman_t::clear()
{
  for (groupdef_listp_t::iterator it=groups.begin(); 
       it != groups.end();
       ++it)
  {
    pgroupdef_t gd = *it;
    gd->clear();
    delete gd;
  }
  groups.clear();
  all_nds.clear();
}

//--------------------------------------------------------------------------
groupdef_t *groupman_t::add_group(pgroupdef_t gd)
{
  if (gd == NULL)
    gd = new groupdef_t();

  groups.push_back(gd);
  return gd;
}

//--------------------------------------------------------------------------
bool groupman_t::emit(const char *filename)
{
  FILE *fp = qfopen(filename, "w");
  if (fp == NULL)
    return false;

  for (groupdef_listp_t::iterator it=groups.begin();
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
        pnodedef_list_t nl = *it;

        qfprintf(fp, "(");
        size_t c = nl->size();
        for (nodedef_list_t::iterator it = nl->begin();it != nl->end(); ++it)
        {
          nodedef_t *nd = *it;
          qfprintf(fp, "%d : %a : %a", nd->nid, nd->start, nd->end);
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
bool groupman_t::parse(
    const char *filename, 
    bool init_cache)
{
  std::ifstream in_file(filename);
  if (!in_file.is_open())
    return false;

  // Remember the opened file name
  this->filename = filename;

  // Clear previous items
  clear();

  std::string line;
  while (in_file.good())
  {
    // Read the line
    std::getline(in_file, line);

    // Skip comment or empty lines
    char *s = skip_spaces((char *)line.c_str());
    if (s[0] == '\0' || s[0] == '#')
      continue;

    // Take a copy of the line so we tokenize it
    char *a_line = qstrdup(s);
    s = a_line;

    // Create a new group definition per line
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
      val = skip_spaces(val);

      // Set key pointer
      char *key = skip_spaces(token);

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
        if (!parse_nodeset(g, val))
          break;
      }
    }
    // Free this line
    qfree(a_line);
  }
  in_file.close();

  // Initialize cache
  if (init_cache)
    initialize_lookups();

  return true;
}

//--------------------------------------------------------------------------
void groupdef_t::clear()
{
  nodegroups.free_ndls(true);
  nodegroups.clear();
}

//--------------------------------------------------------------------------
void groupdef_t::remove_node_group(pnodedef_list_t ndl)
{
  //TODO: test me
  for (nodegroup_listp_t::iterator it=nodegroups.begin();
       it != nodegroups.end();
       ++it)
  {
    pnodedef_list_t fndl = *it;
    if (fndl == ndl)
    {
      nodegroups.erase(it);
      break;
    }
  }
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
  for (nid2ndef_t::iterator it = all_nds.begin();
       it != all_nds.end();
       ++it)
  {
    pnodedef_t nd = it->second;
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

//--------------------------------------------------------------------------
pnodedef_t groupman_t::get_first_nd()
{
  if (groups.empty())
    return NULL;

  // Get the first group
  pgroupdef_t first_group = (*(groups.begin()));
  if (first_group->group_count() == 0)
    return NULL;

  // Get NDL count in the group
  if (first_group->nodegroups.empty())
    return NULL;

  // Get first NDL
  nodedef_listp_t *ndls = *(first_group->nodegroups.begin());
  if (ndls->empty())
    return NULL;
  else
    return *(ndls->begin());
}
