#ifndef OBJECT_H_
#define OBJECT_H_
#include <stddef.h>
#include <assert.h>
#include <errno.h>

#include "../conf.h"
#include "../util.h"
#include "bc.h"

struct Sparrow;
struct ObjModule;
struct Runtime;
struct ObjIterator;
struct ObjUdata;

/* Garbage collector header for each value.
 * The gc header is just a pointer to next.
 * Currently we just use a stop the world GC, in the future if we want
 * to fancy stuff , the pointer can be overlapped with 2 bits states */
enum {
  GC_UNMARKED = 0,
  GC_MARKED
};

/* Threshold for whether a string is large or not */
#ifndef LARGE_STRING_SIZE
#define LARGE_STRING_SIZE 512
#endif

/* Initial string pool size for one Sparrow Thread */
#ifndef STRING_POOL_SIZE
#define STRING_POOL_SIZE 1024
#endif

uint32_t StringHash( const char* , size_t len );

uint32_t LargeStringHash( const char* , size_t len );

static SPARROW_INLINE
uint32_t HashString( const char* str , size_t len ) {
  if(len < LARGE_STRING_SIZE)
    return StringHash(str,len);
  else
    return LargeStringHash(str,len);
}

/* This is obviously not cache-friendly GC header */
struct GCRef {
  struct GCRef* next;
  uint32_t gc_state : 4;
  uint32_t gtype : 28;
};

/* Put this as the first element in each structure to
 * allow us to do correct pointer casting */
#define DEFINE_GCOBJECT struct GCRef gc

/* Value slot is based on NAN-tagging scheme and heavily influenced
 * by LuaJIT. To understand what is Nan-tagging, please google it.
 */

#define VALUE_NUMBER (0xfff8000000000000) /* no need to set number */
#define VALUE_TRUE (0xfff9100000000000)
#define VALUE_FALSE (0xfff9200000000000)
#define VALUE_NULL (0xfff9300000000000)
#define VALUE_GCOBJECT (0xfffa000000000000)

/* pointer related stuff */
#define VALUE_PTR_MASK (0xffff000000000000)

/* This fields are stored inside of GCRef header . The NAN-tagging inside
 * of Value object is only used to indicate whether it is a *GC* object.
 * The actual type information is *stored* inside of the GC header of each
 * GCed objects */
enum {
  VALUE_LIST,
  VALUE_MAP,
  VALUE_PROTO,
  VALUE_CLOSURE,
  VALUE_METHOD,
  VALUE_UDATA,
  VALUE_STRING,
  VALUE_ITERATOR,
  VALUE_MODULE,
  VALUE_COMPONENT,
  VALUE_LOOP,/* specialized loop objects for VM to execut the common loop
              * faster */
  VALUE_LOOP_ITERATOR
};

typedef union _Value {
  double num;
  uint64_t ipart;
  intptr_t ptr;
} Value;

typedef void(*CDataDestroyFunction)(void*);

/* Msic functions */
typedef int (*MetaGetI) ( struct Sparrow* , Value ,
    enum IntrinsicAttribute ,Value* );
typedef int (*MetaGet ) ( struct Sparrow* , Value , Value , Value* );
typedef int (*MetaSet ) ( struct Sparrow* , Value , Value , Value  );
typedef int (*MetaSetI) ( struct Sparrow* , Value ,
    enum IntrinsicAttribute ,Value );
typedef int (*MetaHash) ( struct Sparrow* , Value , int32_t* );
typedef int (*MetaKey ) ( struct Sparrow* , Value , Value* );
typedef int (*MetaExist)( struct Sparrow* , Value , int* );
typedef int (*MetaSize) ( struct Sparrow* , Value , size_t*  );
typedef int (*MetaIter) ( struct Sparrow* , Value , struct ObjIterator* );
typedef int (*MetaPrint)( struct Sparrow* , Value , struct StrBuf* );

/* Conversion */
typedef int (*MetaToStr)( struct Sparrow* , Value , Value* );

/* To boolean SHOULD NOT FAIL, it can only return OK or pass. In current
 * version the FAIL returned by to_boolean will be just ignored */
typedef int (*MetaToBoolean)( struct Sparrow* , Value , Value* );
typedef int (*MetaToNumber)( struct Sparrow* , Value, Value* );

