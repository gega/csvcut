#ifndef CCSV_H
#define CCSV_H

/* https://github.com/gega/ccsv */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

struct ccsv
{
  char *line;
  char delim;
};

/* parse one line of a csv according to rfc4180 
 */

/* init ccsv struct with one line of csv and delimiter 
 */
#define ccsv_init_ex(_csv, _ln, _dlm) \
do { \
  if(NULL!=(_csv)) { ((struct ccsv *)(_csv))->line=(_ln); ((struct ccsv *)(_csv))->delim=_dlm;  } \
} while(0)

/* init ccsv struct with one line of csv and ',' as default delimiter 
 */
#define ccsv_init(_csv, _ln) \
do { \
  if(NULL!=(_csv)) { ((struct ccsv *)(_csv))->line=(_ln); ((struct ccsv *)(_csv))->delim=',';  } \
} while(0)


/* returns the next \0 terminated field or NULL if end of line or error
   ccsv - previously initialized ccsv structure
   type - output: '"' if the field was quoted
 */
char *ccsv_nextfield(struct ccsv *ccsv, char *type)
{
  char *ret=(ccsv)->line;
  char *base=(ccsv)->line;

  if(NULL!=ret)
  {
    char sep=(ccsv)->delim;
    while(isspace(*ret)) ++ret;
    if(*ret=='"') { sep='"'; ret++; }
    if(NULL!=type) *type=sep;
    if(NULL!=((ccsv)->line=strchr(ret,sep)))
    {
      while(NULL!=(ccsv)->line&&sep=='"'&&*((ccsv)->line)=='"'&&*((ccsv)->line+1)=='"') (ccsv)->line=strchr((ccsv)->line+2,sep);
      if(NULL!=(ccsv)->line)
      {
        char *p=((ccsv)->line++)-1;
        if(sep=='"'&&NULL!=((ccsv)->line=strchr(p,(ccsv)->delim))) ++(ccsv)->line;
        if(sep!='"') while(p>base&&isspace(*p)) --p;
        *++p='\0';
      }
    }
    if(NULL==(ccsv)->line)
    {
      char *p=&ret[strlen(ret)-1];
      if(sep!='"') while(isspace(*p)) --p;
      *++p='\0';
    }
  }
  return(ret);
}

#endif
