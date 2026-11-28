/* gate: agent_moa_loop_py___degraded_notice -> real _degraded_notice(labels, policy). Input "labelsJSON|||policy". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "json.h"
extern char *moa_u_degraded_notice(const char *labels_json, const char *policy);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ char buf[4096]; snprintf(buf,sizeof buf,"%s",argv[i]);
  char *sep=strstr(buf,"|||"); const char *lj=buf, *pol="";
  if(sep){*sep=0; pol=sep+3;}
  WCUP_BEGIN("moa_u_degraded_notice"); fflush(stdout);
  char*r=moa_u_degraded_notice(lj,pol); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("moa_u_degraded_notice"); free(r);} return 0;}