#define METAOPS_LIST(__) \
  __(MetaGetI,geti,"__geti") \
  __(MetaGet , get,"__get" ) \
  __(MetaSet , set,"__set" ) \
  __(MetaSetI,seti,"__seti") \
  __(MetaHash,hash,"__hash") \
  __(MetaKey , key,"__key" ) \
  __(MetaExist,exist,"__exist") \
  __(MetaSize,size,"__size") \
  __(MetaIter,iter,"__iter") \
  __(MetaPrint,print,"__print") \
  __(MetaToStr,to_str,"__to_str") \
  __(MetaToBoolean,to_boolean,"__to_bolean") \
  __(MetaToNumber,to_number,"__to_number")

/* MetaOps is the key to extend our internal data model , they are kind
 * of like the prototype in javascript or metatable in lua except they
 * are not table at all. They are just simpler way for you to extend the
 * runtime behavior of certain data types.
 * Only 2 types are supporting such mechanism. They are map which always
 * created in the script side ; and udata which is always created in the
 * host side. */

struct MetaOps {
/* Define a meta field */
#define DEFINE_METAFIELD(A,B,C) A B; Value hook_##B;
  METAOPS_LIST(DEFINE_METAFIELD)
#undef DEFINE_METAFIELD /* DEFINE_METAFIELD */
};

/* MetaOps name */
#define __(A,B,C) extern const char* MetaOpsName_##B;
METAOPS_LIST(__)
#undef __ /* __ */

#define METAOPS_NAME(NAME) MetaOpsName_##NAME

