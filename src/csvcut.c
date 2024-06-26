/*
  BSD 3-Clause License

  Copyright (c) 2024, Gergely Gati

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice, this
     list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.

  3. Neither the name of the copyright holder nor the names of its
     contributors may be used to endorse or promote products derived from
     this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <err.h>
#include <ctype.h>
#include <limits.h>

#include "config.h"

#include "ccsv.h" /* https://github.com/gega/ccsv */

#define BUFCHUNK (512)
#define FLDBUFSIZ (256)
#define TAG "csvcut"
#ifndef VERSION_NUMBER
#error Missing VERSION_NUMBER macro
#endif
#define INF (INT_MAX)
#define COMBINE_UNION  (INT_MIN)
#define COMBINE_LONGER (INT_MIN+1)
#define COMBINES_MAX   (COMBINE_LONGER)


enum outtype
{
  OT_CSV,
  OT_JSON,
  OT_XML,
  OTNUM
};

struct otypes
{
  char *name;
  enum outtype type;
  int is_header;
};

static struct otypes otc[]=
{
  { "csv",  OT_CSV, 	1 },
  { "json", OT_JSON, 	0 },
  { "xml",  OT_XML, 	0 },
  {NULL}
};

static char *positions=NULL;
static size_t autostart, autostop, maxval;
static int Hflag=0; /* skip first row */
static int sflag=0; /* skip processing the first row */
static int qflag=0; /* do not quote fields */
static enum outtype otype=OT_CSV;
static char Dchar[]=","; /* default output delimiter is ',' */
static char **cb=NULL; /* callout command names indexed for fields */
static int cbsize=0; /* number of callout commands -- should be >= # fields or 0 */
static int cb_pass_fld_max=0;
static char *cb_pass_fld=NULL;
static int *reorder_fields=NULL;


static void version(void)
{
  fprintf(stderr,"%s %s\n",TAG,VERSION_NUMBER);
  fprintf(stderr,"Copyright (C) 2024 Gergely Gati\nBSD 3-Clause License\nBuild timestamp: " __DATE__ " " __TIME__ "\n");
  exit(0);
}

/* from cut.c https://github.com/freebsd/freebsd-src/blob/937a0055858a098027f464abf0b2b1ec5d36748f/usr.bin/cut/cut.c
 */
static void needpos(size_t n)
{
  static size_t npos;
  size_t oldnpos;

  /* Grow the positions array to at least the specified size. */
  if (n > npos) {
    oldnpos = npos;
    if (npos == 0)
      npos = n;
    while (n > npos)
      npos *= 2;
    if ((positions = realloc(positions, npos)) == NULL)
      err(1, "realloc");
    memset((char *)positions + oldnpos, 0, npos - oldnpos);
  }
}

/* from cut.c https://github.com/freebsd/freebsd-src/blob/937a0055858a098027f464abf0b2b1ec5d36748f/usr.bin/cut/cut.c
 */
static void get_list(char *list)
{
  size_t setautostart, start, stop;
  char *pos;
  char *p;

  /*
   * set a byte in the positions array to indicate if a field or
   * column is to be selected; use +1, it's 1-based, not 0-based.
   * Numbers and number ranges may be overlapping, repeated, and in
   * any order. We handle "-3-5" although there's no real reason to.
   */
  for (; (p = strsep(&list, ", \t")) != NULL;) {
    setautostart = start = stop = 0;
    if (*p == '-') {
      ++p;
      setautostart = 1;
    }
    if (isdigit((unsigned char)*p)) {
      start = stop = (size_t)strtol(p, &p, 10);
      if (setautostart && start > autostart)
        autostart = start;
    }
    if (*p == '-') {
      if (isdigit((unsigned char)p[1]))
        stop = strtol(p + 1, &p, 10);
      if (*p == '-') {
        ++p;
        if (!autostop || autostop > stop)
          autostop = stop;
      }
    }
    if (*p)
      errx(1, "[-bcf] list: illegal list value");
    if (!stop || !start)
      errx(1, "[-bcf] list: values may not include zero");
    if (maxval < stop) {
      maxval = stop;
      needpos(maxval + 1);
    }
    for (pos = positions + start; start++ <= stop; *pos++ = 1);
  }

  /* overlapping ranges */
  if (autostop && maxval > autostop) {
    maxval = autostop;
    needpos(maxval + 1);
  }

  /* reversed range with autostart */
  if (maxval < autostart) {
    maxval = autostart;
    needpos(maxval + 1);
  }

  /* set autostart */
  if (autostart)
    memset(positions + 1, '1', autostart);
}

