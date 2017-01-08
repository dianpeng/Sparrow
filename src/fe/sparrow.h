#ifndef SPARROW_H_
#define SPARROW_H_
#include "object.h"

int RunString( struct Sparrow* , const char* source , struct ObjMap* env,
    Value* ret , struct CStr* err );

int RunFile( struct Sparrow* , const char* filepath , struct ObjMap* env,
    Value* ret , struct CStr* err );

#endif /* SPARROW_H_ */
