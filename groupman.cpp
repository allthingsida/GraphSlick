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
10/30/2013 - eliasb             - renamed classes to less confusing names: supergroup, groups, grouplist, nodedef
--------------------------------------------------------------------------*/

#include "groupman.h"
#define USE_STANDARD_FILE_FUNCTIONS
#include <fpro.h>
#include <string>
#include <fstream>
#include <iostream>
#include "util.h"

//--------------------------------------------------------------------------
static const char STR_ID[]          = "ID";
static const char STR_MATCH_COUNT[] = "MC";
static const char STR_INST_COUNT[]  = "IC";
static const char STR_GROUPPED[]    = "GROUPPED";
static const char STR_SELECTED[]    = "SELECTED";
static const char STR_NODESET[]     = "NODESET";
static const char STR_GROUP_NAME[]  = "GROUPNAME";

//--------------------------------------------------------------------------
//--  NODEGROUP_LIST CLASS  ------------------------------------------------
//--------------------------------------------------------------------------
void nodegroup_list_t::free_nodegroup(bool free_nodes)
{
  for (iterator it=begin(); it != end(); ++it)
  {
    pnodegroup_t ng = *it;
    if (free_nodes)
      ng->free_nodes();

    delete ng;
  }
}

//--------------------------------------------------------------------------
//--  NODEGROUP CLASS  -----------------------------------------------------
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
void nodegroup_t::free_nodes()
{
  for (iterator it=begin(); it != end(); ++it)
  {
    pnodedef_t nd = *it;
    delete nd;
  }
}

//--------------------------------------------------------------------------
pnodedef_t nodegroup_t::add_node(pnodedef_t nd)
{
  if (nd == NULL)
    nd = new nodedef_t();

  push_back(nd);
  return nd;
}

//--------------------------------------------------------------------------
//--  SUPER GROUP CLASS  ---------------------------------------------------
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
supergroup_t::~supergroup_t()
{
  clear();
}

//--------------------------------------------------------------------------
supergroup_t::supergroup_t(): is_synthetic(false)
{
}

//--------------------------------------------------------------------------
pnodegroup_t supergroup_t::add_nodegroup(pnodegroup_t ng)
{
  if (ng == NULL)
    ng = new nodegroup_t();

  groups.push_back(ng);
  return ng;
}

//--------------------------------------------------------------------------
//--  GROUP MANAGER CLASS  -------------------------------------------------
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
bool groupman_t::parse_nodeset(
      psupergroup_t sg,
      char *grpstr)
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

    // Add a new group
    pnodegroup_t ng = sg->add_nodegroup();

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

      // Create an ND
      nodedef_t *nd = ng->add_node();
      nd->nid = nid;
      nd->start = start;
      nd->end = end;

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
  nid2loc.clear();

  // Build new cache
  for (supergroup_listp_t::iterator it=sgroups.begin();
       it != sgroups.end();
       ++it)
  {
    // Walk each super group
    psupergroup_t sg = *it;
    for (nodegroup_list_t::iterator it=sg->groups.begin();
         it != sg->groups.end();
         ++it)
    {
      // Walk each group contents
      pnodegroup_t ng = *it;
      for (nodegroup_t::iterator it=ng->begin();
           it != ng->end();
           ++it)
      {
        // Grab each node def
        nodedef_t *nd = *it;
        
        // Remember where this node is located
        nid2loc[nd->nid] = nodeloc_t(sg, ng, nd);
      }
    }
  }
}

//--------------------------------------------------------------------------
groupman_t::~groupman_t()
{
  clear();
}

//--------------------------------------------------------------------------
void groupman_t::clear()
{
  for (supergroup_listp_t::iterator it=sgroups.begin(); 
       it != sgroups.end();
       ++it)
  {
    psupergroup_t sg = *it;
    sg->clear();
    delete sg;
  }
  sgroups.clear();
  all_nds.clear();
}

//--------------------------------------------------------------------------
psupergroup_t groupman_t::add_supergroup(psupergroup_t sg)
{
  if (sg == NULL)
    sg = new supergroup_t();

  sgroups.push_back(sg);
  return sg;
}

//--------------------------------------------------------------------------
bool groupman_t::emit(const char *filename)
{
  FILE *fp = qfopen(filename, "w");
  if (fp == NULL)
    return false;

  for (supergroup_listp_t::iterator it=sgroups.begin();
       it != sgroups.end();
       ++it)
  {
    psupergroup_t sg = *it;

    // Write ID
    qfprintf(fp, "%s:%s;", STR_ID, sg->id.c_str());

    size_t group_count = sg->groups.size();
    if (group_count > 0)
    {
      nodegroup_list_t &ngl = sg->groups;
      for (nodegroup_list_t::iterator it = ngl.begin(); 
           it != ngl.end(); 
           ++it)
      {
        pnodegroup_t ng = *it;

        qfprintf(fp, "(");

        size_t c = ng->size();
        for (nodegroup_t::iterator it = ng->begin();
             it != ng->end();
             ++it)
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

  //TODO: generate dummy group names ; int group_dummy_name;

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

    // Create a new super group definition per line
    psupergroup_t sg = add_supergroup();

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
        sg->id = val;  
      }
      else if (stricmp(key, STR_NODESET) == 0)
      {
        if (!parse_nodeset(sg, val))
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
void supergroup_t::clear()
{
  groups.free_nodegroup(true);
  groups.clear();
}

//--------------------------------------------------------------------------
bool supergroup_t::remove_nodegroup(pnodegroup_t ng)
{
  //TODO: test me
  for (nodegroup_list_t::iterator it=groups.begin();
       it != groups.end();
       ++it)
  {
    pnodegroup_t f_ng = *it;
    if (f_ng == ng)
    {
      groups.erase(it);
      return true;
    }
  }
  return false;
}

//--------------------------------------------------------------------------
nodeloc_t *groupman_t::find_nodeid_loc(int nid)
{
  nid2nloc_map_t::iterator it = nid2loc.find(nid);
  if (it == nid2loc.end())
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
  // No super groups defined?
  if (sgroups.empty())
    return NULL;

  // Get the first super group
  psupergroup_t first_sgroup = (*(sgroups.begin()));
  if (first_sgroup->gcount() == 0)
    return NULL;

  // Get groups count in the group
  if (first_sgroup->groups.empty())
    return NULL;

  // Get first group
  pnodegroup_t ng = *(first_sgroup->groups.begin());
  if (ng->empty())
  {
    // No nodes in the first group?
    return NULL;
  }
  else
  {
    // REturn the first node in the first group
    return *(ng->begin());
  }
}
