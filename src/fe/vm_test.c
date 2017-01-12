#include "vm.h"
#include "parser.h"
#include "object.h"
#include "list.h"
#include "map.h"
#include "../util.h"

#include <sys/time.h>
#include <inttypes.h>

#define STRINGIFY(...) #__VA_ARGS__

#if 0
static uint64_t now_in_microseconds() {
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return tv.tv_sec * 1000000 + tv.tv_usec;
}
#endif

/* A list formatter with format string
 * Format string:
 * 1. %s , a string
 * 2. %d , a integer number
 * 3. %f , a floating number
 * 3. null , a null literal
 * 4. true/false , a true/false literal
 * 5. [...] , a list comprehension
 * 6. {...} , a map comprehension */

enum {
  FTK_STRING,
  FTK_INTEGER,
  FTK_REAL,
  FTK_NULL,
  FTK_TRUE,
  FTK_FALSE,
  FTK_LSQR,
  FTK_RSQR,
  FTK_COMMA,
  FTK_LBRA,
  FTK_RBRA,
  FTK_EOF
};

struct flexer {
  const char* format;
  size_t pos;
  int token;
};

#define flexer_init(L,F) \
  do { \
    (L)->format = (F); \
    (L)->pos = 0; \
    (L)->token = -1; \
    flexer_next((L)); \
  } while(0)

int flexer_next( struct flexer* l ) {
  int c;
  do {
    c = l->format[l->pos];
    switch(c) {
      case 0:
        return l->token = FTK_EOF;
      case ' ': case '\t': case '\n':case '\b':case '\v':
        l->pos += 1;
        continue;
      case '[' :
        l->pos += 1;
        return l->token = FTK_LSQR;
      case ']' :
        l->pos += 1;
        return l->token = FTK_RSQR;
      case '{' :
        l->pos += 1;
        return l->token = FTK_LBRA;
      case '}':
        l->pos += 1;
        return l->token = FTK_RBRA;
      case ',':
        l->pos += 1;
        return l->token = FTK_COMMA;
      case '%': {
        int nc = l->format[l->pos+1];
        switch(nc) {
          case 's' :
            l->pos += 2; return l->token = FTK_STRING;
          case 'd':
            l->pos += 2; return l->token = FTK_INTEGER;
          case 'f':
            l->pos += 2; return l->token = FTK_REAL;
          default:
            assert(!"Formatted string error!!!");
            return -1;
        }
        break;
      }
      case 'n':
        if(l->format[l->pos+1] == 'u' &&
           l->format[l->pos+2] == 'l' &&
           l->format[l->pos+3] == 'l') {
          l->pos += 4;
          return l->token = FTK_NULL;
        }
        assert(!"Formatted string error!!");
        break;
      case 't':
        if(l->format[l->pos+1] == 'r' &&
           l->format[l->pos+2] == 'u' &&
           l->format[l->pos+3] == 'e' ) {
          l->pos += 4;
          return l->token = FTK_TRUE;
        }
        assert(!"Formatted string error!!");
        break;
      case 'f':
        if(l->format[l->pos+1] == 'a' &&
           l->format[l->pos+2] == 'l' &&
           l->format[l->pos+3] == 's' &&
           l->format[l->pos+4] == 'e' ) {
          l->pos += 5;
          return l->token = FTK_FALSE;
        }
        assert(!"Formatted string error!!");
        break;
      default:
        assert(!"Formatted string error!!");
        break;
    }
  } while(1);
  assert(!"Formatted string error!!");
  return -1;
}

/* Code for doing job */
static void create_list( struct Sparrow* , struct flexer* ,
    Value* , va_list );

static void create_map( struct Sparrow* , struct flexer* ,
    Value* , va_list );

static void create_object( struct Sparrow* sparrow , struct flexer* l ,
    Value* ret , va_list vl ) {
  switch(l->token) {
    case FTK_STRING:
    {
      const char* str = va_arg(vl,const char*);
      struct ObjStr* ostr = ObjNewStrNoGC( sparrow , str , strlen(str) );
      Vset_str( ret , ostr );
      flexer_next(l);
      break;
    }
    case FTK_REAL:
    {
      double num = va_arg(vl,double);
      Vset_number( ret , num );
      flexer_next(l);
      break;
    }
    case FTK_INTEGER:
    {
      int num = va_arg(vl,int);
      Vset_number( ret, (double)num );
      flexer_next(l);
      break;
    }
    case FTK_NULL:
      Vset_null(ret);
      flexer_next(l);
      break;
    case FTK_TRUE:
      Vset_true(ret);
      flexer_next(l);
      break;
    case FTK_FALSE:
      Vset_false(ret);
      flexer_next(l);
      break;
    case FTK_LSQR:
      create_list(sparrow,l,ret,vl);
      break;
    case FTK_LBRA:
      create_map(sparrow,l,ret,vl);
      break;
    default:
      assert(!"Cannot reach here, format string error!!");
      break;
  }
}

static void create_list( struct Sparrow* sparrow , struct flexer* l ,
    Value* ret , va_list vl ) {
  assert( l->token == FTK_LSQR );
  flexer_next(l);
  if(l->token == FTK_RSQR) {
    struct ObjList* list = ObjNewListNoGC(sparrow,0);
    Vset_list(ret,list);
    flexer_next(l);
  } else {
    struct ObjList* list = ObjNewListNoGC(sparrow,16);
    do {
      Value element;
      create_object(sparrow,l,&element,vl);
      ObjListPush(list,element);
      if( l->token == FTK_COMMA ) {
        flexer_next(l); continue;
      } else if( l->token == FTK_RSQR ) {
        flexer_next(l); break;
      } else {
        assert(!"Unrecognized format string in list!");
        break;
      }
    } while(1);
    Vset_list(ret,list);
  }
}