static int countquotes(char const * str, int *len, char, int *)
{
  int ret;
  for(ret=0,*len=0;*str!='\0';(*len)++,str++) if(*str=='"') ret++;
  return(ret);
}

static int countquotes_fld(char const * str, int *len, char dchar, int *fields)
{
  int ret;
  for(ret=0,*len=0;*str!='\0';(*len)++,str++) 
  {
    if(*str==dchar&&(ret%2)==0) (*fields)++;
    if(*str=='"') ret++;
  }
  return(ret);
}

static char *escape(char *str)
{
  static const char esc[]={'\n','\t','\r','"','\0'};
  static const char rpl[]={ 'n', 't', 'r','"','\0'};
  static char *buf=NULL;
  static int buflen=0;
  int len;
  char *b,*e;
  
  if(NULL==str)
  {
    if(NULL!=buf) free(buf);
    buf=NULL;
    buflen=0;
    return(NULL);
  }
  len=strlen(str);
  if(buflen<len*2)
  {
    buflen=len*2;
    if(NULL!=buf) free(buf);
    if(NULL==(buf=malloc(buflen+1))) err(1, "malloc");
    buf[0]='\0';
  }
  for(b=buf+1;*str;str++)
  {
    e=strchr(esc,*str);
    if(e==NULL)
    {
      *b++=*str;
    }
    else
    {
      if(*(b-1)!='"')
      {
        *b++='\\';
        *b++=rpl[e-esc];
      }
    }
  }
  *b='\0';

  return(buf+1);
}

static void xmltagsanitize(char *field)
{
  if(!isalpha(*field)) *field='_';
  for(;*field;field++)
  {
    if(!isalnum(*field)) *field='_';
  }
}

static void print_field_csv(char * const field, int col, int prcol, char const * fname)
{
  printf("%s%s%s%s",(prcol==0?"":Dchar),(qflag?"":"\""),(NULL==field?"":field),(qflag?"":"\""));
}

static void print_field_json(char * const field, int col, int prcol, char const * fname)
{
  printf("%s\"%s\":\"%s\"",(prcol==0?"{":","),fname,(NULL==field?"":escape(field)));
}

static void print_field_xml(char * const field, int col, int prcol, char const * fname)
{
  char str[32];
  if(prcol==0) printf("<row>");
  if(fname==NULL||strlen(fname)==0)
  {
    snprintf(str,sizeof(str),"column_%d",prcol);
    fname=str;
  }
  printf("<%s>%s</%s>",fname,(NULL==field?"":field),fname);
}

