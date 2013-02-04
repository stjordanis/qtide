/* utils - app specific utils */

#include <QDirIterator>
#include <QFont>
#include <QPoint>
#include <QProcess>
#include <QWidget>

#include "base.h"
#include "dialog.h"
#include "note.h"
#include "proj.h"
#include "recent.h"
#include "state.h"
#include "tedit.h"
#include "term.h"

using namespace std;

// ---------------------------------------------------------------------
// convert name to full path name
QString cpath(QString s)
{
  int t;
  QString f,p;

  if ((s.size() == 0) || isroot(s))
    return cfcase(s);
  t=(int) (s.at(0)=='~');
  int n = s.indexOf('/');
  if (n < 0) {
    f=s.mid(t);
    p="";
  } else {
    f=s.mid(t,n-t);
    p=s.mid(n);
  }

  if (f.size() == 0) f = "home";
  bool par = f.at(0) == '.';
  if (par) f.remove(0,1);

  n = config.AllFolderKeys.indexOf(f);
  if (n<0) return cfcase(s);
  f = config.AllFolderValues.at(n);

  if (par) f = cfpath(f);
  return cfcase(f + p);
}

// ---------------------------------------------------------------------
QString defext(QString s)
{
  if (s.isEmpty() || s.contains('.')) return s;
  return s + config.DefExt;
}

// ---------------------------------------------------------------------
int fkeynum(int key,bool c,bool s)
{
  return key + (c*100) + (s*100000);
}

// ---------------------------------------------------------------------
void fontdiff(int n)
{
  config.Font.setPointSize(n+config.Font.pointSize());
  fontset(config.Font);
}

// ---------------------------------------------------------------------
void fontset(QFont font)
{
  config.Font=font;
  tedit->setFont(font);
  if (note) {
    note->setfont(font);
    if (note2)
      note2->setfont(font);
  }
}

// ---------------------------------------------------------------------
// get command string in form: mode)text
QString getcmd(QString mode,QString t)
{
  string v=q2s(t.trimmed());
  const char *c=v.c_str();
  int i=0,p=0,s=v.size();
  for (; i<s; i++) {
    if (c[i]==')') p=i;
    if (! (isalnum(c[i]) || c[i]==')' || c[i]=='.')) break;
  }
  if (p==0) return mode + ")" + t;
  size_t b = v.find_last_of(')',p-1);
  if (b==string::npos) return t;
  v.erase(0,b+1);
  return s2q(v);
}

// ---------------------------------------------------------------------
// return status or empty if not git
QString gitstatus(QString path)
{
  return (shell("git status",path)).at(0);
}

// ---------------------------------------------------------------------
// return status or empty if not git
void gitgui(QString path)
{
  QProcess p;
  p.startDetached("git",QStringList() << "gui",path);
}

// ---------------------------------------------------------------------
QStringList globalassigns(QString s,QString ext)
{
  QStringList p,r;
  QString t;
  QRegExp rx;
  int c,i,pos = 0;

  t=rxassign(ext,true);
  if (t.isEmpty()) return r;
  rx.setPattern("(([a-z]|[A-Z])\\w*)"+t);

  while ((pos = rx.indexIn(s, pos)) != -1) {
    p << rx.cap(1);
    pos += rx.matchedLength();
  }

  qSort(p);
  i=0;
  while (i<p.size()) {
    t=p.at(i);
    c=i++;
    while (i<p.size()&&t==p.at(i)) i++;
    if (1<i-c)
      t=t + " (" + QString::number(i-c) + ")";
    r.append(t);
  }
  return r;
}

// ---------------------------------------------------------------------
void helpabout()
{
  QStringList s=state_about();
  about(s.at(0),s.at(1));
}

// ---------------------------------------------------------------------
void newfile(QWidget *w)
{
  QString s = dialogsaveas(w,"New File", getfilepath());
  if (s.isEmpty()) return;
  if (!s.contains('.'))
    s+=config.DefExt;
  cfcreate(s);
  note->fileopen(s);
}

// ---------------------------------------------------------------------
QString newtempscript()
{
  int i,m,len,t;
  QString f;
  QStringList s=cflist(config.TempPath.absolutePath(),"*" + config.DefExt);

  QList<int> n;
  len = config.DefExt.size();
  foreach (QString p, s) {
    p.chop(len);
    m=p.toInt();
    if (m)
      n.append(m);
  }
  t=1;
  if (n.size()) {
    qSort(n);
    for (i=1; i<n.size()+2; i++)
      if (!n.contains(i)) {
        t=i;
        break;
      }
  }
  return config.TempPath.filePath(QString::number(t) + config.DefExt);
}

// ---------------------------------------------------------------------
void openfile(QWidget *w,QString type)
{
  QString f = dialogfileopen(w,type);
  if (f.isEmpty()) return;
  openfile1(f);
}