/* Macro for invoking the meta operations in an object */
#define INVOKE_METAOPS(TNAME,RT,OPS,NAME,RET,SP,OBJ,...) \
  do { \
    if((OPS)) { \
      if( !Vis_null(&(OPS)->hook_##NAME) ) { \
        (RET) = MetaOps_##NAME((OPS)->hook_##NAME,SP,OBJ,__VA_ARGS__); \
      } else { \
        if( (OPS)->NAME ) { \
          (RET) = (OPS)->NAME(SP,OBJ,__VA_ARGS__); \
        } else { \
          RuntimeError(RT,PERR_METAOPS_ERROR,TNAME,METAOPS_NAME(NAME)); \
          (RET) = -1; \
        } \
      } \
    } else { \
      abort(); \
      RuntimeError(RT,PERR_METAOPS_ERROR,TNAME,METAOPS_NAME(NAME)); \
      (RET) = -1; \
    } \
  } while(0)

/* The following objects are all GC managed objects and they forms
 * the core part of how our small language runs */

struct ObjStr {
  DEFINE_GCOBJECT; /* GC reference */
  const char* str; /* It just means a byte array now */
  size_t len;
  uint32_t hash;   /* hash of string object */
  uint32_t next : 31; /* next point for collision */
  uint32_t more : 1 ;
};

typedef int (*CMethod)( struct Sparrow* , Value , Value* );

struct ObjMethod {
  DEFINE_GCOBJECT; /* GC object */
  CMethod method;
  Value object;
  struct ObjStr* name; /* Name of this method */
};

struct ObjList {
  DEFINE_GCOBJECT; /* GC object */
  size_t size;
  size_t cap;
  Value* arr;
};

struct ObjMapEntry {
  struct ObjStr* key;
  Value value;
  uint32_t fhash;
  uint32_t next: 29;
  uint32_t more : 1; /* more pending chain */
  uint32_t del : 1;
  uint32_t used: 1;
};

struct ObjMap {
  DEFINE_GCOBJECT; /* GC object */
  size_t scnt; /* slot count */
  size_t size;
  size_t cap;
  struct ObjMapEntry* entry; /* hash entry */
  struct MetaOps* mops; /* meta Operations */
};

typedef void (*UdataGCMarkFunction) ( struct ObjUdata* );

typedef int (*UdataCall) ( struct Sparrow* , struct ObjUdata* , Value* );
struct ObjUdata {
  DEFINE_GCOBJECT; /* GC object */
  struct CStr name; /* Name of the Udata */
  void* udata;
  CDataDestroyFunction destroy;
  UdataGCMarkFunction mark;
  UdataCall call;
  /* metaops */
  struct MetaOps* mops;
};

struct ObjIterator;

typedef int (*IterHasNextFunction)( struct Sparrow* ,
    struct ObjIterator* );
typedef void (*IterDerefFunction) ( struct Sparrow* ,
    struct ObjIterator* , Value* key , Value* value );
typedef void (*IterMoveFunction) ( struct Sparrow* ,
    struct ObjIterator* );

struct ObjIterator {
  DEFINE_GCOBJECT;
  IterHasNextFunction has_next;
  IterDerefFunction deref;
  IterMoveFunction move;
  CDataDestroyFunction destroy;
  Value obj;
  union {
    void* ptr;
    int index;
  } u;
};

/* UpValue index */
#define UPVALUE_INDEX_EMBED 0
#define UPVALUE_INDEX_DETACH 1

struct UpValueIndex {
  uint32_t idx : 31;
  uint32_t state: 1;
};

/* Represented a compiled closure */
struct ObjProto {
  DEFINE_GCOBJECT; /* GC object */
  struct CodeBuffer code_buf; /* code buffer */
  /* Number table */
  double* num_arr;
  size_t num_size;
  size_t num_cap;
  /* String table */
  struct ObjStr** str_arr;
  size_t str_size;
  size_t str_cap;
  /* Upvalue */
  struct UpValueIndex* uv_arr;
  size_t uv_cap;
  size_t uv_size;
  /* Closure index */
  struct ObjModule* module;
  int cls_idx;
  /* The following is for debugging purpose */
  struct CStr proto; /* Prototype */
  size_t narg; /* Argument size */
  size_t start;/* Start of source */
  size_t end;  /* End of the source */
};

struct ObjClosure {
  DEFINE_GCOBJECT;
  struct ObjProto* proto; /* Protocol for this closure */
  Value* upval;
};

/* Each file compiles to a module , a module will
 * hold all meta information about one source file */
struct ObjModule {
  DEFINE_GCOBJECT;
  /* linked list of ObjModule, used to do caching */
  struct ObjModule* mod_next;
  struct ObjModule* mod_prev;

  struct ObjProto** cls_arr; /* all closures in certain modules */
  size_t cls_cap;
  size_t cls_size;
  struct CStr source; /* source code */
  struct CStr source_path; /* source code path */
};

#define ObjModuleGetEntry(MOD) ((MOD)->cls_arr[0])

/* Component is a runtime object corresponding to a module. */
struct ObjComponent {
  DEFINE_GCOBJECT;
  struct ObjModule* module; /* Module */
  struct ObjMap* env;       /* Environment */
};

/* Loop object . Used to represent common loop */
struct ObjLoop {
  DEFINE_GCOBJECT;
  int start;
  int end;
  int step;
};

struct ObjLoopIterator {
  DEFINE_GCOBJECT;
  int index;
  /* Duplicated from ObjLoop, avoid cache miss */
  int end;
  int step;
  struct ObjLoop* loop;
};

/* factory function for each GC type. The XXXNoGC version means this
 * function *wont* trigger GC routine internally. Used it on specific
 * place when you know what you are doing */
struct ObjMethod* ObjNewMethodNoGC( struct Sparrow* , CMethod method ,
    Value object , struct ObjStr* name );
struct ObjMethod* ObjNewMethod( struct Sparrow* , CMethod method ,
    Value object , struct ObjStr* name );

struct ObjStr* ObjNewStrNoGC( struct Sparrow* , const char*  , size_t len );
struct ObjStr* ObjNewStr( struct Sparrow* , const char* , size_t len );

struct ObjStr* ObjNewStrFromChar( struct Sparrow* , char  );
struct ObjStr* ObjNewStrFromCharNoGC( struct Sparrow* , char  );

struct ObjList* ObjNewListNoGC( struct Sparrow* , size_t cap );
struct ObjList* ObjNewList( struct Sparrow* , size_t cap );

struct ObjMap* ObjNewMapNoGC( struct Sparrow* , size_t cap );
struct ObjMap* ObjNewMap( struct Sparrow* , size_t cap );

struct ObjUdata* ObjNewUdataNoGC( struct Sparrow* ,
    const char* name,
    void* ,
    UdataGCMarkFunction mark_func,
    CDataDestroyFunction destroy_func,
    UdataCall call_func);

struct ObjUdata* ObjNewUdata( struct Sparrow* ,
    const char* name,
    void* ,
    UdataGCMarkFunction mark_func,
    CDataDestroyFunction destroy_func,
    UdataCall call_func);

struct ObjProto* ObjNewProtoNoGC( struct Sparrow* , struct ObjModule* );
struct ObjProto* ObjNewProto( struct Sparrow* , struct ObjModule* );

struct ObjClosure* ObjNewClosureNoGC( struct Sparrow* ,
    struct ObjProto* );
struct ObjClosure* ObjNewClosure( struct Sparrow* ,
    struct ObjProto* );

struct ObjIterator* ObjNewIteratorNoGC( struct Sparrow* );
struct ObjIterator* ObjNewIterator( struct Sparrow* );

struct ObjModule* ObjNewModule( struct Sparrow* ,
    const char* fpath , const char* source );
struct ObjModule* ObjNewModuleNoGC( struct Sparrow* ,
    const char* fpath , const char* source );

struct ObjModule* ObjFindModule( struct Sparrow* ,
    const char* fpath );

struct ObjComponent* ObjNewComponent( struct Sparrow* ,
    struct ObjModule* , struct ObjMap* );

struct ObjComponent* ObjNewComponentNoGC( struct Sparrow* ,
    struct ObjModule* , struct ObjMap* );

struct ObjLoop* ObjNewLoop( struct Sparrow* ,
    int start , int end , int step );

struct ObjLoop* ObjNewLoopNoGC( struct Sparrow* ,
    int start , int end , int step );

struct ObjLoopIterator* ObjNewLoopIterator( struct Sparrow* ,
    struct ObjLoop* );

struct ObjLoopIterator* ObjNewLoopIteratorNoGC( struct Sparrow* ,
    struct ObjLoop* );

static SPARROW_INLINE
void ObjDestroyModule( struct ObjModule* mod ) {
  ListRemove(mod,mod);
  free(mod->cls_arr);
  CStrDestroy(&mod->source);
  CStrDestroy(&mod->source_path);
}

/* Debug purpose */
void ObjDumpModule( struct ObjModule* , FILE* , const char* );

/* Used in parser */
int ConstAddNumber( struct ObjProto* oc , double num );
int ConstAddString( struct ObjProto* oc , struct ObjStr* );

/* Intrinsic function call prototype , must match prototype defined in
 * builtin.h/c file */
typedef void (*IntrinsicCall)( struct Runtime* , Value* , int* );

/* When user tries to update intrinsic call inside of global table,
 * also modifying the intrinsic call table's pointer as well */
struct GlobalEnv {
  struct ObjMap env;
  IntrinsicCall icall[ SIZE_OF_IFUNC ];
};

/* Sparrow */
struct Sparrow {
  struct Runtime* runtime; /* If non null means running */
  size_t max_stacksize;    /* Maximum allowed stack size */
  size_t max_funccall;     /* Maximum allowed function call */

  struct GCRef* gc_start; /* Start of managed objects */
  size_t gc_sz; /* Size of the GC objects list */

  /* GC tune parameters */
  size_t gc_active;   /* Last round of GC's active count */
  size_t gc_inactive; /* Last round of GC's inactive count */
  size_t gc_prevsz;   /* Previous GC size */
  size_t gc_generation; /* Generation count of GC */
  size_t gc_threshold ; /* Threshold of GC */
  double gc_ratio ;     /* GC triggering ratio */
  size_t gc_ps_threshold;  /* Cached value for gc_ratio * gc_prevsz */
  double gc_penalty_ratio; /* GC penalty ratio, when gc_inactive/gc_prevsz
                            * is less than this value, a penalty will trigger
                            * which avoid too much frequent GC collection. */
  size_t gc_adjust_threshold;/* threshold after applied penalty if we have
                              * so. This is also a cached value so should update
                              * it accordinly */
  size_t gc_penalty_times;

  /* Parsed file module */
  struct ObjModule mod_list;

  /* Global string pool */
  struct ObjStr** str_arr;
  size_t str_size;
  size_t str_cap;

  /* Global envrionment */
  struct GlobalEnv global_env;

  /* Resource that is global */
  /* All the intrinsic function(builtin)'s name in static ObjStr
   * structure. They don't participate in GC at all. A little bit
   * optimization from traditionall large root sets */
#define __(A,B,C) struct ObjStr* BuiltinFuncName_##B;
  INTRINSIC_FUNCTION(__)
#undef __ /* __ */

  /* All the intrinsic attribute's name in static ObjStr
   * structure. They don't participate in GC at all. */
#define __(A,C) struct ObjStr* IAttrName_##A;
  INTRINSIC_ATTRIBUTE(__)
#undef __ /* __ */
};

#define IFUNC_NAME(SP,NAME) (((SP)->BuiltinFuncName_##NAME))
#define IATTR_NAME(SP,NAME) ((SP)->IAttrName_##NAME)

/* Function for configuring GC trigger formula */
static SPARROW_INLINE
void SparrowGCConfig( struct Sparrow* sparrow, size_t threshold ,
    double ratio, double penalty_ratio ) {
  /* Rules out insane value */
  if(ratio < 0 || ratio > 1.0f ||
     penalty_ratio < 0 || penalty_ratio > 1.0f ||
     threshold ==0) return;

  if(ratio) {
    sparrow->gc_ratio = ratio;
    sparrow->gc_ps_threshold = sparrow->gc_prevsz * ratio;
  }
  if(threshold) {
    int64_t diff;
    diff = (int64_t)threshold - (int64_t)(sparrow->gc_threshold);
    sparrow->gc_threshold = threshold;
    sparrow->gc_adjust_threshold =
      (int64_t)(sparrow->gc_adjust_threshold) + diff;

    if(penalty_ratio) {
      double pr = sparrow->gc_inactive/sparrow->gc_prevsz;
      assert(pr<=1.0f);
      if(pr <penalty_ratio) {
        sparrow->gc_adjust_threshold =
          (2.0f - pr)*sparrow->gc_adjust_threshold;
      }
    }
  } else if(penalty_ratio) {
    double pr = sparrow->gc_inactive/sparrow->gc_prevsz;
    assert(pr<=1.0f);
    if(pr <penalty_ratio) {
      sparrow->gc_adjust_threshold =
        (2.0f - pr)*sparrow->gc_adjust_threshold;
    }
  }
}

void SparrowInit( struct Sparrow* sth );

/* Only used when doing parsing , user don't need to remove
 * sparrow_thread since it is removed automatically */
void SparrowDestroy( struct Sparrow* );

/* Used to get intrinsic attribute name */
struct ObjStr* IAttrGetObjStr( struct Sparrow* , enum IntrinsicAttribute );

const char* ValueGetTypeString( Value );

#define obj2gc(X) ((struct GCRef*)(X))
#define gc2obj(X,T) ((T*)(X))

/* internal helper macro */
#define _Vset_type(V,type) ((V)->ipart = (type))
#define _iptr(PTR) ((intptr_t)(PTR) & VALUE_PTR_MASK)
#define _gptr(PTR) ((intptr_t)(PTR) & (~VALUE_PTR_MASK))
#define _check_ptr(PTR) assert( _iptr(PTR) == 0 )
#define _set_gctype(PTR,TYPE) (obj2gc(PTR)->gtype = (TYPE))
#define _get_gctype(PTR) (obj2gc(PTR)->gtype)
/* Used to be macro bu causing tons of problem */
static SPARROW_INLINE
void _Vset_ptr( Value* value , void* ptr , int type ) {
  _check_ptr(ptr);
  value->ptr = ((intptr_t)(ptr)) | VALUE_GCOBJECT;
  _set_gctype(ptr,type);
}

/* type test */
#define Vis_number(V) (((V)->ipart < VALUE_NUMBER))
#define Vis_null(V) (((V)->ipart) == VALUE_NULL)
#define Vis_false(V)(((V)->ipart)== VALUE_FALSE)
#define Vis_true(V) (((V)->ipart) == VALUE_TRUE)
#define Vis_boolean(V) (Vis_true(V) || Vis_false(V))
#define Vis_gcobject(V) ((((V)->ipart) & VALUE_GCOBJECT) == VALUE_GCOBJECT)

#define _DEFINE(type,TYPE) \
  static int SPARROW_INLINE Vis_##type( Value* v ) { \
    if(Vis_gcobject(v)) { \
      return _get_gctype(_gptr(v->ptr)) == VALUE_##TYPE; \
    } else { \
      return 0; \
    } \
  }

