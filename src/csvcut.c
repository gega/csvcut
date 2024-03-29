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
#include "config.h"

#include "ccsv.h" /* https://github.com/gega/ccsv */

#define BUFCHUNK (512)
#define FLDBUFSIZ (256)
#define TAG "csvcut"
#ifndef VERSION_NUMBER
#error Missing VERSION_NUMBER macro
#endif


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
static enum outtype otype=OT_CSV;
static char Dchar[]=","; /* default output delimiter is ',' */
static char **cb=NULL; /* callout command names indexed for fields */
static int cbsize=0; /* number of callout commands -- should be >= # fields or 0 */


static void version(void)
{
  fprintf(stderr,"%s %s\n",TAG,VERSION_NUMBER);
  fprintf(stderr,"Copyright (C) 2024 Gergely Gati\nBSD 3-Clause License\n");
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
      start = stop = strtol(p, &p, 10);
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
  static const char esc[]={'\n','\t','\r','\"','\0'};
  static const char rpl[]={ 'n', 't', 'r', '"','\0'};
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
    buf=malloc(buflen+1);
    buf[0]='\0';
  }
  for(b=buf+1;*str;str++)
  {
    e=strchr(esc,*str);
    if(e==NULL) *b++=*str;
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

static void print_field_csv(char * const field, int col, int prcol, char * const fname)
{
  printf("%s\"%s\"",(prcol==0?"":Dchar),field);
}

static void print_field_json(char * const field, int col, int prcol, char * const fname)
{
  printf("%s\"%s\": \"%s\"",(prcol==0?"{ ":", "),fname,escape(field));
}

static void print_field_xml(char * const field, int col, int prcol, char * const fname)
{
  if(prcol==0) printf("<row>");
  printf("<%s>%s</%s>",fname,field,fname);
}

static char *check_callout(char * const field, int col, int prcol, char * const fname)
{
  static char buf[FLDBUFSIZ];
  static char *local_field=NULL;
  static int local_field_size=0;
  char *ret=field;
  char *cmd=buf;
  int cmdsiz=sizeof(buf);
  int ln,l;
  FILE *p;
  
  if(NULL==field)
  {
    if(NULL!=local_field) free(local_field);
    local_field=NULL;
    local_field_size=0;
    return(ret);
  }
  if(cb==NULL) return(ret);
  if(local_field==NULL)
  {
    local_field_size=FLDBUFSIZ;
    local_field=malloc(local_field_size);
    local_field[0]='\0';
  }
  if(cbsize>col&&NULL!=cb[col])
  {
    cmdsiz=1+snprintf(cmd,cmdsiz,"%s %d %d \"%s\" \"%s\"",cb[col],col,prcol,fname,field);
    if(cmdsiz>sizeof(buf))
    {
      cmd=malloc(cmdsiz);
      snprintf(cmd,cmdsiz,"%s %d %d \"%s\" \"%s\"",cb[col],col,prcol,fname,field);
      
    }
    if((p=popen(cmd,"r")))
    {
      for(ln=0,l=FLDBUFSIZ;l==FLDBUFSIZ;ln+=l)
      {
        l=fread(&local_field[ln],1,FLDBUFSIZ,p);
        if(l==FLDBUFSIZ) local_field=realloc(local_field,ln+FLDBUFSIZ);
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
    if(cmd!=buf&&cmd!=NULL) free(cmd);
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
  void (*prfld)(char * const, int, int, char * const);

  bufsiz=BUFCHUNK;
  buf=malloc(bufsiz);
  end=buf;
  countq=countquotes_fld;
  prfld=print_field_csv;
  if(OT_JSON==otype)
  {
    prfld=print_field_json;
    printf("[\n");
  }
  else if(OT_XML==otype)
  {
    prfld=print_field_xml;
    printf("<xml>\n");
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
        }
      }
      noq=noqc%2;
    }
    lineno++;
    countq=countquotes;
    if(lineno==1) fields=calloc(fldnum,sizeof(char *));

    if('\0'!=buf[0])
    {
      if(lineno>2)
      {
        if(OT_JSON==otype) printf(",\n");
        else if(OT_XML==otype) printf("</row>\n");
      }
      // get line
      ccsv_init_ex(&c,buf,dchar);
      for(i=col=0;NULL!=(f=ccsv_nextfield(&c,&typ));i++)
      {
        if(lineno==1)
        {
          fields[i]=strdup(f);
          if(OT_XML==otype) xmltagsanitize(fields[i]);
        }
        if(Hflag&&lineno==1) continue;
        if(positions==NULL||(autostop>1&&autostop<(i+1))||(maxval>i&&positions[i+1]!=0))
        {
          f=check_callout(f,i,col,fields[i]);
          if(NULL!=f) prfld(f,i,col,fields[i]);
          col++;
        }
      }
      if(OT_JSON==otype&&lineno>1) printf("}");
      if(!((Hflag&&lineno==1)||OT_JSON==otype)) printf("\n");
    }
  }
  if(OT_JSON==otype) printf("\n]\n");
  else if(OT_XML==otype) printf("</row></xml>\n");
  if(NULL!=fields)
  {
    for(i=0;i<fldnum;i++) if(NULL!=fields[i]) free(fields[i]);
    free(fields);
    fields=NULL;
  }
  free(buf);
  escape(NULL);
  if(NULL!=positions) free(positions);
  positions=NULL;
  check_callout(NULL,0,0,NULL);

  return(0);
}

static void usage(char *argv0, int st)
{
  (void)fprintf(stderr, "usage: %s [-f list] [-H] [-o csv|json|xml] [-d delim] [-D output-delim] [-c field:cmd] [file ...]\n", argv0);
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

static char *parse_range(char *s, char d, int *min, int *max)
{
  char *ret=NULL;
  
  if(NULL==s||NULL==min||NULL==max||'\0'==d||strlen(s)<3) return(NULL);
  /* TODO: parse actual range instead of just one item */
  *min=*max=atoi(s);
  if(0>=*min||0>=*max) return(NULL);
  ret=strchr(s,d);
  if(NULL!=ret) ret++;
  return(ret);
}

static void setup_callout(char *arg)
{
  // "2-3:./procfield23.sh"
  char *cmd;
  int min,max,i;
  
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
  cmd=parse_range(arg,':',&min,&max);
  if(NULL==cmd) errx(1, "wrong field range");
  if(cbsize<max+1)
  {
    if(cb==NULL)
    {
      cb=calloc(max+1,sizeof(char *));
      cbsize=max+1;
    }
    else
    {
      cb=realloc(cb,sizeof(char *)*(max+1));
      for(i=cbsize;i<max+1;i++) cb[i]=NULL;
      cbsize=max+1;
    }
  }
  for(i=min;i<max+1;i++) cb[i]=strdup(cmd);
}

/* based on cut.c https://github.com/freebsd/freebsd-src/blob/937a0055858a098027f464abf0b2b1ec5d36748f/usr.bin/cut/cut.c
 */
int main(int argc, char *argv[])
{
  FILE *fp;
  int ch, rval;
  char dchar=','; /* default delimiter is ',' */

  while ((ch = getopt(argc, argv, "d:f:Hho:D:c:v")) != -1)
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
        get_list(optarg);
        break;
      case 'H':
        Hflag = 1;
        break;
      case 'o':
        get_type(optarg);
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
  exit(rval);
}
