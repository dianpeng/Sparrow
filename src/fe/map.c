#include "map.h"
#include <stdlib.h>

static void insert( struct ObjMap* map ,
    struct ObjStr* key , uint32_t fhash, Value val );

static uint32_t key_hash( const char* str, size_t len ) {
  if(len <LARGE_STRING_SIZE)
    return StringHash(str,len);
  else
    return LargeStringHash(str,len);
}

enum {
  DO_FIND,
  DO_INSERT
};

static struct ObjMapEntry* find_entry( struct ObjMap* map , const char* key ,
    uint32_t fhash , int opt ) {
  /* Try to find the main position */
  int idx = fhash & (map->cap-1);
  struct ObjMapEntry* entry = map->entry + idx;
  struct ObjMapEntry* prev;

  if(!entry->used) {
    if(opt == DO_INSERT) {
      entry->used = 0;
      entry->fhash = fhash;
      return entry;
    } else {
      return NULL;
    }
  } else if( entry->del && opt == DO_INSERT ) {
    entry->used = 0;
    entry->fhash = fhash;
    return entry;
  }

  do {
    if(entry->del) {
      if( opt == DO_INSERT )
        return entry;
    } else if(entry->fhash == fhash && strcmp(entry->key->str,key) == 0) {
      assert(entry->used);
      return entry;
    }

    prev = entry;
    if(!entry->more)
      break;
    else
      entry = map->entry+entry->next;
  } while(1);
  if(opt == DO_INSERT) {
    uint32_t h = fhash;
    do
      entry = map->entry+ (++h & (map->cap-1));
    while( entry->used );
    assert(!entry->del);
    prev->next = (entry-map->entry);
    prev->more = 1;
    entry->fhash = fhash;
    return entry;
  } else {
    return NULL;
  }
}

static void rehash( struct ObjMap* map ) {
  size_t i;
  struct ObjMap temp_map;
  size_t ncap = map->cap == 0 ? 2 : map->cap * 2;
  ObjMapInit(&temp_map,ncap);
  for( i = 0 ; i < map->cap ; ++i ) {
    struct ObjMapEntry* ent = map->entry+i;
    if(ent->used && !ent->del) {
      insert(&temp_map,ent->key,ent->fhash,ent->value);
    }
  }
  free(map->entry); /* free the existed entry */
  map->entry = temp_map.entry;
  map->scnt = temp_map.scnt;
  map->size = temp_map.size;
  map->cap = temp_map.cap;
}

static void insert( struct ObjMap* map , struct ObjStr* key , uint32_t fhash,
  Value val ) {
  struct ObjMapEntry* entry;
  if(map->scnt == map->cap)
    rehash(map);
  entry = find_entry(
      map,
      key->str,
      fhash,
      DO_INSERT
      );
  assert(entry);
  entry->key = key;
  entry->value = val;
  entry->del = 0;
  entry->used = 1;
  ++map->size;
  ++map->scnt;
}

void ObjMapClear( struct ObjMap* map ) {
  memset(map->entry,0,sizeof(struct ObjMapEntry)*map->cap);
  map->size = 0;
  map->scnt = 0;
}

void ObjMapDestroy( struct ObjMap* map ) {
  free(map->entry);
  map->entry = NULL;
  map->cap = 0;
  map->size = 0;
  map->scnt = 0;
}

void ObjMapPut( struct ObjMap* map , struct ObjStr* key ,
  Value val ) {
  insert(map,key,key->hash,val);
}

int ObjMapFind( struct ObjMap* map , const struct ObjStr* key ,
    Value* val ) {
  struct ObjMapEntry* entry = find_entry(
      map,
      key->str,
      key->hash,
      DO_FIND);
  if(entry) {
    if(val) *val = entry->value;
    return 0;
  } else {
    return -1;
  }
}

int ObjMapFindStr( struct ObjMap* map , const char* key ,
    Value* val ) {
  struct ObjMapEntry* entry = find_entry(
      map,
      key,
      key_hash(key,strlen(key)),
      DO_FIND);
  if(entry) {
    if(val) *val = entry->value;
    return 0;
  } else {
    return -1;
  }
}

int ObjMapRemove( struct ObjMap* map , const struct ObjStr* key ,
    Value* val ) {
  struct ObjMapEntry* entry = find_entry(
      map,
      key->str,
      key->hash,
      DO_FIND);
  if(entry) {
    entry->del = 1;
    if(val) *val = entry->value;
    --map->size;
    return 0;
  } else {
    return -1;
  }
}

/* ============================================
 * Map iterators
 * ==========================================*/
static int map_iter_has_next( struct Sparrow* sth ,
    struct ObjIterator* itr ) {
  struct ObjMap* m;
  UNUSE_ARG(sth);
  m = Vget_map(&(itr->obj));
  if( (size_t)itr->u.index >= m->cap ) {
    return -1;
  } else {
    int c = (int)(m->cap);
    int i;
    for( i = itr->u.index ; i < c ; ++i ) {
      if(m->entry[i].used && !(m->entry[i].del)) {
        break;
      }
    }
    itr->u.index = i;
    return i < c ? 0 : -1;
  }
}

static void map_iter_deref( struct Sparrow* sth ,
    struct ObjIterator* itr ,
    Value* key,
    Value* value ) {
  struct ObjMap* m;
  UNUSE_ARG(sth);
  m = Vget_map(&(itr->obj));
  assert((size_t)(itr->u.index) < m->cap);
  assert(m->entry[itr->u.index].used && !m->entry[itr->u.index].del);
  if(key) Vset_str(key,m->entry[itr->u.index].key);
  if(value) *value = m->entry[itr->u.index].value;
}

static void map_iter_move( struct Sparrow* sth ,
    struct ObjIterator* itr ) {
  UNUSE_ARG(sth);
  ++itr->u.index;
}

void ObjMapIterInit( struct ObjMap* map, struct ObjIterator* itr ) {
  itr->has_next = map_iter_has_next;
  itr->deref = map_iter_deref;
  itr->move = map_iter_move;
  itr->destroy = NULL;
  Vset_map(&(itr->obj),map);
  itr->u.index = 0;
}
