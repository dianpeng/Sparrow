#include "builtin.h"
#include "bc.h"
#include "list.h"
#include "map.h"
#include "gc.h"
#include "sparrow.h"
#include <sys/time.h>
#include <limits.h>

static void print( struct Runtime* rt , Value* ret , int* fail ,
    FILE* output ) {
  size_t narg = RuntimeGetArgSize(rt);
  if(narg >=1) {
    size_t i;
    struct StrBuf sbuf;
    StrBufInit(&sbuf,128);
    for( i = 0 ; i < narg ; ++i ) {
      Value arg = RuntimeGetArg(rt,i);
      ValuePrint(RTSparrow(rt),&sbuf,arg);
    }
    fwrite(sbuf.buf,1,sbuf.size,output);
    if(output == stderr) fflush(stderr);
    StrBufDestroy(&sbuf);
  }
  Vset_null(ret);
  *fail = 0;
}

void Builtin_Print( struct Runtime* rt , Value* ret , int* fail ) {
  print(rt,ret,fail,stdout);
}

void Builtin_Error( struct Runtime* rt , Value* ret , int* fail ) {
  print(rt,ret,fail,stderr);
}

void Builtin_Import( struct Runtime* rt , Value* ret , int* fail ) {
  BUILTIN_CHECK_ARGUMENT(rt,"import",1,ARG_STRING) {
    struct Sparrow* sparrow = RTSparrow(rt);
    struct CStr error;
    Value path = RuntimeGetArg(rt,0);
    int status = RunFile(sparrow,Vget_str(&path)->str,NULL,ret,&error);
    if(status) {
      RuntimeError(rt,"function import failed due to reason :%s!",
          error.str);
      CStrDestroy(&error);
    }
    *fail = status;
  }
}

void Builtin_RunString( struct Runtime* rt , Value* ret , int* fail ) {
  BUILTIN_CHECK_ARGUMENT(rt,"run_string",1,ARG_STRING) {
    struct CStr err;
    Value src = RuntimeGetArg(rt,0);
    int status = RunString(RTSparrow(rt),Vget_str(&src)->str,NULL,ret,&err);
    if(status) {
      RuntimeError(rt,"function RunString failed due to reason :%s!",
          err.str);
      CStrDestroy(&err);
    }
    *fail = status;
  }
}

#define same_sign(A,B) ((A)*(B)>0)

void Builtin_Range( struct Runtime* rt , Value* ret , int* fail ) {
  BUILTIN_CHECK_ARGUMENT(rt,"range",3,ARG_CONV_NUMBER,
                                      ARG_CONV_NUMBER,
                                      ARG_CONV_NUMBER) {
    Value start = RuntimeGetArg(rt,0);
    Value end = RuntimeGetArg(rt,1);
    Value step= RuntimeGetArg(rt,2);
    int istart,iend,istep;
    struct ObjList* list;
    if(ConvNum(Vget_number(&start),&istart)) {
      RuntimeError(rt,PERR_ARGUMENT_OUT_OF_RANGE,"start");
      goto fail;
    }
    if(ConvNum(Vget_number(&end),&iend)) {
      RuntimeError(rt,PERR_ARGUMENT_OUT_OF_RANGE,"end");
      goto fail;
    }
    if(ConvNum(Vget_number(&step),&istep) || istep ==0) {
      RuntimeError(rt,PERR_ARGUMENT_OUT_OF_RANGE,"step");
      goto fail;
    }
    if(!same_sign(iend-istart,istep)) {
      RuntimeError(rt,"function range's step argument is invalid!");
      goto fail;
    }

    assert( (iend-istart)/istep > 0);
    list = ObjNewList(RTSparrow(rt),(iend-istart)/istep);

    for( ; istart < iend ; istart += istep ) {
      Value ele;
      Vset_number(&ele,istart);
      ObjListPush(list,ele);
    }
    Vset_list(ret,list);
    *fail = 0;
  }
  return;

fail:
  *fail = 1;
}

void Builtin_MSec( struct Runtime* rt , Value* ret , int* fail ) {
  BUILTIN_CHECK_ARGUMENT(rt,"msec",0) {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    Vset_number(ret,(double)(tv.tv_sec*1000000+tv.tv_usec));
    *fail = 0;
  }
}

