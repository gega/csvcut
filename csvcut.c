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

#include "ccsv.h" /* https://github.com/gega/ccsv */

#define BUFCHUNK (512)


static char *positions=NULL;
static size_t autostart, autostop, maxval;
static int Hflag=0; /* skip first row */

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

static int countchar(char const * str, char ch, int *len)
{
  int ret;
  for(ret=0,*len=0;*str!='\0';(*len)++,str++) if(*str==ch) ret++;
  return(ret);
}

static int csv_cut(FILE *fp, const char *fnam, char dchar)
{
  struct ccsv c;
  char *f,typ,lastchar;
  char *buf;
  int bufsiz,lnxsiz;
  char *end;
  int noq,req=0,len,col,i,lnx,noqc,lineno=0;

  bufsiz=BUFCHUNK;
  buf=malloc(bufsiz);
  end=buf;
  while(NULL!=end)
  {
    for(buf[0]='\0',req=1,lnx=0,lnxsiz=bufsiz,noq=0; NULL!=end&&noq!=req; )
    {
      req=1-req;
      for(noqc=0,lastchar='\0';lastchar!='\n';)
      {
        end=fgets(&buf[lnx],lnxsiz,fp);
        if(NULL==end) break;
        noqc+=countchar(&buf[lnx],'"',&len);
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
    
    if(Hflag&&lineno==1) continue;

    if('\0'!=buf[0])
    {
      // get line
      ccsv_init_ex(&c,buf,dchar);
      for(i=col=0;NULL!=(f=ccsv_nextfield(&c,&typ));i++)
      {
        if(positions==NULL||(autostop>0&&autostop<i)||positions[i]!=0)
        {
          printf("%s\"%s\"",(col==0?"":","),f);
          col++;
        }
      }
      printf("\n");
    }
  }
  free(buf);

  return(0);
}

static void usage(char *argv0)
{
  (void)fprintf(stderr, "usage: %s -f list [-H] [-d delim] [file ...]\n", argv0);
  exit(1);
}

/* based on cut.c https://github.com/freebsd/freebsd-src/blob/937a0055858a098027f464abf0b2b1ec5d36748f/usr.bin/cut/cut.c
 */
int main(int argc, char *argv[])
{
  FILE *fp;
  int ch, rval;
  char dchar=','; /* default delimiter is ',' */

  while ((ch = getopt(argc, argv, "d:f:Hh")) != -1)
  {
    switch(ch) 
    {
      case 'd':
        dchar = optarg[0];
        if(dchar == '\0') errx(1, "bad delimiter");
        break;
      case 'f':
        get_list(optarg);
        break;
      case 'H':
        Hflag = 1;
        break;
      case 'h':
        usage(argv[0]);
        break;
      case '?':
      default:
        usage(argv[0]);
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
  exit(rval);
}
