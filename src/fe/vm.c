#include "vm.h"
#include "map.h"
#include "list.h"
#include "bc.h"
#include "error.h"
#include "builtin.h"
#include <math.h>

/* helper macros */
#define current_frame(THREAD) ((THREAD)->frame+((THREAD)->frame_size-1))
#define top(THREAD,IDX) ((THREAD)->stack[(THREAD)->stack_size-1-(IDX)])
#define left(THREAD) top(THREAD,1)
#define right(THREAD) top(THREAD,0)
#define replace(THREAD,VALUE) \
  ((THREAD)->stack[(THREAD)->stack_size-1] = (VALUE))
#define global_env(RT) ((RTSparrow(RT)->global_env))

#ifndef NDEBUG
static SPARROW_INLINE
void pop( struct CallThread* frame , size_t narg ) {
  assert(frame->stack_size >= narg );
  frame->stack_size -= narg;
}
#else
#define pop(THREAD,NARG) do { ((THREAD)->stack_size-=(NARG)); } while(0)
#endif /* NDEBUG */

/* Used to indicate that the current function is *called* via a C function
 * so once the interpreter finish *current frame*, it should return to the
 * C caller function */
#define RETURN_TO_HOST -1

static void exec_error( struct Runtime* , const char* , ... );

static SPARROW_INLINE
int push( struct CallThread* thread ,Value val ) {
  if(SP_UNLIKELY(thread->stack_size == thread->stack_cap)) {
    size_t ncap = 2 * thread->stack_cap;
    thread->stack = realloc(thread->stack,sizeof(Value)*ncap);
  }
  thread->stack[thread->stack_size++] = val;
  return 0;
}

/* Runtime checking */
static SPARROW_INLINE
const char* arg_get_type_name( enum ArgType type ) {
#define __(A,B) case A: return B;
  switch(type) {
  ARGTYPE(__)
  default: return NULL;
#undef __ /* __ */
  }
}

static SPARROW_INLINE
int check_arg_type( struct Runtime* rt, Value val , enum ArgType type ,
    int index , const char* funcname ) {
#define CHECK(EXPR) \
  do { \
    if(!(EXPR)) { \
      exec_error(rt,PERR_FUNCCALL_ARG_TYPE_MISMATCH, \
          funcname,index,\
          ValueGetTypeString(val), \
          arg_get_type_name(type)); \
      return -1; \
    } else { \
      return 0; \
    } \
  } while(0)

  switch(type) {
    case ARG_NUMBER: CHECK(Vis_number(&val));
    case ARG_TRUE: CHECK(Vis_true(&val));
    case ARG_FALSE:CHECK(Vis_false(&val));
    case ARG_NULL: CHECK(Vis_null(&val));
    case ARG_LIST: CHECK(Vis_list(&val));
    case ARG_MAP: CHECK(Vis_map(&val));
    case ARG_PROTO: CHECK(Vis_proto(&val));
    case ARG_CLOSURE: CHECK(Vis_closure(&val));
    case ARG_METHOD: CHECK(Vis_method(&val));
    case ARG_UDATA: CHECK(Vis_udata(&val));
    case ARG_STRING: CHECK(Vis_str(&val));
    case ARG_ITERATOR: CHECK(Vis_iterator(&val));
    case ARG_MODULE: CHECK(Vis_module(&val));
    case ARG_COMPONENT: CHECK(Vis_component(&val));
    case ARG_ANY: return 0;
    case ARG_CONV_NUMBER: CHECK(Vis_number(&val) || Vis_boolean(&val));
    case ARG_BOOLEAN: CHECK(Vis_boolean(&val));
    case ARG_CONV_BOOLEAN: return 0;
    case ARG_GCOBJECT: CHECK(Vis_gcobject(&val));
    default: assert(!"unreachable!"); return -1;
  }

#undef CHECK /* CHECK */
}

int RuntimeCheckArg( struct Runtime* rt , const char* funcname ,
    size_t narg , ... ) {
  size_t i;
  va_list vl;
  va_start(vl,narg);
  if(narg != RuntimeGetArgSize(rt)) {
    RuntimeError(rt,PERR_FUNCCALL_ARG_SIZE_MISMATCH,
        funcname,narg,RuntimeGetArgSize(rt));
    return -1;
  }
  for( i = 0 ; i < narg ; ++i ) {
    enum ArgType t = va_arg(vl,enum ArgType);
    if(check_arg_type(rt,RuntimeGetArg(rt,i),t,(int)i,funcname)) {
      return -1;
    }
  }
  return 0;
}

/* Do a simple stack unwind */
static void do_stackunwind( struct Runtime* rt , struct StrBuf* buffer ) {
  /* Walk the stackwind */
  int i;
  struct CallThread* thread = RTCallThread(rt);
  int framelen = (int)(thread->frame_size);
  for( i = 0 ; i < framelen ; ++i ) {
    struct CallFrame* frame = thread->frame + i;
    if(frame->closure) {
      // We have closure object so this call is a closure
      StrBufPrintF(buffer,"proto(%s) %d %zu %zu\n",
          frame->closure->proto->proto.str,
          frame->base_ptr,
          frame->pc,
          frame->narg);
    } else {
      if(Vis_udata(&(frame->callable))) {
        struct ObjUdata* udata = Vget_udata(&(frame->callable));
        StrBufPrintF(buffer,"udata(%s) %d %zu %zu\n",
            udata->name.str,
            frame->base_ptr,
            frame->pc,
            frame->narg);
      } else if(Vis_method(&(frame->callable))) {
        struct ObjMethod* method = Vget_method(&(frame->callable));
        StrBufPrintF(buffer,"method(%s) %d %zu %zu\n",
            method->name->str,
            frame->base_ptr,
            frame->pc,
            frame->narg);
      } else if(Vis_str(&(frame->callable))) {
        struct ObjStr*  str = Vget_str(&(frame->callable));
        StrBufPrintF(buffer,"intrinsic(%s) %d %zu %zu\n",
            str->str,
            frame->base_ptr,
            frame->pc,
            frame->narg);
      } else {
        assert(!"unreachable!"); /* We should never reach here */
      }
    }
  }
}

void RuntimeError( struct Runtime* rt , const char* format , ... ) {
  va_list vl;
  struct StrBuf sbuf;
  StrBufInit(&sbuf,1024);

  /* Append the old error string */
  StrBufAppendCStr(&sbuf,&rt->error);

  va_start(vl,format);

  /* Then do a error message format */
  StrBufVPrintF(&sbuf,format,vl);

  /* Append a linebreak */
  StrBufAppendChar(&sbuf,'\n');

  rt->error = StrBufToCStr(&sbuf);

  StrBufDestroy(&sbuf);
}

static void exec_error( struct Runtime* rt , const char* format , ... ) {
  va_list vl;
  struct StrBuf sbuf;
  StrBufInit(&sbuf,1024);
  va_start(vl,format);

  /* Append any existed error information */
  StrBufAppendCStr(&sbuf,&(rt->error));

  /* Then do a error message format */
  StrBufVPrintF(&sbuf,format,vl);

  /* Append a linebreak */
  StrBufAppendChar(&sbuf,'\n');

  /* Now do a stack unwind */
  do_stackunwind(rt,&sbuf);

  rt->error = StrBufToCStr(&sbuf);
  StrBufDestroy(&sbuf);
}

/* VV type handler. Those handlers will do type checking and serve as
 * polymorphism operators. It is typically the slow path */
static SPARROW_INLINE
Value vm_addvv( struct Runtime* rt , Value l , Value r , int* fail ) {
  Value ret;
  double ln,rn;

  /* 1. Try resolve it as number */
  ln = ValueConvNumber(l,fail);
  if(!*fail) {
    rn = ValueConvNumber(r,fail);
    if(!*fail) {
      Vset_number(&ret,ln+rn);
      return ret;
    }
  }

  /* 2. Try resolve it as string */
  if(Vis_str(&l) && Vis_str(&r)) {
    Vset_str(&ret,ObjStrCat(
          RTSparrow(rt),
          Vget_str(&l),
          Vget_str(&r)));
    *fail = 0;
    return ret;
  }

  exec_error(rt,PERR_OPERATOR_TYPE_MISMATCH,
      "+",
      ValueGetTypeString(l),
      ValueGetTypeString(r));
  *fail = 1;
  return ret;
}

#define DO(OP,NAME,INSTR,CHECK) \
  static SPARROW_INLINE \
  Value vm_##INSTR( struct Runtime* rt , Value l , Value r , int* fail ) { \
    Value ret; \
    double ln, rn; \
    Vset_null(&ret); \
    ln = ValueConvNumber(l,fail); \
    if(!*fail) { \
      rn = ValueConvNumber(r,fail); \
      if(!*fail) { \
        CHECK(rn); \
        Vset_number(&ret,ln OP rn); \
        return ret; \
      } \
    } \
    *fail = 1; \
    exec_error(rt,PERR_OPERATOR_TYPE_MISMATCH, \
        NAME, \
        ValueGetTypeString(l), \
        ValueGetTypeString(r)); \
    return ret; \
  }

#define NULL_CHECK(X) (void)(X)
#define ZERO_CHECK(X) \
  do { \
    if((X) == 0) { \
      *fail = 1; \
      exec_error(rt,PERR_DIVIDE_ZERO); \
      return ret; \
    } \
  } while(0)

/* vm_subvv */
DO(-,"-",subvv,NULL_CHECK)

/* vm_mulvv */
DO(*,"*",mulvv,NULL_CHECK)

/* vm_divvv */
DO(/,"/",divvv,ZERO_CHECK)

#undef DO
#undef ZERO_CHECK
#undef NULL_CHECK

