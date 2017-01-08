#include "list.h"
#include "vm.h"
#include "error.h"

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
  assert((size_t)itr->u.index < l->size);
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
