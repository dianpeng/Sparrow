#include "loader.h"
#include "object.h"
#include "parser.h"
#include "vm.h"
#include "util.h"

static int run_code( struct Sparrow* sparrow , const char* fpath ,
    const char* source , struct ObjMap* env , Value* ret ,
    struct CStr* err ) {
  struct ObjModule* mod; /* new modules */
  struct ObjComponent* component; /* new runtime component */
  mod = Parse(sparrow,fpath,source,err);
  if(!mod) {
    return -1;
  }
  /* You have to use NOGC version here since env and component
   * is not referenced by anybody so if a GC triggered inside of
   * these new functions , then we will end up with invalid env
   * and component pointer */
  if(!env) env = ObjNewMapNoGC( sparrow , 8 );
  component = ObjNewComponentNoGC(sparrow,mod,env);
  return Execute(sparrow,component,ret,err);
}

int RunFile( struct Sparrow* sparrow , const char* fpath , struct ObjMap* env ,
    Value* ret , struct CStr* err ) {
  return run_code(sparrow,fpath,NULL,env,ret,err);
}

int RunString( struct Sparrow* sparrow, const char* src , struct ObjMap* env ,
    Value* ret , struct CStr* err ) {
  return run_code(sparrow,NULL,src,env,ret,err);
}