static SPARROW_INLINE
Value vm_modvv( struct Runtime* rt , Value  l , Value r , int* fail ) {
  Value ret;
  double ln,rn;
  Vset_null(&ret);
  ln = ValueConvNumber(l,fail);
  if(!*fail) {
    rn = ValueConvNumber(r,fail);
    if(!*fail) {
      int li,ri;
      if(rn == 0) {
        goto fail_dzero;
      }
      if(ToInt(ln,&li)) {
        exec_error(rt, PERR_MOD_OUT_OF_RANGE,"left");
        goto fail_oob;
      }
      if(ToInt(rn,&ri)) {
        exec_error(rt, PERR_MOD_OUT_OF_RANGE,"right");
        goto fail_oob;
      }
      Vset_number(&ret,li % ri);
      return ret;
    }
  }
  *fail = 1;
  exec_error(rt,
      PERR_OPERATOR_TYPE_MISMATCH,
      "%",
      ValueGetTypeString(l),
      ValueGetTypeString(r));
  return ret;

fail_dzero:
  *fail = 1;
  exec_error(rt, PERR_DIVIDE_ZERO, "%");
  return ret;

fail_oob:
  *fail = 1;
  return ret;
}

static SPARROW_INLINE
Value vm_powvv( struct Runtime* rt , Value l , Value r , int* fail ) {
  Value ret;
  double ln,rn;
  Vset_null(&ret);
  ln = ValueConvNumber(l,fail);
  if(!*fail) {
    rn = ValueConvNumber(r,fail);
    if(!*fail) {
      Vset_number(&ret,pow(ln,rn));
      return ret;
    }
  }
  *fail = 1;
  exec_error(rt,
      PERR_OPERATOR_TYPE_MISMATCH,
      "^",
      ValueGetTypeString(l),
      ValueGetTypeString(r));
  return ret;
}

static SPARROW_INLINE
Value vm_not( struct Runtime* rt , Value tos ) {
  Value ret;
  UNUSE_ARG(rt);
  Vset_boolean(&ret,!ValueToBoolean(rt,tos));
  return ret;
}

static SPARROW_INLINE
Value vm_test( struct Runtime* rt, Value tos ) {
  Value ret;
  UNUSE_ARG(rt);
  Vset_boolean(&ret,ValueToBoolean(rt,tos));
  return ret;
}

#define DO(OP,NAME,INSTR,OTHER_CHECK) \
  static SPARROW_INLINE \
  Value vm_##INSTR( struct Runtime* rt, Value l, Value r, int* fail ) { \
    Value ret; \
    UNUSE_ARG(rt); \
    double ln,rn; \
    Vset_null(&ret); \
    ln = ValueConvNumber(l,fail); \
    if(!*fail) { \
      rn = ValueConvNumber(r,fail); \
      if(!*fail) { \
        Vset_boolean(&ret,ln OP rn); \
        return ret; \
      } else { \
        /* Don't need to test string */ \
        goto other; \
      } \
    } \
    if(Vis_str(&l) && Vis_str(&r)) { \
      Vset_boolean(&ret,ObjStrCmp(Vget_str(&l),Vget_str(&r)) OP 0); \
      *fail = 0; \
      return ret; \
    } \
other: \
    OTHER_CHECK(); \
    *fail = 1; \
    exec_error(rt, \
        PERR_OPERATOR_TYPE_MISMATCH, \
        NAME, \
        ValueGetTypeString(l), \
        ValueGetTypeString(r)); \
    return ret; \
  }

#define NULL_CHECK() (void)(NULL)

/* vm_ltvv */
DO(<,"<",ltvv,NULL_CHECK)

/* vm_levv */
DO(<=,"<=",levv,NULL_CHECK)

/* vm_gtvv */
DO(>,">",gtvv,NULL_CHECK)

/* vm_gevv */
DO(>=,">=",gevv,NULL_CHECK)

/* vm_eqvv */
#define NULL_EQ_TYPE_CHECK() \
  do { \
    if(Vis_null(&l) || Vis_null(&r)) { \
      Vset_boolean(&ret,Vis_null(&l) && Vis_null(&r)); \
      *fail = 0; \
      return ret; \
    } \
  } while(0)

DO(==,"==",eqvv,NULL_EQ_TYPE_CHECK)

/* vm_nevv */
#define NULL_NE_TYPE_CHECK() \
  do { \
    if(Vis_null(&l) || Vis_null(&r)) { \
      Vset_boolean(&ret,(Vis_null(&l) ^ Vis_null(&r))); \
      *fail = 0; \
      return ret; \
    } \
  } while(0)

DO(!=,"!=",nevv,NULL_NE_TYPE_CHECK)

#undef NULL_CHECK
#undef NULL_EQ_TYPE_CHECK
#undef NULL_NE_TYPE_CHECK
#undef DO

static SPARROW_INLINE
Value vm_newlist( struct Runtime* rt, int narg ) {
  struct ObjList* l;
  struct CallThread* thread= RTCallThread(rt);
  int i;
  Value ret;
  l = ObjNewList(thread->sparrow,narg);
  for( i = narg-1 ; i >= 0 ; --i ) {
    ObjListPush(l,top(thread,i));
  }
  Vset_list(&ret,l);
  return ret;
}

static SPARROW_INLINE
Value vm_newmap( struct Runtime* rt , int narg , int* fail ) {
  struct ObjMap* m;
  struct CallThread* thread = RTCallThread(rt);
  int i;
  Value ret;
  Vset_null(&ret);
  m = ObjNewMap(thread->sparrow,NextPowerOf2Size(narg));
  for( i = 2 *(narg-1) ; i>=0 ; i -= 2 ) {
    Value key = top(thread,i+1);
    Value val = top(thread,i);
    if(!Vis_str(&key)) {
      *fail = 1;
      exec_error(rt,
          PERR_MAP_KEY_NOT_STRING,
          ValueGetTypeString(key));
      return ret;
    }
    ObjMapPut(m,Vget_str(&key),val);
  }
  Vset_map(&ret,m);
  *fail = 0;
  return ret;
}

static SPARROW_INLINE
Value vm_agets( struct Runtime* rt , Value obj ,
    struct ObjStr* key , int* fail ) {
  struct Sparrow* sparrow = RTSparrow(rt);
  Value ret;
  if(Vis_map(&obj)) {
    struct ObjMap* map = Vget_map(&obj);
    if(map->mops) {
      int r;
      Value k;
      Vset_str(&k,key);
      INVOKE_METAOPS("map",rt,
          map->mops,
          get,
          r,
          sparrow,
          obj,
          k,
          &ret);
      *fail = r ? 1 : 0;
    } else {
      if(ObjMapFind(Vget_map(&obj),key,&ret)) {
        *fail = 1;
        exec_error(rt,PERR_TYPE_NO_ATTRIBUTE,"map",key->str);
        return ret;
      }
      *fail = 0;
    }
    return ret;
  } else if(Vis_component(&obj)) {
    struct ObjComponent* comp = Vget_component(&obj);
    if(ObjMapFind(comp->env,key,&ret)) {
      *fail = 1;
      exec_error(rt,PERR_TYPE_NO_ATTRIBUTE,"component",key->str);
      return ret;
    }
    *fail = 0;
    return ret;
  } else if(Vis_udata(&obj)) {
    struct ObjUdata* udata = Vget_udata(&obj);
    Value k;
    Vset_str(&k,key);
    int r;
    INVOKE_METAOPS(udata->name.str,
        rt,
        (udata->mops),
        get,
        r,
        sparrow,
        obj,
        k,
        &ret);
    if(r == 0) {
      *fail = 0;
    } else {
      *fail = 1;
    }
    return ret;
  } else {
    exec_error(rt,PERR_TYPE_NO_ATTRIBUTE,ValueGetTypeString(obj),key->str);
    *fail = 1;
    return ret;
  }
}

static SPARROW_INLINE
Value vm_agetn( struct Runtime* rt , Value obj,
    size_t index , int* fail ) {
  struct Sparrow* sparrow = RTSparrow(rt);
  Value ret;
  if(Vis_str(&obj)) {
    struct ObjStr* str = Vget_str(&obj);
    struct ObjStr* res;
    if(index >= str->len) {
      exec_error(rt,PERR_INDEX_OUT_OF_RANGE);
      *fail = 1;
      return ret;
    }
    res = ObjNewStrFromChar(sparrow,str->str[index]);
    *fail = 0;
    Vset_str(&ret,res);
    return ret;
  } else if(Vis_list(&obj)) {
    struct ObjList* list = Vget_list(&obj);
    if(index >= list->size) {
      exec_error(rt,PERR_INDEX_OUT_OF_RANGE);
      *fail = 1;
      return ret;
    }
    *fail = 0;
    return list->arr[index];
  } else if(Vis_map(&obj)) {
    struct ObjMap* map = Vget_map(&obj);
    if(map->mops) {
      int r;
      Value idx;
      Vset_number(&idx,index);
      INVOKE_METAOPS("map",
          rt,
          map->mops,
          get,
          r,
          RTSparrow(rt),
          obj,
          idx,
          &ret);
      *fail = r ? 1 : 0;
      return ret;
    } else goto failed;
  } else if(Vis_udata(&obj)) {
    Value key;
    struct ObjUdata* udata = Vget_udata(&obj);
    Vset_number(&key,index);
    int r;
    INVOKE_METAOPS(udata->name.str,
        rt,
        (udata->mops),
        get,
        r,
        RTSparrow(rt),
        obj,
        key,
        &ret);
    *fail = r ? 1 : 0;
    return ret;
  } else {

failed:
    *fail = 1;
    exec_error(rt,PERR_TYPE_NO_INDEX,ValueGetTypeString(obj),index);
    return ret;
  }
}

