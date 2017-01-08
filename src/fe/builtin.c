#include "builtin.h"
#include "list.h"
#include "map.h"
#include "bc.h"
#include "sparrow.h"
#include "gc.h"
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

void Builtin_GCForce( struct Runtime* rt , Value* ret , int* fail ) {
  BUILTIN_CHECK_ARGUMENT(rt,"gc_force",0) {
    GCForce(RTSparrow(rt));
    Vset_number(ret,RTSparrow(rt)->gc_inactive);
    *fail = 0;
  }
}

void Builtin_GCTry( struct Runtime* rt , Value* ret , int* fail ) {
  BUILTIN_CHECK_ARGUMENT(rt,"gc_try",0) {
    int r = GCTry(RTSparrow(rt));
    Vset_boolean(ret,r);
    *fail = 0;
  }
}

void Builtin_GCStat( struct Runtime* rt , Value* ret , int* fail ) {
  BUILTIN_CHECK_ARGUMENT(rt,"gc_stat",0) {
    struct Sparrow* sparrow = RTSparrow(rt);
    struct ObjMap* map = ObjNewMapNoGC(sparrow,8);
    Value v;

#define ADD(X) \
    do { \
      Vset_number(&v,sparrow->X); \
      ObjMapPut(map,ObjNewStrNoGC(sparrow,#X,STRING_SIZE(#X)),v); \
    } while(0)

    ADD(gc_generation);
    ADD(gc_prevsz);
    ADD(gc_sz);
    ADD(gc_active);
    ADD(gc_inactive);
    ADD(gc_ratio);
    ADD(gc_threshold);
    ADD(gc_adjust_threshold);
    ADD(gc_penalty_ratio);
    ADD(gc_ps_threshold);

#undef ADD /* ADD */

    *fail = 0;
    Vset_map(ret,map);
  }
}

void Builtin_GCConfig( struct Runtime* rt , Value* ret, int* fail ) {
  BUILTIN_CHECK_ARGUMENT(rt,"gc_config",1,ARG_MAP) {
    struct Sparrow* sparrow = RTSparrow(rt);
    Value a = RuntimeGetArg(rt,0);
    struct ObjMap* m = Vget_map(&a);
    Value v_ratio;
    Value v_threshold;
    Value v_penalty_ratio;

    size_t threshold= 0;
    float ratio = 0.0f;
    float penalty_ratio = 0.0f;

    if(ObjMapFindStr(m,"ratio",&v_ratio)==0) {
      if(Vis_number(&v_ratio)) {
        ratio = (float)Vget_number(&v_ratio);
      }
    }

    if(ObjMapFindStr(m,"threshold",&v_threshold)==0) {
      if(Vis_number(&v_threshold)) {
        double n = Vget_number(&v_threshold);
        if(n>0 && n < INT_MAX) threshold = (size_t)n;
      }
    }

    if(ObjMapFindStr(m,"penalty_ratio",&v_penalty_ratio)==0) {
      if(Vis_number(&v_penalty_ratio)) {
        penalty_ratio = (float)Vget_number(&v_penalty_ratio);
      }
    }

    SparrowGCConfig(sparrow,threshold,ratio,penalty_ratio);
    *fail = 0;
    Vset_null(ret);
  }
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

#define META_CHECK_ARGUMENT(...) \
  do { \
    if(RuntimeCheckArg(__VA_ARGS__)) \
      return MOPS_FAIL; \
  } while(0);


#define MMETHODNAME(TYPE,NAME) TYPE #NAME

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
    RuntimeError(runtime,"function list.push needs at least 1 "
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
  if(RuntimeCheckArg(runtime,"list.extend",2,ARG_LIST,ARG_LIST))
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

  if(RuntimeCheckArg(runtime,"list.resize",2,ARG_LIST,ARG_CONV_NUMBER))
    return -1;
  a1 = RuntimeGetArg(runtime,0);
  a2 = RuntimeGetArg(runtime,1);
  if(ToSize(Vget_number(&a2),&size)) {
    RuntimeError(runtime,"list.resize" PERR_SIZE_OVERFLOW,Vget_number(&a2));
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

  if(RuntimeCheckArg(runtime,"list.pop",1,ARG_LIST)) return -1;
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
  if(RuntimeCheckArg(runtime,"list.size",1,ARG_LIST)) return -1;
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
  if(RuntimeCheckArg(runtime,"list.empty",1,ARG_LIST)) return -1;
  arg = RuntimeGetArg(runtime,0);
  Vset_boolean(ret,ObjListSize(Vget_list(&arg))==0);
  return 0;
}

static int list_index( struct Sparrow* sth,
    Value obj,
    Value* ret ) {
  struct Runtime* runtime = sth->runtime;
  Value a1,a2;
  size_t idx;

  assert(Vis_udata(&obj));

  if(RuntimeCheckArg(runtime,"list.index",2,ARG_LIST,
                                            ARG_CONV_NUMBER))
    return -1;

  a1 = RuntimeGetArg(runtime,0);
  a2 = RuntimeGetArg(runtime,1);

  if(ToSize(Vget_number(&a2),&idx) ||
    (idx >= ObjListSize(Vget_list(&a1)))) {
    RuntimeError(runtime,"list.index " PERR_INDEX_OUT_OF_RANGE);
    return -1;
  }
  *ret = ObjListIndex(Vget_list(&a1),idx);
  return 0;
}

static int list_clear( struct Sparrow* sth ,
    Value obj,
    Value* ret ) {
  struct Runtime* runtime = sth->runtime;
  Value arg;
  assert(Vis_udata(&obj));
  if(RuntimeCheckArg(runtime,"list.clear",1,ARG_LIST)) return -1;
  arg = RuntimeGetArg(runtime,0);
  ObjListClear(Vget_list(&arg));
  Vset_null(ret);
  return 0;
}

/* opaque pointer for global list object */
struct listobj_pri {
  /* Cached method for intrinsic attributes */
  Value method[ SIZE_OF_IATTR ];
};

#define LIST_UDATA_NAME "__list__"

static enum MetaStatus list_Mcall( struct Sparrow* sparrow , Value udata ,
    Value* ret ) {
  struct Runtime* rt = sparrow->runtime;
  assert(Vis_udata(&udata));
  assert(rt);
  META_CHECK_ARGUMENT(rt,MMETHODNAME(LIST_UDATA_NAME,__call),
      1,ARG_CONV_NUMBER) {
    Value arg = RuntimeGetArg( rt , 0 );
    size_t sz;
    if(ToSize( Vget_number(&arg) , &sz)) {
      RuntimeError(rt,PERR_SIZE_OVERFLOW,Vget_number(&arg));
      return MOPS_FAIL;
    }
    Vset_list(ret,ObjNewList(sparrow,sz));
    return MOPS_OK;
  }
}

static enum MetaStatus list_Mget( struct Sparrow* sparrow , Value udata ,
    Value key , Value* ret ) {
  struct Runtime* runtime = sparrow->runtime;
  struct ObjUdata* u = Vget_udata(&udata);
  if(Vis_str(&key)) {
    struct ObjStr* k = Vget_str(&key);
    struct listobj_pri* pri = (struct listobj_pri*)(u->udata);
    enum IntrinsicAttribute iattr = IAttrGetIndex(k->str);
    if(iattr == SIZE_OF_IATTR) {
      RuntimeError(runtime,PERR_TYPE_NO_ATTRIBUTE,LIST_UDATA_NAME,k->str);
      return MOPS_FAIL;
    }
    *ret = pri->method[iattr];
    return MOPS_OK;
  } else {
    RuntimeError(runtime,PERR_ATTRIBUTE_TYPE,LIST_UDATA_NAME,
        ValueGetTypeString(key));
    return MOPS_FAIL;
  }
}

static enum MetaStatus list_Mgeti( struct Sparrow* sparrow , Value udata ,
    enum IntrinsicAttribute iattr , Value* ret ) {
  struct ObjUdata* u = Vget_udata(&udata);
  struct listobj_pri* pri = (struct listobj_pri*)(u->udata);
  UNUSE_ARG(sparrow);
  *ret = pri->method[iattr];
  return MOPS_OK;
}

static void list_Mmark( struct ObjUdata* udata ) {
  struct listobj_pri* pri = (struct listobj_pri*)(udata->udata);
  GCMarkMethod(Vget_method(pri->method+IATTR_PUSH));
  GCMarkMethod(Vget_method(pri->method+IATTR_POP));
  GCMarkMethod(Vget_method(pri->method+IATTR_EXTEND));
  GCMarkMethod(Vget_method(pri->method+IATTR_SIZE));
  GCMarkMethod(Vget_method(pri->method+IATTR_RESIZE));
  GCMarkMethod(Vget_method(pri->method+IATTR_INDEX));
  GCMarkMethod(Vget_method(pri->method+IATTR_EMPTY));
  GCMarkMethod(Vget_method(pri->method+IATTR_CLEAR));
}

static void list_Mdestroy( void* udata ) {
  free(udata);
}

struct ObjUdata* GCreateListUdata( struct Sparrow* sparrow ) {
  size_t i;
  struct listobj_pri* pri = malloc(sizeof(*pri));
  struct ObjUdata* udata = ObjNewUdataNoGC(sparrow,
      LIST_UDATA_NAME,
      pri,
      list_Mmark,
      list_Mdestroy);
  Value self;
  Vset_udata(&self,udata);

  /* clear the method table */
  for( i = 0 ;i < SIZE_OF_IATTR ; ++i )
    Vset_null(pri->method+i);

#define ADD(TYPE,FUNC) \
  do { \
    struct ObjMethod* method = ObjNewMethod(sparrow,FUNC,self,\
        &(sparrow->IAttrName_##TYPE)); \
    Vset_method(pri->method+IATTR_##TYPE,method); \
  } while(0)

  ADD(PUSH,list_push);
  ADD(POP,list_pop);
  ADD(EXTEND,list_extend);
  ADD(SIZE,list_size);
  ADD(RESIZE,list_resize);
  ADD(INDEX,list_index);
  ADD(EMPTY,list_empty);
  ADD(CLEAR,list_clear);

#undef ADD /* ADD */

  /* Initialize Metaops table */
  udata->mops.get = list_Mget;
  udata->mops.geti= list_Mgeti;
  udata->mops.call= list_Mcall;

  return udata;
}
