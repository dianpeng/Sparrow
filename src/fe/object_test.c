#define STRING_POOL_SIZE 4
#include "object.h"
#include <float.h>

// Mostly test MACROs around Value object
static void test_value_primitive() {
  assert(sizeof(Value) == sizeof(void*));
  {
    Value v;
    Vset_number(&v,1);
    assert(Vis_number(&v));
    assert(!Vis_true(&v));
    assert(!Vis_false(&v));
    assert(!Vis_null(&v));
    assert(!Vis_gcobject(&v));
    assert(Vget_number(&v) == 1);
  }
  {
    Value v;
    Vset_number(&v,0);
    assert(Vis_number(&v));
    assert(!Vis_true(&v));
    assert(!Vis_false(&v));
    assert(!Vis_null(&v));
    assert(!Vis_gcobject(&v));
    assert(Vget_number(&v) == 0);
  }
  {
    Value v;
    Vset_number(&v,DBL_MAX);
    assert(Vis_number(&v));
    assert(!Vis_true(&v));
    assert(!Vis_false(&v));
    assert(!Vis_null(&v));
    assert(!Vis_gcobject(&v));
    assert(Vget_number(&v) == DBL_MAX);
  }
  {
    Value v;
    Vset_number(&v,DBL_MIN);
    assert(Vis_number(&v));
    assert(!Vis_true(&v));
    assert(!Vis_false(&v));
    assert(!Vis_null(&v));
    assert(!Vis_gcobject(&v));
    assert(Vget_number(&v) == DBL_MIN);
  }
  {
    Value v;
    Vset_number(&v,0.00001);
    assert(Vis_number(&v));
    assert(Vget_number(&v) == 0.00001);
  }
  {
    Value v;
    Vset_null(&v);
    assert(Vis_null(&v));
    assert(!Vis_true(&v));
    assert(!Vis_false(&v));
    assert(!Vis_number(&v));
    assert(!Vis_gcobject(&v));
  }
  {
    Value v;
    Vset_true(&v);
    assert(Vis_true(&v));
    assert(!Vis_null(&v));
    assert(!Vis_false(&v));
    assert(!Vis_number(&v));
    assert(!Vis_gcobject(&v));
    assert(Vget_boolean(&v));
  }
  {
    Value v;
    Vset_false(&v);
    assert(Vis_false(&v));
    assert(!Vis_true(&v));
    assert(!Vis_null(&v));
    assert(!Vis_number(&v));
    assert(!Vis_gcobject(&v));
    assert(Vget_boolean(&v) == 0);
  }
  {
    Value v;
    struct ObjStr str;
    Vset_str(&v,&str);
    assert(Vis_str(&v));
    assert(!Vis_true(&v));
    assert(!Vis_false(&v));
    assert(!Vis_null(&v));
    assert(!Vis_number(&v));
    assert(Vget_str(&v) == &str);
  }
  {
    Value v;
    struct ObjList lst;
    Vset_list(&v,&lst);
    assert(Vis_list(&v));
    assert(!Vis_true(&v));
    assert(!Vis_false(&v));
    assert(!Vis_null(&v));
    assert(!Vis_number(&v));
    assert(Vget_list(&v) == &lst);
  }
  {
    Value v;
    struct ObjMap m;
    Vset_map(&v,&m);
    assert(Vis_map(&v));
    assert(!Vis_true(&v));
    assert(!Vis_false(&v));
    assert(!Vis_null(&v));
    assert(!Vis_number(&v));
    assert(Vget_map(&v)==&m);
  }
  {
    Value v;
    struct ObjUdata u;
    Vset_udata(&v,&u);
    assert(Vis_udata(&v));
    assert(!Vis_true(&v));
    assert(!Vis_false(&v));
    assert(!Vis_null(&v));
    assert(!Vis_number(&v));
    assert(Vget_udata(&v) == &u);
  }
  {
    Value v;
    struct ObjIterator itr;
    Vset_iterator(&v,&itr);
    assert(Vis_iterator(&v));
    assert(!Vis_true(&v));
    assert(!Vis_false(&v));
    assert(!Vis_null(&v));
    assert(!Vis_number(&v));
    assert(Vget_iterator(&v) == &itr);
  }
}

