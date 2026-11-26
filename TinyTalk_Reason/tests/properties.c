/* Deterministic property checks; numbers are generated after model training. */
#define main reason_program_main
#include "../src/reason.c"
#undef main
#include <assert.h>
static unsigned seed=4291,checks;
static unsigned next(void){seed=seed*1664525u+1013904223u;return seed;}
static Reply run(Model *m,State *s,const char *q){Reply r;respond(m,s,q,&r,0);return r;}
static void expect(Reply r,const char *code){checks++;if(strcmp(r.code,code)){fprintf(stderr,"expected %s got %s: %s\n",code,r.code,r.answer);exit(1);}}
int main(int argc,char **argv){
 if(argc!=2)return 2;Model *m=alloc(sizeof *m);checkpoint(m,argv[1],0);
 char q[256],want[128];
 for(int trial=0;trial<200;trial++){
  State s={0};long goal=1000+next()%4000,sum=0,last=0;
  snprintf(q,sizeof q,"цель %ld ккал",goal);expect(run(m,&s,q),"GOAL");
  int n=1+next()%12;
  for(int j=0;j<n;j++){last=next()%900;sum+=last;snprintf(q,sizeof q,"я съел %ld ккал",last);expect(run(m,&s,q),"MEAL");}
  long replacement=next()%900;sum+=replacement-last;
  snprintf(q,sizeof q,"нет было %ld",replacement);expect(run(m,&s,q),"CORRECTED");
  Reply r=run(m,&s,"сколько осталось");expect(r,"REMAINING");
  if(goal>=sum)snprintf(want,sizeof want,"Осталось %ld ккал.",goal-sum);
  else snprintf(want,sizeof want,"Цель превышена на %ld ккал.",sum-goal);
  checks++;assert(!strcmp(r.answer,want));
  expect(run(m,&s,"не добавляй 999 ккал"),"NEGATED");
  checks++;assert(s.meals==n&&s.meal[n-1].value==replacement);
  expect(run(m,&s,"поставь таймер"),"ASK_DURATION");
  long duration=1+next()%100;snprintf(q,sizeof q,"на %ld минут",duration);
  expect(run(m,&s,q),"TIMER");checks++;assert(s.timer[0].value==duration*60);
 }
 State s={0};
 for(int i=0;i<CAP;i++)expect(run(m,&s,"я съел 1 ккал"),"MEAL");
 expect(run(m,&s,"я съел 1 ккал"),"FULL");checks++;assert(s.meals==CAP);
 expect(run(m,&s,"/reset"),"RESET");
 expect(run(m,&s,"ключ у анна"),"FACT");
 expect(run(m,&s,"книга у борис"),"FACT");
 expect(run(m,&s,"у кого ключ?"),"OWNER");
 expect(run(m,&s,"передай его от анна к вера"),"TRANSFER");
 checks++;assert(!strcmp(s.fact[0].owner,"вера"));
 expect(run(m,&s,"\xff"),"INVALID");expect(run(m,&s,"\xc0\xaf"),"INVALID");
 expect(run(m,&s,"\xed\xa0\x80"),"INVALID");
 char longtext[3000];memset(longtext,'x',sizeof longtext-1);longtext[sizeof longtext-1]=0;
 expect(run(m,&s,longtext),"INVALID");
 printf("PASS %u checks; 200 generated numeric dialogues; capacity, references, invalid UTF-8.\n",checks);
 for(int i=0;i<m->c;i++)free(m->answer[i]);free(m);return 0;
}
