/* gate: format_turn_summary(elapsed, tally, max_segments) -> str. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
#include "port_turn_summary.h"

static char *dup_s(const char *s){ return strdup(s ? s : ""); }

int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("format_turn_summary");
  json_t *in=json_parse(argv[i],NULL);
  double elapsed = 0.0;
  json_t *ej=json_obj_get(in,"elapsed");
  if(json_is_number(ej)) elapsed=ej->num_val;
  int max_seg = 4;
  json_t *mj=json_obj_get(in,"max_segments");
  if(json_is_number(mj)) max_seg=(int)mj->num_val;

  turn_tally_t t; memset(&t,0,sizeof t);
  json_t *tj=json_obj_get(in,"tally");
  if(json_is_object(tj)){
      json_t *oj=json_obj_get(tj,"other_tools");
      if(json_is_number(oj)) t.other_tools=(int)oj->num_val;
      json_t *la=json_obj_get(tj,"lines_added");
      if(json_is_number(la)) t.lines_added=(int)la->num_val;
      json_t *lr=json_obj_get(tj,"lines_removed");
      if(json_is_number(lr)) t.lines_removed=(int)lr->num_val;
      json_t *hd=json_obj_get(tj,"has_line_deltas");
      if(json_is_bool(hd)) t.has_line_deltas=hd->bool_val;
      json_t *vj=json_obj_get(tj,"verbs");
      if(json_is_object(vj)){
          size_t nv=json_object_size(vj);
          t.verbs=calloc(nv?nv:1,sizeof(verb_entry_t)); t.cap_verbs=nv;
          for(size_t a=0;a<nv;a++){
              const char *verb=json_object_get_key_at(vj,a);
              json_t *nj=json_object_get_at(vj,a);
              if(!verb||!json_is_object(nj)) continue;
              verb_entry_t *ve=&t.verbs[t.n_verbs++];
              ve->verb=dup_s(verb);
              size_t nn=json_object_size(nj);
              ve->nouns=calloc(nn?nn:1,sizeof(noun_entry_t)); ve->cap_nouns=nn;
              for(size_t b=0;b<nn;b++){
                  const char *pl=json_object_get_key_at(nj,b);
                  json_t *cj=json_object_get_at(nj,b);
                  if(!pl) continue;
                  noun_entry_t *ne=&ve->nouns[ve->n_nouns++];
                  ne->plural=dup_s(pl);
                  ne->count=json_is_number(cj)?(int)cj->num_val:0;
              }
          }
      }
  }
  char *r=turn_summary_format(elapsed,&t,max_seg);
  WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("format_turn_summary");
  if(r) free(r);
  for(size_t v=0;v<t.n_verbs;v++){ for(size_t k=0;k<t.verbs[v].n_nouns;k++) free(t.verbs[v].nouns[k].plural);
      free(t.verbs[v].nouns); free(t.verbs[v].verb); }
  free(t.verbs); if(in) json_free(in);
 } return 0;}