static char *check_callout(char * const field, int col, int prcol, char * const fname, char **values, int valuescnt)
{
  static char buf[FLDBUFSIZ];
  static char *local_field=NULL;
  static int local_field_size=0;
  static char *cmd=buf;
  static int cmdsiz=sizeof(buf);
  char *ret=field;
  int ln,l,cs,i;
  FILE *p;
  
  if(NULL==field)
  {
    if(NULL!=local_field) free(local_field);
    local_field=NULL;
    local_field_size=0;
    if(cmd!=buf) free(cmd);
    cmd=buf;
    cmdsiz=sizeof(buf);
    return(ret);
  }
  if(cb==NULL) return(ret);
  if(local_field==NULL)
  {
    local_field_size=FLDBUFSIZ;
    if(NULL==(local_field=malloc(local_field_size))) err(1, "malloc");
    local_field[0]='\0';
  }
  if(cbsize>col&&NULL!=cb[col])
  {
    cs=1+snprintf(cmd,cmdsiz,"%s %d %d \"%s\" \"%s\"",cb[col],col,prcol,fname,field);
    for(i=0;i<cb_pass_fld_max;i++)
    {
      if(cb_pass_fld[i]!=0&&values[i]!=NULL) cs+=3+strlen(values[i]);
    }
    if(cs>cmdsiz)
    {
      cs*=2;
      if(cmd!=buf) free(cmd);
      if(NULL==(cmd=malloc(cs))) err(1, "malloc");
      cmdsiz=cs;
      snprintf(cmd,cmdsiz,"%s %d %d \"%s\" \"%s\"",cb[col],col,prcol,fname,field);
    }
    for(i=0;i<cb_pass_fld_max;i++)
    {
      if(cb_pass_fld[i]!=0&&values[i]!=NULL)
      {
        strcat(cmd," \"");
        strcat(cmd,values[i]);
        strcat(cmd,"\"");
      }
    }
    if((p=popen(cmd,"r")))
    {
      for(ln=0,l=FLDBUFSIZ;l==FLDBUFSIZ;ln+=l)
      {
        l=fread(&local_field[ln],1,FLDBUFSIZ,p);
        if(l==FLDBUFSIZ) if(NULL==(local_field=realloc(local_field,ln+FLDBUFSIZ))) err(1, "realloc");
      }
      if(0!=(pclose(p)))
      {
        fprintf(stderr,"Failed to call '%s'\n",cb[col]);
        free(cb[col]);
        cb[col]=NULL;
      }
      else if(ln>0)
      {
        local_field[ln]='\0';
        ret=local_field;
      }
    }
    else
    {
      fprintf(stderr,"Cannot open '%s'\n",cb[col]);
      free(cb[col]);
      cb[col]=NULL;
    }
  }
  
  return(ret);
}