_DEFINE(str,STRING)
_DEFINE(list,LIST)
_DEFINE(map,MAP)
_DEFINE(method,METHOD)
_DEFINE(closure,CLOSURE)
_DEFINE(proto,PROTO)
_DEFINE(udata,UDATA)
_DEFINE(iterator,ITERATOR)
_DEFINE(module,MODULE)
_DEFINE(component,COMPONENT)
_DEFINE(loop,LOOP)
_DEFINE(loop_iterator,LOOP_ITERATOR)

#undef _DEFINE

/* setters */
#define Vset_number(V,NUM) ((V)->num = (NUM))
#define Vset_null(V) _Vset_type(V,VALUE_NULL)
#define Vset_true(V) _Vset_type(V,VALUE_TRUE)
#define Vset_false(V) _Vset_type(V,VALUE_FALSE)
#define Vset_boolean(V,B) _Vset_type(V,(B) ? VALUE_TRUE : VALUE_FALSE)
#define Vset_str(V,STR) _Vset_ptr(V,STR,VALUE_STRING)
#define Vset_list(V,L) _Vset_ptr(V,L,VALUE_LIST)
#define Vset_map(V,M) _Vset_ptr(V,M,VALUE_MAP)
#define Vset_closure(V,CLO) _Vset_ptr(V,CLO,VALUE_CLOSURE)
#define Vset_proto(V,PRO) _Vset_ptr(V,PRO,VALUE_PROTO)
#define Vset_method(V,M) _Vset_ptr(V,M,VALUE_METHOD)
#define Vset_udata(V,UD) _Vset_ptr(V,UD,VALUE_UDATA)
#define Vset_iterator(V,ITR) _Vset_ptr(V,ITR,VALUE_ITERATOR)
#define Vset_module(V,MOD) _Vset_ptr(V,MOD,VALUE_MODULE)
#define Vset_component(V,COMP) _Vset_ptr(V,COMP,VALUE_COMPONENT)
#define Vset_loop(V,LOOP) _Vset_ptr(V,LOOP,VALUE_LOOP)
#define Vset_loop_iterator(V,LITR) _Vset_ptr(V,LITR,VALUE_LOOP_ITERATOR)

