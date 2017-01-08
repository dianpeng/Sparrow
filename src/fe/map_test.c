#include "map.h"
#include <stdlib.h>

static uint32_t key_hash( const char* str, size_t len ) {
  if(len <LARGE_STRING_SIZE)
    return StringHash(str,len);
  else
    return LargeStringHash(str,len);
}

static struct ObjStr*
new_str( const char* str , struct ObjStr* buf ) {
  buf->str = str;
  buf->len = strlen(str);
  buf->hash= key_hash(str,strlen(str));
  return buf;
}

static void test_map_basic() {
  {
    struct ObjMap m;
    struct ObjStr k1,k2,k3,k4,k5,k;
    Value v;
    ObjMapInit(&m,2);
    {
      Value v;
      Vset_number(&v,1);
      ObjMapPut(&m,new_str("k1",&k1),v);
      assert(m.size == 1);
      assert(m.cap == 2);
    }
    {
      Value v;
      Vset_number(&v,2);
      ObjMapPut(&m,new_str("k2",&k2),v);
      assert( m.size == 2);
      assert(m.cap == 2);
    }
    {
      Value v;
      Vset_number(&v,3);
      ObjMapPut(&m,new_str("k3",&k3),v);
      assert( m.size == 3);
      assert( m.cap >= m.size);
    }
    {
      Value v;
      Vset_number(&v,4);
      ObjMapPut(&m,new_str("k4",&k4),v);
      assert(m.size == 4);
    }
    {
      Value v;
      Vset_number(&v,5);
      ObjMapPut(&m,new_str("k5",&k5),v);
      assert(m.size == 5);
    }
    assert( ObjMapFind(&m , new_str("k1",&k) , &v) == 0);
    assert(Vget_number(&v) == 1);

    assert( ObjMapFind(&m , new_str("k2",&k) , &v) == 0);
    assert(Vget_number(&v) == 2);

    assert( ObjMapFind(&m , new_str("k2",&k) , &v) == 0);
    assert(Vget_number(&v) == 2);

    assert( ObjMapFind(&m , new_str("k2",&k) , &v) == 0);
    assert(Vget_number(&v) == 2);

    assert( ObjMapFind(&m , new_str("k2",&k) , &v) == 0);
    assert(Vget_number(&v) == 2);

    // Iterators
    {
      struct ObjIterator itr;
      ObjMapIterInit(&m,&itr);
      size_t cnt = 0;
      while(itr.has_next(NULL,&itr) == 0) {
        Value k;
        Value v;
        double num;
        itr.deref(NULL,&itr,&k,&v);
        num = Vget_number(&v);
        assert( num == 1 ||
                num == 2 ||
                num == 3 ||
                num == 4 ||
                num == 5 );
        (void)k;
        itr.move(NULL,&itr);
        ++cnt;
      }
      assert(cnt == 5);
    }
    ObjMapDestroy(&m);
  }
  // Test updating
  {
    struct ObjMap m;
    Value v;
    struct ObjStr k1,k2;
    ObjMapInit(&m,2);
    Vset_number(&v,10);
    ObjMapPut(&m,new_str("Key2",&k1),v);
    assert( ObjMapFind(&m,&k1,&v) == 0);
    assert(Vget_number(&v) == 10);
    Vset_number(&v,1);
    ObjMapPut(&m,new_str("Key2",&k2),v);
    assert( ObjMapFind(&m,&k2,&v) == 0);
    assert(Vget_number(&v) == 1);
    assert(m.size == 2);
    ObjMapDestroy(&m);
  }
  {
    struct ObjMap m;
    Value v;
    struct ObjStr k1,k2;
    ObjMapInit(&m,2);
    Vset_number(&v,10);
    ObjMapPut(&m,new_str("Key",&k1),v);
    assert( ObjMapFind(&m,&k1,&v) == 0);
    assert( Vget_number(&v) == 10);
    assert( m.size == 1 );
    // Delete this entry
    assert( ObjMapRemove(&m,new_str("Key",&k2),&v) == 0);
    assert( m.size == 0);
    assert( ObjMapFind(&m,&k2,&v) != 0 );
    ObjMapDestroy(&m);
  }
}

int main() {
  test_map_basic();
  return 0;
}
