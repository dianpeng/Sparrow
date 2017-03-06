#include <vm/list.h>
#include <stdlib.h>

static void test_list_basic() {
  struct ObjList l;
  Value v;
  Vset_number(&v,10);
  ObjListInit(&l,2);
  assert(l.size == 0);
  assert(l.cap == 2);
  ObjListPush(&l,v);
  ObjListPush(&l,v);
  assert(l.size == 2);
  assert(l.cap == 2);
  ObjListPush(&l,v);
  assert(l.size == 3);
  assert(l.cap >= l.size);
  ObjListPop(&l);
  assert(l.size == 2);
  v = ObjListIndex(&l,0);
  assert(Vget_number(&v) == 10);
  v = ObjListIndex(&l,1);
  assert(Vget_number(&v) == 10);
  ObjListClear(&l);
  assert(l.size == 0);
  ObjListDestroy(&l);
}

static void test_list_iter() {
  struct ObjList l;
  struct ObjIterator itr;
  Value v;
  size_t cnt = 0;
  Vset_number(&v,10);
  ObjListInit(&l,2);
  ObjListPush(&l,v);
  ObjListPush(&l,v);
  ObjListPush(&l,v);

  ObjListIterInit(&l,&itr);
  while(itr.has_next(NULL,&itr) == 0) {
    Value v;
    Value k;
    itr.deref(NULL,&itr,&k,&v);
    (void)k;
    assert(Vget_number(&v) == 10);
    itr.move(NULL,&itr);
    ++cnt;
  }
  assert(cnt == 3);
  ObjListDestroy(&l);
}

int main() {
  test_list_basic();
  test_list_iter();
  return 0;
}