/* getters */
static SPARROW_INLINE double Vget_number( Value* v ) {
  assert(Vis_number(v));
  return v->num;
}

static SPARROW_INLINE int Vget_boolean( Value* v ) {
  assert(Vis_true(v) || Vis_false(v));
  return Vis_true(v);
}

static SPARROW_INLINE struct GCRef* Vget_gcobject( Value* v ) {
  assert(Vis_gcobject(v));
  return (struct GCRef*)(_gptr(v->ptr));
}

#define _DEFINE(TYPE,NAME,TAG) \
  static SPARROW_INLINE struct TYPE* Vget_##NAME( Value* v ) { \
    assert(Vis_##NAME(v)); \
    return gc2obj((struct GCRef*)(_gptr(v->ptr)),struct TYPE); \
  }

_DEFINE(ObjStr,str,VALUE_STRING)
_DEFINE(ObjList,list,VALUE_LIST)
_DEFINE(ObjMap,map,VALUE_MAP)
_DEFINE(ObjMethod,method,VALUE_METHOD)
_DEFINE(ObjProto,proto,VALUE_PROTO)
_DEFINE(ObjClosure,closure,VALUE_CLOSURE)
_DEFINE(ObjUdata,udata,VALUE_UDATA)
_DEFINE(ObjIterator,iterator,VALUE_ITERATOR)
_DEFINE(ObjModule,module,VALUE_MODULE)
_DEFINE(ObjComponent,component,VALUE_COMPONENT)
_DEFINE(ObjLoop,loop,VALUE_LOOP)
_DEFINE(ObjLoopIterator,loop_iterator,VALUE_LOOP_ITERATOR)