static void create_map( struct Sparrow* sparrow, struct flexer* l,
    Value* ret , va_list vl ) {
  assert( l->token == FTK_LBRA );
  flexer_next(l);
  if(l->token == FTK_RBRA) {
    struct ObjMap* m = ObjNewMapNoGC(sparrow,0);
    Vset_map(ret,m);
    flexer_next(l);
  } else {
    struct ObjMap* m = ObjNewMapNoGC(sparrow,16);
    do {
      const char* key = va_arg(vl,const char*);
      struct ObjStr* okey = ObjNewStrNoGC(sparrow,key,strlen(key));
      Value val;
      create_object(sparrow,l,&val,vl);
      ObjMapPut(m,okey,val);
      if( l->token == FTK_COMMA ) {
        flexer_next(l); continue;
      } else if( l->token == FTK_RBRA ) {
        flexer_next(l); break;
      } else {
        assert(!"Unrecognized format string in map!");
        break;
      }
    } while(1);
    Vset_map(ret,m);
  }
}

static Value createV(struct Sparrow* sparrow , const char* format ,
    va_list vl ) {
  struct flexer l;
  Value ret;
  flexer_init(&l,format);
  create_object(sparrow,&l,&ret,vl);
  return ret;
}

#if 0
static Value create( struct Sparrow* sparrow , const char* format , ... ) {
  va_list vl;
  va_start(vl,format);
  return createV(sparrow,format,vl);
}
#endif

/* Execute a specific code */
static void run_code( struct Sparrow* sparrow , const char* src ,
    Value* ret , int dump_bc ) {
  struct CStr err;
  struct ObjModule* mod;
  mod = Parse(sparrow,"test",src,&err);
  if(dump_bc && mod) ObjDumpModule(mod,stdout,"test");
  if(!mod) {
    fprintf(stderr,"Failed %s\n",err.str);
    abort();
  } else {
    struct ObjMap* m = ObjNewMapNoGC(sparrow,2);
    struct ObjComponent* comp = ObjNewComponentNoGC( sparrow , mod , m );
    int r = Execute(sparrow,comp,ret,&err);
    if(r) {
      ObjDumpModule(mod,stdout,"test");
      fprintf(stderr,"Execution error:%s",err.str);
      abort();
    }
  }
}

#if 0

static void dump_code( const char* src ) {
  struct Sparrow sparrow;
  Value ret;
  SparrowInit(&sparrow);
  run_code(&sparrow,src,&ret,1);
  {
    struct StrBuf sbuf;
    struct CStr str;
    StrBufInit(&sbuf,1024);
    ValuePrint(&sparrow,&sbuf,ret);
    str = StrBufToCStr(&sbuf);
    fprintf(stderr,"%s\n",str.str);
    StrBufDestroy(&sbuf);
    CStrDestroy(&str);
  }
  SparrowDestroy(&sparrow);
}

static void benchmark( const char* src ) {
  struct Sparrow sparrow;
  Value ret;
  uint64_t end;
  uint64_t start = now_in_microseconds();
  SparrowInit(&sparrow);
  run_code(&sparrow,src,&ret,0);
  SparrowDestroy(&sparrow);
  end = now_in_microseconds();
  fprintf(stderr,"%lld\n",(long long int)(end-start));
}

#endif

static void compare_value( Value, Value);

static void compare_list( Value ret, Value expect ) {
  struct ObjList* left = Vget_list(&ret);
  struct ObjList* right= Vget_list(&expect);
  size_t i;
  assert(left->size == right->size);
  for( i = 0 ; i < left->size ; ++i ) {
    compare_value(left->arr[i],right->arr[i]);
  }
}

static void compare_map ( Value ret, Value expect ) {
  struct ObjMap* left = Vget_map(&ret);
  struct ObjMap* right= Vget_map(&expect);
  struct ObjIterator itr;
  assert(left->size == right->size);
  ObjMapIterInit(left,&itr);
  while(itr.has_next(NULL,&itr) == 0) {
    Value k;
    Value v;
    struct ObjStr* key;
    Value rv;
    itr.deref(NULL,&itr,&k,&v);
    key = Vget_str(&k);
    assert(ObjMapFind(right,key,&rv) == 0);
    compare_value(v,rv);
    itr.move(NULL,&itr);
  }
}

static void compare_value( Value ret , Value expect ) {
  /* In sparrow, there's no equality concept at all,
   * so we try to mimic the equality which is just for
   * our testing purpose */
  if(Vis_str(&ret)) {
    if(Vis_str(&expect)) {
      assert(ObjStrCmp(Vget_str(&ret),Vget_str(&expect)) == 0);
    } else {
      assert(0);
    }
  } else if(Vis_number(&ret)) {
    if(Vis_number(&expect)) {
      assert(Vget_number(&ret) == Vget_number(&expect));
    } else {
      assert(0);
    }
  } else if(Vis_true(&ret)) {
    assert(Vis_true(&expect));
  } else if(Vis_false(&ret)) {
    assert(Vis_false(&expect));
  } else if(Vis_null(&ret)) {
    assert(Vis_null(&expect));
  } else if(Vis_list(&ret)) {
    if(Vis_list(&expect)) compare_list(ret,expect);
    else assert(0);
  } else if(Vis_map(&ret)) {
    if(Vis_map(&expect)) compare_map(ret,expect);
    else assert(0);
  } else {
    assert(!"Unsupported type!");
  }
}

static int COUNT = 0;

static void expect( const char* src , const char* format , ... ) {
  Value ret;
  Value expect;
  struct Sparrow sparrow;
  va_list vl;
  SparrowInit(&sparrow);
  run_code(&sparrow,src,&ret,0);
  va_start(vl,format);
  expect = createV(&sparrow,format,vl);
  compare_value(ret,expect);
  SparrowDestroy(&sparrow);
  ++COUNT;
}