void Builtin_Min( struct Runtime* rt , Value* v , int* f ) {
  UNUSE_ARG(rt);
  UNUSE_ARG(v);
  UNUSE_ARG(f);
  UNIMPLEMENTED();
}

void Builtin_Max( struct Runtime* rt , Value* v , int* f ) {
  UNUSE_ARG(rt);
  UNUSE_ARG(v);
  UNUSE_ARG(f);
  UNIMPLEMENTED();
}

void Builtin_Sort(struct Runtime* rt , Value* v , int* f ) {
  UNUSE_ARG(rt);
  UNUSE_ARG(v);
  UNUSE_ARG(f);
  UNIMPLEMENTED();
}

void Builtin_Set( struct Runtime* rt , Value* v , int* f ) {
  UNUSE_ARG(rt);
  UNUSE_ARG(v);
  UNUSE_ARG(f);
  UNIMPLEMENTED();
}

void Builtin_Get( struct Runtime* rt , Value* v , int* f ) {
  UNUSE_ARG(rt);
  UNUSE_ARG(v);
  UNUSE_ARG(f);
  UNIMPLEMENTED();
}

void Builtin_Exist(struct Runtime* rt , Value* v , int* f ){
  UNUSE_ARG(rt);
  UNUSE_ARG(v);
  UNUSE_ARG(f);
  UNIMPLEMENTED();
}

/* Builtin objects creation */

/* ================================
 * list udata
 * ==============================*/

