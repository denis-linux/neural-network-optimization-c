#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <time.h>
#include <float.h>
#define H 16
#define P (3*H+H+1)
#define MAXB 65536
#define MAGIC 0x41493131u
static double w[P]; static unsigned version, target, nextjob, rejected; static unsigned batch=1024; static pthread_mutex_t mu=PTHREAD_MUTEX_INITIALIZER;
static unsigned active;
static double start; static unsigned long long samples, bytes;
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static uint32_t rng(uint32_t *s){*s=*s*1664525u+1013904223u;return *s;}
static double rand01(uint32_t *s){return (rng(s)>>8)/16777216.0;}
static void init(void){uint32_t s=42;for(int i=0;i<P;i++)w[i]=(rand01(&s)-.5)*.8;}
static double forward(const double *v,double x,double y,double *h){double z=v[P-1];for(int j=0;j<H;j++){h[j]=tanh(v[3*j]*x+v[3*j+1]*y+v[3*j+2]);z+=v[3*H+j]*h[j];}return 1/(1+exp(-z));}
static void example(uint32_t *s,double *x,double *y,double *t){*x=2*rand01(s)-1;*y=2*rand01(s)-1;*t=(*x>0)!=(*y>0);}
static void gradient(const double *v,unsigned job,unsigned n,double *g){uint32_t s=job*2654435761u+123u;memset(g,0,P*sizeof(double));for(unsigned i=0;i<n;i++){double x,y,t,h[H];example(&s,&x,&y,&t);double d=forward(v,x,y,h)-t;g[P-1]+=d;for(int j=0;j<H;j++){g[3*H+j]+=d*h[j];double a=d*v[3*H+j]*(1-h[j]*h[j]);g[3*j]+=a*x;g[3*j+1]+=a*y;g[3*j+2]+=a;}}for(int j=0;j<P;j++)g[j]/=n;}
static void metrics(void){uint32_t s=987654321;double loss=0;unsigned correct=0;for(int i=0;i<10000;i++){double x,y,t,h[H];example(&s,&x,&y,&t);double p=forward(w,x,y,h);loss-=t*log(fmax(p,1e-15))+(1-t)*log(fmax(1-p,1e-15));correct+=(p>=.5)==(t>0);}printf("version=%u loss=%.8f accuracy=%.4f samples=%llu rejected=%u payload_bytes=%llu seconds=%.6f\n",version,loss/10000,correct/10000.,samples,rejected,bytes,now()-start);fflush(stdout);}
static int io(int fd,void *p,size_t n,int sendit){char *b=p;while(n){ssize_t k=sendit?send(fd,b,n,0):recv(fd,b,n,0);if(k<0&&errno==EINTR)continue;if(k<=0)return 0;b+=k;n-=k;}return 1;}
static int u32(int fd,unsigned *v,int out){uint32_t a=out?htonl(*v):0;if(!io(fd,&a,4,out))return 0;if(!out)*v=ntohl(a);return 1;}
/* IEEE-754 binary64 encoded in network byte order, independent of host endianness. */
static int vec(int fd,double *v,int out){unsigned char b[8*P];if(out)for(int j=0;j<P;j++){uint64_t a;memcpy(&a,&v[j],8);for(int k=0;k<8;k++)b[j*8+k]=(unsigned char)(a>>(56-8*k));}if(!io(fd,b,sizeof b,out))return 0;if(!out)for(int j=0;j<P;j++){uint64_t a=0;for(int k=0;k<8;k++)a=(a<<8)|b[j*8+k];memcpy(&v[j],&a,8);}return 1;}
static void update(double *g,unsigned n,unsigned age){double scale=.8/(1.+age);for(int j=0;j<P;j++)w[j]-=scale*g[j];samples+=n;version++;}
static void *client(void *arg){int fd=*(int*)arg;free(arg);int one=1;setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&one,sizeof one);struct timeval tv={5,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof tv);setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof tv);unsigned magic=0;if(!u32(fd,&magic,0)||magic!=MAGIC)goto done;for(;;){double v[P],g[P];unsigned job,base,n=batch,go;pthread_mutex_lock(&mu);go=version<target;base=version;job=nextjob++;memcpy(v,w,sizeof v);pthread_mutex_unlock(&mu);if(!u32(fd,&go,1)||!go)break;if(!u32(fd,&job,1)||!u32(fd,&base,1)||!u32(fd,&n,1)||!vec(fd,v,1)||!vec(fd,g,0))break;pthread_mutex_lock(&mu);bytes+=16+16*P;unsigned age=version-base;int valid=1;for(int j=0;j<P;j++)if(!isfinite(g[j])||fabs(g[j])>4)valid=0;if(version<target){if(valid&&age<=8)update(g,n,age);else rejected++;if(version==target){metrics();FILE *f=fopen("model.txt.tmp","w");if(f){int ok=1;for(int j=0;j<P;j++)if(fprintf(f,"%.17g\n",w[j])<0)ok=0;if(fclose(f))ok=0;if(!ok||rename("model.txt.tmp","model.txt"))fprintf(stderr,"model save failed\n");}else perror("model save");}}pthread_mutex_unlock(&mu);}done:close(fd);pthread_mutex_lock(&mu);active--;pthread_mutex_unlock(&mu);return NULL;}
static unsigned num(const char *s,unsigned min,unsigned max){char *e;unsigned long a=strtoul(s,&e,10);if(!*s||*e||a<min||a>max){fprintf(stderr,"invalid number: %s\n",s);exit(2);}return a;}
int main(int argc,char **argv){signal(SIGPIPE,SIG_IGN);if(sizeof(double)!=8 || DBL_MANT_DIG!=53 || DBL_MAX_EXP!=1024)return 2;init();start=now();if(argc>=2&&!strcmp(argv[1],"local")){target=argc>2?num(argv[2],1,1000000):1000;batch=argc>3?num(argv[3],1,MAXB):1024;metrics();while(version<target){double g[P];gradient(w,version,batch,g);update(g,batch,0);}metrics();return 0;}if(argc>=2&&!strcmp(argv[1],"server")){if(argc<4)return 2;unsigned port=num(argv[2],1,65535);target=num(argv[3],1,1000000);batch=argc>4?num(argv[4],1,MAXB):1024;const char *host=argc>5?argv[5]:"127.0.0.1";int fd=socket(AF_INET,SOCK_STREAM,0),one=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one);struct sockaddr_in a={.sin_family=AF_INET,.sin_port=htons(port)};if(inet_pton(AF_INET,host,&a.sin_addr)!=1||bind(fd,(void*)&a,sizeof a)||listen(fd,64)){perror("server");return 1;}metrics();for(;;){int c=accept(fd,NULL,NULL);if(c<0){if(errno==EINTR)continue;perror("accept");return 1;}pthread_t th;pthread_mutex_lock(&mu);if(active>=64){pthread_mutex_unlock(&mu);close(c);continue;}active++;pthread_mutex_unlock(&mu);int *p=malloc(sizeof *p);if(p){*p=c;if(!pthread_create(&th,NULL,client,p)){pthread_detach(th);continue;}free(p);}close(c);pthread_mutex_lock(&mu);active--;pthread_mutex_unlock(&mu);} }if(argc>=2&&!strcmp(argv[1],"worker")){if(argc<4)return 2;unsigned port=num(argv[3],1,65535),delay=argc>4?num(argv[4],0,60000):0;int fd=socket(AF_INET,SOCK_STREAM,0);struct sockaddr_in a={.sin_family=AF_INET,.sin_port=htons(port)};if(inet_pton(AF_INET,argv[2],&a.sin_addr)!=1||connect(fd,(void*)&a,sizeof a)){perror("connect");return 1;} int one=1;setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&one,sizeof one);struct timeval tv={10,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof tv);setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof tv);unsigned magic=MAGIC,go,job,base,n,count=0;if(!u32(fd,&magic,1))return 1;for(;;){if(!u32(fd,&go,0)||go>1){close(fd);return 1;}if(!go)break;double v[P],g[P];if(!u32(fd,&job,0)||!u32(fd,&base,0)||!u32(fd,&n,0)||n<1||n>MAXB||!vec(fd,v,0))return 1;gradient(v,job,n,g);if(delay){struct timespec t={delay/1000,(delay%1000)*1000000L};nanosleep(&t,NULL);}if(!vec(fd,g,1))return 1;count++;}printf("worker completed=%u\n",count);close(fd);return 0;}fprintf(stderr,"usage: local [steps batch] | server PORT STEPS [BATCH BIND_IP] | worker IP PORT [delay_ms]\n");return 2;}
