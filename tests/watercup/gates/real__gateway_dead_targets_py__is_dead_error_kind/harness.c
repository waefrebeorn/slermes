/* gate: is_dead_error_kind -> membership in _DEAD_ERROR_KINDS. */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
#include <stdbool.h>
bool dead_target_is_dead_error_kind(const char *error_kind);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("is_dead_error_kind");
  WCUP_VALSTR(dead_target_is_dead_error_kind(argv[i])?"True":"False");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup); WCUP_END("is_dead_error_kind"); }
 return 0;}
