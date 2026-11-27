/* TinyTalk Reason 1.0: a bounded hybrid dialogue prototype, not consciousness.
 * The original MLP proposes an intent. Typed parsers, explicit state and
 * conservative guards decide whether that interpretation may be applied. */
#ifdef main
#undef main
#define REASON_TEST_INCLUDE
#endif
#define main tinytalk_baseline_main
#include "tiny_base.c"
#undef main
#include <stdarg.h>
#include <ctype.h>
#include <time.h>

#define CAP 64
#define TEXT 160
typedef struct {long value; unsigned turn;} Event;
typedef struct {char object[TEXT], owner[TEXT]; unsigned turn;} Fact;
typedef struct {
 long goal; unsigned goal_turn,turn; int has_goal, meals, timers, facts;
 Event meal[CAP],timer[CAP]; Fact fact[CAP];
 int topic; /* 0 none, 1 calories, 2 timer, 3 ownership */
 int pending_timer; char last_object[TEXT];
} State;
typedef struct {char code[40],answer[1024],proof[2048]; int changed;} Reply;
static int rules_only=0;
static void say(Reply *r,const char *code,const char *fmt,...){
 snprintf(r->code,sizeof r->code,"%s",code);
 va_list a;va_start(a,fmt);vsnprintf(r->answer,sizeof r->answer,fmt,a);va_end(a);
}
static void proof(Reply *r,const char *fmt,...){va_list a;va_start(a,fmt);vsnprintf(r->proof,sizeof r->proof,fmt,a);va_end(a);}
/* Human-authored descriptions of the actually selected rule, not introspection. */
static void explain(const Reply *r){
 const char *task="уточнить запрос; подтверждённого действия нет";
 struct {const char *code,*task;} map[]={
 {"GOAL","задать цель питания"},{"MEAL","добавить приём пищи"},
 {"CORRECTED","исправить прежнее значение без добавления события"},
 {"REMAINING","вычесть сумму приёмов пищи из цели"},{"TOTAL","сложить записанные калории"},
 {"ASK_DURATION","уточнить длительность перед созданием таймера"},
 {"TIMER","записать учебный таймер; системного запуска нет"},
 {"CANCELLED","отменить выбранную операцию или запись"},
 {"FACT","сохранить факт владения объектом"},{"TRANSFER","обновить владельца объекта"},
 {"OWNER","сообщить владельца по памяти разговора"},
 {"CONFLICT","остановить изменение: обнаружено противоречие"},
 {"NEGATED","сохранить состояние: действие отрицается"},
 {"RESET","очистить память диалога"},{"STATE","показать состояние диалога"},
 {"MISSING_GOAL","уточнить цель питания"},{"UNKNOWN_FACT","запросить отсутствующий факт"},
 {"HELLO","ответить на приветствие"}};
 for(size_t i=0;i<sizeof map/sizeof map[0];i++)if(!strcmp(r->code,map[i].code)){task=map[i].task;break;}
 printf("Разбор: %s.\nТребуемая операция: %s.\n",r->code,task);
 if(*r->proof)printf("Основание: %s\n",r->proof);
}
/* Restricted UTF-8 normalizer. Reject malformed, oversized and control input.
 * Russian/ASCII lowercase, yo->e; retain digits/punctuation for exact parsing. */