static int csv_cut(FILE *fp, const char *fnam, char dchar)
{
  struct ccsv c;
  char *f,typ='\0',lastchar;
  char *buf;
  int bufsiz,lnxsiz;
  char *end;
  int noq,req=0,len,col,i,lnx,noqc,lineno=0;
  int (*countq)(char const *,int *,char,int *);
  int fldnum=1;
  char **fields=NULL;
  char **values=NULL;
  char **procval=NULL;
  char **out_fields=NULL;
  int *cmbn_flds=NULL;
  int cmbn_type;
  void (*prfld)(char * const, int, int, char const *);

  bufsiz=BUFCHUNK;
  if(NULL==(buf=malloc(bufsiz))) err(1, "malloc");
  end=buf;
  countq=countquotes_fld;
  prfld=print_field_csv;
  if(OT_JSON==otype)
  {
    prfld=print_field_json;
    printf("[");
  }
  else if(OT_XML==otype)
  {
    prfld=print_field_xml;
    printf("<xml>");
  }
  while(NULL!=end)
  {
    for(buf[0]='\0',req=1,lnx=0,lnxsiz=bufsiz,noq=0; NULL!=end&&noq!=req; )
    {
      req=1-req;
      for(noqc=0,lastchar='\0';lastchar!='\n';)
      {
        end=fgets(&buf[lnx],lnxsiz,fp);
        if(NULL==end) break;
        noqc+=countq(&buf[lnx],&len,dchar,&fldnum);
        lnx+=len;
        lnxsiz-=len;
        lastchar=buf[lnx-1];
        if('\n'!=lastchar)
        {
          bufsiz+=BUFCHUNK;
          lnxsiz+=BUFCHUNK;
          buf=realloc(buf,bufsiz);
          if(NULL==buf) err(1, "realloc");
        }
      }
      noq=noqc%2;
    }
    lineno++;
    countq=countquotes;
    if(lineno==1)
    {
      fields=calloc(fldnum,sizeof(char *));
      values=calloc(fldnum,sizeof(char *));
      procval=calloc(fldnum,sizeof(char *));
      cmbn_flds=calloc(fldnum*2,sizeof(int));
    }

    if('\0'!=buf[0])
    {
      if(lineno>2)
      {
        if(OT_JSON==otype) printf(",");
        else if(OT_XML==otype) printf("</row>");
      }
      // get line
      ccsv_init_ex(&c,buf,dchar);
      for(i=col=0;NULL!=(f=ccsv_nextfield(&c,&typ));i++)
      {
        if(i>=fldnum) continue;
        values[i]=NULL;
        procval[i]=NULL;
        if(1==lineno)
        {
          fields[i]=strdup(f);
          if(NULL==fields[i]) err(1, "strdup");
          if(OT_XML==otype) xmltagsanitize(fields[i]);
        }
        if(1==lineno&&Hflag) continue;
        if(NULL==positions||(autostop>1&&autostop<(i+1))||(maxval>i&&positions[i+1]!=0))
        {
          if(NULL!=f) values[i]=strdup(f);
          col++;
        }
      }
      if(1==lineno&&NULL!=reorder_fields&&OT_JSON==otype)
      {
        char str[32];
        for(col=0;0!=reorder_fields[col];col++);
        out_fields=calloc(col+1,sizeof(char *));
        for(i=0;0!=reorder_fields[i];i++)
        {
          snprintf(str,sizeof(str),"%d",i+1);
          out_fields[i]=strdup(str);
        }
      }
      for(i=col=0;i<fldnum;i++)
      {
        if(NULL!=values[i])
        {
          if(lineno>1||!sflag)
          {
            if(NULL!=(f=check_callout(values[i],i,col,fields[i],values,fldnum)))
            {
              procval[i]=values[i];
              values[i]=strdup(f);
            }
          }
          if(NULL==reorder_fields) prfld(values[i],i,col,fields[i]);
          col++;
        }
      }
      if(NULL!=reorder_fields&&(lineno>1||!Hflag))
      {
        int j,cmbn;
        for(i=0,col=cmbn=0;0!=reorder_fields[i];i++,col++)
        {
          if(reorder_fields[i]==INF) prfld("",0,col,(OT_JSON==otype?out_fields[col]:""));
          if(reorder_fields[i]<=COMBINES_MAX)
          {
            if(cmbn<((fldnum*2)-1))
            {
              cmbn_flds[cmbn++]=reorder_fields[i+1]-1;
              cmbn_type=reorder_fields[i];
            }
            else err(1, "combine");
            ++i;
            --col;
            continue;
          }
          if(abs(reorder_fields[i])>fldnum) continue;
          if(0==cmbn)
          {
            if(reorder_fields[i]>0) prfld(values[reorder_fields[i]-1],reorder_fields[i]-1,col,(OT_JSON==otype?out_fields[col]:fields[reorder_fields[i]-1]));
            else for(j=-reorder_fields[i];j<=fldnum;j++,col++) prfld(values[j-1],j-1,col,(OT_JSON==otype?out_fields[col]:fields[j-1]));
          }
          else
          {
            cmbn_flds[cmbn++]=reorder_fields[i]-1;
            if(COMBINE_LONGER==cmbn_type)
            {
              int maxj=cmbn_flds[0],maxl=-1,l;
              for(j=0;j<cmbn;j++)
              {
                l=strlen(values[cmbn_flds[j]]);
                if(l>maxl)
                { 
                  maxj=cmbn_flds[j];
                  maxl=l;
                }
              }
              prfld(values[maxj],maxj,col,(OT_JSON==otype?out_fields[col]:fields[maxj]));
            }
            else if(COMBINE_UNION==cmbn_type)
            {
              int len=0;
              char *cmb;
              for(j=0;j<cmbn;j++) len+=strlen(values[cmbn_flds[j]]);
              cmb=malloc(len+1);
              if(NULL==cmb) err(1, "malloc");
              cmb[0]='\0';
              for(j=0;j<cmbn;j++) strcat(cmb,values[cmbn_flds[j]]);
              prfld(cmb,cmbn_flds[0],col,(OT_JSON==otype?out_fields[col]:fields[cmbn_flds[0]]));
              free(cmb);
            }
            else errx(1, "cmbn_type=%d cmbn=%d",cmbn_type,cmbn);
            cmbn=0;
          }
        }
      }
      for(i=0;i<fldnum;i++)
      {
        if(NULL!=values[i]) free(values[i]);
        if(NULL!=procval[i]) free(procval[i]);
        values[i]=NULL;
        procval[i]=NULL;
      }
      if(OT_JSON==otype&&lineno>1) printf("}");
      if(!((Hflag&&lineno==1)||OT_JSON==otype)) printf("\n");
    }
  }
  if(OT_JSON==otype) printf("]");
  else if(OT_XML==otype) printf("</row></xml>");
  if(NULL!=values)
  {
    free(values);
    values=NULL;
  }
  if(NULL!=procval)
  {
    free(procval);
    procval=NULL;
  }
  if(NULL!=out_fields)
  {
    for(i=0;NULL!=out_fields[i];i++) free(out_fields[i]);
    free(out_fields);
  }
  if(NULL!=fields)
  {
    for(i=0;i<fldnum;i++) if(NULL!=fields[i]) free(fields[i]);
    free(fields);
    fields=NULL;
  }
  if(NULL!=cmbn_flds)
  {
    free(cmbn_flds);
    cmbn_flds=NULL;
  }
  free(buf);
  escape(NULL);
  if(NULL!=positions) free(positions);
  positions=NULL;
  check_callout(NULL,0,0,NULL,NULL,0);

  return(0);
}