static SPARROW_INLINE
Value vm_ageti( struct Runtime* rt , Value obj,
    int index , int* fail ) {
  enum IntrinsicAttribute iattr = (enum IntrinsicAttribute)(index);
  struct Sparrow* sparrow = RTSparrow(rt);
  Value ret;
  if(Vis_map(&obj)) {
    struct ObjMap* map = Vget_map(&obj);
    if(map->mops) {
      int r;
      INVOKE_METAOPS("map",
          rt,
          map->mops,
          geti,
          r,
          sparrow,
          obj,
          iattr,
          &ret);
      *fail = r ? 1 : 0;
    } else {
      const struct ObjStr* name = IAttrGetObjStr(sparrow,iattr);
      if(ObjMapFind(map,name,&ret)) {
        *fail = 1;
        exec_error(rt,PERR_TYPE_NO_ATTRIBUTE,"map",name);
        return ret;
      }
      *fail = 0;
    }
    return ret;
  } else if(Vis_udata(&obj)) {
    struct ObjUdata* udata = Vget_udata(&obj);
    int r;
    INVOKE_METAOPS(udata->name.str,
        rt,
        (udata->mops),
        geti,
        r,
        sparrow,
        obj,
        iattr,
        &ret);
    if(r == 0) {
      *fail = 0;
    } else {
      *fail = 1;
    }
    return ret;
  } else {
    *fail = 1;
    exec_error(rt,PERR_TYPE_NO_ATTRIBUTE,ValueGetTypeString(obj),
      IAttrGetName(iattr));
    return ret;
  }
}

static SPARROW_INLINE
Value vm_aget( struct Runtime* rt, Value object, Value key, int* fail ) {
  struct Sparrow* sparrow = RTSparrow(rt);
  Value ret;
  if(Vis_str(&object)) {
    if(Vis_number(&key)) {
      size_t index;
      if(ToSize(Vget_number(&key),&index) ||
         index >= Vget_str(&object)->len) {
        exec_error(rt,PERR_INDEX_OUT_OF_RANGE);
        *fail = 1;
        return ret;
      }
      Vset_str(&ret,ObjNewStrFromChar(sparrow,Vget_str(&object)->str[index]));
    } else {
      *fail = 1;
      exec_error(rt,PERR_ATTRIBUTE_TYPE,"string",ValueGetTypeString(key));
      return ret;
    }
    *fail = 0;
    return ret;
  } else if(Vis_list(&object)) {
    if(Vis_number(&key)) {
      size_t index;
      if(ToSize(Vget_number(&key),&index) ||
         index >= Vget_list(&object)->size) {
        exec_error(rt,PERR_INDEX_OUT_OF_RANGE);
        *fail = 1;
        return ret;
      }
      ret = Vget_list(&object)->arr[index];
    } else {
      *fail = 1;
      exec_error(rt,PERR_ATTRIBUTE_TYPE,"list",ValueGetTypeString(key));
      return ret;
    }
    *fail = 0;
    return ret;
  } else if(Vis_map(&object)) {
    struct ObjMap* map = Vget_map(&object);
    if(map->mops) {
      int r;
      INVOKE_METAOPS("map",
          rt,
          map->mops,
          get,
          r,
          RTSparrow(rt),
          object,
          key,
          &ret);
      *fail = r ? 1 : 0;
    } else {
      if(Vis_str(&key)) {
        if(ObjMapFind(Vget_map(&object),Vget_str(&key),&ret)) {
          *fail = 1;
          exec_error(rt,PERR_TYPE_NO_ATTRIBUTE,"map",Vget_str(&key)->str);
          return ret;
        }
      } else {
        *fail = 1;
        exec_error(rt,PERR_ATTRIBUTE_TYPE,"map",ValueGetTypeString(key));
        return ret;
      }
      *fail = 0;
    }
    return ret;
  } else if(Vis_udata(&object)) {
    struct ObjUdata* udata = Vget_udata(&object);
    int r;
    INVOKE_METAOPS(udata->name.str,
        rt,
        (udata->mops),
        get,
        r,
        sparrow,
        object,
        key,
        &ret);
    if(r == 0) {
      *fail = 0;
    } else {
      *fail = 1;
    }
    return ret;
  } else {
    exec_error(rt,PERR_ATTRIBUTE_TYPE,ValueGetTypeString(object),
        ValueGetTypeString(key));
    *fail = 1;
    return ret;
  }
}

static SPARROW_INLINE
void vm_asetn( struct Runtime* rt,
    Value object,
    size_t index,
    Value value ,
    int* fail ) {
  if(Vis_list(&object)) {
    struct ObjList* l = Vget_list(&object);
    ObjListAssign(l,index,value);
    *fail = 0;
  } else if(Vis_map(&object)) {
    struct ObjMap* map = Vget_map(&object);
    if(map->mops) {
      int r;
      Value idx;
      Vset_number(&idx,index);
      INVOKE_METAOPS("map",
          rt,
          map->mops,
          set,
          r,
          RTSparrow(rt),
          object,
          idx,
          value);
      *fail = r ? 1 : 0;
    } else goto failed;
  } else if(Vis_udata(&object)) {
    struct ObjUdata* udata = Vget_udata(&object);
    struct Sparrow* sparrow = RTSparrow(rt);
    Value key;
    int r;
    Vset_number(&key,index);
    INVOKE_METAOPS(udata->name.str,
        rt,
        (udata->mops),
        set,
        r,
        sparrow,
        object,
        key,
        value);
    if(r == 0) {
      *fail = 0;
    } else {
      *fail = 1;
    }
  } else {
failed:
    *fail = 1;
    exec_error(rt,PERR_TYPE_NO_SET_INDEX,ValueGetTypeString(object));
  }
}

static SPARROW_INLINE
void vm_asets( struct Runtime* rt,
    Value object,
    struct ObjStr* key,
    Value value,
    int* fail ) {
  if(Vis_map(&object)) {
    struct ObjMap* map = Vget_map(&object);
    if(map->mops) {
      int r;
      Value k;
      Vset_str(&k,key);
      INVOKE_METAOPS("map",
          rt,
          map->mops,
          set,
          r,
          RTSparrow(rt),
          object,
          k,
          value);
      *fail = r ? 1 : 0;
    } else {
      ObjMapPut(map,key,value);
      *fail = 0;
    }
  } else if(Vis_udata(&object)) {
    struct ObjUdata* udata = Vget_udata(&object);
    struct Sparrow* sparrow = RTSparrow(rt);
    Value k;
    int r;

    Vset_str(&k,key);
    INVOKE_METAOPS(udata->name.str,
        rt,
        (udata->mops),
        set,
        r,
        sparrow,
        object,
        k,
        value);
    if(r == 0) {
      *fail = 0;
    } else {
      *fail = 1;
    }
  } else {
    exec_error(rt,PERR_TYPE_NO_SET_ATTRIBUTE,
        ValueGetTypeString(object),key->str);
    *fail = 1;
    return;
  }
}

static SPARROW_INLINE
void vm_aset( struct Runtime* rt,
    Value object,
    Value key,
    Value value,
    int* fail ) {
  *fail = 0;
  if(Vis_map(&object)) {
    struct ObjMap* map = Vget_map(&object);
    if(map->mops) {
      int r;
      INVOKE_METAOPS("map",
          rt,
          map->mops,
          set,
          r,
          RTSparrow(rt),
          object,
          key,
          value);
      *fail = r ? 1 : 0;
    } else {
      if(Vis_str(&key)) {
        ObjMapPut(map,Vget_str(&key),value);
        return;
      } else *fail =1;
    }
  } else if(Vis_list(&object)) {
    size_t index;
    struct ObjList* l = Vget_list(&object);
    if(Vis_number(&key)) {
      if(ToSize(Vget_number(&key),&index)) {
        exec_error(rt,PERR_SIZE_OVERFLOW,Vget_number(&key));
        *fail = 1;
      } else {
        ObjListAssign(l,index,value);
      }
    } else {
      exec_error(rt,PERR_ATTRIBUTE_TYPE,"list",ValueGetTypeString(key));
      *fail = 1;
    }
  } else if(Vis_udata(&object)) {
    struct ObjUdata* udata = Vget_udata(&object);
    struct Sparrow* sparrow = RTSparrow(rt);
    int r;
    INVOKE_METAOPS(udata->name.str,
        rt,
        (udata->mops),
        set,
        r,
        sparrow,
        object,
        key,
        value);
    if(r != 0) {
      *fail = 1;
    }
  } else {
    *fail = 1;
    exec_error(rt,PERR_TYPE_NO_SET_OPERATOR,ValueGetTypeString(object));
  }
}

static SPARROW_INLINE
void vm_aseti( struct Runtime* rt, Value object , Value value ,
    int index , int* fail ) {
  enum IntrinsicAttribute iattr = (enum IntrinsicAttribute)(index);
  if(Vis_map(&object)) {
    struct ObjMap* m = Vget_map(&object);
    if(m->mops) {
      int r;
      INVOKE_METAOPS("map",
          rt,
          m->mops,
          seti,
          r,
          RTSparrow(rt),
          object,
          iattr,
          value);
      *fail = r ? 1 : 0;
    } else {
      struct ObjStr* oname = IAttrGetObjStr(RTSparrow(rt),iattr);
      ObjMapPut(m,oname,value);
      *fail = 0;
    }
  } else if(Vis_udata(&object)) {
    struct ObjUdata* udata = Vget_udata(&object);
    int r;
    INVOKE_METAOPS(udata->name.str,
        rt,
        (udata->mops),
        seti,
        r,
        RTSparrow(rt),
        object,
        iattr,
        value);
    if(r == 0 ) {
      *fail = 0;
    } else {
      *fail = 1;
    }
  } else {
    exec_error(rt,PERR_TYPE_NO_SET_OPERATOR,ValueGetTypeString(object));
    *fail = 1;
  }
}

/* upvalue setting and getting */
static SPARROW_INLINE
Value vm_uget( struct Runtime* rt , int index ) {
  struct ObjClosure* closure = current_frame(RTCallThread(rt))->closure;
  assert(closure);
  assert(index < closure->proto->uv_size);
  return closure->upval[index];
}

static SPARROW_INLINE
void vm_uset( struct Runtime* rt , int index , Value val ) {
  struct ObjClosure* closure = current_frame(RTCallThread(rt))->closure;
  assert(index < closure->proto->uv_size);
  closure->upval[index] = val;
}

