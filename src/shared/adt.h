#ifndef ADT_H_
#define ADT_H_
#include <sparrow.h>

struct ArenaAllocator;

/* Some abstract data type or utility data structure */
typedef uint32_t (*SparrowHashKeyHashFunction) ( void* );
typedef int (*SparrowHashKeyEqualFunction) ( void* , void* );

struct SparrowHashKeyAttribute {
  SparrowHashKeyHashFunction hash;
  SparrowHashKeyEqualFunction eq ;
  size_t len;
};

struct SparrowHash {
  struct SparrowHashKeyAttribute key_attr;
  struct ArenaAllocator* alloc; /* Allocator */
  size_t obj_len;               /* Object length */

  void* entry;
  size_t cap;
  size_t size;
  size_t occupy;
};

void  SparrowHashInit( struct SparrowHash* ,
    const struct SparrowHashKeyAttribute* attr,
    size_t obj_len ,
    size_t capacity,
    struct ArenaAllocator* alloc );

void* SparrowHashInsert( struct SparrowHash* , void* key );
void* SparrowHashFind  ( struct SparrowHash* , void* key );
int   SparrowHashDelete( struct SparrowHash* , void* key , void* );
void  SparrowHashClear ( struct SparrowHash* );
void  SparrowHashDestroy(struct SparrowHash* );

#endif /* ADT_H_ */