static void usage(char *argv0, int st)
{
  (void)fprintf(stderr, "usage: %s [-f list|-r list] [-H] [-s] [-q] [-o csv|json|xml] [-d delim] [-D output-delim] [-c field/args:cmd] [file ...]\n", argv0);
  exit(st);
}

static void get_type(char *type)
{
  int i;
  
  for(i=0;otc[i].name!=NULL;i++)
  {
    if(strncmp(otc[i].name,type,strlen(otc[i].name))==0)
    {
      otype=otc[i].type;
      if(!otc[i].is_header) Hflag=1;
      break;
    }
  }
  if(NULL==otc[i].name) errx(1, "Invalid type");
}

// parse one range which ends with one of the chars in 'd'
// "+"      new field
// "1-4"    exact range
// "-4"     range from first (negative value)
// "4-"     range to fldnum
// "4+6"    combine multiple fields (parse only the "4+" part)
// "4^6"    select the longest field (parse until the operator)
static char *parse_range(char *s, char *d, int *min, int *max)
{
  char *ret=NULL;

  if(NULL==s||NULL==min||NULL==max||NULL==d||strlen(s)<1) return(NULL);
  *min=*max=(int)strtol(s,&ret,10);
  if(*min<0)
  {
    // negative number means range from 1 to N
    *max=-*min;
    *min=1;
  }
  else
  {
    if('-'==*ret)
    {
      s=++ret;
      if(strchr(d,*s)==NULL) *max=(int)strtol(s,&ret,10);
      else *max=INF;
    }
    if(*min>*max)
    {
      int t=*min;
      *min=*max;
      *max=t;
    }
  }
  if(0>*min||0>*max||(*ret!='\0'&&NULL==strchr(d,*ret))) return(NULL);

  return(ret);
}

static void setup_callout(char *arg)
{
  // "2-3/1-2,4:./procfield23.sh"
  char *cmd;
  int min,max,i,rmn,rmx;
  char *delim;
  
  if(NULL==arg)
  {
    if(NULL!=cb)
    {
      for(i=0;i<cbsize;i++) if(NULL!=cb[i]) free(cb[i]);
      free(cb);
      cb=NULL;
      cbsize=0;
    }
    return;
  }
  cmd=parse_range(arg,":/",&min,&max);
  --min;
  if(NULL==cmd) errx(1, "wrong field range %d",__LINE__);
  while(*cmd=='/'||*cmd==',')
  {
    cmd=parse_range(cmd+1,":,",&rmn,&rmx);
    if(NULL==cmd) errx(1, "wrong field range %d",__LINE__);
    --rmn;
    if(rmx>cb_pass_fld_max)
    {
      cb_pass_fld=realloc(cb_pass_fld,rmx);
      memset(&cb_pass_fld[cb_pass_fld_max],0,rmx-cb_pass_fld_max);
      cb_pass_fld_max=rmx;
    }
    for(i=rmn;i<rmx;i++) cb_pass_fld[i]=1;
  }
  if(NULL==cmd) errx(1, "wrong field range %d",__LINE__);
  cmd++;
  if(cbsize<max)
  {
    if(cb==NULL)
    {
      cb=calloc(max,sizeof(char *));
      cbsize=max;
    }
    else
    {
      cb=realloc(cb,sizeof(char *)*max);
      if(NULL==cb) err(1, "realloc");
      for(i=cbsize;i<max;i++) cb[i]=NULL;
      cbsize=max;
    }
  }
  for(i=min;i<max;i++) if(NULL==(cb[i]=strdup(cmd))) err(1, "malloc");
}