static int normalize(const char *s,char *out,size_t cap){
 const unsigned char *p=(const unsigned char*)s;size_t k=0;
 while(*p){uint32_t u=*p++;int n=0;uint32_t min=0;
  if(u>=0xc2&&u<=0xdf){u&=31;n=1;min=128;}
  else if(u>=0xe0&&u<=0xef){u&=15;n=2;min=2048;}
  else if(u>=0xf0&&u<=0xf4){u&=7;n=3;min=65536;}
  else if(u>=128)return 0;
  for(int j=0;j<n;j++){if((*p&192)!=128)return 0;u=(u<<6)|(*p++&63);}
  if((n&&u<min)||u>0x10ffff||(u>=0xd800&&u<=0xdfff))return 0;
  if(u<32&&u!='\t')return 0;
  if(u>='A'&&u<='Z')u+=32;
  if(u>=0x410&&u<=0x42f)u+=32;
  if(u==0x401||u==0x451)u=0x435;
  unsigned char b[4];size_t len;
  if(u<128){b[0]=(unsigned char)u;len=1;}
  else if(u<2048){b[0]=192|(u>>6);b[1]=128|(u&63);len=2;}
  else if(u<65536){b[0]=224|(u>>12);b[1]=128|((u>>6)&63);b[2]=128|(u&63);len=3;}
  else{b[0]=240|(u>>18);b[1]=128|((u>>12)&63);b[2]=128|((u>>6)&63);b[3]=128|(u&63);len=4;}
  if(k+len>=cap)return 0;
  memcpy(out+k,b,len);k+=len;
 }out[k]=0;return 1;
}
static int has(const char *s,const char *part){return strstr(s,part)!=NULL;}
static int word(const char *s,const char *w){size_t n=strlen(w);const char *p=s;
 while((p=strstr(p,w))){int left=p==s||strchr(" \t,.;:!?",p[-1]);int right=!p[n]||strchr(" \t,.;:!?",p[n]);if(left&&right)return 1;p+=n;}return 0;}
/* Integer-only quantities. Decimals, signs and >2 numbers are explicit refusals. */
static int numbers(const char *s,long v[2]){int n=0;for(const char *p=s;*p;){
 if(*p>='0'&&*p<='9'){
  if(p>s&&(p[-1]=='-'||p[-1]=='+'||p[-1]=='.'||p[-1]==','))return -1;
  char *end;errno=0;long x=strtol(p,&end,10);
  if(errno||x>1000000||n==2||((*end=='.'||*end==',')&&isdigit((unsigned char)end[1])))return -1;
  v[n++]=x;p=end;
 }else p++;
 }return n;}