#undef _DEFINE

/* String APIs */
static SPARROW_INLINE
int ObjStrEqual( const struct ObjStr* left , const struct ObjStr* right ) {
  return (left == right ||
         (left->hash == right->hash &&
          left->len == right->len &&
          memcmp(left->str,right->str,left->len) == 0));
}

static SPARROW_INLINE
int ObjStrCmp( const struct ObjStr* left , const struct ObjStr* right ) {
  if( left == right )
    return 0;
  else {
    /* Cannot use strcmp or strncmp since the string
     * may not be null terminated. Just use the raw
     * byte comparison here */
    if(left->len !=  right->len) {
      size_t l = MIN(left->len,right->len);
      int ret = memcmp(left->str,right->str,l);
      if(!ret) {
        if(l == left->len) return -1;
        else return 1;
      } else return ret;
    } else {
      return memcmp(left->str,right->str,left->len);
    }
  }
}

static SPARROW_INLINE
int ObjStrCmpCStr( const struct ObjStr* left ,
    const struct CStr* right ) {
  struct ObjStr str;
  str.str = right->str;
  str.len = right->len;
  return ObjStrCmp(left,&str);
}

static SPARROW_INLINE
int ObjStrCmpStr( const struct ObjStr* left ,
    const char* right ) {
  const size_t l = strlen(right);
  struct CStr str = { right , l };
  return ObjStrCmpCStr(left,&str);
}

