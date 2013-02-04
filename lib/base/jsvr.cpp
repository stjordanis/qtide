#include <QApplication>
#include <QByteArray>
#include <QDebug>
#include <QFile>

#include <csignal>
#include <assert.h>

#ifdef __MACH__
#include <mach-o/dyld.h>
#endif

#include "jsvr.h"
#include "util.h"

#ifdef ANDROID
#include <sys/stat.h>
#endif

using namespace std;

typedef void* (_stdcall *JInitType)     ();
typedef int   (_stdcall *JDoType)       (void*, C*);
typedef A   (_stdcall *JGetAType)       (void*, I n, C*);
typedef C*    (_stdcall *JGetLocaleType)(void*);
typedef void  (_stdcall *JSMType)       (void*, void*);
typedef void  (_stdcall *JFreeType)     (void*);
typedef A     (_stdcall *JgaType)       (J jt, I t, I n, I r, I*s);
typedef int   (_stdcall *JSetAType)      (void*, I n, C*, I x, C*);

typedef void  (_stdcall * outputtype)(J,int,C*);
typedef int   (_stdcall * dowdtype)  (J,int, A, A*);
typedef C* (_stdcall * inputtype) (J,C*);

int _stdcall JDo(J jt,C*);                   /* run sentence */
C* _stdcall JGetLocale(J jt);                /* get locale */
J _stdcall JInit();                          /* init instance */
int _stdcall JFree(J jt);                    /* free instance */
A _stdcall JGetA(J jt,I n,C* name);          /* get 3!:1 from name */
I _stdcall JSetA(J jt,I n,C* name,I x,C* d); /* name=:3!:2 data */
void _stdcall JSM(J jt, void*callbacks[]);  /* set callbacks */

A jega(I t, I n, I r, I*s);
char* jegetlocale();
void* jehjdll();

char **adadbreak;
char inputline[BUFLEN+1];
J jt;

static char path[PLEN];
static char pathdll[PLEN];

static JDoType jdo;
static JFreeType jfree;
static JgaType jga;
static JGetAType jgeta;
static JGetLocaleType jgetlocale;
static void* hjdll;
static JSetAType jseta;

extern QString LibName;

// ---------------------------------------------------------------------
void addargv(int argc, char* argv[], C* d)
{
  C *p,*q;
  I i;

  p=d+strlen(d);
  for(i=0; i<argc; ++i) {
    if(sizeof(inputline)<(100+strlen(d)+2*strlen(argv[i]))) exit(100);
    if(1==argc) {
      *p++=',';
      *p++='<';
    }
    if(i)*p++=';';
    *p++='\'';
    q=argv[i];
    while(*q) {
      *p++=*q++;
      if('\''==*(p-1))*p++='\'';
    }
    *p++='\'';
  }
  *p=0;
}

// ---------------------------------------------------------------------
int jedo(char* sentence)
{
  if (!jt) return 0;
  return jdo(jt,sentence);
}

// ---------------------------------------------------------------------
void jefree()
{
  if (jt) jfree(jt);
}

// ---------------------------------------------------------------------
char* jegetlocale()
{
  if (!jt) return (char *)"base";
  return jgetlocale(jt);
}

// ---------------------------------------------------------------------
A jega(I t, I n, I r, I*s)
{
  if (!jt) return 0;
  return jga(jt,t,n,r,s);
}

// ---------------------------------------------------------------------
void* jehjdll()
{
  return hjdll;
}

// ---------------------------------------------------------------------
// load JE, Jinit, getprocaddresses, JSM
J jeload(void* callbacks)
{
#ifdef _WIN32
  WCHAR wpath[PLEN];
  MultiByteToWideChar(CP_UTF8,0,pathdll,1+(int)strlen(pathdll),wpath,PLEN);
  hjdll=LoadLibraryW(wpath);
  if(!hjdll)return 0;
  jt=((JInitType)GETPROCADDRESS((HMODULE)hjdll,"JInit"))();
  if(!jt) return 0;
  ((JSMType)GETPROCADDRESS((HMODULE)hjdll,"JSM"))(jt,callbacks);
  jdo=(JDoType)GETPROCADDRESS((HMODULE)hjdll,"JDo");
  jfree=(JFreeType)GETPROCADDRESS((HMODULE)hjdll,"JFree");
  jga=(JgaType)GETPROCADDRESS((HMODULE)hjdll,"Jga");
  jgeta=(JGetAType)GETPROCADDRESS((HMODULE)hjdll,"JGetA");
  jgetlocale=(JGetLocaleType)GETPROCADDRESS((HMODULE)hjdll,"JGetLocale");
  jseta=(JSetAType)GETPROCADDRESS((HMODULE)hjdll,"JSetA");
  return jt;
#else
  hjdll=dlopen(pathdll,RTLD_LAZY);
  if(!hjdll)return 0;
  jt=((JInitType)GETPROCADDRESS(hjdll,"JInit"))();
  if(!jt) return 0;
  ((JSMType)GETPROCADDRESS(hjdll,"JSM"))(jt,callbacks);
  jdo=(JDoType)GETPROCADDRESS(hjdll,"JDo");
  jfree=(JFreeType)GETPROCADDRESS(hjdll,"JFree");
  jga=(JgaType)GETPROCADDRESS(hjdll,"Jga");
  jgeta=(JGetAType)GETPROCADDRESS(hjdll,"JGetA");
  jgetlocale=(JGetLocaleType)GETPROCADDRESS(hjdll,"JGetLocale");
  jseta=(JSetAType)GETPROCADDRESS(hjdll,"JSetA");
  return jt;
#endif

}

