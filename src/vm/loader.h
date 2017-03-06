#ifndef LOADER_H_
#define LOADER_H_
#include <vm/object.h>

int RunString( struct Sparrow* , const char* source , struct ObjMap* env,
    Value* ret , struct CStr* err );

int RunFile( struct Sparrow* , const char* filepath , struct ObjMap* env,
    Value* ret , struct CStr* err );

#endif /* LOADER_H_ */
