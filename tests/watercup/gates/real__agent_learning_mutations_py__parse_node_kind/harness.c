/* gate: parse_node_kind(node_id) -> "memory" | "skill". */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
char *learning_mutations_parse_node_kind(const char *node_id);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("parse_node_kind");
  char *r=learning_mutations_parse_node_kind(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("parse_node_kind"); if(r) free(r);} return 0;}