static void test_basic_arithmatic() {
  expect("return true;","true");
  expect("return false;","false");
  expect("return null;","null");

  expect("return --10;","%d",10);
  expect("return !!10;","true");
  expect("return ----10;","%d",10);
  expect("return ---1;","%d",-1);
  expect("return -!!10;","%d",-1);
  expect("return -1;","%d",-1);
  expect("return -true;","%d",-1);
  expect("return -0;","%d",0);
  expect("return !0;","true");
  expect("return !1;","false");
  expect("return !true;","false");
  expect("return !false;","true");

  expect("return !-!-!!1;","true");

  expect("return 0;","%d",0);
  expect("return 1+2;","%d",3);
  expect("return 1*10;","%d",10);
  expect("return 1/10;","%f",0.1);
  expect("return 4^2;","%d",16);
  expect("return (1+2-3)*10+1;","%d",1);
  expect("return 1.0;","%f",1.0);
  expect("return 3%5;","%d",3);
  expect("return 1+true;","%d",2);
  expect("return 10*false;","%d",0);
  expect("return 10*true;","%d",10);
  expect("return 1/true;","%d",1);
  expect("return true+false;","%d",1);
  expect("return 1+2*3;","%d",7);
  expect("return 20*3/2+5+2^2;","%d",39);

  expect("return \"Hello World\";","%s","Hello World");
  expect("return \"\";","%s","");
  expect("return \"hello\" + \" \" + \"world\";","%s","hello world");
  expect("return typeof(\"H\");","%s","string");
  expect("return \"\" + \"Hello World\";","%s","Hello World");

  /* List */
  expect("return [];","[]");
  expect("return [1];","[%d]",1);
  expect("return [true];","[true]");
  expect("return [false];","[false]");
  expect("return [null];","[null]");
  expect("return [true,false,null];","[true,false,null]");
  expect("return [1+1,2+2,3*3];","[%d,%d,%d]",2,4,9);
  expect("return [\"Hello World\"];","[%s]","Hello World");
  expect("return [1,2,3,[]];","[%d,%d,%d,[]]",1,2,3);
  expect("return [[[[]]]];","[[[[]]]]");
  expect("return [1,2,[\"A\",true,false,null,[null]]];",
      "[%d,%d,[%s,true,false,null,[null]]]",1,2,"A");

  /* Map */
  expect("return {};","{}");
  expect("return {\"A\":1};","{%d}","A",1);
  expect("return {\"A\":1,\"B\":2};","{%d,%d}","A",1,"B",2);
  expect("return {\"A\":true,\"B\":\"C\",\"C\":false,\"D\":null};",
      "{true,%s,false,null}","A","B","C","C","D");
  expect("return {\"a\":\"b\"};","{%s}","a","b");
  expect("return {\"a\":[1,2,3,4]};","{[%d,%d,%d,%d]}","a",1,2,3,4);
  expect("return {\"A\":{\"A\":{}}};","{{{}}}","A","A");
  expect("return {\"A\":[],\"B\":[{},{\"D\":[]}]};","{[],[{},{[]}]}","A","B","D");
}

static void test_basic_comparison() {
  expect("return 1>2;","false");
  expect("return 1<2;","true");
  expect("return 1==2;","false");
  expect("return 1!=2;","true");
  expect("return 1>=2;","false");
  expect("return 2>=2;","true");
  expect("return 2<=2;","true");
  expect("return 1<=2;","true");
  expect("return \"A\" < \"B\";","true");
  expect("return \"A\" <= \"B\";","true");
  expect("return \"Hello World\" == \"Hello World\";","true");
  expect("return \"Hello World\" >= \"Hello World\";","true");
  expect("return \"abc\" >= \"ABC\";","true");
  expect("return \"abc\" <= \"def\";","true");
  expect("return \"abc\" < \"def\";","true");
}

static void test_basic_logic() {
  expect("return true && true;","true");
  expect("return true && false;","false");
  expect("return true && 0;","false");
  expect("return true && 1 > 2;","false");
  expect("return true || false;","true");
  expect("return false || false;","false");
  expect("return true || false || false || true;","true");
  expect("return false || 1;","true");
  expect("return false || 0;","false");
  expect("return 0 || 1;","true");
  expect("return !0 || 0;","true");
  expect("return !1 || 0;","false");
}

static void test_arithmatic() {
  /* Unary */
  expect("a = 10; return -a;","%d",-10);
  expect("a = true; return !a;","false");
  expect("a = false;return !a;","true");
  expect("a = null; return a;","null");
  expect("a = -10;return -a;","%d",10);
  expect("a = --100;return ---a;","%d",-100);
  expect("a = true; return !!!a;","false");
  expect("a = false;return !!a;","false");
  expect("a = true; return !!a;","true");
  expect("a = 1;return !-!-!!a;","true");

  expect("a = null;return !a;","true");
  expect("return !null;","true");
  expect("return !!null;","false");
  expect("a = null;return !!a;","false");

  /* Arithmatic */
  expect("a = 10; return a+1;","%d",11);
  expect("a = 11; return a-1;","%d",10);
  expect("a = 10; return a*2;","%d",20);
  expect("a = 10; return 2*a;","%d",20);
  expect("a = 10; return a/2;","%d",5);
  expect("a = 2; return a^4;","%d",16);
  expect("a = 10; return 11-a;","%d",1);
  expect("a = 11; return 1+a;","%d",12);
  expect("a = 10; return 20/a;","%d",2);
  expect("a = 2; return 10/a;","%d",5);
  expect("a = 3; return a % 5;","%d",3);
  expect("b = 5; return 3 % b;","%d",3);
  expect("a = 2; return 4^a;","%d",16);
  expect("a = 4; return a^2;","%d",16);

  expect("a = 2; b= 3; return a + b;" , "%d",5);
  expect("a = 2; b =3; return a * b;" , "%d",6);
  expect("a = 2; b =4; return b / a;" , "%d",2);
  expect("a = 2; b =4; return b ^ a;" , "%d",16);
  expect("a = 3; b =5; return a % b;" , "%d",3);

  expect("a = 2; b =3; return 1+a*b;" , "%d",7);

  expect("a = \"Hello World\"; return a + \" \";","%s","Hello World ");
  expect("a = \"Hello \"; return \"World \"+a;","%s","World Hello ");
}