// ---------------------------------------------------------------------
// set path and pathdll (wpath also set for win)
// WIN arg is 0, Unix arg is argv[0]
void jepath(char* arg)
{
#ifdef _WIN32
  WCHAR wpath[PLEN];
  GetModuleFileNameW(0,wpath,_MAX_PATH);
  *(wcsrchr(wpath, '\\')) = 0;
  WideCharToMultiByte(CP_UTF8,0,wpath,1+(int)wcslen(wpath),path,PLEN,0,0);
#elif defined(ANDROID)
  Q_UNUSED(arg);
  strcpy(path,QCoreApplication::applicationDirPath().toUtf8().data());
#else
#define sz 4000
  char arg2[sz],arg3[sz];
  char* src,*snk;
  int n;
// fprintf(stderr,"arg0 %s\n",arg);
// try host dependent way to get path to executable
// use arg if they fail (arg command in PATH won't work)
#ifdef __MACH__
  uint32_t len=sz;
  n=_NSGetExecutablePath(arg2,&len);
  if(0!=n) strcat(arg2,arg);
#else
  n=readlink("/proc/self/exe",arg2,sizeof(arg2));
  if(-1==n) strcpy(arg2,arg);
  else arg2[n]=0;
#endif
// fprintf(stderr,"arg2 %s\n",arg2);
// arg2 is path (abs or relative) to executable or soft link
  n=readlink(arg2,arg3,sz);
  if(-1==n) strcpy(arg3,arg2);
  else arg3[n]=0;
// fprintf(stderr,"arg3 %s\n",arg3);
  if('/'==*arg3)
    strcpy(path,arg3);
  else {
    char *c=getcwd(path,sizeof(path));
    Q_UNUSED(c);
    strcat(path,"/");
    strcat(path,arg3);
  }
  *(1+strrchr(path,'/'))=0;
// remove ./ and backoff ../
  snk=src=path;
  while(*src) {
    if('/'==*src&&'.'==*(1+src)&&'.'==*(2+src)&&'/'==*(3+src)) {
      *snk=0;
      snk=strrchr(path,'/');
      snk=0==snk?path:snk;
      src+=3;
    } else if('/'==*src&&'.'==*(1+src)&&'/'==*(2+src))
      src+=2;
    else
      *snk++=*src++;
  }
  *snk=0;
  snk=path+strlen(path)-1;
  if('/'==*snk) *snk=0;
#endif
  strcpy(pathdll,path);
  strcat(pathdll,JDLLNAME);
// fprintf(stderr,"arg4 %s\n",path);
}

// ---------------------------------------------------------------------
void jefail(char* msg)
{
  strcpy(msg, "Load library ");
  strcat(msg, pathdll);
  strcat(msg," failed.");
}