#define META_CHECK_ARGUMENT(RT,OBJNAME,MNAME,...) \
  do { \
    char buf[246]; \
    sprintf(buf,"%s.%s",(OBJNAME),#MNAME); \
    if(RuntimeCheckArg(RT,buf,__VA_ARGS__)) \
      return -1; \
  } while(0);

/* This function pointer is used to create a value which is called during
 * MetaCall callback function */
typedef int (*MCallCallback)( struct Runtime* , Value* ret );

/* private data for storing global objects' intrinsic
 * attributes */
struct gvar_dt_pri {
  Value method[ SIZE_OF_IATTR ];
  MCallCallback mcall_cb;
};

static int gvar_dt_Mget( struct Sparrow* sparrow , Value udata ,
    Value key , Value* ret ) {
  struct Runtime* runtime = sparrow->runtime;
  struct ObjUdata* u = Vget_udata(&udata);
  struct gvar_dt_pri* pri = (struct gvar_dt_pri*)(u->udata);

  if(Vis_str(&key)) {
    struct ObjStr* k = Vget_str(&key);
    enum IntrinsicAttribute iattr = IAttrGetIndex(k->str);
    if(iattr == SIZE_OF_IATTR) {
      RuntimeError(runtime,PERR_TYPE_NO_ATTRIBUTE,u->name.str,k->str);
      return -1;
    }
    *ret = pri->method[iattr];
    return 0;
  } else {
    RuntimeError(runtime,PERR_ATTRIBUTE_TYPE,u->name.str,
        ValueGetTypeString(key));
    return -1;
  }
}

static int gvar_dt_Mgeti( struct Sparrow* sparrow , Value udata ,
    enum IntrinsicAttribute iattr , Value* ret ) {
  struct ObjUdata* u = Vget_udata(&udata);
  struct gvar_dt_pri* pri = (struct gvar_dt_pri*)(u->udata);
  UNUSE_ARG(sparrow);
  *ret = pri->method[iattr];
  return 0;
}

static void gvar_dt_Mmark( struct ObjUdata* udata ) {
  struct gvar_dt_pri* pri = (struct gvar_dt_pri*)(udata->udata);
  size_t i;
  for( i = 0 ; i < SIZE_OF_IATTR; ++i ) {
    GCMark(pri->method[i]);
  }
}

static void gvar_dt_Mdestroy( void* udata ) {
  free(udata);
}

struct cmethod_ptr {
  CMethod ptr;
  struct ObjStr* name;
};

static struct ObjUdata* gvar_dt_uobj_create( struct Sparrow* sparrow ,
    const char* objname, MCallCallback mcall_cb,
    struct cmethod_ptr* methods ) {
  struct gvar_dt_pri* pri = malloc(sizeof(*pri));
  struct ObjUdata* u = ObjNewUdataNoGC(sparrow,objname,pri,
      gvar_dt_Mmark,gvar_dt_Mdestroy,NULL);
  size_t i;
  Value self;

  Vset_udata(&self,u);
  for( i = 0; i < SIZE_OF_IATTR ; ++i ) {
    if(methods[i].ptr) {
      struct ObjMethod* m = ObjNewMethodNoGC(sparrow,methods[i].ptr,
          self,methods[i].name);
      Vset_method(pri->method+i,m);
    } else {
      Vset_null(pri->method+i);
    }
  }
  pri->mcall_cb = NULL;

  u->mops = NewMetaOps();

  u->mops->get = gvar_dt_Mget;
  u->mops->geti = gvar_dt_Mgeti;
  return u;
}

/* list attributes */
static int list_push( struct Sparrow* sth ,
    Value obj ,
    Value* ret ) {
  Value arg;
  struct ObjList* l;
  struct Runtime* runtime = sth->runtime;
  size_t narg = RuntimeGetArgSize(runtime);
  size_t i;
  assert(Vis_udata(&obj));

  if(narg ==0) {
    RuntimeError(runtime,"function push needs at least 1 "
        "argument, but got 0!");
    return  -1;
  }

  arg = RuntimeGetArg(runtime,0);
  if(!Vis_list(&arg)) {
    RuntimeError(runtime,PERR_FUNCCALL_ARG_TYPE_MISMATCH,1,"list",
        ValueGetTypeString(arg));
    return -1;
  }
  l = Vget_list(&arg);

  for( i = 1 ; i < narg ; ++i ) {
    ObjListPush(l,RuntimeGetArg(runtime,i));
  }
  Vset_number(ret,narg-1);
  return 0;
}

static int list_extend( struct Sparrow* sth ,
    Value obj ,
    Value* ret ) {
  struct Runtime* runtime = sth->runtime;
  Value a1,a2;
  assert(Vis_udata(&obj));
  if(RuntimeCheckArg(runtime,"extend",2,ARG_LIST,ARG_LIST))
    return -1;
  a1 = RuntimeGetArg(runtime,0);
  a2 = RuntimeGetArg(runtime,1);
  ObjListExtend(Vget_list(&a1),Vget_list(&a2));
  Vset_null(ret);
  return 0;
}

static int list_resize( struct Sparrow* sth ,
    Value obj,
    Value* ret ) {
  struct Runtime* runtime = sth->runtime;
  Value a1,a2;
  size_t size;
  assert(Vis_udata(&obj));

  if(RuntimeCheckArg(runtime,"resize",2,ARG_LIST,ARG_CONV_NUMBER))
    return -1;
  a1 = RuntimeGetArg(runtime,0);
  a2 = RuntimeGetArg(runtime,1);
  if(ToSize(Vget_number(&a2),&size)) {
    RuntimeError(runtime,"resize" PERR_SIZE_OVERFLOW,Vget_number(&a2));
    return -1;
  }

  /* ignore negative size value */
  if(size >0) ObjListResize(Vget_list(&a1),size);
  Vset_number(ret,(double)size);
  return 0;
}

static int list_pop( struct Sparrow* sth ,
    Value obj ,
    Value* ret ) {
  struct Runtime* runtime = sth->runtime;
  Value arg;
  assert(Vis_udata(&obj));

  if(RuntimeCheckArg(runtime,"pop",1,ARG_LIST)) return -1;
  arg = RuntimeGetArg(runtime,0);
  ObjListPop(Vget_list(&arg));
  Vset_null(ret);
  return 0;
}

static int list_size( struct Sparrow* sth,
    Value obj ,
    Value* ret ) {
  struct Runtime* runtime = sth->runtime;
  Value arg;
  assert(Vis_udata(&obj));
  if(RuntimeCheckArg(runtime,"size",1,ARG_LIST)) return -1;
  arg = RuntimeGetArg(runtime,0);
  Vset_number(ret,ObjListSize(Vget_list(&arg)));
  return 0;
}

static int list_empty( struct Sparrow* sth,
    Value obj,
    Value* ret ) {
  struct Runtime* runtime = sth->runtime;
  Value arg;
  assert(Vis_udata(&obj));
  if(RuntimeCheckArg(runtime,"empty",1,ARG_LIST)) return -1;
  arg = RuntimeGetArg(runtime,0);
  Vset_boolean(ret,ObjListSize(Vget_list(&arg))==0);
  return 0;
}

static int list_clear( struct Sparrow* sth , Value obj, Value* ret ) {
  struct Runtime* runtime = sth->runtime;
  Value arg;
  assert(Vis_udata(&obj));
  if(RuntimeCheckArg(runtime,"clear",1,ARG_LIST)) return -1;
  arg = RuntimeGetArg(runtime,0);
  ObjListClear(Vget_list(&arg));
  Vset_null(ret);
  return 0;
}

static int list_slice( struct Sparrow* sparrow , Value obj , Value* ret ) {
  struct Runtime* runtime = sparrow->runtime;
  struct ObjList* new_list;
  struct ObjList* old_list;
  Value a1, a2, a3;
  size_t start,end;

  assert(Vis_udata(&obj));
  if(RuntimeCheckArg(runtime,"slice",3,ARG_LIST,
                                            ARG_CONV_NUMBER,
                                            ARG_CONV_NUMBER))
    return -1;

  a1 = RuntimeGetArg(runtime,0);
  a2 = RuntimeGetArg(runtime,1);
  a3 = RuntimeGetArg(runtime,2);

  if(ToSize(Vget_number(&a2),&start)) {
    RuntimeError(runtime,PERR_SIZE_OVERFLOW,Vget_number(&a2));
    return -1;
  }
  if(ToSize(Vget_number(&a3),&end)) {
    RuntimeError(runtime,PERR_SIZE_OVERFLOW,Vget_number(&a3));
    return -1;
  }
  old_list = Vget_list(&a1);
  if(start > old_list->size) start = old_list->size;
  if(end < start) end = start;
  new_list = ObjListSlice(sparrow,old_list,start,end);
  Vset_list(ret,new_list);
  return 0;
}

#define LIST_UDATA_NAME "__list__"

struct ObjUdata* GCreateListUdata( struct Sparrow* sparrow ) {
  struct cmethod_ptr method_ptr[ SIZE_OF_IATTR ] = {
    { list_extend , IATTR_NAME(sparrow,EXTEND) },
    { list_push   , IATTR_NAME(sparrow,PUSH)   },
    { list_pop    , IATTR_NAME(sparrow,POP)    },
    { list_size   , IATTR_NAME(sparrow,SIZE)   },
    { list_resize , IATTR_NAME(sparrow,RESIZE) },
    { list_empty  , IATTR_NAME(sparrow,EMPTY)  },
    { list_clear  , IATTR_NAME(sparrow,CLEAR)  },
    { list_slice  , IATTR_NAME(sparrow,SLICE)  },
    { NULL        , IATTR_NAME(sparrow,EXIST)  }
  };
  return gvar_dt_uobj_create(sparrow,LIST_UDATA_NAME,NULL,method_ptr);
}

/* ===========================
 * String global variable
 * =========================*/
static int string_size( struct Sparrow* sth , Value obj, Value* ret ) {
  struct Runtime* runtime = sth->runtime;
  Value arg;
  assert( Vis_udata(&obj) );
  if(RuntimeCheckArg(runtime,"size",1,ARG_STRING)) return -1;
  arg = RuntimeGetArg(runtime,0);
  Vset_number(ret,Vget_str(&arg)->len);
  return 0;
}

static int string_empty( struct Sparrow* sth , Value obj, Value* ret ) {
  struct Runtime* runtime = sth->runtime;
  Value arg;
  assert( Vis_udata(&obj) );
  if(RuntimeCheckArg(runtime,"empty",1,ARG_STRING)) return -1;
  arg = RuntimeGetArg(runtime,0);
  Vset_boolean(ret,Vget_str(&arg)->len == 0);
  return 0;
}

static int string_slice( struct Sparrow* sth , Value obj , Value* ret ) {
  struct Runtime* runtime = sth->runtime;
  Value a1,a2,a3;
  size_t start,end;
  struct ObjStr* str;
  struct ObjStr* rstr;
  char sbuf[ LARGE_STRING_SIZE ];
  char* buf = sbuf;

  assert( Vis_udata(&obj) );
  if(RuntimeCheckArg(runtime,"slice",3,ARG_STRING,
                                       ARG_CONV_NUMBER,
                                       ARG_CONV_NUMBER))
    return -1;
  a1 = RuntimeGetArg(runtime,0);
  a2 = RuntimeGetArg(runtime,1);
  a3 = RuntimeGetArg(runtime,2);
  if( ToSize(Vget_number(&a2),&start) ) {
    RuntimeError(runtime,PERR_SIZE_OVERFLOW,Vget_number(&a2));
    return -1;
  }
  if( ToSize(Vget_number(&a3),&end) ) {
    RuntimeError(runtime,PERR_SIZE_OVERFLOW,Vget_number(&a3));
    return -1;
  }
  str = Vget_str(&a1);

  if(start >= str->len) start = str->len;
  if(end < start) end = start;

  if(end-start > LARGE_STRING_SIZE) {
    buf = malloc(end-start);
  }
  memcpy(buf,str->str+start,(end-start));

  /* Create a managed string , here we have no way to avoid
   * a duplicate memory allocation */
  rstr = ObjNewStr(sth,buf,(end-start));

  /* Free the memory if we need to since it is on the heap */
  if(buf != sbuf) free(buf);

  Vset_str(ret,rstr);
  return 0;
}

#define STRING_UDATA_NAME "__string__"

struct ObjUdata* GCreateStringUdata( struct Sparrow* sparrow ) {
  struct cmethod_ptr method_ptr[ SIZE_OF_IATTR ] = {
    { NULL , IATTR_NAME(sparrow,EXTEND) },
    { NULL , IATTR_NAME(sparrow,PUSH)   },
    { NULL , IATTR_NAME(sparrow,POP)    },
    { string_size , IATTR_NAME(sparrow,SIZE)   },
    { NULL , IATTR_NAME(sparrow,RESIZE) },
    { string_empty , IATTR_NAME(sparrow,EMPTY)  },
    { NULL , IATTR_NAME(sparrow,CLEAR)  },
    { string_slice , IATTR_NAME(sparrow,SLICE)  },
    { NULL , IATTR_NAME(sparrow,EXIST) }
  };
  return gvar_dt_uobj_create(sparrow,STRING_UDATA_NAME,NULL,method_ptr);
}

/* ======================
 * Map global variable
 * ====================*/

static int map_pop( struct Sparrow* sparrow , Value obj , Value* ret ) {
  struct Runtime* runtime = sparrow->runtime;
  Value a1,a2;
  struct ObjMap* m;
  assert(Vis_udata(&obj));
  if(RuntimeCheckArg(runtime,"pop",2,ARG_MAP,ARG_STRING))
    return -1;
  a1 = RuntimeGetArg(runtime,0);
  a2 = RuntimeGetArg(runtime,1);
  m = Vget_map(&a1);
  Vset_boolean(ret,ObjMapRemove(m,Vget_str(&a2),NULL)==0);
  return 0;
}

static int map_extend( struct Sparrow* sparrow , Value obj , Value* ret ) {
  struct Runtime* runtime = sparrow->runtime;
  Value a1,a2;
  struct ObjMap* src;
  struct ObjMap* dest;
  struct ObjIterator oitr;
  assert(Vis_udata(&obj));
  if(RuntimeCheckArg(runtime,"extend",2,ARG_MAP,ARG_MAP))
    return -1;
  a1 = RuntimeGetArg(runtime,0);
  a2 = RuntimeGetArg(runtime,1);
  src = Vget_map(&a2); dest = Vget_map(&a1);

  ObjMapIterInit(dest,&oitr);
  while(oitr.has_next(sparrow,&oitr) == 0) {
    Value key;
    Value val;
    oitr.deref(sparrow,&oitr,&key,&val);
    ObjMapPut(src,Vget_str(&key),val);
    oitr.move(sparrow,&oitr);
  }
  Vset_map(ret,src);
  return 0;
}

static int map_size( struct Sparrow* sparrow , Value obj , Value* ret ) {
  struct Runtime* runtime = sparrow->runtime;
  Value a1;
  assert(Vis_udata(&obj));
  if(RuntimeCheckArg(runtime,"size",1,ARG_MAP))
    return -1;
  a1 = RuntimeGetArg(runtime,0);
  Vset_number(ret,Vget_map(&a1)->size);
  return 0;
}

static int map_empty( struct Sparrow* sparrow , Value obj , Value* ret ) {
  struct Runtime* runtime = sparrow->runtime;
  Value a1;
  assert(Vis_udata(&obj));
  if(RuntimeCheckArg(runtime,"empty",1,ARG_MAP))
    return -1;
  a1 = RuntimeGetArg(runtime,0);
  Vset_boolean(ret,Vget_map(&a1)->size == 0);
  return 0;
}

static int map_exist( struct Sparrow* sparrow , Value obj , Value* ret ) {
  struct Runtime* runtime = sparrow->runtime;
  Value a1,a2;
  struct ObjMap* map;
  struct ObjStr* key;
  assert(Vis_udata(&obj));

  if(RuntimeCheckArg(runtime,"exist",2,ARG_MAP,
                                           ARG_STRING))
    return -1;
  a1 = RuntimeGetArg(runtime,0);
  a2 = RuntimeGetArg(runtime,1);

  map = Vget_map(&a1);
  key = Vget_str(&a2);

  Vset_boolean(ret,ObjMapFind(map,key,NULL) ==0);
  return 0;
}

static int map_clear( struct Sparrow* sparrow , Value obj , Value* ret ) {
  struct Runtime* runtime = sparrow->runtime;
  Value a1;
  assert(Vis_udata(&obj));
  if(RuntimeCheckArg(runtime,"clear",1,ARG_MAP)) return -1;
  a1 = RuntimeGetArg(runtime,0);
  ObjMapClear(Vget_map(&a1));
  Vset_null(ret);
  return 0;
}

#define MAP_UDATA_NAME "__map__"

struct ObjUdata* GCreateMapUdata( struct Sparrow* sparrow ) {
  struct cmethod_ptr method_ptr[ SIZE_OF_IATTR ] = {
    { map_extend , IATTR_NAME(sparrow,EXTEND) },
    { NULL , IATTR_NAME(sparrow,PUSH)   },
    { map_pop , IATTR_NAME(sparrow,POP)    },
    { map_size , IATTR_NAME(sparrow,SIZE)   },
    { NULL , IATTR_NAME(sparrow,RESIZE) },
    { map_empty , IATTR_NAME(sparrow,EMPTY)  },
    { map_clear , IATTR_NAME(sparrow,CLEAR)  },
    { NULL , IATTR_NAME(sparrow,SLICE)  },
    { map_exist , IATTR_NAME(sparrow,EXIST) }
  };
  return gvar_dt_uobj_create(sparrow,MAP_UDATA_NAME,NULL,method_ptr);
}

typedef int (*AttrHook) ( struct Sparrow* sparrow , struct ObjUdata* udata,
    struct ObjStr* key , Value* ret );

struct gvar_general_pri {
  struct ObjMap* attr;
  AttrHook hook;
};

static int gvar_general_Mget( struct Sparrow* sparrow , Value object ,
    Value key , Value* ret ) {
  struct Runtime* runtime = sparrow->runtime;
  struct ObjUdata* udata  = Vget_udata(&object);
  struct gvar_general_pri* pri = (struct gvar_general_pri*)(udata->udata);
  if(Vis_str(&key)) {
    if(pri->hook) {
      if(pri->hook(sparrow,udata,Vget_str(&key),ret)) goto retry;
      return 0;
    } else {
retry:
      if(ObjMapFind(pri->attr,Vget_str(&key),ret)) {
        RuntimeError(runtime,PERR_TYPE_NO_ATTRIBUTE,udata->name.str,
            Vget_str(&key)->str);
        return -1;
      } else {
        return 0;
      }
    }
  } else {
    RuntimeError(runtime,PERR_ATTRIBUTE_TYPE,ValueGetTypeString(key));
    return -1;
  }
}

static int gvar_general_Mgeti( struct Sparrow* sparrow , Value object ,
    enum IntrinsicAttribute iattr , Value* ret ) {
  struct Runtime* runtime = sparrow->runtime;
  struct ObjUdata* udata  = Vget_udata(&object);
  struct gvar_general_pri* pri = (struct gvar_general_pri*)(udata->udata);
  struct ObjStr* key = IAttrGetObjStr(sparrow,iattr);
  if(pri->hook) {
    if(pri->hook(sparrow,udata,key,ret)) goto retry;
    return 0;
  } else {
retry:
    if(ObjMapFind(pri->attr,key,ret)) {
      RuntimeError(runtime,PERR_TYPE_NO_ATTRIBUTE,udata->name.str,key->str);
      return -1;
    } else {
      return 0;
    }
  }
}

static void gvar_general_Mmark( struct ObjUdata* udata ) {
  struct gvar_general_pri* pri = (struct gvar_general_pri*)(udata->udata);
  GCMarkMap(pri->attr);
}

static void gvar_general_Mdestroy( void* udata ) {
  free(udata);
}

static struct ObjUdata* gvar_general_create( struct Sparrow* sparrow ,
    const char* name ,
    AttrHook attr_hook,
    struct cmethod_ptr* methods ,
    size_t len ) {
  Value self;
  struct gvar_general_pri* pri = malloc(sizeof(*pri));
  struct ObjUdata* udata = ObjNewUdataNoGC(sparrow,name,pri,
      gvar_general_Mmark,
      gvar_general_Mdestroy,
      NULL);
  size_t i;

  pri->attr = ObjNewMapNoGC(sparrow,32);
  pri->hook = attr_hook;
  Vset_udata(&self,udata);
  for( i = 0 ; i < len ; ++i ) {
    Value v;
    struct ObjMethod* method = ObjNewMethodNoGC(sparrow,methods[i].ptr,
        self,methods[i].name);
    Vset_method(&v,method);
    ObjMapPut(pri->attr,methods[i].name,v);
  }

  udata->mops = NewMetaOps();
  udata->mops->get = gvar_general_Mget;
  udata->mops->geti= gvar_general_Mgeti;
  return udata;
}

/* For GC */
static int gc_attr_hook( struct Sparrow* sparrow , struct ObjUdata* udata,
    struct ObjStr* key, Value* ret) {
  if(ObjStrCmpStr(key,"sz") ==0) {
    Vset_number(ret,sparrow->gc_sz);
    return 0;
  } else if(ObjStrCmpStr(key,"active") ==0) {
    Vset_number(ret,sparrow->gc_active);
    return 0;
  } else if(ObjStrCmpStr(key,"inactive") ==0) {
    Vset_number(ret,sparrow->gc_inactive);
    return 0;
  } else if(ObjStrCmpStr(key,"prevsz") ==0) {
    Vset_number(ret,sparrow->gc_prevsz);
    return 0;
  } else if(ObjStrCmpStr(key,"generation") ==0) {
    Vset_number(ret,sparrow->gc_generation);
    return 0;
  } else if(ObjStrCmpStr(key,"threshold") ==0) {
    Vset_number(ret,sparrow->gc_threshold);
    return 0;
  } else if(ObjStrCmpStr(key,"ratio") ==0) {
    Vset_number(ret,sparrow->gc_ratio);
    return 0;
  } else if(ObjStrCmpStr(key,"ps_threshold") ==0) {
    Vset_number(ret,sparrow->gc_ps_threshold);
    return 0;
  } else if(ObjStrCmpStr(key,"penalty_ratio") ==0) {
    Vset_number(ret,sparrow->gc_penalty_ratio);
    return 0;
  } else if(ObjStrCmpStr(key,"penalty_times") == 0) {
    Vset_number(ret,sparrow->gc_penalty_times);
    return 0;
  } else if(ObjStrCmpStr(key,"adjust_threshold") ==0) {
    Vset_number(ret,sparrow->gc_adjust_threshold);
    return 0;
  } else {
    return -1;
  }
}

static int gc_force( struct Sparrow* sparrow , Value obj , Value* ret ) {
  struct Runtime* runtime = sparrow->runtime;
  assert(Vis_udata(&obj));
  if(RuntimeCheckArg(runtime,"force",0)) return -1;
  GCForce(sparrow);
  Vset_null(ret);
  return 0;
}

static int gc_try( struct Sparrow* sparrow , Value obj , Value* ret ) {
  struct Runtime* runtime = sparrow->runtime;
  assert(Vis_udata(&obj));
  if(RuntimeCheckArg(runtime,"try",0)) return -1;
  GCTry(sparrow);
  Vset_null(ret);
  return 0;
}

static int gc_stat( struct Sparrow* sparrow , Value obj , Value* ret ) {
  struct Runtime* runtime = sparrow->runtime;
  struct ObjMap* map = ObjNewMap(sparrow,16);
  Value v;
  assert(Vis_udata(&obj));
  if(RuntimeCheckArg(runtime,"stat",0)) return -1;
#define ADD(X) \
  do { \
    Vset_number(&v,sparrow->gc_##X); \
    ObjMapPut(map,ObjNewStrNoGC(sparrow,#X,STRING_SIZE(#X)),v); \
  } while(0)

  ADD(generation);
  ADD(prevsz);
  ADD(sz);
  ADD(active);
  ADD(inactive);
  ADD(ratio);
  ADD(threshold);
  ADD(adjust_threshold);
  ADD(penalty_ratio);
  ADD(ps_threshold);
  ADD(penalty_times);

#undef ADD /* ADD */
  Vset_map(ret,map);
  return 0;
}

static int gc_config( struct Sparrow* sparrow , Value obj , Value* ret ) {
  struct Runtime* runtime = sparrow->runtime;
  struct ObjMap* m;
  Value v_ratio;
  Value v_threshold;
  Value v_penalty_ratio;
  Value arg;
  size_t threshold= 0;
  float ratio = 0.0f;
  float penalty_ratio = 0.0f;
  if(RuntimeCheckArg(runtime,"config",1,ARG_MAP)) return -1;

  arg = RuntimeGetArg(runtime,0);
  m = Vget_map(&arg);

  if(ObjMapFindStr(sparrow,m,"ratio",&v_ratio)==0) {
    if(Vis_number(&v_ratio)) {
      ratio = (float)Vget_number(&v_ratio);
    }
  }

  if(ObjMapFindStr(sparrow,m,"threshold",&v_threshold)==0) {
    if(Vis_number(&v_threshold)) {
      double n = Vget_number(&v_threshold);
      if(n>0 && n < SPARROW_SIZE_MAX) threshold = (size_t)n;
    }
  }

  if(ObjMapFindStr(sparrow,m,"penalty_ratio",&v_penalty_ratio)==0) {
    if(Vis_number(&v_penalty_ratio)) {
      penalty_ratio = (float)Vget_number(&v_penalty_ratio);
    }
  }

  SparrowGCConfig(sparrow,threshold,ratio,penalty_ratio);
  Vset_null(ret);
  return 0;
}

struct ObjUdata* GCreateGCUdata( struct Sparrow* sparrow ) {
  struct cmethod_ptr methods[4];
#define STRING_LEN(X) (X), STRING_SIZE((X))

  methods[0].ptr = gc_try;
  methods[0].name = ObjNewStrNoGC(sparrow,STRING_LEN("try"));
  methods[1].ptr = gc_force;
  methods[1].name = ObjNewStrNoGC(sparrow,STRING_LEN("force"));
  methods[2].ptr = gc_stat;
  methods[2].name = ObjNewStrNoGC(sparrow,STRING_LEN("stat"));
  methods[3].ptr = gc_config;
  methods[3].name = ObjNewStrNoGC(sparrow,STRING_LEN("config"));

#undef STRING_LEN /* STRING_LEN */
  return gvar_general_create(sparrow,"gc",gc_attr_hook,methods,4);
}
