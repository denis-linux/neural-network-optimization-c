/* General intent evaluation using the same trained TinyTalk MLP.
 * Predicting a task label does not supply arguments or execute the task. */
#define main original_tiny_main
#include "tiny_base.c"
#undef main
static void show(Model *m,const char *text,float threshold,const char *catalog){
 Features x;float h[H],p[MAXC];features(text,&x);int best=forward(m,&x,h,p);
 printf("Гипотеза намерения: %s; score=%.4f (не калиброванная вероятность).\n",m->answer[best],p[best]);
 if(p[best]<threshold||!strcmp(m->answer[best],"oos")){puts("Нужно сделать: уточнить запрос; уверенности недостаточно.\nДействие не выполнялось.");return;}
 FILE *f=fopen(catalog,"r");char line[LINE],*a,*b;int found=0;
 if(f){while(read_line(f,line)){if(parse(line,&a,&b)&&!strcmp(a,m->answer[best])){printf("Нужно сделать: %s\n",b);found=1;break;}}fclose(f);}
 if(!found)printf("Нужно сделать: уточнить параметры задачи класса %s.\n",m->answer[best]);
 puts("Параметры: не извлечены этим классификатором.\nСтатус: предложение задачи; действие не выполнялось.");
}
int main(int argc,char **argv){
 if(argc<4){fprintf(stderr,"intent MODEL predict TEST.tsv | chat THRESHOLD CATALOG.tsv | ask THRESHOLD CATALOG.tsv TEXT\n");return 2;}
 Model *m=alloc(sizeof *m);checkpoint(m,argv[1],0);char line[LINE];
 if(!strcmp(argv[2],"predict")){
  if(argc!=4)die("predict: need TEST.tsv");FILE *f=fopen(argv[3],"r");if(!f)die(argv[3]);char *a,*b;Features x;float h[H],p[MAXC];
  while(read_line(f,line))if(parse(line,&a,&b)){features(a,&x);int best=forward(m,&x,h,p);printf("%s\t%s\t%.9g\n",b,m->answer[best],p[best]);}fclose(f);
 }else{
  if(argc<5)die("need threshold and catalog");char *end;float threshold=strtof(argv[3],&end);if(*end||!isfinite(threshold)||threshold<0||threshold>1.0000011f)die("threshold must be 0..1");
  if(!strcmp(argv[2],"ask")){if(argc!=6)die("ask: need quoted TEXT");show(m,argv[5],threshold,argv[4]);}
  else if(!strcmp(argv[2],"chat")){puts("Введите запрос; /quit — выход. Это классификатор задач, не исполнитель.");while(read_line(stdin,line)){line[strcspn(line,"\r\n")]=0;if(!strcmp(line,"/quit"))break;show(m,line,threshold,argv[4]);}}
  else die("unknown mode");
 }
 for(int i=0;i<m->c;i++)free(m->answer[i]);free(m);return 0;
}
