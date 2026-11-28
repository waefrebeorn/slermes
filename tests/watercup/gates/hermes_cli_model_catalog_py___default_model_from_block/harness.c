/* gate: hermes_cli_model_catalog_py___default_model_from_block -> real _default_model_from_block (block JSON -> string|null). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "json.h"
extern char *hermes_cli_model_catalog_u_default_model_from_block(const char *block_json);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("hermes_cli_model_catalog_u_default_model_from_block"); fflush(stdout);
  char*r=hermes_cli_model_catalog_u_default_model_from_block(argv[i]);
  WCUP_VALSTR(r?r:"null"); char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("hermes_cli_model_catalog_u_default_model_from_block"); free(r); } return 0; }