static int *parse_rangeset(char *arg)
{
  int neg;
  char *cmd=arg;
  int i,rmn,rmx,n,cat=0,val;
  int rs_cnt=0;
  int *ret=NULL;
  
  if(NULL!=arg)
  {
    --cmd;
    do
    {
      neg=1;
      cmd=parse_range(cmd+1,(cat==0?",+^*":",^*"),&rmn,&rmx);
      if(NULL==cmd) errx(1, "wrong field range %d",__LINE__);
      if(rmx==INF)
      {
        rmx=rmn;
        neg=-1;
      }
      --rmn;
      n=rmx-rmn;
      if('+'==*cmd)
      {
        ++cmd;
        rmn=INF-1;
        cat=0;
      }
      else if('^'==*cmd)
      {
        if(n!=1||(0!=cat&&COMBINE_LONGER!=cat)) errx(1, "wrong combine field %d",__LINE__);
        cat=COMBINE_LONGER;
        n=2;
      }
      else if('*'==*cmd)
      {
        if(n!=1||(0!=cat&&COMBINE_UNION!=cat)) errx(1, "wrong union field %d n=%d cat=%d",__LINE__,n,cat);
        cat=COMBINE_UNION;
        n=2;
      }
      else cat=0;
      ret=realloc(ret,(rs_cnt+n+1)*sizeof(int));
      memset(&ret[rs_cnt],0,(n+1)*sizeof(int));
      val=cat;
      for(i=rs_cnt;i<rs_cnt+n;i++,rmn++,val=rmn) ret[i]=(cat==0?(rmn+1)*neg:val);
      rs_cnt+=n;
    }
    while(*cmd!='\0'&&strchr(",*^",*cmd)!=NULL);
  }
  
  return(ret);
}

/* based on cut.c https://github.com/freebsd/freebsd-src/blob/937a0055858a098027f464abf0b2b1ec5d36748f/usr.bin/cut/cut.c
 */
int main(int argc, char *argv[])
{
  FILE *fp;
  int ch, rval;
  char dchar=','; /* default delimiter is ',' */

  while ((ch = getopt(argc, argv, "d:f:Hho:D:c:vsqr:")) != -1)
  {
    switch(ch) 
    {
      case 'c':
        setup_callout(optarg);
        break;
      case 'd':
        dchar = optarg[0];
        if(dchar == '\0') errx(1, "bad delimiter");
        break;
      case 'D':
        Dchar[0] = optarg[0];
        if(Dchar[0] == '\0') errx(1, "bad delimiter");
        break;
      case 'f':
        if(NULL!=reorder_fields) errx(1, "cannot use -f if -r is used");
        get_list(optarg);
        break;
      case 'r':
        if(NULL!=positions) errx(1, "cannot use -r if -f is used");
        reorder_fields=parse_rangeset(optarg);
        break;
      case 'H':
        Hflag = 1;
        break;
      case 's':
        sflag = 1;
        break;
      case 'o':
        get_type(optarg);
        break;
      case 'q':
        qflag = 1;
        break;
      case 'h':
        usage(argv[0],0);
        break;
      case 'v':
        version();
        break;
      case '?':
      default:
        usage(argv[0],1);
    }
  }
  argc -= optind;
  argv += optind;

  rval = 0;
  if (*argv)
    for (; *argv; ++argv) {
      if (strcmp(*argv, "-") == 0)
        rval |= csv_cut(stdin, "stdin", dchar);
      else {
        if (!(fp = fopen(*argv, "r"))) {
          warn("%s", *argv);
          rval = 1;
          continue;
        }
        csv_cut(fp, *argv, dchar);
        (void)fclose(fp);
      }
    }
  else rval = csv_cut(stdin, "stdin", dchar);
  setup_callout(NULL);
  if(NULL!=cb_pass_fld) free(cb_pass_fld);
  if(NULL!=reorder_fields) free(reorder_fields);
  exit(rval);
}