static SPARROW_INLINE
int add_callframe( struct Runtime* rt ,int argnum ,
    struct ObjClosure* cls , Value tos ) {
  struct CallThread* thread = RTCallThread(rt);
  struct CallFrame* frame;
  if(SP_UNLIKELY(thread->frame_size == thread->frame_cap)) {
    if(SP_UNLIKELY(thread->frame_size >= rt->max_funccall)) {
      exec_error(rt,PERR_TOO_MANY_FUNCCALL);
      return -1;
    } else {
      size_t ncap = thread->frame_cap*2;
      thread->frame = realloc(thread->frame,ncap*sizeof(struct CallFrame));
      thread->frame_cap = ncap;
    }
  }
  frame = thread->frame + thread->frame_size;
  frame->narg = argnum;
  frame->closure = cls;
  frame->callable = tos;
  frame->base_ptr = thread->stack_size - argnum;
  frame->pc = 0;
  ++thread->frame_size;
  return 0;
}

static SPARROW_INLINE
int del_callframe( struct Runtime* rt ) {
  struct CallThread* thread = RTCallThread(rt);
  assert(thread->frame_size >0);
  /* restore the previous stack_size via base_ptr */
  thread->stack_size = RTCurFrame(rt)->base_ptr;
  --thread->frame_size;
  if(thread->frame_size > 0) {
    if(current_frame(thread)->base_ptr== RETURN_TO_HOST)
      return -1;
    return 0;
  } else {
    return -1; /* We don't have any frame in callstack , so
                * return to the outer most caller in C */
  }
}

enum {
  CALLERROR,
  SPARROWFUNC,
  CFUNC,
};

static SPARROW_INLINE
int vm_call( struct Runtime* rt , Value tos , int argnum , Value* ret ) {
  if(Vis_method(&tos)) {
    struct ObjMethod* method = Vget_method(&tos);
    if(add_callframe(rt,argnum,NULL,tos)) return CALLERROR;
    if(method->method(RTSparrow(rt),method->object,ret)) {
      exec_error(rt,PERR_FUNCCALL_FAILED);
      return CALLERROR;
    }
    del_callframe(rt);
    return CFUNC;
  } else if(Vis_udata(&tos)) {
    struct ObjUdata* udata = Vget_udata(&tos);
    int r;
    if(add_callframe(rt,argnum,NULL,tos)) return CALLERROR;
    r = udata->call(RTSparrow(rt),udata,ret);
    if(r == 0) {
      del_callframe(rt);
      return CFUNC;
    } else {
      return CALLERROR;
    }
  } else if(Vis_closure(&tos)) {
    struct ObjClosure* cls = Vget_closure(&tos);
    Value null;
    Vset_null(&null);
    if(add_callframe(rt,argnum,cls,null)) return CALLERROR;
    return SPARROWFUNC;
  } else {
    exec_error(rt,PERR_NOT_FUNCCALL,ValueGetTypeString(tos));
    return CALLERROR;
  }
}

static SPARROW_INLINE
Value vm_forprep( struct Runtime* rt , Value tos , int* invalid , int* fail ) {
  struct ObjIterator* itr;
  Value ret;
  Vset_null(&ret);
  *fail = 0;
  if(Vis_str(&tos)) {
    itr = ObjNewIterator(RTSparrow(rt));
    ObjStrIterInit(Vget_str(&tos),itr);
    Vset_iterator(&ret,itr);
    *invalid = itr->has_next(RTSparrow(rt),itr);
    return ret;
  } else if(Vis_list(&tos)) {
    itr = ObjNewIterator(RTSparrow(rt));
    ObjListIterInit(Vget_list(&tos),itr);
    Vset_iterator(&ret,itr);
    *invalid = itr->has_next(RTSparrow(rt),itr);
    return ret;
  } else if(Vis_map(&tos)) {
    itr = ObjNewIterator(RTSparrow(rt));
    ObjMapIterInit(Vget_map(&tos),itr);
    Vset_iterator(&ret,itr);
    *invalid = itr->has_next(RTSparrow(rt),itr);
    return ret;
  } else if(Vis_loop(&tos)) {
    struct ObjLoopIterator* litr = ObjNewLoopIterator(RTSparrow(rt),
        Vget_loop(&tos));
    Vset_loop_iterator(&ret,litr);
    *invalid = litr->index >= litr->end;
    return ret;
  } else if(Vis_udata(&tos)) {
    struct ObjUdata* udata = Vget_udata(&tos);
    int r;
    struct ObjIterator* itr = ObjNewIterator(RTSparrow(rt));
    INVOKE_METAOPS(udata->name.str,
        rt,
        (udata->mops),
        iter,
        r,
        RTSparrow(rt),
        tos,
        itr);
    if(r == 0) {
      *fail = 0;
      Vset_iterator(&ret,itr);
      *invalid = itr->has_next(RTSparrow(rt),itr);
    } else {
      *fail = 1;
    }
    return ret;
  }
  *fail = 1;
  exec_error(rt,PERR_TYPE_ITERATOR,ValueGetTypeString(tos));
  return ret;
}

static SPARROW_INLINE
Value vm_gget( struct Runtime* rt , struct ObjStr* key , int* fail ) {
  /* Try to find out which global value is in component wide environment */
  Value ret;
  if(ObjMapFind(RTCallThread(rt)->component->env,key,&ret)) {
    /* Find in global table */
    if(ObjMapFind(&(global_env(rt).env),key,&ret)) {
      *fail = 1;
      exec_error(rt,"Cannot find global variable %s!",key->str);
      return ret;
    }
  }
  *fail = 0;
  return ret;
}

/* vm_main related macro helpers */
#define FATAL(...) \
  do { \
    exec_error(__VA_ARGS__); \
    goto fail; \
  } while(0)

#define DECODE_ARG() \
  do { \
    opr = CodeBufferDecodeArg(&(proto->code_buf),frame->pc); \
    frame->pc += 3; \
  } while(0)

#define SKIP_ARG() \
  do { \
    frame->pc += 3; \
  } while(0)

#define check &fail); if(fail) goto fail; (void)(NULL

