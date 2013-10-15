#include "groupman.h"

//--------------------------------------------------------------------------
int main()
{
  groupman_t gm;

  gm.parse("f1.txt");
  gm.emit("f2.txt");

  nodeloc_t *nl = gm.find_node_loc(0x4012DE);
  if (nl != NULL)
  {
    printf("nid=%d\n", nl->nd->nid);
  }

  return 0;
}