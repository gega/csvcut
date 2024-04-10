#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
  char *p;
  int lcp;
  
  /* col,prcol,fname,field */
  
  if(argc>=5)
  {
    lcp=0;
    if(argc>5) lcp=atoi(&argv[5][strlen(argv[5])-1])%2;
    if(lcp==0) for(p=&argv[4][0];*p;p++) *p=toupper(*p);
    else       for(p=&argv[4][0];*p;p++) *p=tolower(*p);
    printf("%s",argv[4]);
  }
  
  return(0);
}
