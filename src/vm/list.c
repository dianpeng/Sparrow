#include "list.h"
#include "vm.h"
#include "error.h"
#include "object.h"

void ObjListAssign( struct ObjList* self , size_t index , Value value ) {
  size_t i;
  if(index >= self->cap) {
    size_t ncap = index + 1 + ( self->cap - self->size );
    self->arr = realloc(self->arr,ncap*sizeof(Value));
    self->cap = ncap;
  }
  for( i = self->size ; i < index ; ++i ) {
    Vset_null(self->arr+i);
  }
  self->arr[index] = value;
  if(index >= self->size) self->size = index + 1;
}

void ObjListExtend( struct ObjList* self , const struct ObjList* that ) {
  size_t i;
  /* reserve enough memory */
  if(self->size + that->size > self->cap) {
    size_t ncap = that->size + self->cap;
    self->arr = realloc(self->arr,ncap*sizeof(Value));
  }
  for( i = 0 ; i < that->size ; ++i ) {
    self->arr[self->size++] = that->arr[i];
  }
}

void ObjListResize( struct ObjList* self, size_t size ) {
  if(size < self->size) {
    self->size = size;
  } else if(size <= self->cap) {
    size_t i;
    for( i = self->size ; i <= size; ++i ) {
      Vset_null(self->arr+i);
    }
    self->size = size;
  } else {
    size_t i;
    size_t ncap = self->cap - self->size + size;
    self->arr = realloc(self->arr,ncap*sizeof(Value));
    for( i = self->size ; i < size ; ++i ) {
      Vset_null(self->arr+i);
    }
    self->size = size;
    self->cap = ncap;
  }
}

struct ObjList* ObjListSlice( struct Sparrow* sparrow , struct ObjList* list ,
    size_t start , size_t end ) {
  struct ObjList* new_list;
  SPARROW_ASSERT(start <= end);
  new_list = ObjNewList(sparrow,(end-start));
  SPARROW_ASSERT( new_list->cap >= (end-start) );
  memcpy(new_list->arr,list->arr+start,(end-start)*sizeof(Value));
  new_list->size = (end-start);
  return new_list;
}

static int list_iter_has_next( struct Sparrow* sth ,
    struct ObjIterator* itr ) {
  struct ObjList* l;
  l = Vget_list(&(itr->obj));
  if( (size_t)itr->u.index >= l->size ) {
    return -1;
  } else {
    return 0;
  }
}

static void list_iter_deref( struct Sparrow* sth ,
    struct ObjIterator* itr ,
    Value* key,
    Value* value ) {
  struct ObjList* l;
  l = Vget_list(&(itr->obj));
  SPARROW_ASSERT((size_t)itr->u.index < l->size);
  if(value) *value = l->arr[itr->u.index];
  if(key) Vset_number(key,itr->u.index);
}

static void list_iter_move( struct Sparrow* sth ,
    struct ObjIterator* itr ) {
  ++itr->u.index;
}

void ObjListIterInit( struct ObjList* l , struct ObjIterator* itr ) {
  Vset_list(&(itr->obj),l);
  itr->u.index = 0;
  itr->has_next = list_iter_has_next;
  itr->deref = list_iter_deref;
  itr->move = list_iter_move;
  itr->destroy = NULL;
}