static void test_comparison() {
  /* Number comparison */
  expect("a = 9 ; return a > 9; " , "false");
  expect("a = 9 ; return 10 >a; " , "true" );
  expect("a = 10; return a >= 10;" , "true" );
  expect("a = 10; return 10 >= a;" , "true" );
  expect("a = 1 ; return a < 0 ;" , "false");
  expect("a = 1 ; return -1 < a; " , "true" );
  expect("a = 0 ; return a <= 0; ", "true");
  expect("a = 0 ; return 10 < a; ","false");
  expect("a = 10; return a == 10;","true");
  expect("a = 10; return 10== a; ","true");
  expect("a = 1 ; return a != 0; ","true");
  expect("a = 1 ; return 1 != a; ","false");
  /* String comparison */
  expect("a = \"Hello\"; return a > \"Hello\";","false");
  expect("a = \"Hello\"; return \"Hello\" > a;","false");
  expect("a = \"World\"; return a < \"World\";","false");
  expect("a = \"World\"; return \"World\" < a;","false");
  expect("a = \"World\"; return a>= \"World\";","true");
  expect("a = \"World\"; return \"World\" >= a;","true");
  expect("a = \"World\"; return a <= \"World\";","true");
  expect("a = \"World\"; return \"World\" <= a;","true");
  expect("a = \"World\"; return a == \"World\";","true");
  expect("a = \"World\"; return \"Hello\" == a;","false");
  expect("a = \"World\"; return a != \"You\";","true");
  expect("a = \"World\"; return \"World\"!=a;","false");
  /* Other comparable type */
  expect("a = true; return a > true;","false");
  expect("a = true; return true >a ;","false");
  expect("a = false;return a >= true;","false");
  expect("a = false;return false>=a ;","true");
  expect("a = true; return a < true; ","false");
  expect("a = true; return true <a ; ","false");
  expect("a = true; return a <= true;","true");
  expect("a = true; return true<=a  ;","true");
  expect("a = true; return a == false;","false");
  expect("a= false; return false ==a; ","true");
  expect("a = true; return a != true;","false");
  expect("a = true; return true !=a ;","false");
  expect("a = false;return a != true;","true");
  expect("a = false;return false !=a;","false");
  /* Null */
  expect("a = null ; b = (a != null); return b;","false");
  expect("var a = null ; return null != a;","false");
  expect("a = null ; return a == null;","true");
  expect("a = 1; return null == a;","false");
  /* VV comparison */
  expect("a = 10 ; b = 100; return a >= b;","false");
  expect("a = true;b = 500; return a <= b;","true");
  expect("a =false;b = true;return a > b ;","false");
  expect("a = false;b= true;return a < b ;","true");
  expect("a = 10 ; b = 100; return a ==b ;","false");
  expect("a = 10 ; b = 100; return a !=b ;","true");
  expect("a = \"abc\"; b= \"ABC\"; return a > b;","true");
  expect("a = \"abc\"; b= \"ABC\";return a < b;","false");
  expect("a = \"abc\"; b= \"ABC\";return a >= b;","true");
  expect("a = \"abc\"; b= \"ABC\";return a <= b;","false");
  expect("a = \"abc\"; b= \"abc\";return a == b;","true");
  expect("a = \"abc\"; b= \"ABC\";return a != b;","true");
  expect("a = null; b= null; return a == b;","true");
  expect("a = null; b= false;return a != b;","true");
}

static void test_logic() {
  expect("a = 10; return a && true;","true");
  expect("a = 10; return true && a;","true");
  expect("a = true; return a || false;","true");
  expect("a = false;return false || a;","false");
  /* Short circulet algorithm, though b is not defined, it should not
   * raise an error since the b's value will *never* be evaluated */
  expect("a = 100; return a || b;","true");
  expect("a = 0; return a && func();","false");
  /* Chain of logic */
  expect("a = 10; b = false ; c = 0 ; d = 100; return a || b || c || d;","true");
  expect("a = 10; b = true; c = false; d =1000; return a && b && d && 100 && c && e;","false");
  expect(" return true || c || d || e || f ||func() || gg() || false;","true");
  expect(" return true && 100 && 10000 && 0 && c && d && e &&f;","false");
}

static void test_branch() {
  expect("a = 10; if(a) return true; else return false;","true");
  expect("a = false; b = 10; if(a) b = 1000; else b = b + 1; return b;","%d",11);
  expect("var a = false; if(a) { var c = 100; return c + 100 *2; } else { var d = 10; return d *10; }","%d",100);
  expect(STRINGIFY(
        var a = 10;
        if(a) {
          var b = false;
          if(b) {
            var c = true;
            if(c) return 10;
          } else {
            var d = false;
            if(!d) {
              return 100;
            }
          }
        }
        return -10;
        ),"%d",100);
  expect(STRINGIFY(
        var a = 0;
        if(!a) {
          var b = 10;
          if(b == 0) {
            return 1;
          } else if(b == 2) {
            return 2;
          } else if(b == 10) {
            return 100;
          } else {
            return -100;
          }
        } else {
          return -200;
        }
        ),"%d",100);
  expect(STRINGIFY(
        if(true) {
          if(!false) {
            if(1) {
              if(!0) {
                if("Hello World") {
                  if(false) {
                    return "shit";
                  } else {
                    return 1000;
                  }
                }
              }
            }
          }
        }
        return -100;
        ),
      "%d",1000);
  expect(STRINGIFY(
        if(true)
          if(true)
            if(true)
              if(true)
                if(true)
                  if(true)
                    if(true)
                      if(null) return -100;
                      else {
                        if(1)
                          if(0) return 100;
                          else if(1) return 1000;
                          else return -100;
                      }
        return null;
        ),"%d",1000);
  expect(STRINGIFY(
        var a = 10;
        if(!a) {
          return 100;
        } else if(a == 2) {
          return 1000;
        } else if(a == 10) {
          var b = false + true;
          if(!b) {
            return -100;
          } else if(b == true) {
            return 99;
          } else {
            return 100;
          }
        } else {
          return 300;
        }
        return a * 1000;
      ),"%d",99);
}

