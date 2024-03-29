#include <stdio.h>
#include <ctype.h>

int main(int argc, char **argv)
{
  char *p;
  
  /* col,prcol,fname,field */
  
  if(argc==5)
  {
    for(p=&argv[4][0];*p;p++) *p=toupper(*p);
    printf("%s",argv[4]);
  }
  
  return(0);
}