static int fact_index(State *s,const char *obj){for(int i=0;i<s->facts;i++)if(!strcmp(obj,s->fact[i].object))return i;return -1;}
static int ident(const char *s){if(!*s||strlen(s)>=TEXT)return 0;for(const unsigned char *p=(const unsigned char*)s;*p;p++)if(*p<128&&!isalnum(*p)&&*p!='_'&&*p!='-')return 0;return 1;}
/* Controlled relation language: ownership is never guessed from vague text. */
static int relation(State *s,const char *u,Reply *r){
 char obj[TEXT],from[TEXT],to[TEXT],extra[TEXT];
 if(!strncmp(u,"у кого ",strlen("у кого "))){
  const char *tail=u+strlen("у кого ");
  if(strlen(tail)>=sizeof obj){say(r,"CLARIFY","Слишком длинное имя объекта.");return 1;}
  memcpy(obj,tail,strlen(tail)+1);obj[strcspn(obj,"?.!")]=0;
  if(!ident(obj)){say(r,"CLARIFY","Назовите один объект: у кого ключ?");return 1;}
  int i=fact_index(s,obj);s->topic=3;
  if(i<0){s->last_object[0]=0;say(r,"UNKNOWN_FACT","Не знаю, у кого %s.",obj);}
  else{snprintf(s->last_object,TEXT,"%s",obj);say(r,"OWNER","%s: владелец — %s.",obj,s->fact[i].owner);proof(r,"Факт из реплики %u: owner(%s)=%s.",s->fact[i].turn,obj,s->fact[i].owner);}
  return 1;
 }
 if(sscanf(u,"%159s у %159s %159s",obj,to,extra)==2&&ident(obj)&&ident(to)){
  int i=fact_index(s,obj);
  if(i>=0&&strcmp(s->fact[i].owner,to)){say(r,"CONFLICT","Уже записан другой владелец. Укажите передачу: передай %s от %s к %s",obj,s->fact[i].owner,to);return 1;}
  if(i<0){if(s->facts==CAP){say(r,"FULL","Память объектов заполнена.");return 1;}i=s->facts++;}
  snprintf(s->fact[i].object,TEXT,"%s",obj);snprintf(s->fact[i].owner,TEXT,"%s",to);s->fact[i].turn=s->turn;
  snprintf(s->last_object,TEXT,"%s",obj);s->topic=3;r->changed=1;say(r,"FACT","Запомнил: %s у %s.",obj,to);proof(r,"Добавлен факт из реплики %u.",s->turn);return 1;
 }
 if(sscanf(u,"передай %159s от %159s к %159s %159s",obj,from,to,extra)==3&&ident(obj)&&ident(from)&&ident(to)){
  if(!strcmp(obj,"его")){if(!*s->last_object){say(r,"CLARIFY","Какой объект передать?");return 1;}snprintf(obj,TEXT,"%s",s->last_object);}
  int i=fact_index(s,obj);if(i<0){say(r,"UNKNOWN_FACT","Неизвестен исходный владелец объекта %s.",obj);return 1;}
  if(strcmp(s->fact[i].owner,from)){say(r,"CONFLICT","Передача противоречит памяти: %s у %s.",obj,s->fact[i].owner);return 1;}
  proof(r,"Факт из реплики %u: owner(%s)=%s. Передача из реплики %u: %s -> %s. Новый owner(%s)=%s.",s->fact[i].turn,obj,from,s->turn,from,to,obj,to);
  snprintf(s->fact[i].owner,TEXT,"%s",to);s->fact[i].turn=s->turn;snprintf(s->last_object,TEXT,"%s",obj);s->topic=3;r->changed=1;
  say(r,"TRANSFER","Теперь %s у %s. Это запись в памяти, не внешнее действие.",obj,to);return 1;
 }return 0;
}
static void respond(Model *m,State *s,const char *input,Reply *r,int trace){
 memset(r,0,sizeof *r);char u[2048];
 if(!normalize(input,u,sizeof u)){say(r,"INVALID","Некорректный UTF-8 или слишком длинное сообщение.");return;}
 if(!strcmp(u,"/reset")){memset(s,0,sizeof *s);say(r,"RESET","Память разговора очищена.");return;}
 if(!strcmp(u,"/state")){say(r,"STATE","тема=%d цель=%ld задана=%d еда=%d таймеры=%d объекты=%d ожидание_времени=%d",s->topic,s->goal,s->has_goal,s->meals,s->timers,s->facts,s->pending_timer);return;}
 s->turn++;
 Features x;float h[H],p[MAXC];const char *intent="disabled";float score=0;
 if(!rules_only){features(u,&x);int c=forward(m,&x,h,p);intent=m->answer[c];score=p[c];}
 if(trace)fprintf(stderr,"[MLP] intent=%s score=%.4f (не калиброванная вероятность)\n",intent,score);
 long v[2]={0};int n=numbers(u,v);
 if(n<0){say(r,"CLARIFY","Нужны не более двух целых неотрицательных чисел до 1000000.");return;}
 if(word(u,"не")&&!(n==2&&word(u,"а"))){say(r,"NEGATED","Отрицание: ничего не изменено. Переформулируйте положительное действие или отмените явно.");return;}
 if(word(u,"если")||word(u,"возможно")||word(u,"может")||word(u,"завтра")){say(r,"CLARIFY","Условные и будущие события пока не записываю. Уточните фактическое действие.");return;}
 if(relation(s,u,r))return;
 int cal=has(u,"ккал")||has(u,"калори"),timer=has(u,"таймер");
 if(cal&&timer){say(r,"CLARIFY","Разделите запрос о питании и таймере на два сообщения.");return;}
 int correction=has(u,"исправ")||word(u,"нет")||(word(u,"не")&&word(u,"а"));
 if(correction){
  if((cal||s->topic==1)&&!timer){
   int target_goal=has(u,"цел")||has(u,"лимит");
   if((target_goal&&!s->has_goal)||(!target_goal&&!s->meals)||!n){say(r,"CLARIFY","Нет однозначного значения для исправления.");return;}
   long old=target_goal?s->goal:s->meal[s->meals-1].value;
   if(n==2&&v[0]!=old){say(r,"CONFLICT","Старое значение не совпадает: в памяти %ld.",old);return;}
   long value=v[n-1];if(target_goal){s->goal=value;s->goal_turn=s->turn;}else{s->meal[s->meals-1].value=value;s->meal[s->meals-1].turn=s->turn;}
   r->changed=1;s->topic=1;say(r,"CORRECTED","Исправлено: %ld -> %ld ккал.",old,value);proof(r,"Замена %s, не добавление нового события; реплика %u.",target_goal?"цели":"последнего приёма пищи",s->turn);return;
  }say(r,"CLARIFY","Уточните, какое значение питания исправить.");return;
 }
 if(has(u,"отмен")||has(u,"удали послед")){
  if(s->pending_timer&&!cal){s->pending_timer=0;r->changed=1;say(r,"CANCELLED","Создание таймера отменено.");return;}
  if(timer||s->topic==2){if(s->timers>1){say(r,"CLARIFY","Несколько таймеров: отмена по идентификатору пока не поддерживается. Используйте /reset для очистки.");return;}
   if(!s->timers){say(r,"NOTHING","Нет таймера для отмены.");return;}s->timers=0;r->changed=1;say(r,"CANCELLED","Учебный таймер отменён.");return;}
  if(cal||s->topic==1){if(!s->meals){say(r,"NOTHING","Нет приёма пищи для отмены.");return;}s->meals--;r->changed=1;say(r,"CANCELLED","Последний приём пищи удалён.");return;}
  say(r,"CLARIFY","Что именно отменить?");return;
 }
 int question=has(u,"сколько")||has(u,"какой")||has(u,"покажи")||has(u,"посчитай");
 if((has(u,"цел")||has(u,"лимит"))&&!question){
  if(!cal||n!=1){say(r,"CLARIFY","Укажите цель одним числом и единицей ккал.");return;}
  s->goal=v[0];s->has_goal=1;s->goal_turn=s->turn;s->topic=1;r->changed=1;
  say(r,"GOAL","Цель: %ld ккал.",s->goal);proof(r,"goal=%ld; источник — реплика %u.",s->goal,s->turn);return;
 }
 if(!question&&(has(u,"съел")||has(u,"съела")||has(u,"поел")||has(u,"завтрак")||has(u,"обед")||has(u,"ужин"))){
  if(!cal||n!=1){say(r,"CLARIFY","Укажите один приём пищи и его энергию в ккал.");return;}
  if(s->meals==CAP){say(r,"FULL","Память питания заполнена.");return;}
  s->meal[s->meals++]=(Event){v[0],s->turn};s->topic=1;r->changed=1;
  say(r,"MEAL","Записано: %ld ккал.",v[0]);proof(r,"Событие питания #%d из реплики %u.",s->meals,s->turn);return;
 }
 /* Learned classifier handles alternative question wording, but cannot mutate
  * state by itself. Explicit numeric/time parsers gate every write. */
 int remaining=has(u,"остал")||has(u,"остаток")||(score>=.85f&&!strcmp(intent,"remaining")&&n==0);
 int total=has(u,"итого")||has(u,"сумм")||(score>=.85f&&!strcmp(intent,"total")&&n==0);
 if(remaining||total){
  if(timer||(!cal&&s->topic==2)){say(r,"UNSUPPORTED","Таймер учебный: течение времени не отслеживается.");return;}
  if(!cal&&s->topic!=1){say(r,"CLARIFY","Остаток чего? Укажите калории.");return;}
  long sum=0;for(int i=0;i<s->meals;i++)sum+=s->meal[i].value;
  s->topic=1;
  if(remaining&&!s->has_goal){say(r,"MISSING_GOAL","Сначала укажите цель в ккал.");return;}
  if(remaining){long left=s->goal-sum;if(left>=0)say(r,"REMAINING","Осталось %ld ккал.",left);else say(r,"REMAINING","Цель превышена на %ld ккал.",-left);
   proof(r,"goal из реплики %u: %ld; сумма %d событий=%ld; %ld - %ld = %ld ккал.",s->goal_turn,s->goal,s->meals,sum,s->goal,sum,left);
  }else{say(r,"TOTAL","Всего %ld ккал.",sum);proof(r,"Сумма %d сохранённых событий питания.",s->meals);}return;
 }
 if(timer||s->pending_timer){
  int minutes=has(u,"минут"),seconds=has(u,"секунд");
  if(n==0&&timer){s->pending_timer=1;s->topic=2;say(r,"ASK_DURATION","На сколько минут или секунд?");return;}
  if(n!=1||minutes==seconds||v[0]==0){say(r,"CLARIFY","Укажите одно положительное число и единицу: минуты или секунды.");return;}
  if(s->timers==CAP){say(r,"FULL","Память таймеров заполнена.");return;}
  long duration=v[0]*(minutes?60:1);s->timer[s->timers++]=(Event){duration,s->turn};s->pending_timer=0;s->topic=2;r->changed=1;
  say(r,"TIMER","Учебный таймер #%d: %ld секунд. Системный таймер не запускается.",s->timers,duration);proof(r,"%ld %s -> %ld секунд; параметр связан с текущим запросом.",v[0],minutes?"минут":"секунд",duration);return;
 }
 if(word(u,"привет")||word(u,"здравствуй")){say(r,"HELLO","Привет! Могу вести учёт ккал, учебные таймеры и связи владельцев объектов.");return;}
 say(r,"UNKNOWN","Не удалось надёжно разобрать запрос. Уточните действие, объект и параметры.");
}
#ifdef REASON_TEST_INCLUDE
#define main reason_program_main
#endif
int main(int argc,char **argv){
 if(argc<2){fprintf(stderr,"Usage: reason MODEL [--batch] [--trace] [--explain] [--rules-only] | --test TESTS\n");return 2;}
 Model *m=alloc(sizeof *m);checkpoint(m,argv[1],0);State s={0};int trace=0,batch=0,explain_on=0,fail=0,count=0;
 FILE *in=stdin;int testing=argc==4&&!strcmp(argv[2],"--test");
 if(testing){in=fopen(argv[3],"r");if(!in)die(argv[3]);}
 else for(int i=2;i<argc;i++){if(!strcmp(argv[i],"--trace"))trace=1;else if(!strcmp(argv[i],"--batch"))batch=1;else if(!strcmp(argv[i],"--explain"))explain_on=1;else if(!strcmp(argv[i],"--rules-only"))rules_only=1;else die("неизвестный аргумент");}
 if(!testing&&!batch)puts("TinyTalk Reason 1.1 | /reset /state /quit | Введите сообщение.");
 char line[LINE];clock_t start=clock();
 while(fgets(line,sizeof line,in)){
  if(!strchr(line,'\n')&&!feof(in)){int ch;while((ch=fgetc(in))!='\n'&&ch!=EOF){}puts("INVALID | Строка слишком длинная.");if(testing)fail++;continue;}
  line[strcspn(line,"\r\n")]=0;if(!*line||*line=='#')continue;if(!strcmp(line,"/quit"))break;
  char *expected=NULL,*snippet=NULL;
  if(testing){expected=strchr(line,'\t');if(!expected)die("test: missing TAB");*expected++=0;snippet=strchr(expected,'\t');if(snippet)*snippet++=0;}
  Reply r;respond(m,&s,line,&r,trace);count++;
  if(testing){int ok=!strcmp(expected,r.code)&&(!snippet||strstr(r.answer,snippet));fail+=!ok;printf("%s | %s | %s | %s\n",ok?"PASS":"FAIL",line,r.code,r.answer);}
  else printf("%s | %s\n",r.code,r.answer);
  if(explain_on)explain(&r);
  if(trace&&*r.proof)fprintf(stderr,"[Основание] %s\n",r.proof);
 }
 if(testing){printf("RESULT %d/%d; CPU %.6f s; sizeof(State)=%zu bytes; sizeof(Model)=%zu bytes\n",count-fail,count,(double)(clock()-start)/CLOCKS_PER_SEC,sizeof s,sizeof *m);fclose(in);}
 for(int i=0;i<m->c;i++)free(m->answer[i]);free(m);return fail?1:0;
}
