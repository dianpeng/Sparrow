#ifndef MAP_H_
#define MAP_H_
#include "object.h"

void ObjMapInit( struct ObjMap* map , size_t capacity );
void ObjMapPut( struct ObjMap* , struct ObjStr* key , Value val );
int ObjMapFind( struct ObjMap* , const struct ObjStr*,Value* );
int ObjMapFindStr( struct ObjMap* , const char* , Value* );
int ObjMapRemove( struct ObjMap*, const struct ObjStr* ,Value* );
void ObjMapClear( struct ObjMap* );
void ObjMapDestroy( struct ObjMap* );
void ObjMapIterInit( struct ObjMap* , struct ObjIterator* );

#endif /* MAP_H_ */