static int vm_main( struct Runtime* rt , Value* ret ) {
  struct Sparrow* sparrow= RTSparrow(rt);

  /* operand registers */
  Value l, r, tos , res;
  int fail;
  uint8_t op;
  uint32_t opr;

  /* context variables */
  struct CallThread* thread = rt->cur_thread;
  struct CallFrame* frame = current_frame(thread);
  struct ObjClosure* closure = frame->closure;
  struct ObjProto* proto = closure->proto;

  /* sink static analyzer's stupid error */
  Vset_null(ret);

#ifndef SPARROW_VM_NO_THREADING
  /* when we reach here, it means we will do a threading
   * interpreter. This relies on compiler to provide us
   * computed goto statements */
  static const void* jump_table[] = {
#define __(A,B,C) &&label_##A,
    BYTECODE(__)
    NULL
#undef __
  };
#define CASE(X) label_##X:

#ifndef SPARROW_VM_INSTRUCTION_CHECK
#define DISPATCH() \
  do { \
    op = proto->code_buf.buf[frame->pc++]; \
    goto *jump_table[op]; \
  } while(0)
#else
#define DISPATCH() \
  do { \
    op = proto->code_buf.buf[frame->pc++]; \
    verify(op >=0 && op < SIZE_OF_BYTECODE); \
    goto *jump_table[op]; \
  } while(0)
#endif /* SPARROW_VM_INSTRUCTION_CHECK */

  DISPATCH(); /* very first dispatch */
#else
  /* Now we do a normal for loop switch case dispatch table
   * interpreter. */
#define CASE(X) case X:
#define DISPATCH() break
  while(1) {
    op = proto->code_buf.buf[frame->pc++];
    switch(op) {
#endif /* SPARROW_VM_NO_THREADING */

  CASE(BC_ADDNV) {
    double ln;
    DECODE_ARG();
    ln = proto->num_arr[opr];
    r = top(thread,0);
    if(Vis_number(&r)) {
      Vset_number(&res,ln + Vget_number(&r));
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"right","number",ValueGetTypeString(r));
    }
    DISPATCH();
  }

  CASE(BC_ADDVN) {
    double rn;
    DECODE_ARG();
    rn = proto->num_arr[opr];
    l = top(thread,0);
    if(Vis_number(&l)) {
      Vset_number(&res,Vget_number(&l)+rn);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"left","number",ValueGetTypeString(l));
    }
    DISPATCH();
  }

  CASE(BC_ADDVV) {
    l = left(thread);
    r = right(thread);
    res = vm_addvv( rt , l , r , check );
    pop(thread,2);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_ADDSV) {
    struct ObjStr* lstr;

    DECODE_ARG();
    lstr = proto->str_arr[opr];
    r = top(thread,0);
    if(Vis_str(&r)) {
      Vset_str(&res,ObjStrCat(thread->sparrow,lstr,Vget_str(&r)));
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"right","string",ValueGetTypeString(r));
    }
    DISPATCH();
  }

  CASE(BC_ADDVS) {
    struct ObjStr* rstr;
    DECODE_ARG();
    rstr = proto->str_arr[opr];
    l = top(thread,0);
    if(Vis_str(&l)) {
      Vset_str(&res,ObjStrCat(thread->sparrow,Vget_str(&l),rstr));
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"left","string",ValueGetTypeString(l));
    }
    DISPATCH();
  }

  CASE(BC_SUBNV) {
    double ln;
    DECODE_ARG();
    ln = proto->num_arr[opr];
    r = top(thread,0);
    if(Vis_number(&r)) {
      Vset_number(&res,ln-Vget_number(&r));
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"right","number",ValueGetTypeString(r));
    }
    DISPATCH();
  }

  CASE(BC_SUBVN) {
    double rn;
    DECODE_ARG();
    rn = proto->num_arr[opr];
    l = top(thread,0);
    if(Vis_number(&l)) {
      Vset_number(&res,Vget_number(&l)-rn);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"left","number",ValueGetTypeString(l));
    }
    DISPATCH();
  }

  CASE(BC_SUBVV) {
    l = left(thread);
    r = right(thread);
    res = vm_subvv(rt,l,r,check);
    pop(thread,2);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_MULNV) {
    double ln;
    DECODE_ARG();
    ln = proto->num_arr[opr];
    r = top(thread,0);
    if(Vis_number(&r)) {
      Vset_number(&res,ln*Vget_number(&r));
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"right","number",ValueGetTypeString(r));
    }
    DISPATCH();
  }

  CASE(BC_MULVN) {
    double rn;
    DECODE_ARG();
    rn = proto->num_arr[opr];
    l = top(thread,0);
    if(Vis_number(&l)) {
      Vset_number(&res,rn*Vget_number(&l));
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"left","number",ValueGetTypeString(l));
    }
    DISPATCH();
  }

  CASE(BC_MULVV) {
    l = left(thread);
    r = right(thread);
    res = vm_mulvv(rt,l,r,check);
    pop(thread,2);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_DIVNV) {
    double ln;
    DECODE_ARG();
    ln = proto->num_arr[opr];
    r = top(thread,0);
    if(Vis_number(&r)) {
      double rn = Vget_number(&r);
      if(rn ==0) {
        FATAL(rt,PERR_DIVIDE_ZERO);
      }
      Vset_number(&res,ln/rn);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"right","number",ValueGetTypeString(r));
    }
    DISPATCH();
  }

  CASE(BC_DIVVN) {
    double rn;
    DECODE_ARG();
    rn = proto->num_arr[opr];
    l = top(thread,0);
    if(Vis_number(&l)) {
      double ln = Vget_number(&l);
      if(ln == 0) {
        FATAL(rt,PERR_DIVIDE_ZERO);
      }
      Vset_number(&res,ln/rn);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"left","number",ValueGetTypeString(l));
    }
    DISPATCH();
  }

  CASE(BC_DIVVV) {
    l = left(thread);
    r = right(thread);
    res = vm_divvv(rt,l,r,check);
    pop(thread,2);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_MODVN) {
    double rn;
    DECODE_ARG();
    rn = proto->num_arr[opr];
    l = top(thread,0);
    if(Vis_number(&l)) {
      double ln = Vget_number(&l);
      int ri , li;
      if(rn == 0) {
        FATAL(rt,PERR_DIVIDE_ZERO);
      }
      if(ToInt(rn,&ri)) {
        FATAL(rt,PERR_MOD_OUT_OF_RANGE,"right");
      }
      if(ToInt(ln,&li)) {
        FATAL(rt,PERR_MOD_OUT_OF_RANGE,"left");
      }
      Vset_number(&res,li % ri);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"left","number",ValueGetTypeString(l));
    }
    DISPATCH();
  }

  CASE(BC_MODNV) {
    double ln;
    DECODE_ARG();
    ln = proto->num_arr[opr];
    r = top(thread,0);
    if(Vis_number(&r)) {
      double rn = Vget_number(&r);
      int ri , li;
      if(rn == 0) {
        FATAL(rt,PERR_DIVIDE_ZERO);
      }
      if(ToInt(rn,&ri)) {
        FATAL(rt,PERR_MOD_OUT_OF_RANGE,"right");
      }
      if(ToInt(ln,&li)) {
        FATAL(rt,PERR_MOD_OUT_OF_RANGE,"left");
      }
      Vset_number(&res,li % ri);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"left","number",ValueGetTypeString(r));
    }
    DISPATCH();
  }

  CASE(BC_MODVV) {
    l = left(thread);
    r = right(thread);
    res = vm_modvv(rt,l,r,check);
    pop(thread,2);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_POWNV) {
    double ln;
    DECODE_ARG();
    ln = proto->num_arr[opr];
    r = top(thread,0);
    if(Vis_number(&r)) {
      Vset_number(&res,pow(ln,Vget_number(&r)));
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"right","number",ValueGetTypeString(r));
    }
    DISPATCH();
  }

  CASE(BC_POWVN) {
    double rn;
    DECODE_ARG();
    rn = proto->num_arr[opr];
    l = top(thread,0);
    if(Vis_number(&l)) {
      Vset_number(&res,pow(Vget_number(&l),rn));
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"left","number",ValueGetTypeString(l));
    }
    DISPATCH();
  }

  CASE(BC_POWVV) {
    l = left(thread);
    r = right(thread);
    res = vm_powvv(rt,l,r,check);
    pop(thread,2);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_NEG) {
    double num;
    tos = top(thread,0);
    num = ValueConvNumber(tos,&fail);
    if(fail) {
      FATAL(rt,PERR_TOS_TYPE_MISMATCH,ValueGetTypeString(tos),"number");
    } else {
      Vset_number(&res,-num);
      replace(thread,res);
    }
    DISPATCH();
  }

  CASE(BC_NOT) {
    tos = top(thread,0);
    res = vm_not(rt,tos);
    replace(thread,res);
    DISPATCH();
  }

  CASE(BC_TEST) {
    tos = top(thread,0);
    res = vm_test(rt,tos);
    replace(thread,res);
    DISPATCH();
  }

  CASE(BC_LOADN) {
    DECODE_ARG();
    Vset_number(&res,proto->num_arr[opr]);
    push(thread,res);
    DISPATCH();
  }

#define DO(NUM) \
  CASE(BC_LOADN##NUM) { \
    Vset_number(&res,NUM); \
    push(thread,res); \
    DISPATCH(); \
  }

  /* BC_LOADN0 */
  DO(0)

  /* BC_LOADN1 */
  DO(1)

  /* BC_LOADN2 */
  DO(2)

  /* BC_LOADN3 */
  DO(3)

  /* BC_LOADN4 */
  DO(4)

  /* BC_LOADN5 */
  DO(5)

#undef DO

#define DO(NUM) \
  CASE(BC_LOADNN##NUM) { \
    Vset_number(&res,-(NUM)); \
    push(thread,res); \
    DISPATCH(); \
  }

  /* BC_LOADNN1 */
  DO(1)

  /* BC_LOADNN2 */
  DO(2)

  /* BC_LOADNN3 */
  DO(3)

  /* BC_LOADNN4 */
  DO(4)

  /* BC_LOADNN5 */
  DO(5)

#undef DO /* DO */

  CASE(BC_LOADS) {
    DECODE_ARG();
    Vset_str(&res,proto->str_arr[opr]);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_LOADV) {
    DECODE_ARG();
    if(frame->base_ptr + opr >= thread->stack_size) {
      /* We can have code that actually tries to load a stack slot that
       * is not in used now , ie no previous push actually push any value
       * to that slot. It is happened when user write code like:
       * var c = c;
       *
       * In this case the LOADV will have a stack slot that is larger than
       * the current stack size. In this case we just load a NULL value into
       * c.
       *
       * We need to support this grammar for closure recursive call reason. */
      Value v; Vset_null(&v);
      push(thread,v);
    } else {
      push(thread,thread->stack[frame->base_ptr+opr]);
    }
    DISPATCH();
  }

  CASE(BC_LOADNULL) {
    Vset_null(&res);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_LOADTRUE) {
    Vset_true(&res);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_LOADFALSE) {
    Vset_false(&res);
    push(thread,res);
    DISPATCH();
      replace(thread,res);
  }

  CASE(BC_MOVE) {
    tos = top(thread,0);
    DECODE_ARG();
    thread->stack[frame->base_ptr+opr] = tos;
    pop(thread,1);
    DISPATCH();
  }

  CASE(BC_MOVETRUE) {
    DECODE_ARG();
    Vset_true(&res);
    thread->stack[frame->base_ptr+opr] = res;
    DISPATCH();
  }

  CASE(BC_MOVEFALSE) {
    DECODE_ARG();
    Vset_false(&res);
    thread->stack[frame->base_ptr+opr] = res;
    DISPATCH();
  }

  CASE(BC_MOVENULL) {
    DECODE_ARG();
    Vset_null(&res);
    thread->stack[frame->base_ptr+opr] = res;
    DISPATCH();
  }

#define DO(NUM) \
  CASE(BC_MOVEN##NUM) { \
    DECODE_ARG(); \
    Vset_number(&res,NUM); \
    thread->stack[frame->base_ptr+opr] = res; \
    DISPATCH(); \
  }

  /* BC_MOVEN0 */
  DO(0)

  /* BC_MOVEN1 */
  DO(1)

  /* BC_MOVEN2 */
  DO(2)

  /* BC_MOVEN3 */
  DO(3)

  /* BC_MOVEN4 */
  DO(4)

  /* BC_MOVEN5 */
  DO(5)

#undef DO /* DO */

#define DO(NUM) \
  CASE(BC_MOVENN##NUM) { \
    DECODE_ARG(); \
    Vset_number(&res,-(NUM)); \
    thread->stack[frame->base_ptr+opr] = res; \
    DISPATCH(); \
  }

  /* BC_MOVENN1 */
  DO(1)

  /* BC_MOVENN2 */
  DO(2)

  /* BC_MOVENN3 */
  DO(3)

  /* BC_MOVENN4 */
  DO(4)

  /* BC_MOVENN5 */
  DO(5)

