#include "../src/vm/object.h"
#include "../src/vm/vm.h"
#include "../src/vm/parser.h"
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>

static int show_bc( const char* path ) {
  struct ObjModule* mod;
  struct CStr err;
  struct Sparrow sparrow;
  SparrowInit(&sparrow);
  mod = Parse(&sparrow,path,NULL,&err);
  if(!mod) {
    fprintf(stderr,"Cannot parse file %s due to reason %s!\n",path,err.str);
    CStrDestroy(&err);
    return -1;
  }
  ObjDumpModule(mod,stdout,path);
  return 0;
}

int main(int argc , char* argv[]) {
  if(argc == 2) {
    return show_bc(argv[1]);
  } else {
    fprintf(stderr,"Usage:: --file!\n");
    return -1;
  }
}
