/* SymbolicSearch_C: C11 intent retrieval, no neural network or learned weights. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define MAX 4096
typedef struct { char text[1024],label[64]; } Row;
static Row rows[MAX]; static int count;
static int next(const unsigned char **p) {
 unsigned c=*(*p)++; if(c<128)return (int)c;
 int n=c<224?1:c<240?2:3; unsigned u=c&((1u<<(6-n))-1);
 for(int i=0;i<n;i++){if((**p&192)!=128)return '?';u=(u<<6)|(*(*p)++&63);}
 return (int)u;
}
static int lower(int c) {
 if(c>='A'&&c<='Z')return c+32;
 if(c>=0x410&&c<=0x42f)return c+32;
 if(c==0x401||c==0x451)return 0x435;
 return c;
}
typedef struct {int code[64],n;} Word;
typedef struct {Word w[128];int n;} Words;
static Words tokenize(const char *s){
 Words v={0}; const unsigned char *p=(const unsigned char*)s;
 while(*p){
  int c=lower(next(&p));
  int ok=(c>='a'&&c<='z')||(c>='0'&&c<='9')||(c>=0x430&&c<=0x44f);
  if(ok){if(!v.n||v.w[v.n-1].n){} if(v.n<128){Word *w=&v.w[v.n];if(w->n<64)w->code[w->n++]=c;}}
  else if(v.n<128&&v.w[v.n].n)v.n++;
 }
 if(v.n<128&&v.w[v.n].n)v.n++;
 return v;
}
static const struct {const char *stem;int concept;} dict[]={
 {"остат",1},{"остал",1},{"доступн",1},{"запас",1},
 {"сумм",2},{"итого",2},{"всего",2},{"суммар",2},
 {"таймер",3},{"отсчет",3},
 {"цель",4},{"лимит",4},{"норма",4},
 {"съел",5},{"съела",5},{"поел",5},{"завтрак",5},{"обед",5},{"ужин",5},
 {"исправ",6},{"поправ",6},{"замен",6},
 {"отмен",7},{"удал",7},
 {"переда",8},{"отдай",8},
 {"владел",9},{"принадлеж",9},{"чей",9},
 {"привет",10},{"здравств",10},{"добрый",10},
 {"калор",11},{"ккал",11},{"питан",11},
 {"свет",12},{"ламп",12},{"освещ",12},
 {"выключ",13},{"отключ",13},{"погас",13},
 {"включ",14},{"зажг",14}
};
static int prefix(const Word *w,const char *s){
 const unsigned char *p=(const unsigned char*)s;int i=0;
 while(*p){if(i>=w->n||w->code[i++]!=lower(next(&p)))return 0;}return 1;
}
static int concept(const Word *w){
 for(size_t i=0;i<sizeof(dict)/sizeof(dict[0]);i++)
  if(prefix(w,dict[i].stem))return dict[i].concept;
 return 0;
}
static double similarity(const Words *a,const Words *b){
 double score=0;int matches=0;
 for(int i=0;i<a->n;i++){
  double best=0;int ca=concept(&a->w[i]);
  for(int j=0;j<b->n;j++){
   int cb=concept(&b->w[j]);double v=0;
   if(ca&&cb&&ca==cb)v=4;
   else {
    int n=a->w[i].n<b->w[j].n?a->w[i].n:b->w[j].n,k=0;
    while(k<n&&a->w[i].code[k]==b->w[j].code[k])k++;
    if(k>=4)v=1.0+(double)k/10.0;
   }
   if(v>best)best=v;
  }
  if(best>0){score+=best;matches++;}
 }
 return a->n&&b->n ? score/(4.0*(a->n>b->n?a->n:b->n)) : 0;
}
int main(int argc,char **argv){
 if(argc<3){fprintf(stderr,"usage: %s examples.tsv 'query'\n",argv[0]);return 2;}
 FILE *f=fopen(argv[1],"r");if(!f){perror(argv[1]);return 1;}
 char line[2048];
 while(fgets(line,sizeof(line),f)&&count<MAX){
  char *t=strchr(line,'\t');if(!t||line[0]=='#')continue;*t++=0;
  t[strcspn(t,"\r\n")]=0;
  if(!*line||!*t||strlen(line)>=sizeof(rows[0].text)||strlen(t)>=sizeof(rows[0].label))continue;
  strcpy(rows[count].text,line);strcpy(rows[count].label,t);count++;
 }
 fclose(f);
 Words q=tokenize(argv[2]);double top=0,second=0;int winner=-1;
 for(int i=0;i<count;i++){
  Words w=tokenize(rows[i].text);double s=similarity(&q,&w);
  if(s>top){if(winner<0||strcmp(rows[winner].label,rows[i].label))second=top;top=s;winner=i;}
  else if(winner>=0&&strcmp(rows[winner].label,rows[i].label)&&s>second)second=s;
 }
 int accepted=winner>=0&&top>=0.22&&top-second>=0.03;
 printf("intent=%s score=%.4f margin=%.4f\n",accepted?rows[winner].label:"UNCERTAIN",top,top-second);
 if(winner>=0)printf("nearest=%s\n",rows[winner].text);
 return 0;
}
