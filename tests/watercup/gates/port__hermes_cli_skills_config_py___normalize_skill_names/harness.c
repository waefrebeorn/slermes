/* gate: _normalize_skill_names(values) -> sorted JSON array (set rendering). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern char *hermes_cli_skills_config_u_normalize_skill_names(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_normalize_skill_names");
  char *r=hermes_cli_skills_config_u_normalize_skill_names(argv[i]); WCUP_VALSTR(r?r:"[]");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_normalize_skill_names"); if(r) free(r);} return 0;}