#undef DO /* DO */

  CASE(BC_LTNV) {
    double ln,rn;
    DECODE_ARG();
    ln = proto->num_arr[opr];
    r = top(thread,0);
    rn = ValueConvNumber(r,&fail);
    if(!fail) {
      Vset_boolean(&res, ln < rn);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"right","number",ValueGetTypeString(r));
    }
    DISPATCH();
  }

  CASE(BC_LTVN) {
    double rn,ln;
    DECODE_ARG();
    rn = proto->num_arr[opr];
    l = top(thread,0);
    ln = ValueConvNumber(l,&fail);
    if(!fail) {
      Vset_boolean(&res,ln<rn);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"left","number",ValueGetTypeString(l));
    }
    DISPATCH();
  }

  CASE(BC_LTSV) {
    struct ObjStr* ls;
    DECODE_ARG();
    ls = proto->str_arr[opr];
    r = top(thread,0);
    if(Vis_str(&r)) {
      Vset_boolean(&res,ObjStrCmp(ls,Vget_str(&r))<0);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"right","string",ValueGetTypeString(r));
    }
    DISPATCH();
  }

  CASE(BC_LTVS) {
    struct ObjStr* rs;
    DECODE_ARG();
    rs = proto->str_arr[opr];
    l = top(thread,0);
    if(Vis_str(&l)) {
      Vset_boolean(&res,ObjStrCmp(rs,Vget_str(&l))<0);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"left","string",ValueGetTypeString(l));
    }
    DISPATCH();
  }

  CASE(BC_LTVV) {
    l = left(thread);
    r = right(thread);
    res = vm_ltvv(rt,l,r,check);
    pop(thread,2);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_LENV) {
    double ln,rn;
    DECODE_ARG();
    ln = proto->num_arr[opr];
    r = top(thread,0);
    rn = ValueConvNumber(r,&fail);
    if(!fail) {
      Vset_boolean(&res,ln <= rn);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"right","number",ValueGetTypeString(r));
    }
    DISPATCH();
  }

  CASE(BC_LEVN) {
    double rn,ln;
    DECODE_ARG();
    rn = proto->num_arr[opr];
    l = top(thread,0);
    ln = ValueConvNumber(l,&fail);
    if(!fail) {
      Vset_boolean(&res,ln<=rn);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"left","number",ValueGetTypeString(l));
    }
    DISPATCH();
  }

  CASE(BC_LESV) {
    struct ObjStr* lstr;
    DECODE_ARG();
    lstr = proto->str_arr[opr];
    r = top(thread,0);
    if(Vis_str(&r)) {
      Vset_boolean(&res,ObjStrCmp(lstr,Vget_str(&r))<=0);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"right","string",ValueGetTypeString(r));
    }
    DISPATCH();
  }

  CASE(BC_LEVS) {
    struct ObjStr* rstr;
    DECODE_ARG();
    rstr = proto->str_arr[opr];
    l = top(thread,0);
    if(Vis_str(&l)) {
      Vset_boolean(&res,ObjStrCmp(Vget_str(&l),rstr)<=0);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"left","string",ValueGetTypeString(l));
    }
    DISPATCH();
  }

  CASE(BC_LEVV) {
    l = left(thread);
    r = right(thread);
    res = vm_levv(rt,l,r,check);
    pop(thread,2);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_GTNV) {
    double ln,rn;
    DECODE_ARG();
    ln = proto->num_arr[opr];
    r = top(thread,0);
    rn = ValueConvNumber(r,&fail);
    if(!fail) {
      Vset_boolean(&res,ln > rn);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"right","number",ValueGetTypeString(r));
    }
    DISPATCH();
  }

  CASE(BC_GTVN) {
    double rn,ln;
    DECODE_ARG();
    rn = proto->num_arr[opr];
    l = top(thread,0);
    ln = ValueConvNumber(l,&fail);
    if(!fail) {
      Vset_boolean(&res,ln  > rn);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"left","number",ValueGetTypeString(l));
    }
    DISPATCH();
  }

  CASE(BC_GTSV) {
    struct ObjStr* lstr;
    DECODE_ARG();
    lstr = proto->str_arr[opr];
    r = top(thread,0);
    if(Vis_str(&r)) {
      Vset_boolean(&res,ObjStrCmp(lstr,Vget_str(&r))>0);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"right","string",ValueGetTypeString(r));
    }
    DISPATCH();
  }

  CASE(BC_GTVS) {
    struct ObjStr* rstr;
    DECODE_ARG();
    rstr = proto->str_arr[opr];
    l = top(thread,0);
    if(Vis_str(&l)) {
      Vset_boolean(&res,ObjStrCmp(Vget_str(&l),rstr)>0);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"left","string",ValueGetTypeString(r));
    }
    DISPATCH();
  }

  CASE(BC_GTVV) {
    l = left(thread);
    r = right(thread);
    res = vm_gtvv(rt,l,r,check);
    pop(thread,2);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_GENV) {
    double ln,rn;
    DECODE_ARG();
    ln = proto->num_arr[opr];
    r = top(thread,0);
    rn = ValueConvNumber(r,&fail);
    if(!fail) {
      Vset_boolean(&res,ln >= rn);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"right","number",ValueGetTypeString(r));
    }
    DISPATCH();
  }

  CASE(BC_GEVN) {
    double rn,ln;
    DECODE_ARG();
    rn = proto->num_arr[opr];
    l = top(thread,0);
    ln = ValueConvNumber(l,&fail);
    if(!fail) {
      Vset_boolean(&res,ln >= rn);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"left","number",ValueGetTypeString(l));
    }
    DISPATCH();
  }

  CASE(BC_GESV) {
    struct ObjStr* lstr;
    DECODE_ARG();
    lstr = proto->str_arr[opr];
    r = top(thread,0);
    if(Vis_str(&r)) {
      Vset_boolean(&res,ObjStrCmp(lstr,Vget_str(&r))>=0);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"right","string",ValueGetTypeString(r));
    }
    DISPATCH();
  }

  CASE(BC_GEVS) {
    struct ObjStr* rstr;
    DECODE_ARG();
    rstr = proto->str_arr[opr];
    l = top(thread,0);
    if(Vis_str(&l)) {
      Vset_boolean(&res,ObjStrCmp(Vget_str(&l),rstr)>=0);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"left","string",ValueGetTypeString(l));
    }
    DISPATCH();
  }

  CASE(BC_GEVV) {
    l = left(thread);
    r = right(thread);
    res = vm_gevv(rt,l,r,check);
    pop(thread,2);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_EQNV) {
    double ln,rn;
    DECODE_ARG();
    ln = proto->num_arr[opr];
    r = top(thread,0);
    rn = ValueConvNumber(r,&fail);
    if(!fail) {
      Vset_boolean(&res,ln == rn);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"right","number",ValueGetTypeString(r));
    }
    DISPATCH();
  }

  CASE(BC_EQVN) {
    double rn,ln;
    DECODE_ARG();
    rn = proto->num_arr[opr];
    l = top(thread,0);
    ln = ValueConvNumber(l,&fail);
    if(!fail) {
      Vset_boolean(&res,ln == rn);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"left","string",ValueGetTypeString(r));
    }
    DISPATCH();
  }

  CASE(BC_EQSV) {
    struct ObjStr* lstr;
    DECODE_ARG();
    lstr = proto->str_arr[opr];
    r = top(thread,0);
    if(Vis_str(&r)) {
      Vset_boolean(&res,ObjStrEqual(lstr,Vget_str(&r)));
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"right","string",ValueGetTypeString(r));
    }
    DISPATCH();
  }

  CASE(BC_EQVS) {
    struct ObjStr* rstr;
    DECODE_ARG();
    rstr = proto->str_arr[opr];
    l = top(thread,0);
    if(Vis_str(&l)) {
      Vset_boolean(&res,ObjStrEqual(Vget_str(&l),rstr));
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"left","string",ValueGetTypeString(l));
    }
    DISPATCH();
  }

  CASE(BC_EQVNULL) {
    l = top(thread,0);
    if(Vis_null(&l)) {
      Vset_true(&res);
    } else {
      Vset_false(&res);
    }
    replace(thread,res);
    DISPATCH();
  }

  CASE(BC_EQNULLV) {
    r = top(thread,0);
    if(Vis_null(&r)) {
      Vset_true(&res);
    } else {
      Vset_false(&res);
    }
    replace(thread,res);
    DISPATCH();
  }

  CASE(BC_EQVV) {
    l = left(thread);
    r = right(thread);
    res = vm_eqvv(rt,l,r,check);
    pop(thread,2);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_NENV) {
    double ln,rn;
    DECODE_ARG();
    ln = proto->num_arr[opr];
    r = top(thread,0);
    rn = ValueConvNumber(r,&fail);
    if(!fail) {
      Vset_boolean(&res,ln != rn);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"right","number",ValueGetTypeString(r));
    }
    DISPATCH();
  }

  CASE(BC_NEVN) {
    double rn,ln;
    DECODE_ARG();
    rn = proto->num_arr[opr];
    l = top(thread,0);
    ln = ValueConvNumber(l,&fail);
    if(!fail) {
      Vset_boolean(&res,ln != rn);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"left","number",ValueGetTypeString(l));
    }
    DISPATCH();
  }

  CASE(BC_NESV) {
    struct ObjStr* lstr;
    DECODE_ARG();
    lstr = proto->str_arr[opr];
    r = top(thread,0);
    if(Vis_str(&r)) {
      Vset_boolean(&res,ObjStrCmp(lstr,Vget_str(&r))!=0);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"right","string",ValueGetTypeString(r));
    }
    DISPATCH();
  }

  CASE(BC_NEVS) {
    struct ObjStr* rstr;
    DECODE_ARG();
    rstr = proto->str_arr[opr];
    l = top(thread,0);
    if(Vis_str(&l)) {
      Vset_boolean(&res,ObjStrCmp(Vget_str(&l),rstr)!=0);
      replace(thread,res);
    } else {
      FATAL(rt,PERR_TYPE_MISMATCH,"left","string",ValueGetTypeString(l));
    }
    DISPATCH();
  }

  CASE(BC_NEVNULL) {
    l = top(thread,0);
    if(Vis_null(&l)) {
      Vset_false(&res);
    } else {
      Vset_true(&res);
    }
    replace(thread,res);
    DISPATCH();
  }

  CASE(BC_NENULLV) {
    r = top(thread,0);
    if(Vis_null(&r)) {
      Vset_false(&res);
    } else {
      Vset_true(&res);
    }
    replace(thread,res);
    DISPATCH();
  }

  CASE(BC_NEVV) {
    l = left(thread);
    r = right(thread);
    res = vm_nevv(rt,l,r,check);
    pop(thread,2);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_BRK)
  CASE(BC_CONT)
  CASE(BC_ENDIF) {
    DECODE_ARG();
    frame->pc = opr;
    DISPATCH();
  }

  CASE(BC_IF) {
    DECODE_ARG();
    tos = top(thread,0);
    if(!ValueToBoolean(rt,tos)) {
      frame->pc = opr;
    }
    pop(thread,1);
    DISPATCH();
  }

  CASE(BC_BRT) {
    tos = top(thread,0);
    if(ValueToBoolean(rt,tos)) {
      /* We replace the TOS value and convert it to boolean
       * this save us a test instruction when do code gen */
      Vset_true(&tos);
      replace(thread,tos);
      DECODE_ARG();
      frame->pc = opr;
    } else {
      SKIP_ARG(); /* Skip the argument */
      pop(thread,1);
    }
    DISPATCH();
  }

  CASE(BC_BRF) {
    tos = top(thread,0);
    if(!ValueToBoolean(rt,tos)) {
      Vset_false(&tos);
      replace(thread,tos);
      DECODE_ARG();
      frame->pc = opr;
    } else {
      SKIP_ARG(); /* Skip the argument */
      pop(thread,1);
    }
    DISPATCH();
  }

  CASE(BC_NEWL0) {
    Vset_list(&res,ObjNewList(thread->sparrow,0));
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_NEWL1) {
    res = vm_newlist(rt,1);
    replace(thread,res);
    DISPATCH();
  }

  CASE(BC_NEWL2) {
    res = vm_newlist(rt,2);
    pop(thread,2);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_NEWL3) {
    res = vm_newlist(rt,3);
    pop(thread,3);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_NEWL4) {
    res = vm_newlist(rt,4);
    pop(thread,4);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_NEWL) {
    DECODE_ARG();
    res = vm_newlist(rt,opr);
    pop(thread,opr);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_NEWM0) {
    Vset_map(&res,ObjNewMap(thread->sparrow,0));
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_NEWM1) {
    res = vm_newmap(rt,1,check);
    pop(thread,2);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_NEWM2) {
    res = vm_newmap(rt,2,check);
    pop(thread,4);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_NEWM3) {
    res = vm_newmap(rt,3,check);
    pop(thread,6);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_NEWM4) {
    res = vm_newmap(rt,4,check);
    pop(thread,8);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_NEWM) {
    DECODE_ARG();
    res = vm_newmap(rt,opr,check);
    /* key and value pair, so pop opr*2 elements out */
    pop(thread,opr*2);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_AGETS) {
    struct ObjStr* key;
    DECODE_ARG();
    tos = top(thread,0);
    key = proto->str_arr[opr];
    res = vm_agets(rt,tos,key,check);
    replace(thread,res);
    DISPATCH();
  }

  CASE(BC_AGETN) {
    double idx;
    size_t iidx;
    DECODE_ARG();
    idx = proto->num_arr[opr];
    tos = top(thread,0);
    if(ToSize(idx,&iidx)) {
      FATAL(rt,PERR_INDEX_OUT_OF_RANGE);
    }
    res = vm_agetn(rt,tos,iidx,check);
    replace(thread,res);
    DISPATCH();
  }

  CASE(BC_AGETI) {
    DECODE_ARG();
    tos = top(thread,0);
    res = vm_ageti(rt,tos,opr,check);
    replace(thread,res);
    DISPATCH();
  }

  CASE(BC_AGET) {
    l = top(thread,1);
    r = top(thread,0);
    res = vm_aget(rt,l,r,check);
    pop(thread,2);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_ASETN) {
    double n;
    size_t index;
    DECODE_ARG();
    n = proto->num_arr[opr];
    if(ToSize(n,&index)) {
      FATAL(rt,PERR_INDEX_OUT_OF_RANGE);
    }
    l = top(thread,1);
    r = top(thread,0);
    vm_asetn(rt,l,index,r,check);
    pop(thread,2);
    DISPATCH();
  }

  CASE(BC_ASETS) {
    struct ObjStr* str;
    DECODE_ARG();
    str = proto->str_arr[opr];
    l = top(thread,1);
    r = top(thread,0);
    vm_asets(rt,l,str,r,check);
    pop(thread,2);
    DISPATCH();
  }

  CASE(BC_ASET) {
    vm_aset(rt,top(thread,2),top(thread,1),top(thread,0),check);
    pop(thread,3);
    DISPATCH();
  }

  CASE(BC_ASETI) {
    DECODE_ARG();
    vm_aseti(rt,top(thread,1),top(thread,0),opr,check);
    pop(thread,2);
    DISPATCH();
  }

  CASE(BC_UGET) {
    DECODE_ARG();
    res = vm_uget(rt,opr);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_USET) {
    DECODE_ARG();
    tos = top(thread,0);
    vm_uset(rt,opr,tos);
    pop(thread,1);
    DISPATCH();
  }

  CASE(BC_USETTRUE) {
    DECODE_ARG();
    Vset_true(&res);
    vm_uset(rt,opr,res);
    DISPATCH();
  }

  CASE(BC_USETFALSE) {
    DECODE_ARG();
    Vset_false(&res);
    vm_uset(rt,opr,res);
    DISPATCH();
  }

  CASE(BC_USETNULL) {
    DECODE_ARG();
    Vset_null(&res);
    vm_uset(rt,opr,res);
    DISPATCH();
  }

  CASE(BC_POP) {
    DECODE_ARG();
    pop(thread,opr);
    DISPATCH();
  }

  /* general function call */
  CASE(BC_CALL) {
    int call_type;
    DECODE_ARG();
    tos = top(thread,opr);
    call_type = vm_call( rt , tos , opr , &res );
    switch(call_type) {
      case SPARROWFUNC:
        frame = current_frame(RTCallThread(rt));
        closure = frame->closure;
        proto = closure->proto;
        break;
      case CFUNC:
        replace(thread,res);
        break;
      default:
        goto fail;
    }
    DISPATCH();
  }