static SPARROW_INLINE
struct ObjStr* ObjStrCatNoGC( struct Sparrow* sth ,
    const struct ObjStr* left ,
    const struct ObjStr* right ) {
  char* buf = malloc(left->len + right->len+1);
  struct ObjStr* ret;
  memcpy(buf,left->str,left->len);
  memcpy(buf+left->len,right->str,right->len);
  ret = ObjNewStrNoGC(sth,buf,left->len+right->len);
  free(buf);
  return ret;
}

struct ObjStr* ObjStrCat( struct Sparrow* sth ,
    const struct ObjStr* left ,
    const struct ObjStr* right );

#define StrBufToObjStrNoGC(STH,SBUF) \
  ObjNewStrNoGC((STH),(SBUF)->buf,(SBUF)->size)

#define StrBufToObjStr(STH,SBUF) \
  ObjNewStr((STH),(SBUF)->buf,(SBUF)->size)

/* Polymorphism Value related APIs */
static SPARROW_INLINE
double ValueConvNumber( Value v , int* fail ) {
  if(Vis_true(&v)) {
    *fail = 0;
    return 1;
  } else if(Vis_false(&v)) {
    *fail = 0;
    return 0;
  } else if(Vis_number(&v)) {
    *fail = 0;
    return Vget_number(&v);
  } else {
    *fail = 1;
    return 0;
  }
}

double ValueToNumber( struct Runtime* , Value , int* );
struct ObjStr* ValueToString( struct Runtime* , Value , int* );
int ValueToBoolean( struct Runtime* rt , Value v );
size_t ValueSize( struct Runtime* , Value v , int* fail );

/* Print a value in a human readable format to StrBuf object, internally
 * it will only *append* string to that buffer */
void ValuePrint( struct Sparrow* , struct StrBuf* , Value v );

/* Create a string iterator */
void ObjStrIterInit( struct ObjStr* , struct ObjIterator* );

static SPARROW_INLINE struct MetaOps* NewMetaOps() {
  struct MetaOps* mops = malloc(sizeof(*mops));
#define __(A,B,C) mops->B = NULL; Vset_null(&(mops->hook_##B));
  METAOPS_LIST(__)
#undef __ /* __ */
  return mops;
}

/* Wrapper function for MetaOps callback. These wrapper functions
 * are used to invoke script side defined MetaOps callback function. */
int MetaOps_geti( Value func , struct Sparrow* sparrow ,
    Value object , enum IntrinsicAttribute iattr , Value* ret );

int  MetaOps_get ( Value func , struct Sparrow* sparrow ,
    Value object , Value key , Value* ret );

int MetaOps_seti( Value func , struct Sparrow* sparrow ,
    Value object , enum IntrinsicAttribute iattr , Value value );

int MetaOps_set ( Value func , struct Sparrow* sparrow ,
    Value object , Value key , Value value );

int MetaOps_hash( Value func , struct Sparrow* sparrow ,
    Value object , int32_t* ret );

int  MetaOps_key ( Value func , struct Sparrow* sparrow ,
    Value object , Value* );

int MetaOps_exist( Value func , struct Sparrow* sparrow ,
    Value object , Value key , int* );

int MetaOps_size ( Value func , struct Sparrow* sparrow ,
    Value object , size_t* );

/* Currently doesn't support user defined iterator in script side */
static SPARROW_INLINE
int MetaOps_iter ( Value func , struct Sparrow* sparrow ,
    Value object , struct ObjIterator* iterator ) {
  UNUSE_ARG(func);
  UNUSE_ARG(sparrow);
  UNUSE_ARG(object);
  UNUSE_ARG(iterator);
  UNIMPLEMENTED();
  return -1;
}

int MetaOps_print( Value func , struct Sparrow* sparrow ,
    Value object , struct StrBuf* );

int MetaOps_to_str( Value func , struct Sparrow* sparrow ,
    Value object , Value* );

int MetaOps_to_boolean( Value func , struct Sparrow* sparrow ,
    Value object , Value* );

int MetaOps_to_number( Value func , struct Sparrow* sparrow ,
    Value object , Value* );

#endif /* OBJECT_H_ */