static void test_loop() {
  expect(STRINGIFY(
        var a = [1,2,3,4,5];
        var cnt = 0;
        for( i , x in a ) {
          if(cnt != i ) return false;
          if( x != i + 1) return false;
          cnt = cnt + 1;
        }
        cnt = 0;
        for( i in a ) {
          if(cnt !=i) return false;
          cnt = cnt + 1;
        }
        return cnt;
        ),"%d",5);
  expect(STRINGIFY(
        var a = [1,2,3,4,5,6,7,8,9,0];
        var sum = 0;
        for( _ , val in a ) {
          sum = sum + val;
        }
        return sum;
        ),
      "%d",45);
  expect(STRINGIFY(
        var i = 0;
        var cnt = 0;
        for( x in loop(i+1,10,i+1) ) {
          if(x != cnt+1) return false;
          cnt = cnt + 1;
        }
        if(cnt != 9) return false;
        return true;
        ),"true");
  expect(STRINGIFY(
        var i =0;
        var sum=0;
        for( x in loop(1,10,1) ) {
          if(x % 2 ==0) {
            sum = sum + x;
          }
        }
        return sum;
        ),"%d",20);
  expect(STRINGIFY(
        var i = 0;
        var sum =0;
        for( x in loop(1,10,1) ) {
          var temp = 10;
          if( x > 5 ) break;
          sum = sum + temp + x;
        }
        var largest = 2;
        return sum * largest;
        ), "%d",130);
  expect(STRINGIFY(
        var i = 0;
        var sum = 0;
        for( x in loop(1,5,1) )
          for( y in loop(1,10,1) )
            sum = sum + y;
        return sum;
        ),"%d",180);
  expect(STRINGIFY(
        var i = 0;
        var sum = 0;
        for( x in loop(1,5,1) ) {
          for( y in loop(1,10,1) ) {
            if( y > 5 ) break;
            sum = sum + y;
          }
          sum = sum + 10;
        }
        return sum;
        ),"%d",100);
  expect(STRINGIFY(
        for( x , y in loop(1,10,1) ) {
          assert( x == y , "Failed!" );
        }
        return true;
        ),"true");
  expect(STRINGIFY(
        for( k , v in {"a":1,"b":2,"c":3} ) {
          if( k == "a" && v != 1 ) return false;
          if( k == "b" && v != 2 ) return false;
          if( k == "c" && v != 3 ) return false;
        }
        return true;
        ),"true");

  /* LOOP CONTROL */
  expect(STRINGIFY(
        var sum = 0;
         for( i in loop(1,10,1) ) {
           if(i %2 ==0) continue;
           sum = sum + i;
         }
         return sum == 1+3+5+7+9;
        ),"true");

  expect(STRINGIFY(
        var sum = 0;
        for( i in loop(1,10,1) ) {
          var local_sum = 0;
          for( j in loop(1,10,1) ) {
            var temp = 1;
            local_sum = local_sum * temp + j;
            var str = "Hello World";
            var t = true;
            var f = false;
            if( j >= 5 ) {
              var ff = true;
              var uu = false;
              break;
            }
          }
          sum = sum + local_sum + i;
        }
        return sum;
        ),"%d",180);

  expect(STRINGIFY(
        var sum = 0;
        for( i in loop(1,10,1) ) {
          if(i == 1) sum = sum + 1;
          else if(i == 3) continue;
          else if(i == 4) sum = sum + 1;
          else if(i == 5) continue;
          else if(i >= 8) break;
          else sum = sum + i;
        }
        return sum;
        ),"%d",17);
}