#define DO(NUM) \
  CASE(BC_CALL##NUM) { \
    int call_type; \
    tos = top(thread,NUM); \
    call_type = vm_call(rt,tos,NUM,&res); \
    switch(call_type) { \
      case SPARROWFUNC: \
        frame = current_frame(RTCallThread(rt)); \
        closure = frame->closure; \
        proto = closure->proto; \
        break; \
      case CFUNC: \
        replace(thread,res); \
        break; \
      default: \
        goto fail; \
    } \
    DISPATCH(); \
  }

  /* BC_CALL0 */
  DO(0)

  /* BC_CALL1 */
  DO(1)

  /* BC_CALL2 */
  DO(2)

  /* BC_CALL3 */
  DO(3)

  /* BC_CALL4 */
  DO(4)

#undef DO /* DO */

#define DO(INSTR,RETURN) \
  CASE(INSTR) { \
    RETURN; \
    if(del_callframe(rt)) goto done; \
    replace(thread,res); \
    frame = current_frame(thread); \
    closure = frame->closure; \
    proto = closure->proto; \
    DISPATCH(); \
  }

  /* BC_RET */
  DO(BC_RET,res = top(thread,0);)

  /* BC_RETN */
  DO(BC_RETN,DECODE_ARG();Vset_number(&res,proto->num_arr[opr]))

  /* BC_RETS */
  DO(BC_RETS,DECODE_ARG();Vset_str(&res,proto->str_arr[opr]))

  /* BC_RETT */
  DO(BC_RETT,Vset_true(&res));

  /* BC_RETF */
  DO(BC_RETF,Vset_false(&res));

  /* BC_RETN0 */
  DO(BC_RETN0,Vset_number(&res,0));

  /* BC_RETN1 */
  DO(BC_RETN1,Vset_number(&res,1));

  /* BC_RETNN1 */
  DO(BC_RETNN1,Vset_number(&res,-1));

  /* BC_RETNULL */
  DO(BC_RETNULL,Vset_null(&res));

#undef DO /* DO */

  CASE(BC_LOADCLS) {
    struct ObjProto* new_proto;
    struct ObjClosure* new_cls;
    size_t i;
    DECODE_ARG();
    new_proto = thread->component->module->cls_arr[opr];
    new_cls = ObjNewClosure(RTSparrow(rt),new_proto);
    Vset_closure(&res,new_cls);
    push(thread,res);
    /* Now the stack is correct, then perform upvalue updating,
     * otherwise recursive call will not be performed since the
     * tos (top of stack) is not set up to closure properly */
    for( i = 0 ; i < new_proto->uv_size ; ++i ) {
      if(new_proto->uv_arr[i].state == UPVALUE_INDEX_EMBED) {
        /* index this upvalue inside of the stack */
        new_cls->upval[i] = thread->stack[frame->base_ptr+
          new_proto->uv_arr[i].idx];
      } else {
        assert(closure);
        assert(new_proto->uv_arr[i].idx < proto->uv_size);
        new_cls->upval[i] = closure->upval[ new_proto->uv_arr[i].idx ];
      }
    }
    DISPATCH();
  }

  /* iterator */
  CASE(BC_FORPREP) {
    int invalid = 0;
    tos = top(thread,0);
    res = vm_forprep(rt,tos,&invalid,check);
    replace(thread,res); /* always push iterator to stack */
    if(invalid) {
      DECODE_ARG();
      frame->pc = opr;   /* jump to end of the loop body */
    } else {
      SKIP_ARG();
    }
    DISPATCH();
  }

  CASE(BC_IDREFK) {
    struct ObjIterator* itr;
    Value key;
    tos = top(thread,0);
    if(Vis_iterator(&tos)) {
      itr = Vget_iterator(&tos);
      itr->deref(thread->sparrow,itr,&key,NULL);
      push(thread,key);
    } else {
      struct ObjLoopIterator* litr = Vget_loop_iterator(&tos);
      Vset_number(&res,litr->index);
      push(thread,res);
    }
    DISPATCH();
  }

  CASE(BC_IDREFKV) {
    struct ObjIterator* itr;
    Value key;
    Value val;
    tos = top(thread,0);
    if(Vis_iterator(&tos)) {
      itr = Vget_iterator(&tos);
      itr->deref(thread->sparrow,itr,&key,&val);
      push(thread,key);
      push(thread,val);
    } else {
      struct ObjLoopIterator* litr = Vget_loop_iterator(&tos);
      Vset_number(&res,litr->index);
      push(thread,res);
      push(thread,res);
    }
    DISPATCH();
  }

  CASE(BC_FORLOOP) {
    struct ObjIterator* itr;
    tos = top(thread,0);
    if(Vis_iterator(&tos)) {
      itr = Vget_iterator(&tos);
      itr->move(thread->sparrow,itr);
      if(itr->has_next(thread->sparrow,itr) == 0) {
        /* go back to the head of the loop */
        DECODE_ARG();
        frame->pc = opr;
      } else {
        SKIP_ARG();
      }
    } else {
      struct ObjLoopIterator* litr = Vget_loop_iterator(&tos);
      litr->index += litr->step; /* move */
      if(litr->index >= litr->end) {
        SKIP_ARG();
      } else {
        DECODE_ARG();
        frame->pc = opr;
      }
    }
    DISPATCH();
  }

  CASE(BC_GGET) {
    struct ObjStr* key;
    DECODE_ARG();
    key = proto->str_arr[opr];
    res = vm_gget(rt,key,check);
    push(thread,res);
    DISPATCH();
  }

  CASE(BC_GSET) {
    struct ObjStr* key;
    DECODE_ARG();
    key = proto->str_arr[opr];
    tos = top(thread,0);
    ObjMapPut(thread->component->env,key,tos);
    pop(thread,1);
    DISPATCH();
  }

  CASE(BC_GSETTRUE) {
    struct ObjStr* key;
    DECODE_ARG();
    key = proto->str_arr[opr];
    Vset_true(&res);
    ObjMapPut(thread->component->env,key,res);
    DISPATCH();
  }

  CASE(BC_GSETFALSE) {
    struct ObjStr* key;
    DECODE_ARG();
    key = proto->str_arr[opr];
    Vset_false(&res);
    ObjMapPut(thread->component->env,key,res);
    DISPATCH();
  }

  CASE(BC_GSETNULL) {
    struct ObjStr* key;
    DECODE_ARG();
    key = proto->str_arr[opr];
    Vset_null(&res);
    ObjMapPut(thread->component->env,key,res);
    DISPATCH();
  }

  /* All the intrinsic function calls are dispatched via special
   * instructions and special routines. */
