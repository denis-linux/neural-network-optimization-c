#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#define F 2048
#define H 48
#define MAXC 256
#define LINE 8192
/* Sparse normalized input, learned tanh hidden layer, learned softmax head.
 * No pretrained weights and no input-to-answer lookup at inference time.
 * Labels are whole reply strings; this is response classification, not an LM. */
typedef struct {int n,id[F];float v[F];} Features;
typedef struct {Features x;int y;} Example;
typedef struct {uint32_t rng,epochs;int c;char *answer[MAXC];float w1[H][F],b1[H],w2[MAXC][H],b2[MAXC];} Model;
static void die(const char *s){fprintf(stderr,"Ошибка: %s\n",s);exit(1);}
static void *alloc(size_t n){void *p=calloc(1,n);if(!p)die("не хватает памяти");return p;}
static char *copy(const char *s){char *p=alloc(strlen(s)+1);strcpy(p,s);return p;}
static uint32_t random32(Model *m){uint32_t x=m->rng;x^=x<<13;x^=x>>17;x^=x<<5;return m->rng=x;}
static float rnd(Model *m){return ((random32(m)>>8)/16777216.0f-.5f)*.2f;}
/* Fixed UTF-8 word/character features; new words need no vocabulary rebuild. */
static void features(const char *s,Features *x){
 uint32_t cp[1024];int n=0;const unsigned char *p=(const unsigned char*)s;float counts[F]={0};
 while(*p && n<1024){uint32_t a=*p++;int more=0;if(a>=0xf0){a&=7;more=3;}else if(a>=0xe0){a&=15;more=2;}else if(a>=0xc0){a&=31;more=1;}
  while(more--){if((*p&192)!=128)die("неверный UTF-8");a=(a<<6)|(*p++&63);}
  if(a>='A'&&a<='Z')a+=32;if(a>=0x410&&a<=0x42f)a+=32;if(a==0x401||a==0x451)a=0x435;
  if(!((a>='a'&&a<='z')||(a>='0'&&a<='9')||(a>=0x430&&a<=0x44f)))a=' ';
  if(a!=' '||!n||cp[n-1]!=' ')cp[n++]=a;
 }
 for(int i=0;i<n;i++){uint32_t hash=2166136261u;for(int len=1;len<=4&&i+len<=n;len++){hash=(hash^cp[i+len-1])*16777619u;if(len>=2)counts[hash%F]+=1;}}
 uint32_t hash=2166136261u;int len=0;
 for(int i=0;i<=n;i++){if(i==n||cp[i]==' '){if(len)counts[(hash^0x9e3779b9u)%F]+=3;hash=2166136261u;len=0;}else{hash=(hash^cp[i])*16777619u;len++;}}
 float norm=0;for(int i=0;i<F;i++)norm+=counts[i]*counts[i];norm=sqrtf(norm);x->n=0;
 if(norm)for(int i=0;i<F;i++)if(counts[i]){int j=x->n++;x->id[j]=i;x->v[j]=counts[i]/norm;}
}
static int label(Model *m,const char *s,int add){for(int c=0;c<m->c;c++)if(!strcmp(m->answer[c],s))return c;if(!add)return -1;if(m->c==MAXC)die("больше 256 разных ответов");int c=m->c++;m->answer[c]=copy(s);for(int h=0;h<H;h++)m->w2[c][h]=rnd(m);return c;}
/* h = tanh(W1*x+b1); p = softmax(W2*h+b2). */
static int forward(Model *m,const Features *x,float *h,float *p){
 for(int j=0;j<H;j++){float v=m->b1[j];for(int i=0;i<x->n;i++)v+=m->w1[j][x->id[i]]*x->v[i];h[j]=tanhf(v);}
 float max=-1e30f;int best=0;for(int c=0;c<m->c;c++){float v=m->b2[c];for(int j=0;j<H;j++)v+=m->w2[c][j]*h[j];p[c]=v;if(v>max){max=v;best=c;}}
 float sum=0;for(int c=0;c<m->c;c++){p[c]=expf(p[c]-max);sum+=p[c];}for(int c=0;c<m->c;c++)p[c]/=sum;return best;
}
static void io(void *p,size_t size,size_t n,FILE *f,int write){if((write?fwrite(p,size,n,f):fread(p,size,n,f))!=n)die("ошибка файла модели");}
static void checkpoint(Model *m,const char *path,int write){
 FILE *f=fopen(path,write?"wb":"rb");if(!f)die(path);char magic[8]="TTALK01";int dims[3]={F,H,0};
 if(write){dims[2]=m->c;io(magic,1,8,f,1);io(dims,sizeof(int),3,f,1);}else{char got[8];io(got,1,8,f,0);io(dims,sizeof(int),3,f,0);if(memcmp(got,magic,8)||dims[0]!=F||dims[1]!=H||dims[2]<1||dims[2]>MAXC)die("несовместимая модель");m->c=dims[2];}
 io(&m->rng,4,1,f,write);io(&m->epochs,4,1,f,write);
 for(int c=0;c<m->c;c++){uint32_t n=write?(uint32_t)strlen(m->answer[c]):0;io(&n,4,1,f,write);if(!n||n>=LINE)die("неверная длина ответа");if(!write)m->answer[c]=alloc(n+1);io(m->answer[c],1,n,f,write);}
 io(m->w1,sizeof(float),H*F,f,write);io(m->b1,sizeof(float),H,f,write);io(m->w2,sizeof(float),(size_t)m->c*H,f,write);io(m->b2,sizeof(float),m->c,f,write);
 if(fclose(f))die("не удалось закрыть модель");
}
static int parse(char *line,char **a,char **b){line[strcspn(line,"\r\n")]=0;if(!*line||*line=='#')return 0;*a=line;*b=strchr(line,'\t');if(!*b)die("нужен TAB между вопросом и ответом");*(*b)++=0;if(!**a||!**b)die("пустой вопрос или ответ");return 1;}
static int read_line(FILE *f,char *line){if(!fgets(line,LINE,f))return 0;if(!strchr(line,'\n')&&!feof(f))die("слишком длинная строка");return 1;}
int main(int argc,char **argv){
 if(argc<2){fprintf(stderr,"train DATA OUT EPOCHS [PREVIOUS]\nask MODEL TEXT\nchat MODEL\neval MODEL TESTS\n");return 2;}
 int result=0;Model *m=alloc(sizeof *m);m->rng=123456789u;
 if(!strcmp(argv[1],"train")){
  if(argc!=5&&argc!=6)die("train DATA OUT EPOCHS [PREVIOUS]");char *end;long epochs=strtol(argv[4],&end,10);if(*end||epochs<1||epochs>100000)die("неверное число эпох");
  if(argc==6)checkpoint(m,argv[5],0);else for(int j=0;j<H;j++)for(int i=0;i<F;i++)m->w1[j][i]=rnd(m);
  FILE *f=fopen(argv[2],"r");if(!f)die(argv[2]);Example *data=NULL;int n=0;char line[LINE],*a,*b;
  while(read_line(f,line))if(parse(line,&a,&b)){if(n==20000)die("не более 20000 примеров");Example *next=realloc(data,(size_t)(n+1)*sizeof *data);if(!next)die("память");data=next;data[n].y=label(m,b,1);features(a,&data[n].x);n++;}fclose(f);if(!n)die("нет данных");
  int *order=alloc((size_t)n*sizeof *order);for(int i=0;i<n;i++)order[i]=i;
  printf("examples=%d answers=%d previous_epochs=%u new_epochs=%ld\n",n,m->c,m->epochs,epochs);
  for(long e=0;e<epochs;e++){for(int i=n-1;i>0;i--){int j=(int)(random32(m)%(uint32_t)(i+1)),tmp=order[i];order[i]=order[j];order[j]=tmp;}
   double loss=0;int correct=0;
   for(int k=0;k<n;k++){Example *t=&data[order[k]];float h[H],p[MAXC],dh[H]={0};int best=forward(m,&t->x,h,p);correct+=best==t->y;loss-=log(fmax(p[t->y],1e-12));
    /* Cross entropy derivative: d_logits = p - one_hot(target).
     * Accumulate hidden gradients with OLD output weights before updating. */
    for(int c=0;c<m->c;c++){float d=p[c]-(c==t->y);for(int j=0;j<H;j++){dh[j]+=d*m->w2[c][j];m->w2[c][j]-=.06f*d*h[j];}m->b2[c]-=.06f*d;}
    for(int j=0;j<H;j++){float d=dh[j]*(1-h[j]*h[j]);for(int i=0;i<t->x.n;i++)m->w1[j][t->x.id[i]]-=.06f*d*t->x.v[i];m->b1[j]-=.06f*d;}
   }m->epochs++;if(e==0||(e+1)%20==0||e+1==epochs)printf("epoch=%u loss=%.6f online_train_accuracy=%.2f%%\n",m->epochs,loss/n,100.0*correct/n);
  }checkpoint(m,argv[3],1);free(data);free(order);
 }else{
  if(argc<3)die("нужен файл модели");checkpoint(m,argv[2],0);char line[LINE];float h[H],p[MAXC];Features x;
  if(!strcmp(argv[1],"ask")){if(argc!=4)die("ask MODEL TEXT");features(argv[3],&x);puts(m->answer[forward(m,&x,h,p)]);}
  else if(!strcmp(argv[1],"chat")){puts("Введите реплику; /quit — выход.");while(fputs("> ",stdout),fflush(stdout),read_line(stdin,line)){line[strcspn(line,"\r\n")]=0;if(!strcmp(line,"/quit"))break;if(!*line)continue;features(line,&x);int best=forward(m,&x,h,p);printf("%s [score %.3f]\n",m->answer[best],p[best]);}}
  else if(!strcmp(argv[1],"eval")){if(argc!=4)die("eval MODEL TESTS");FILE *f=fopen(argv[3],"r");if(!f)die(argv[3]);int n=0,ok=0;char *a,*b;while(read_line(f,line))if(parse(line,&a,&b)){features(a,&x);int best=forward(m,&x,h,p);int pass=!strcmp(m->answer[best],b);n++;ok+=pass;printf("%s | %s\n  expected: %s\n  actual:   %s\n",pass?"PASS":"FAIL",a,b,m->answer[best]);}fclose(f);printf("RESULT %d/%d\n",ok,n);if(!n||ok<n)result=3;}
  else die("неизвестная команда");
 }for(int i=0;i<m->c;i++)free(m->answer[i]);free(m);return result;
}