void cls_init( struct ObjProto* cls ) {
  cls->num_arr = NULL;
  cls->num_size= 0;
  cls->num_cap = 0;
  cls->str_arr = NULL;
  cls->str_size = 0;
  cls->str_cap = 0;
  cls->uv_arr = NULL;
  cls->uv_cap = 0;
  cls->uv_size= 0;
}

static void test_const_table() {
  struct ObjProto cls;
  struct Sparrow sth;
  cls_init(&cls);
  SparrowInit(&sth);
  /* testing sequencing */
  {
    size_t i;
    for( i = 0 ; i < 100 ; ++i ) {
      assert(ConstAddNumber(&cls,i) == i);
    }
    for( i = 0 ; i < 100 ; ++i ) {
      assert(ConstAddNumber(&cls,i) == i);
    }
  }
  {
    size_t i;
    size_t gc_sz = sth.gc_sz;
    for( i = 0 ; i < 100 ; ++i ) {
      char buf[1024];
      struct ObjStr* str;
      sprintf(buf,"%zu",i);
      str = ObjNewStrNoGC(&sth,buf,strlen(buf));
      assert(ConstAddString(&cls,str) == i);
    }
    for( i = 0 ; i < 100 ; ++i ) {
      char buf[1024];
      struct ObjStr* str;
      sprintf(buf,"%zu",i);
      str = ObjNewStrNoGC(&sth,buf,strlen(buf));
      assert(ConstAddString(&cls,str) == i);
    }
    /* We should have a chain of GC object which are all string */
    {
      struct GCRef* start = sth.gc_start;
      size_t i = 0 ;
      while(start) {
        ++i;
        assert(start->gtype == VALUE_STRING);
        start = start->next;
      }
      assert( i == 100 + gc_sz );
    }
  }
  SparrowDestroy(&sth);
  free(cls.num_arr);
  free(cls.str_arr);
}

/* testring string */
static void test_string() {
  struct Sparrow sth;
  SparrowInit(&sth);
  srand(0);
  {
    const char* buf = "hello";
    struct ObjStr* str = ObjNewStrNoGC(&sth,buf,strlen(buf));
    size_t i;
    for( i = 0 ; i < 100 ; ++i ) {
      assert( str = ObjNewStrNoGC(&sth,buf,strlen(buf)));
    }
  }
  {
    static const size_t N = 10000;
    char* str_arr[N];
    struct ObjStr* ostr_arr[N];
    size_t i;
    for( i = 0 ; i < N ; ++i ) {
      str_arr[i] = malloc(128);
      sprintf(str_arr[i],"%zu",i);
      assert( ostr_arr[i] = ObjNewStrNoGC(&sth,str_arr[i],strlen(str_arr[i])));
    }
    for( i = 0 ; i < N ; ++i ) {
      assert( ostr_arr[i] == ObjNewStrNoGC(&sth,str_arr[i],strlen(str_arr[i])));
    }
    for( i = 0 ; i < N ; ++i ) {
      free(str_arr[i]);
    }
  }
  { /* Large string */
    char large_str[1024];
    size_t i;
    struct ObjStr* lstr;
    for( i = 0 ; i < 800 ; ++i ) {
      large_str[i] = 'a';
    }
    large_str[800] = 0;
    assert( lstr = ObjNewStrNoGC(&sth,large_str,strlen(large_str)));
    for( i = 0 ; i < 128 ; ++i ) {
      assert( lstr != ObjNewStrNoGC(&sth,large_str,strlen(large_str)));
    }
  }
  SparrowDestroy(&sth);
}

int main() {
  test_value_primitive();
  test_string();
  test_const_table();
  return 0;
}