#define DO(CALLNAME,FUNCNAME) \
  CASE(BC_ICALL_##CALLNAME) { \
    DECODE_ARG(); \
    Vset_str(&tos,&sparrow->BuiltinFuncName_##FUNCNAME); \
    if(add_callframe(rt,opr,NULL,tos)) return -1; \
    if(global_env(rt).icall[ IFUNC_##CALLNAME ] != Builtin_##FUNCNAME) { \
      global_env(rt).icall[ IFUNC_##CALLNAME ](rt,&res,check); \
    } else { \
      /* This path will be inlined , we are doing devirtualization here */ \
      Builtin_##FUNCNAME(rt,&res,check); \
    } \
    del_callframe(rt); \
    push(thread,res); \
    DISPATCH(); \
  }

  /* BC_ICALL_TYPEOF */
  DO(TYPEOF,TypeOf)

  /* BC_ICALL_ISBOOLEAN */
  DO(ISBOOLEAN,IsBoolean)

  /* BC_ICALL_ISSTRING */
  DO(ISSTRING,IsString)

  /* BC_ICALL_ISNUMBER */
  DO(ISNUMBER,IsNumber)

  /* BC_ICALL_ISNULL */
  DO(ISNULL,IsNull)

  /* BC_ICALL_ISLIST */
  DO(ISLIST,IsList)

  /* BC_ICALL_ISMAP */
  DO(ISMAP,IsMap)

  /* BC_ICALL_ISCLOSURE */
  DO(ISCLOSURE,IsClosure)

  /* BC_ICALL_TOSTRING */
  DO(TOSTRING,ToString)

  /* BC_ICALL_TONUMBER */
  DO(TONUMBER,ToNumber)

  /* BC_ICALL_TOBOOLEAN */
  DO(TOBOOLEAN,ToBoolean)

  /* BC_ICALL_PRINT */
  DO(PRINT,Print)

  /* BC_ICALL_ERROR */
  DO(ERROR,Error)

  /* BC_ICALL_ASSERT */
  DO(ASSERT,Assert)

  /* BC_ICALL_IMPORT */
  DO(IMPORT,Import)

  /* BC_ICALL_SIZE */
  DO(SIZE,Size)

  /* BC_ICALL_RANGE */
  DO(RANGE,Range)

  /* BC_ICALL_LOOP */
  DO(LOOP,Loop)

  /* BC_ICALL_RUNSTRING */
  DO(RUNSTRING,RunString)

  /* BC_ICALL_MIN */
  DO(MIN,Min)

  /* BC_ICALL_MAX */
  DO(MAX,Max)

  /* BC_ICALL_SORT */
  DO(SORT,Sort)

  /* BC_ICALL_GET */
  DO(GET,Get)

  /* BC_ICALL_SET */
  DO(SET,Set)

  /* BC_ICALL_EXIST */
  DO(EXIST,Exist)

  /* BC_ICALL_MSEC */
  DO(MSEC,MSec)

  /* Misc Labels not in used right now */
  CASE(BC_LOOP) {
    UNIMPLEMENTED();
  }

  CASE(BC_CLOSURE) {
    UNIMPLEMENTED();
  }

  CASE(BC_OP) {
    UNIMPLEMENTED();
  }

  CASE(BC_A) {
    UNIMPLEMENTED();
  }

  CASE(BC_NOP) {
    DISPATCH();
  }

#ifdef SPARROW_VM_NO_THREADING
  default:
#ifdef SPARROW_VM_INSTRUCTION_CHECK
    verify(!"Unknown instruction!");
#else
    assert(!"Unknown instruction!");
#endif /* SPARROW_VM_INSTRUCTION_CHECK */
    } /* switch */
  }   /* while */
#endif /* SPARROW_VM_NO_THREADING */


  /* End of the execution and this label means the end is successfully
   * due to a correct return from the main entry of the module */
done:
  *ret = res;
  return 0;

  /* We failed the VM execution due to some reason. The error has already
   * logged into the runtime's error string buffer */
fail:
  return -1;
}

/* Helper function to initialize Runtime structure */
static void runtime_init( struct Sparrow* sparrow , struct Runtime* runtime ,
    struct ObjComponent* component) {
  size_t i;
  size_t frame_size = sparrow->max_funccall/8;
  size_t stack_size = sparrow->max_stacksize/128;
  if(frame_size == 0) frame_size = 2;
  if(stack_size == 0) stack_size = 2;

  runtime->cur_thread = runtime->ths + 0;
  for( i = 0 ; i < SIZE_OF_THREADS ; ++i ) {
    runtime->ths[i].sparrow = sparrow;
    runtime->ths[i].component = component;
    runtime->ths[i].runtime = runtime;

    runtime->ths[i].frame = malloc(sizeof(struct CallFrame)*frame_size);
    runtime->ths[i].frame_size = 0;
    runtime->ths[i].frame_cap = frame_size;

    runtime->ths[i].stack = malloc(sizeof(Value)*stack_size);
    runtime->ths[i].stack_size = 0;
    runtime->ths[i].stack_cap = stack_size;
  }

  runtime->max_funccall = sparrow->max_funccall;
  runtime->max_stacksize = sparrow->max_stacksize;
  runtime->error = CStrEmpty();

  /* Mark sparrow context is tied to *this* runtime */
  if(sparrow->runtime) {
    runtime->prev = sparrow->runtime;
    sparrow->runtime = runtime;
  } else {
    runtime->prev = NULL;
    sparrow->runtime = runtime;
  }
}

static void runtime_destroy( struct Sparrow* sp,
    struct Runtime* runtime ) {
  size_t i;
  assert( runtime == sp->runtime );
  sp->runtime = sp->runtime->prev;
  for( i = 0 ; i < SIZE_OF_THREADS ; ++i ) {
    free(runtime->ths[i].frame);
    free(runtime->ths[i].stack);
  }
}

int Execute( struct Sparrow* sp, struct ObjComponent* component ,
    Value* ret , struct CStr* error ) {
  struct Runtime rt;
  struct ObjProto* m = ObjModuleGetEntry(component->module);
  struct CallFrame* frame;
  struct ObjClosure* main_closure = NULL;
  int stat;

  /* initialize the runtime object */
  runtime_init(sp,&rt,component);
  frame = rt.cur_thread->frame;

  /* setup the first callframe */
  main_closure = ObjNewClosureNoGC(sp,m);
  frame->base_ptr = 0;
  frame->pc = 0;
  frame->closure = main_closure;
  frame->narg = 0;
  Vset_null(&(frame->callable));
  rt.cur_thread->frame_size = 1;

  /* run the code */
  stat = vm_main( &rt, ret );

  /* set error string */
  *error = rt.error;

  /* clean our stuff */
  runtime_destroy(sp,&rt);
  return stat;
}

int PushArg( struct Sparrow* sparrow , Value value ) {
  struct Runtime* runtime = sparrow->runtime;
  struct CallThread* thread = RTCallThread(runtime);
  return push(thread,value);
}

int CallFunc( struct Sparrow* sparrow , Value func ,
    int argnum, Value* ret ) {
  struct Runtime* runtime = sparrow->runtime;
  struct CallFrame* frame = RTCurFrame(runtime);
  int rstat;

  assert(runtime);
  assert(frame->closure);
  assert(argnum >= 0);

  rstat = vm_call( runtime , func , argnum , ret );

  switch(rstat) {
    case CFUNC:
      return 0;
    case CALLERROR:
      return -1;
    default:
      assert(frame->base_ptr == 0);
      frame->base_ptr = RETURN_TO_HOST;
      rstat = vm_main(runtime,ret);
      assert(frame->base_ptr == RETURN_TO_HOST);
      frame->base_ptr = 0;
      return rstat;
  }
}