// ---------------------------------------------------------------------
void openfile1(QString f)
{
  term->vieweditor();
  note->fileopen(f);
  recent.filesadd(f);
}

// ---------------------------------------------------------------------
void projectclose()
{
  project.close();
  if (note) {
    note->setindex(note->editIndex());
    note->projectenable();
  }
}

// ---------------------------------------------------------------------
void projectenable()
{
  term->projectenable();
  if (note) {
    note->projectenable();
  }
}

// ---------------------------------------------------------------------
// folder tree from folder name
QStringList project_tree(QString s)
{
  QStringList r;
  r=project_tree1(cpath(s),"");
  r.sort();
  return r;
}

// ---------------------------------------------------------------------
// b is base directory, s is current subdirectory
QStringList project_tree1(QString b,QString s)
{
  QString n,p;
  QString t=b + "/" + s;
  QDirIterator d(t,QDir::Dirs|QDir::NoDotAndDotDot);
  QStringList r;
  while (d.hasNext()) {
    d.next();
    n=d.fileName();
    if (QFileInfo(t + n + "/" + n + config.ProjExt).exists())
      r.append(s + n);
    r = r + project_tree1(b,s + n + "/");
  }
  return r;
}

// ---------------------------------------------------------------------
void projectterminal()
{
  QString d;
  if (project.Id.isEmpty()) {
    if (note->editIndex()<0)
      d=config.TempPath.absolutePath();
    else
      d=cfpath(note->editFile());
  } else
    d=project.Path;
  QProcess p;
  QStringList a;
  p.startDetached(config.Terminal,a,d);
}

// ---------------------------------------------------------------------
QString rxassign(QString ext, bool ifglobal)
{
  QString r;
  if (ext==".ijs"||ext==".ijt")
    r=ifglobal ? "\\s*=[.:]" : "\\s*:";
  else if (ext==".k"||ext==".q")
    r="\\s*:";
  return r;
}

// ---------------------------------------------------------------------
void setwh(QWidget *w, QString s)
{
  QList<int>p=config.winpos_read(s);
  w->resize(qMax(p[2],300),qMax(p[3],300));
}

// ---------------------------------------------------------------------
void setxywh(QWidget *w, QString s)
{
  QList<int>p=config.winpos_read(s);
  w->move(p[0],p[1]);
  w->resize(qMax(p[2],300),qMax(p[3],300));
}

// ---------------------------------------------------------------------
// return standard output, standard error
QStringList shell(QString cmd, QString dir)
{
  QStringList r;
  QProcess p;
  p.setWorkingDirectory(dir);
  p.start(cmd);
  if (!p.waitForStarted())
    return r;
  if (!p.waitForFinished())
    return r;
  r.append((QString)p.readAllStandardOutput());
  r.append((QString)p.readAllStandardError());
  return r;
}

// ---------------------------------------------------------------------
void setnote(Note *n)
{
  if (note2==n) {
    note2=note;
    note=n;
  }
}

// ---------------------------------------------------------------------
// convert file name to folder name
// searches for the best fit
// if none then check for parent folders
QString tofoldername(QString f)
{
  int i;
  QString g,p,s,t;
  if (f.isEmpty()) return f;

  for (i=0; i<config.AllFolderValues.size(); i++) {
    t=config.AllFolderValues.at(i);
    if (matchfolder(t,f) && t.size() > s.size())
      s=t;
    else if (matchfolder(cfpath(t),f) && t.size() > p.size())
      p=t;
  }

//// prefer .folder if one level down?
  //h=f.mid(cfpath(p).size());
  //if (p.size()>0
  //&& cfpath(p).size() > s.size()
  //&& (!h.mid(1).contains('/'))) {
  //i=config.AllFolderValues.indexOf(p);
  //return "~." + config.AllFolderKeys.at(i) + h;
  //}

  if (s.size()) {
    i=config.AllFolderValues.indexOf(s);
    return '~' + config.AllFolderKeys.at(i) + f.mid(s.size());
  }

  if (p.size()) {
    i=config.AllFolderValues.indexOf(p);
    return "~." + config.AllFolderKeys.at(i) + f.mid(cfpath(p).size());
  }

  return f;
}

// ---------------------------------------------------------------------
// shortest name relative to current project if any
QString toprojectname(QString f)
{
  QString s=cpath(f);

  if (project.Id.size() && matchfolder(project.Path,s))
    s=cfsname(s);
  else {
    s=tofoldername(s);
    if ("~"==s.at(0))
      s=s.mid(1);
  }
  return s;
}

// ---------------------------------------------------------------------
void xdiff(QString s,QString t)
{
  if (config.XDiff.size()==0) {
    info ("External Diff","First define XDiff in the config");
    return;
  }
  QStringList a;
  a << s << t;
  QProcess p;
  p.startDetached(config.XDiff,a);
}