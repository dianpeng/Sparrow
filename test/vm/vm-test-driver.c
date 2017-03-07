#include <vm/object.h>
#include <vm/vm.h>
#include <vm/parser.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>

/* This is a simple test driver that reads all the sp file in a folder
 * and then perform execution of them one by one ! It is used for testing
 * purpose */
static int run_code( struct Sparrow* sparrow , const char* path ) {
  struct ObjModule* mod;
  struct ObjMap* env;
  struct ObjComponent* component;
  struct CStr err;
  Value ret;
  int status;
  mod = Parse(sparrow,path,NULL,&err);
  if(!mod) {
    fprintf(stderr,"Cannot parse file %s due to reason %s!\n",path,err.str);
    CStrDestroy(&err);
    return -1;
  }
  env = ObjNewMap(sparrow,16);
  component = ObjNewComponent( sparrow , mod , env );
  status = Execute(sparrow,component,&ret,&err);
  (void)ret; /* ignore the result */
  if(status) {
    ObjDumpModule(mod,stdout,path);
    fprintf(stderr,"Execution failed on file %s due to reason %s!\n",
        path,err.str);
    CStrDestroy(&err);
    return -1;
  } else {
    return 0;
  }
}

int main( int argc , char* argv[] ) {
  if( argc == 1 ) {
    DIR* d;
    struct dirent* dir;
    struct Sparrow sparrow;
    int cnt = 0;
    SparrowInit(&sparrow);
    d = opendir("sparrow-test/");
    if(d) {
      while((dir = readdir(d)) != NULL) {
        char fn[1024];
        if(dir->d_type == DT_REG && dir->d_name[0] != '.') {
          sprintf(fn,"sparrow-test/%s",dir->d_name);
          if(run_code(&sparrow,fn)) abort();
          ++cnt;
        }
      }
    } else {
      fprintf(stderr,"Cannot open folder sparrow-test/!\n");
    }
    closedir(d);
    SparrowDestroy(&sparrow);
    printf("%d tests performed!\n",cnt);
    return 0;
  } else {
    if(argc != 2) {
      fprintf(stderr,"specify file path to run, one argument required!\n");
      return -1;
    } else {
      struct Sparrow sparrow;
      SparrowInit(&sparrow);
      if(run_code(&sparrow,argv[1])) abort();
      SparrowInit(&sparrow);
      printf("finish running %s!\n",argv[1]);
      return 0;
    }
  }
}