static void test_attr() {
  /* Reading */
  expect("a = [1,2,3,4]; return a[3];","%d",4);
  expect("a = [1]; return a[0];","%d",1);
  expect("a = [true,false,null];return a[2];","null");
  expect("a = [false,[1]];return a[1][0];","%d",1);
  expect("a = [[[[1]]],2,3]; return a[0][0][0][0];","%d",1);
  expect("a = [0]; b = 0; return a[b];","%d",0);
  expect(STRINGIFY(
        a = [[[[[1]]]],2,3];
        b = [[[[0]]]];
        return a[0][0][0][0][b[0][0][0][0]];),"%d",1);
  expect(STRINGIFY(
        a = [1,[2],[1,2,[3]],[1,2,3,[4]]];
        b = 0;
        c = 3;
        d = 3;
        return a[c][d][b];
        ),"%d",4);
  expect(STRINGIFY(
        a = "Hello World";
        return a[0] == "H";
        ),"true");
  expect(STRINGIFY(
        a = "Hello World";
        b = 4;
        return a[b] == "o";
        ),"true");
  expect(STRINGIFY(
        a = {"a":1,"b":2};
        return a["a"] == a.a && a.b == a["b"];
        ),"true");
  expect("return {\"a\":true,\"b\":false}.a;","true");
  expect("return {\"a\":true,\"b\":false}[\"a\"];","true");
  expect("var table = {\"Hello\":\"World\"};"
      "return table.Hello == \"World\";",
      "true");
  expect(" var list = [1,2,3,4,5]; return list[2] == 3;","true");
  expect(" var list = [1,2,3,4,5]; return list[true] == 2;","true");
  expect(" var list = [1,2,3,4,5]; index = 1; return list[index] == 2;","true");
  expect(" var list = [1,2,3,4,5]; var index = 1; return list[index];","%d",2);
  expect(" var map = {\"a\":\"b\",\"c\":\"d\"}; var key = \"a\"; return map[key];",
      "%s","b");
  expect(" var map = {\"g\":1}; key = \"g\"; return map[key];","%d",1);
  /* Writing */
  expect(" var map = {\"g\":1}; map[\"g\"] = 2; return map.g;","%d",2);
  expect(" var map = {}; map[\"UU\"] = 100; return map.UU;","%d",100);
  expect(" var list= [1,2,3,4]; list[1] = 10; return list[1];","%d",10);
  expect(" g = {}; g[\"UUVV\"] = 100; return g.UUVV;","%d",100);
  expect(" l = [1,2,34]; l[0] = 100; return l[0];","%d",100);

  expect(" var m = {}; m.a = true ; m.b = false ; return m.a && m.b;","false");
  expect(STRINGIFY(
        var m = {};
        m.append = null;
        m.user = { "bb" : 1 };
        return m.user.bb;
        ), "%d",1);
  expect(STRINGIFY(
        var m = {};
        m["User"] = {};
        m.User.Name = "John";
        m.User.Age = 100;
        m.User.Friend = null;
        return m.User.Name == "John" && m.User.Age == 100 && m.User.Friend == null;
        ),"true");

  expect(STRINGIFY(
        var m = {};
        var b = "Hello";
        var c = 123;
        m[b] = c;
        return m.Hello == c;
        ),"true");
}

static int fib(int a) {
  if(a == 0 || a == 1 || a == 2 ) return a;
  else return fib(a-1) + fib(a-2);
}

static void test_closure() {
  expect(STRINGIFY(
        var m = {
          "age" : function() { return 100; } ,
          "name": function() { return "John"; },
          "Friend": null
          };
        return m.age() == 100 &&
               m.name() == "John" &&
               m.Friend == null;
        ),"true");
  expect(STRINGIFY(
        var uu = 1;
        var zz = 2;
        var m = {
          "add" : function(a,b,c,d) {
            return a+b+c+d;
          },
          "user": function() {
            var sum = 0;
            for( _, v in [1,2,3,4,5,6,7,8,9,10] ) {
              sum = sum + v;
            }
            return sum;
          }
        };
        return m.add(1,2,3,4) == 1+2+3+4 &&
               m.user() == (1+10)*10/2;
     ),"true");
  expect(STRINGIFY(
        var uu = 1;
        var zz = 2;
        var cc = "True";
        var object = {
          "Name" : "John" ,
          "Garbage" : "You" ,
          "Perform" : function( a ) {
            return a ^ a ;
          },
          "Add" : function(a,b) {
            return a + b;
          },
          "Sub" : function(a,b) {
            return a - b;
          }
       };
       return object.Name + cc == "JohnTrue" &&
              object.Sub( object.Perform(uu) + object.Add(zz,uu) , zz ) == 2;
        ),"true");
  /* This test case is pretty slow , TODO:: Add tail call optimization */
  expect(STRINGIFY(
        var fib = function(a) {
          if(a == 0 || a == 1 || a == 2)
            return a;
          else return fib(a-1) + fib(a-2);
          };

          return fib(30);
        ),"%d",fib(30));
  expect(STRINGIFY(
        var uu = 10; // Upvalue
        var vv = 20; // Upvalue
        var fib = function(a) {
          return a + uu - vv;
        };
        return fib(10);
        ),"%d",0);

  expect(STRINGIFY(
        var uu = 1000;
        var fun = function( another ) {
          return another() + 1000;
        };
        return fun( function() { return 100; } ) == 1100;
        ),"true");
  expect(STRINGIFY(
        var sum = 0;
        var fun = function() {
          sum = sum + 1;
          return sum;
        };

        for( i in loop(1,10,1) ) {
          if( i != fun() ) return false;
        }
        return true;
       ),"true");
}