// ---------------------------------------------------------------------
// build and run first sentence to set BINPATH, ARGV, and run profile
// arg is command line ready to set in ARGV_z_
// type is 0 normal, 1 -jprofile xxx, 2 ijx basic, 3 nothing
// profile[ARGV_z_=:...[BINPATH=:....
// profile is from BINPATH, ARGV, ijx basic, or nothing
int jefirst(int type,char* arg)
{
  int r;
  char* p,*q;
  char* input=(char *)malloc(2000+strlen(arg));

#if ANDROID
  char *homepath;
  struct stat st;
  if(!getenv("HOME")) {
    if(!stat("/sdcard",&st)) {
      setenv("HOME","/sdcard",1);
    } else {
      setenv("HOME",path,1);
    }
  }
  homepath=getenv("HOME");
  if(!getenv("TMP")) {
    char tmp[PLEN];
    strcpy(tmp, homepath);
    strcat(tmp, filesepx);
    strcat(tmp, "tmp");
    if(stat(tmp,&st)) mkdir(tmp, S_IRWXU | S_IRWXG | S_IRWXO);
    setenv("TMP",tmp,1);
  }

// assume cwd is .../files
  int v1=0, v2=0;
  QFile *f1 = new QFile("assets:/assets_version.txt");
  QFile *f2 = new QFile("assets_version.txt");
  if (f1->exists()) {
    QString s= cfread(f1);
    if (!(s.isNull() || s.isEmpty())) v1=(int)strtol(s.toUtf8().data(),NULL,0);
  }
  if (f2->exists()) {
    QString s= cfread(f2);
    if (!(s.isNull() || s.isEmpty())) v2=(int)strtol(s.toUtf8().data(),NULL,0);
  }
  delete f1;
  delete f2;
  if (v1>v2) {
    QFile("jqtdata.tgz").remove();
    QFile("tar0.ijs").remove();
    QFile("assets:/jqtdata.tgz").copy("jqtdata.tgz");
    QFile("assets:/tar0.ijs").copy("tar0.ijs");
    QFile::setPermissions("jqtdata.tgz",(QFile::Permission)0x6666);
    QFile::setPermissions("tar0.ijs",(QFile::Permission)0x6666);
    jedo((char *)"script0=: [: 3 : '0!:0 y [ 4!:55<''y''' ]&.:>");
    jedo((char *)"(i.0 0)[4!:55 ::0:<'script0'[18!:55 ::0:<'j'[18!:55 ::0:<'jtar0'[tar_jtar0_ ::0:'x';'jqtdata.tgz';'.'[script0 ::0:'tar0.ijs'");
// neither worked, will be done on load ide/qt
//    QFile::setPermissions("tools/zip/7za",(QFile::Permission)0x7777);
//    QFile::setPermissions("tools/ftp/wget",(QFile::Permission)0x7777);
//    jedo((char *)"(i.0 0)['rwxrwxrwx'1!:7 ::0:<'tools/zip/7za'['rwxrwxrwx'1!:7 ::0:<'tools/ftp/wget'");
    QFile("jqtdata.tgz").remove();
    QFile("tar0.ijs").remove();
    QFile("assets_version.txt").remove();
    QFile("assets:/assets_version.txt").copy("assets_version.txt");
    QFile::setPermissions("assets_version.txt",(QFile::Permission)0x6666);
  }
#endif

  *input=0;
  if(0==type) {
    strcat(input,"(3 : '0!:0 y')<BINPATH,'");
    strcat(input,filesepx);
    strcat(input,"profile.ijs'");
  } else if(1==type)
    strcat(input,"(3 : '0!:0 y')2{ARGV");
  else if(2==type)
    strcat(input,"");   // strcat(input,ijx);
  else
    strcat(input,"i.0 0");
  strcat(input,"[ARGV_z_=:");
  strcat(input,arg);
  strcat(input,"[BINPATH_z_=:'");
#if ANDROID
  strcat(input,homepath);
  strcat(input,"/bin'");
  strcat(input,"[UNAME_z_=:'Android'");
#else
  p=path;
  q=input+strlen(input);
  while(*p) {
    if(*p=='\'') *q++='\'';	// 's doubled
    *q++=*p++;
  }
  *q=0;
  strcat(input,"'");
#endif
  strcat(input,"[IFQT_z_=:1");
  string s="[libjqt_z_=: '"+q2s(LibName)+"'";
  strcat(input,s.c_str());
  r=jedo(input);
  free(input);
  return r;
}

// ---------------------------------------------------------------------
void sigint(int k)
{
  Q_UNUSED(k);
  **adadbreak+=1;
  signal(SIGINT,sigint);
}

// ---------------------------------------------------------------------
// jdo with result (contains 3!:1 rep)
A dora(QString s)
{
  strcpy(inputline,q2s("r_jrx_=:"+s).c_str());
  jdo(jt,inputline);
  return jgeta(jt,6,(char*)"r_jrx_");
}

// ---------------------------------------------------------------------
// jdo with QString result
QString dors(QString s)
{
  QString t;
  if (!jt) return "";
  A r=dora(s);
  AREP p=(AREP) (sizeof(A_RECORD) + (char*)r);
  assert(p->t==2);
  assert(p->r<2);
  if (p->r==0)
    return QString::fromUtf8(((char*)p->s),1);
  else
    return QString::fromUtf8((sizeof(AREP_RECORD)+(char*)p), p->c);
}

// ---------------------------------------------------------------------
// set QString in J
void sets(QString name, QString s)
{
  int n,hlen,nlen,slen,tlen;

  I hdr[5];
  n=sizeof(I);
  hlen=n*5;

  QByteArray nb=name.toUtf8();
  nlen=nb.size();

  QByteArray sb=s.toUtf8();
  slen=sb.size();
  tlen=n*(1+slen/n);

  hdr[0]=(4==n) ? 225 : 227;
  hdr[1]=2;
  hdr[3]=1;
  hdr[2]=hdr[4]=slen;

  C* buf=(C*)calloc(hlen+tlen,sizeof(char));
  memcpy(buf,hdr,hlen);
  strncpy(buf+hlen,sb,slen);
  if (jt) jseta(jt,nlen,(C*)nb.constData(),(hlen+tlen),buf);
  free(buf);
}

// ---------------------------------------------------------------------
void dumpA(A a)
{
  qDebug() << "k " << a->k;
  qDebug() << "flag " << a->flag;
  qDebug() << "m " << a->m;
  qDebug() << "t " << a->t;
  qDebug() << "c " << a->c;
  qDebug() << "n " << a->n;
  qDebug() << "r " << a->r;
  qDebug() << "s " << a->s[0];
}

// ---------------------------------------------------------------------
void dumpAREP(AREP a)
{
  qDebug() << "n " << a->n;
  qDebug() << "t " << a->t;
  qDebug() << "c " << a->c;
  qDebug() << "r " << a->r;
  qDebug() << "s " << a->s[0];
}