#include <sparrow.h>
#include <shared/adt.h>
#include <shared/util.h>

struct SparrowHashNode {
  uint32_t full_hash;
  uint32_t next : 29;
  uint32_t more :  1;
  uint32_t del  :  1;
  uint32_t use  :  1;
};

#define node_size( KEY_LEN , OBJ_SIZE ) \
  (sizeof(struct SparrowHashNode) + KEY_LEN + OBJ_SIZE)

static SPARROW_INLINE void* node_key( struct SparrowHash* hash ,
    struct SparrowHashNode* entry ) {
  size_t offset = sizeof(struct SparrowHashNode);
  return ((char*)(entry) + offset);
}

static SPARROW_INLINE void* node_value(struct SparrowHash* hash ,
    struct SparrowHashNode* entry ) {
  size_t offset = sizeof(struct SparrowHashNode) + hash->key_attr.len;
  return ((char*)(entry) + offset);
}

static void* hash_calloc( struct SparrowHash* hash , size_t len ) {
  if(hash->alloc) {
    void* mem = ArenaAllocatorAlloc(hash->alloc,len);
    memset(mem,0,len);
    return mem;
  } else return calloc(1,len);
}

static void* hash_realloc( struct SparrowHash* hash , void* old , size_t old_len,
    size_t new_len ) {
  return hash->alloc ? ArenaAllocatorRealloc(hash->alloc,old,old_len,new_len) :
    realloc(old,new_len);
}

static struct SparrowHashNode* hash_index( struct SparrowHash* hash ,
    size_t index ) {
  void* ret = hash->entry;
  return (struct SparrowHashNode*)((char*)(ret) +
      index * node_size(hash->key_attr.len,hash->obj_len));
}

static uint32_t node_location( struct SparrowHash* hash ,
    struct SparrowHashNode* node ) {
  const char* c = (const char*)(hash->entry);
  size_t diff = (const char*)(node) - c;
  return diff / node_size(hash->key_attr.len,hash->obj_len);
}

void SparrowHashInit( struct SparrowHash* hash ,
    const struct SparrowHashKeyAttribute* attr ,
    size_t obj_len ,
    size_t capacity ,
    struct ArenaAllocator* alloc ) {
  SPARROW_ASSERT( SparrowAlign(attr->len,4) == attr->len );
  hash->key_attr = *attr;
  hash->alloc = alloc;
  hash->obj_len = obj_len;
  hash->cap = capacity;
  hash->size = 0;
  hash->occupy=0;
  hash->entry = NULL;
  if(capacity) {
    hash->entry = hash_calloc( hash , capacity * node_size(attr->len,obj_len) );
  }
}

enum {
  FIND,
  INSERT
};

static struct SparrowHashNode* find_entry( struct SparrowHash* hash ,
    void* key , uint32_t full_hash , int option ) {
  size_t index = full_hash & (hash->cap - 1);
  struct SparrowHashNode* n = hash_index( hash , index );
  if(!(n->use) && (option == INSERT)) {
    n->full_hash = full_hash;
    return n;
  } else if(n->del && (option == INSERT)) {
    n->full_hash = full_hash;
    return n;
  }

  /* Do a search through the chain */
  do {
    if(n->del) {
      if(option == INSERT) {
        n->use = 0;
        return n;
      }
    } else if( full_hash == n->full_hash &&
        hash->key_attr.eq( node_key(hash,n) , key ) ) {
      SPARROW_ASSERT(n->use);
      return n;
    }
    if(n->more) {
      n = hash_index( hash , n->next );
    } else {
      break;
    }
  } while(1);

  /* Not found */
  if(option == FIND) return NULL;
  else {
    uint32_t f = full_hash;
    struct SparrowHashNode* start = n;
    do
      n = hash_index(hash, (++full_hash & (hash->cap-1)));
    while(n->use);
    SPARROW_ASSERT(!n->del);
    start->more = 1;
    start->next = node_location(hash,n);
    n->full_hash = f;
    return n;
  }
}

static void rehash( struct SparrowHash* hash ) {
  struct SparrowHash new_hash;
  size_t ncap = hash->cap * 2;
  size_t i;
  SPARROW_ASSERT( hash->cap == hash->occupy );
  SPARROW_ASSERT( ncap < SPARROW_HASH_MAX_SIZE );
  SparrowHashInit(&new_hash,hash->key_attr,hash->obj_len,ncap,hash->alloc);
}


