static void test_upval() {
  expect(STRINGIFY(
        var upvalue = 0;
        var fun = function() {
          return upvalue;
        };
        return fun() == upvalue;
        ),"true");
  expect(STRINGIFY(
        var upvalue = 0;
        var fun = function() {
          upvalue = upvalue + 1;
          return upvalue;
        };
        for( i in loop(1,10,1) ) {
          if(fun() != i) return false;
        }
        return true;
        ),"true");
  expect(STRINGIFY(
        var upvalue1 = 10;
        var foo = function() {
          var upvalue2 = 20;
          var bar = function() {
            return upvalue1 + upvalue2;
          };
          return bar;
        };
        return foo()();
        ),"%d",30);
  expect(STRINGIFY(
        var u1 = 1;
        var f1 = function() {
          var u2 = 2;
          var f2 = function() {
            var u3 = 3;
            var f3 = function() {
              var u4 = 4;
              var f4 = function() {
                var u5 = 5;
                return u1+u2+u3+u4+u5;
              };
              return f4();
            };
            return f3();
          };
          return f2();
        };
        return f1();
        ),"%d",1+2+3+4+5);
  expect(STRINGIFY(
        var u1 = 1;
        var f1 = function() {
          var u2 = u1;
          var f2 = function() {
            return u1 + u2;
          };
          return f2();
        };
        return f1();
        ),"%d",2);
  expect(STRINGIFY(
        var u1 = 1;
        var u2 = 2;
        var u3 = 3;
        var f1 = function() {
          var v1 = 1;
          var v2 = 2;
          var v3 = 3;
          var f2 = function() {
            var w1 = 1;
            var w2 = 2;
            var w3 = 3;
            var f3 = function() {
              return u1 + u2 + u3 +
                     v1 + v2 + v3 +
                     w1 + w2 + w3;
            };
            return f3;
          };
          return f2;
        };
        return f1()()();
        ),"%d",18);
  /* UPVALUE modification */
  expect(STRINGIFY(
        var u1 = 1;
        var u2 = 2;
        var f1 = function() {
          u1 = u1 + 1;
          u2 = u1 + u2;
          return u1+u2;
        };

        var v1 = f1();
        var v2 = f1();
        return v1 == 6 && v2 == 10;
        ),"true");
  expect(STRINGIFY(
        var v11 = 1;
        var v12 = 2;
        var f1 = function() {
          var v21 = 1;
          var v22 = 2;
          var f2 = function() {
            var v31 = 1;
            var v32 = 2;
            var f3 = function() {
              v11 = v11 + 1;
              v12 = v12 + 1;
              v21 = v21 + 1;
              v22 = v22 + 1;
              v31 = v31 + 1;
              v32 = v32 + 1;
              return v11 + v12 + v21 + v22 + v31 + v32;
            };
            return f3;
          };
          return f2;
        };
        var t1 = f1()()();
        var t2 = f1()()();
        var ff = f1()();
        var t3 = ff();
        var t4 = ff();
        return t1 == t2 && t3 == 15 && t4 == 21;
        ),"true");
  expect(STRINGIFY(
        var v1 = 0;
        var f = function() {
          v1 = true;
          return v1;
          };
        return f();
        ),"true");
  expect(STRINGIFY(
        var v1 = 0;
        var f = function() {
          v1 = false;
          return v1;
        };
        return f();
        ),"false");
  expect(STRINGIFY(
        var v1 = 0;
        var f = function() {
          v1 = null;
          return v1;
        };
        return f();
        ),"null");
}

static void test_locvar() {
  expect(STRINGIFY(
        var v = 10;
        var f = function() {
          var v = 20;
          return v;
        };
        return f();
        ),"%d",20);
  expect(STRINGIFY(
        var v = 10;
        if(true) {
          var v = 20;
          return v;
        }
        return v;
        ),"%d",20);
  expect(STRINGIFY(
        var v = 10;
        for( i in loop(1,10,1) ) {
          var v = 20;
          if(v != 20) return false;
        }
        return true;
        ),"true");
  expect(STRINGIFY(
        var v = false;
        v = true;
        return v;
        ),"true");
  expect(STRINGIFY(
        var v = true;
        v = false;
        return v;
        ),"false");
  expect(STRINGIFY(
        var v = 0;
        v = null;
        return v;
        ),"null");
  expect(STRINGIFY(
        var v = 10;
        v = 0;
        if(v != 0) return false;
        v = 1;
        if(v != 1) return false;
        v = 2;
        if(v != 2) return false;
        v = 3;
        if(v != 3) return false;
        v = 4;
        if(v != 4) return false;
        v = 5;
        if(v != 5) return false;
        v = -1;
        if(v != -1) return false;
        v = -2;
        if(v != -2) return false;
        v = -3;
        if(v != -3) return false;
        v = -4;
        if(v != -4) return false;
        v = -5;
        if(v != -5) return false;
        return true;
        ),"true");
  expect(STRINGIFY(
        var v1 = 0;
        if(v1 !=0 ) return false;
        var v2 = 1;
        if(v2 != 1) return false;
        var v3 = 2;
        if(v3 != 2) return false;
        var v4 = 3;
        if(v4 != 3) return false;
        var v5 = 4;
        if(v5 != 4) return false;
        var v6 = 5;
        if(v6 != 5) return false;
        var v7 = -1;
        if(v7 != -1) return false;
        var v8 = -2;
        if(v8 != -2) return false;
        var v9 = -3;
        if(v9 != -3) return false;
        var v10 = -4;
        if(v10 != -4) return false;
        var v11 = -5;
        if(v11 != -5) return false;
        return true;
        ),"true");
}

