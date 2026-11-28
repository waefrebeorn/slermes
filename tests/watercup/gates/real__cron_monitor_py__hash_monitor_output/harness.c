/* gate: hash_monitor_output -> real port (str in, hex str out). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *cron_monitor_u_hash_monitor_output(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("hash_monitor_output");
  char *r=cron_monitor_u_hash_monitor_output(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("hash_monitor_output"); if (r) free(r);} return 0;}
