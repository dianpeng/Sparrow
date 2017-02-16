#ifndef MAP_H_
#define MAP_H_
#include "../conf.h"
#include "object.h"

static SPARROW_INLINE
void ObjMapInit( struct ObjMap* map , size_t capacity ) {
  if(capacity == 0) {
    map->entry = NULL;
  } else {
    assert(!(capacity & (capacity-1)));
    map->entry = calloc(capacity , sizeof(struct ObjMapEntry) );
  }
  map->cap = capacity;
  map->size= 0;
  map->scnt = 0;
  map->mops= NULL;
}

void ObjMapPut( struct ObjMap* , struct ObjStr* key , Value val );
int ObjMapFind( struct ObjMap* , const struct ObjStr*,Value* );
int ObjMapFindStr( struct Sparrow* , struct ObjMap* , const char* , Value* );
int ObjMapRemove( struct ObjMap*, const struct ObjStr* ,Value* );
void ObjMapClear( struct ObjMap* );
void ObjMapDestroy( struct ObjMap* );
void ObjMapIterInit( struct ObjMap* , struct ObjIterator* );

#endif /* MAP_H_ */
