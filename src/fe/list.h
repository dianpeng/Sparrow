#ifndef LIST_H_
#define LIST_H_
#include "object.h"

void ObjListInit( struct Sparrow* , struct ObjList* , size_t );

static SPARROW_INLINE void
ObjListDestroy( struct ObjList* list ) {
  free(list->arr);
  list->size = 0;
  list->cap = 0;
  list->arr = NULL;
}

static SPARROW_INLINE void
ObjListPush( struct ObjList* list, Value val ) {
  if(list->size == list->cap) {
    /* Cannot use MemGrow since it is managed memory */
    size_t ncap = list->cap == 0 ? 2 : 2 * list->cap;
    list->arr = realloc(list->arr,ncap*sizeof(Value));
    list->cap = ncap;
  }
  list->arr[list->size] = val;
  ++list->size;
}

void ObjListExtend( struct ObjList* , const struct ObjList* );
void ObjListResize( struct ObjList* , size_t );

static SPARROW_INLINE void
ObjListPop ( struct ObjList* list ) {
  if(list->size >0) --list->size;
}

static SPARROW_INLINE Value
ObjListIndex( struct ObjList* list , size_t idx ) {
  assert(list->size > idx);
  return list->arr[idx];
}

#define ObjListLast(L) ObjListIndex(L,(L)->size-1)
#define ObjListSize(L) ((L)->size)
#define ObjListClear(LIST) ((LIST)->size = 0)
void ObjListIterInit( struct ObjList* , struct ObjIterator* );

#endif /* LIST_H_ */
