#ifndef LIST_H_
#define LIST_H_
#include "object.h"

static SPARROW_INLINE
void ObjListInit( struct Sparrow* sparrow , struct ObjList* list ,
    size_t cap ) {
  if(cap == 0) {
    list->arr = NULL;
    list->size = list->cap = 0;
  } else {
    list->arr = malloc(cap*sizeof(Value));
    list->size = 0;
    list->cap = cap;
  }
}

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
struct ObjList* ObjListSlice( struct Sparrow* , struct ObjList* , size_t ,
    size_t );

static SPARROW_INLINE void
ObjListPop ( struct ObjList* list ) {
  if(list->size >0) --list->size;
}

static SPARROW_INLINE Value
ObjListIndex( struct ObjList* list , size_t idx ) {
  SPARROW_ASSERT(list->size > idx);
  return list->arr[idx];
}

/* It will automatically extend the array and assign the value if
 * we need to do so */
void ObjListAssign( struct ObjList* list , size_t idx , Value value );

#define ObjListLast(L) ObjListIndex(L,(L)->size-1)
#define ObjListSize(L) ((L)->size)
#define ObjListClear(LIST) ((LIST)->size = 0)
void ObjListIterInit( struct ObjList* , struct ObjIterator* );

#endif /* LIST_H_ */