static void test_icall() {
  expect(STRINGIFY(
        assert( typeof("Hello World") == "string" , "string!");
        assert( typeof(123) == "number" , "number!");
        assert( typeof(null) == "null" , "null!");
        assert( typeof(true) == "true", "true!");
        assert( typeof(false)=="false","false!");
        assert( typeof([]) == "list","list!");
        assert( typeof({}) == "map","map!");
        assert( typeof(function(){}) == "closure" , "closure!");
        assert( typeof(loop(1,2,1)) == "loop" , "loop!");
        return true;
        ),"true");
  expect(STRINGIFY(
        assert( is_boolean(true) && is_boolean(false) , "boolean");
        return true; ),"true");
  expect(STRINGIFY(
        assert( is_string("Hello") && is_string("") && !is_string(123) ,"string" );
        return true; ),"true");
  expect(STRINGIFY(
        assert( is_number(123) && is_number(-1) && is_number(0) , "number");
        assert( !is_number(true) && !is_number(false) && !is_number(null), "number" );
        return true; ), "true");
  expect(STRINGIFY(
        assert( is_null(null) , "null" );
        assert(!is_null(false), "null" );
        return true; ),"true");
  expect(STRINGIFY(
        assert( is_list([]) && is_list([1,23,4,5,[[[]]]]) , "list");
        assert( !is_list({}) && !is_list({"a":"b","c":1}) , "list");
        return true;
        ),"true");
  expect(STRINGIFY(
        assert( is_map({}) && is_map({"a":"b","c":"d"}), "map");
        assert( !is_map(null) , "map");
        return true;
        ),"true");
  expect(STRINGIFY(
        assert( to_string(123) == "123" &&
                to_string("H") == "H" &&
                to_string("") == "" &&
                to_string(true) == "1" &&
                to_string(false)== "0", "to_string" );
        return true;),"true");
  expect(STRINGIFY(
        assert( to_boolean(123) == true &&
                to_boolean("H") == true &&
                to_boolean("") == true &&
                to_boolean(true) == true &&
                to_boolean(false) == false &&
                to_boolean(null) == false &&
                to_boolean([]) &&
                to_boolean({}) &&
                to_boolean(function(){}) , "to_boolean");
        return true;
        ),"true");
  expect(STRINGIFY(
        assert( to_number("123") == 123 &&
                to_number("0") == 0 &&
                to_number("-1") == -1 &&
                to_number(123) == 123 &&
                to_number(0) == 0 &&
                to_number(-1) == -1 , "to_number" );
        return true;
        ),"true");
  expect(STRINGIFY(
        assert( size([]) == 0 && size({}) == 0 && size("Hello") == 5 &&
                size([1,2,3,4,5,6]) == 6 && size({"a":1}) == 1 , "size");
        return true;
        ),"true");
  expect(STRINGIFY(
        var lib = import("test.sp");
        return lib.calc();
        ),"%d",45);
  expect(STRINGIFY(
        var lib = import("numeric.sp");
        return lib.sum([1,2,3,4,5,6,7,8,9,10]) + lib.calc();
        ),"%d",(1+10)*5+45);
  expect(STRINGIFY(
        var lib = run_string("var m = {}; m.calc= function(list) { var sum = 0; for( _,x in list ) { sum = sum + x; } return sum; }; return m;");
        return lib.calc([1,2,3,4,5]);),"%d",1+2+3+4+5);
}

static void test_call() {
  expect(STRINGIFY(
        var f = function() { return 10; };
        assert(f() == 10,"call0");
        f = function(a) { return a + 10; };
        assert(f(10) == 20,"call1");
        f = function(a,b) { return a + b + 10; };
        assert(f(10,10) == 30,"call2");
        f = function(a,b,c) { return a + b + c + 10; };
        assert(f(10,10,10) == 40,"call3");
        f = function(a,b,c,d) { return a + b + c + d; };
        assert(f(10,10,10,10) == 40,"call4");
        f = function(a,b,c,d,e,f,g) { return a + b + c + d + e +f +g; };
        assert(f(1,1,1,1,1,1,1) == 7,"callN");
        return true;
        ),"true");
}

static void test_gvar() {
  expect(STRINGIFY(
        var f = [];
        assert(list.size(f) == 0,"list.size");
        list.push(f,1);
        assert(f[0] == 1,"list.push");
        assert(list.size(f) == 1,"list.size");
        list.push(f,2); list.push(f,3);
        assert(f[2] == 3,"list.push");
        assert(list.size(f) == 3,"list.size");
        list.extend(f,[10,11]);
        assert(list.size(f) == 5,"list.extend");
        assert(f[4] == 11,"list.extend");
        list.pop(f);list.pop(f);list.pop(f);
        assert(list.size(f)==2,"list.pop");
        list.resize(f,10);
        assert(f[9] == null,"list.resize");
        assert(list.empty(f) == false,"list.empty");
        list.clear(f);
        assert(list.empty(f),"list.empty");
        list.push(f,1); list.push(f,2);

        f = [1,2,3,4,5];
        var new_list = list.slice(f,1,4); // [2,3,4]
        assert(new_list[0] == 2 &&
               new_list[1] == 3 &&
               new_list[2] == 4 , "list.slice");
        return true;
        ),"true");
  expect(STRINGIFY(
        var f = list.size;
        var l = [1,2,3,4,5,6];
        assert(f(l) == 6,"list.size");
        f = list.pop;
        f(l);
        assert(l[4] == 5,"list.pop");
        f = list.push;
        f(l,6);
        assert(l[5] == 6,"list.push");
        f = list.extend;
        f(l,[1,2,3,4]);
        assert(list.size(l) == 10,"list.size");
        f = list.resize;
        list.clear(l);
        f(l,10);
        assert(l[9] == null,"list.resize");
        f = list.clear;
        f(l);
        assert(list.size(l) == 0,"list.size");
        return true;
        ),"true");
  expect(STRINGIFY(
        var m = {};
        assert(map.size(m) == 0,"map.size");
        assert(map.empty(m),"map.emty");
        m["UU"] = "VV";
        assert(map.exist(m,"UU"),"map.exist");
        assert(map.exist(m,"VV")==false,"map.exist");
        assert(map.pop(m,"UU"),"map.pop");
        assert(map.size(m) == 0,"map.size");
        assert(map.empty(m),"map.empty");
        assert(map.exist(m,"UU")==false,"map.exist");
        m["XX"] = 1;
        m["UU"] = 2;
        assert(!map.empty(m),"map.empty");
        map.clear(m);
        assert(map.empty(m),"map.empty");
        return true;
        ),"true");
  expect(STRINGIFY(
        var s = "Hello";
        assert(string.size(s) == size(s),"string.size");
        assert(string.empty(s) == false,"string.empty");
        assert(string.slice(s,1,3) == "el","string.slice");
        assert(string.size("") == 0,"string.size");
        assert(string.empty(""),"string.empty");
        return true;
        ),"true");
}

int main() {
  test_gvar();
  test_basic_arithmatic();
  test_icall();
  test_basic_comparison();
  test_basic_logic();
  test_arithmatic();
  test_comparison();
  test_logic();
  test_branch();
  test_loop();
  test_attr();
  test_closure();
  test_upval();
  test_locvar();
  test_call();
  printf("\n%d tests has been performed!\n",COUNT);
  return 0;
}
