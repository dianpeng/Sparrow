#include "object.h"
#include "list.h"
#include "map.h"
#include "vm.h"
#include "gc.h"
#include "error.h"
#include "builtin.h"
#include "../util.h"

#define __(A,B,C) const char* MetaOpsName_##B = (C);

METAOPS_LIST(__)

#undef __ /* __ */

#define string_equal(OBJSTR,HASH,STR,LEN) \
  ((OBJSTR)->hash == (HASH) && \
   (OBJSTR)->len == (LEN) && \
   (memcmp((OBJSTR)->str,(STR),(LEN)) == 0))

static void objstr_insert( struct Sparrow* ,
    struct ObjStr* , struct ObjStr* );

uint32_t StringHash( const char* str , size_t len ) {
  uint32_t ret = 17771;
  size_t i;
  assert(len<LARGE_STRING_SIZE);
  for( i = 0 ; i < len ; ++i ) {
    ret = (ret ^((ret << 5) + (ret >> 2))) + (uint32_t)(str[i]);
  }
  return ret;
}

uint32_t LargeStringHash( const char* str , size_t len ) {
  uint32_t ret = 1777771;
  size_t step = (len >> 5) + 1;
  size_t i;
  for( i = 0 ; i < len ; i += step ) {
    ret = ret^((ret<<5)+(ret>>2)+str[i]);
  }
  return ret;
}

int ConstAddNumber( struct ObjProto* oc , double num ) {
  size_t i;
  for( i = 0 ; i < oc->num_size ; ++i ) {
    if( num == oc->num_arr[i] ) return (int)i;
  }
  DynArrPush(oc,num,num);
  return (int)(oc->num_size-1);
}

int ConstAddString( struct ObjProto* oc , struct ObjStr* str ) {
  size_t i;
  for( i = 0 ; i < oc->str_size ; ++i ) {
    if(ObjStrEqual(str,oc->str_arr[i])) return (int)(i);
  }
  DynArrPush(oc,str,str);
  return (int)(oc->str_size-1);
}

/* String is *not* pooling in our implementation */
#define add_gcobject(TH,OBJ,TYPE) \
  do { \
    (OBJ)->gc.gc_state = GC_UNMARKED; \
    (OBJ)->gc.next = (TH)->gc_start; \
    (TH)->gc_start = (struct GCRef*)(OBJ); \
    (TH)->gc_sz++; \
    (OBJ)->gc.gtype = (TYPE); \
  } while(0)

#define init_static_map_gcstate(OBJ,TYPE) \
  do { \
    /* Must be unmarked though map is static, but its entry */ \
    /* can be GC collected so mark phase still needs to work */ \
    (OBJ)->gc.gc_state = GC_UNMARKED; \
    (OBJ)->gc.next = NULL; \
    (OBJ)->gc.gtype = (TYPE); \
    (OBJ)->mops = NULL; \
  } while(0)

struct ObjMethod* ObjNewMethodNoGC( struct Sparrow* sth , CMethod method ,
    Value object , struct ObjStr* name ) {
  struct ObjMethod* ret = malloc(sizeof(*ret));
  add_gcobject(sth,ret,VALUE_METHOD);
  ret->name = name;
  ret->method = method;
  ret->object = object;
  return ret;
}

struct ObjMethod* ObjNewMethod( struct Sparrow* sth , CMethod method ,
    Value object , struct ObjStr* name ) {
  GCTry(sth);
  return ObjNewMethodNoGC(sth,method,object,name);
}

struct ObjList* ObjNewListNoGC( struct Sparrow* sth ,
    size_t cap ) {
  struct ObjList* ret = malloc(sizeof(*ret));
  add_gcobject(sth,ret,VALUE_LIST);
  ObjListInit(sth,ret,cap);
  return ret;
}

struct ObjList* ObjNewList( struct Sparrow* sth ,
    size_t cap ) {
  GCTry(sth);
  return ObjNewListNoGC(sth,cap);
}

struct ObjMap* ObjNewMapNoGC( struct Sparrow* sth ,
    size_t cap ) {
  struct ObjMap* ret = malloc(sizeof(*ret));
  add_gcobject(sth,ret,VALUE_MAP);
  ObjMapInit(ret,cap);
  return ret;
}

struct ObjMap* ObjNewMap( struct Sparrow* sth ,
    size_t cap ) {
  GCTry(sth);
  return ObjNewMapNoGC(sth,cap);
}

struct ObjUdata* ObjNewUdataNoGC( struct Sparrow* sth ,
    const char* name,
    void* udata,
    UdataGCMarkFunction mark_func,
    CDataDestroyFunction destroy_func,
    UdataCall call_func ) {
  struct ObjUdata* ret = malloc(sizeof(*ret));
  add_gcobject(sth,ret,VALUE_UDATA);
  ret->name = CStrDup(name);
  ret->udata= udata;
  ret->destroy = destroy_func;
  ret->mark = mark_func;
  ret->call = call_func;
  ret->mops = NULL;
  return ret;
}

struct ObjUdata* ObjNewUdata( struct Sparrow* sth ,
    const char* name ,
    void* udata,
    UdataGCMarkFunction mark_func,
    CDataDestroyFunction destroy_func,
    UdataCall call_func ) {
  GCTry(sth);
  return ObjNewUdataNoGC(sth,name,udata,mark_func,destroy_func,call_func);
}

struct ObjProto* ObjNewProtoNoGC( struct Sparrow* sth ,
    struct ObjModule* mod ) {
  struct ObjProto* ret = malloc(sizeof(*ret));
  add_gcobject(sth,ret,VALUE_PROTO);
  CodeBufferInit(&(ret->code_buf));
  ret->num_arr = NULL;
  ret->num_size = ret->num_cap = 0;
  ret->str_arr = NULL;
  ret->str_size = ret->str_cap = 0;
  ret->uv_arr = NULL;
  ret->uv_cap = ret->uv_size = 0;
  ret->narg = 0;
  ret->proto = CStrEmpty();
  ret->start = 0;
  ret->end = 0;
  ret->module = mod;
  DynArrPush(mod,cls,ret);
  ret->cls_idx = (int)(mod->cls_size-1);
  return ret;
}

struct ObjProto* ObjNewProto( struct Sparrow* sth ,
    struct ObjModule* mod ) {
  GCTry(sth);
  return ObjNewProtoNoGC(sth,mod);
}

struct ObjClosure* ObjNewClosureNoGC( struct Sparrow* sth ,
    struct ObjProto* proto ) {
  struct ObjClosure* cls = malloc(
      sizeof(*cls)+sizeof(Value)*proto->uv_size);
  UNUSE_ARG(sth);
  cls->proto = proto;
  cls->upval = (Value*)((char*)cls + sizeof(struct ObjClosure));
  add_gcobject(sth,cls,VALUE_CLOSURE);
  return cls;
}

struct ObjClosure* ObjNewClosure( struct Sparrow* sth ,
    struct ObjProto* proto ) {
  GCTry(sth);
  return ObjNewClosureNoGC(sth,proto);
}

struct ObjIterator* ObjNewIteratorNoGC( struct Sparrow* sth ) {
  struct ObjIterator* ret= malloc(sizeof(*ret));
  add_gcobject(sth,ret,VALUE_ITERATOR);
  ret->has_next = NULL;
  ret->deref = NULL;
  ret->move = NULL;
  Vset_null(&(ret->obj));
  return ret;
}

struct ObjIterator* ObjNewIterator( struct Sparrow* sth ) {
  GCTry(sth);
  return ObjNewIteratorNoGC(sth);
}

struct ObjModule* ObjFindModule( struct Sparrow* sth ,
    const char* fpath ) {
  struct ObjModule* mod;
  ListForeach(sth,mod,mod) {
    if(strcmp(mod->source_path.str,fpath)==0) {
      return mod;
    }
  }
  return NULL;
}

struct ObjModule* ObjNewModuleNoGC( struct Sparrow* sth ,
    const char* fpath , const char* source ) {
  /* add a module */
  struct ObjModule* mod;
  assert( ObjFindModule(sth,fpath) == NULL );
  mod =  malloc(sizeof(*mod));
  mod->cls_arr = NULL;
  mod->cls_cap = mod->cls_size = 0;
  mod->source = CStrDup(source);
  mod->source_path = fpath ? CStrDup(fpath) : CStrEmpty();
  add_gcobject(sth,mod,VALUE_MODULE);
  ListLink(sth,mod,mod);
  return mod;
}

struct ObjModule* ObjNewModule( struct Sparrow* sth ,
    const char* source , const char* fpath ) {
  GCTry(sth);
  return ObjNewModuleNoGC(sth,source,fpath);
}


static const char* upvalue_index_str( int type ) {
  if(type)
    return "detach";
  else
    return "embed";
}


void ObjDumpModule( struct ObjModule* mod ,
    FILE* file , const char* prefix ) {
  size_t i;
  if(prefix)
    fprintf(file,"Path:%s and source:%s|%s!\n",
        mod->source_path.str,mod->source.str,prefix);
  else
    fprintf(file,"Source path:%s!\n",mod->source_path.str);
  fprintf(file,"Proto list:%zu!\n",mod->cls_size);
  for( i = 0 ; i < mod->cls_size ; ++i ) {
    struct ObjProto* cls = mod->cls_arr[i];
    size_t j;
    fprintf(file,"%zu. Proto prototype:%s\n",i+1,cls->proto.str);

    fprintf(file,"Proto number table:\n");
    for( j = 0 ; j < cls->num_size ; ++j ) {
      fprintf(file,"%zu. %f\n",j+1,cls->num_arr[j]);
    }

    fprintf(file,"Proto string table:\n");
    for( j = 0 ; j < cls->str_size ; ++j ) {
      fprintf(file,"%zu. %s\n",j+1,cls->str_arr[j]->str);
    }

    fprintf(file,"Proto upvalue table:\n");
    for( j = 0 ; j < cls->uv_size ; ++j ) {
      fprintf(file,"%zu. %d(%s)\n",j+1,
          cls->uv_arr[j].idx,
          upvalue_index_str(cls->uv_arr[j].state));
    }

    CodeBufferDump(&(cls->code_buf),file,NULL);
    printf("\n");
  }
}

struct ObjComponent* ObjNewComponentNoGC( struct Sparrow* sth,
    struct ObjModule* module , struct ObjMap* env ) {
  struct ObjComponent* component;
  component = malloc(sizeof(*component));
  component->env = env;
  component->module = module;
  add_gcobject(sth,component,VALUE_COMPONENT);
  return component;
}

struct ObjComponent* ObjNewComponent( struct Sparrow* sth,
    struct ObjModule* module ,
    struct ObjMap* env ) {
  GCTry(sth);
  return ObjNewComponentNoGC(sth,module,env);
}

struct ObjLoop* ObjNewLoopNoGC( struct Sparrow* sth,
    int start, int end, int step ) {
  struct ObjLoop* loop;
  loop = malloc(sizeof(*loop));
  loop->start = start;
  loop->end = end;
  loop->step = step;
  add_gcobject(sth,loop,VALUE_LOOP);
  return loop;
}

struct ObjLoop* ObjNewLoop( struct Sparrow* sth,
    int start, int end, int step ) {
  GCTry(sth);
  return ObjNewLoopNoGC(sth,start,end,step);
}

struct ObjLoopIterator* ObjNewLoopIteratorNoGC( struct Sparrow* sth,
    struct ObjLoop* loop ) {
  struct ObjLoopIterator* iterator = malloc(sizeof(*iterator));
  iterator->end = loop->end;
  iterator->step= loop->step;
  iterator->loop= loop;
  iterator->index= loop->start;
  add_gcobject(sth,iterator,VALUE_LOOP_ITERATOR);
  return iterator;
}

struct ObjLoopIterator* ObjNewLoopIterator( struct Sparrow* sth,
    struct ObjLoop* loop ) {
  GCTry(sth);
  return ObjNewLoopIteratorNoGC(sth,loop);
}

/* Misc Value functions */
size_t ValueSize( struct Runtime* rt , Value v , int* fail ) {
  *fail = 0;
  if(Vis_list(&v)) {
    return Vget_list(&v)->size;
  } else if(Vis_map(&v)) {
    struct ObjMap* map = Vget_map(&v);
    if(map->mops) {
      int r;
      size_t sz;
      INVOKE_METAOPS("map",
          rt,
          map->mops,
          size,
          r,
          RTSparrow(rt),
          v,
          &sz);
      if(r == 0) return sz;
    } else {
      return map->size;
    }
  } else if(Vis_str(&v)) {
    return Vget_str(&v)->len;
  } else if(Vis_udata(&v)) {
    struct ObjUdata* udata = Vget_udata(&v);
    int r;
    size_t sz;
    INVOKE_METAOPS(udata->name.str,
        rt,
        udata->mops,
        size,
        r,
        RTSparrow(rt),
        v,
        &sz);
    if(r == 0) return sz;
  }
  RuntimeError(rt,PERR_METAOPS_ERROR,ValueGetTypeString(v),METAOPS_NAME(size));
  *fail = 1;
  return 0;
}

static void list_print( struct Sparrow* sth, struct StrBuf* buf ,
    struct ObjList* list ) {
  size_t i;
  StrBufAppendStrLen(buf,"[",1);
  for( i = 0 ; i < list->size ; ++i ) {
    ValuePrint(sth,buf,list->arr[i]);
    if(i != list->size-1) StrBufAppendStrLen(buf,",",1);
  }
  StrBufAppendStrLen(buf,"]",1);
}

static void map_print( struct Sparrow* sth, struct StrBuf* buf ,
    struct ObjMap* map ) {
  size_t i;
  size_t cnt = 0;
  Value k;
  StrBufAppendStrLen(buf,"{",1);
  for( i = 0 ; i < map->cap ; ++i ) {
    struct ObjMapEntry* e = map->entry+i;
    if(!e->used || e->del) continue;
    Vset_str(&k,e->key);
    ValuePrint(sth,buf,k);
    StrBufAppendStrLen(buf,":",1);
    ValuePrint(sth,buf,e->value);
    ++cnt;
    if(cnt != map->size) StrBufAppendStrLen(buf,",",1);
  }
  StrBufAppendStrLen(buf,"}",1);
}


void ValuePrint( struct Sparrow* sth, struct StrBuf* buf ,
    Value v ) {
  if(Vis_number(&v)) {
    StrBufAppendNumString(buf,Vget_number(&v));
  } else if(Vis_true(&v)) {
    StrBufAppendStrLen(buf,"true",4);
  } else if(Vis_false(&v)) {
    StrBufAppendStrLen(buf,"false",5);
  } else if(Vis_null(&v)) {
    StrBufAppendStrLen(buf,"null",4);
  } else if(Vis_list(&v)) {
    list_print(sth,buf,Vget_list(&v));
  } else if(Vis_map(&v)) {
    struct ObjMap* map = Vget_map(&v);
    if(map->mops) {
      int r;
      INVOKE_METAOPS("map",sth->runtime,
          map->mops,
          print,
          r,
          sth,
          v,
          buf);
      if(r) goto map_print;
    } else {
map_print:
      map_print(sth,buf,Vget_map(&v));
    }
  } else if(Vis_proto(&v)) {
    struct ObjProto* proto = Vget_proto(&v);
    StrBufAppendF(buf,"proto(argument:%zu,prototype:%s,module:%p,file:%s)",
        proto->narg,
        proto->proto.str,
        proto->module,
        proto->module->source_path.str);
  } else if(Vis_closure(&v)) {
    struct ObjClosure* closure = Vget_closure(&v);
    StrBufAppendF(buf,"closure(%p)",closure->proto);
  } else if(Vis_method(&v)) {
    struct ObjMethod* method = Vget_method(&v);
    struct StrBuf temp;
    StrBufInit(&temp,128);
    ValuePrint(sth,&temp,method->object);
    StrBufAppendF(buf,"method(%s:address:%p,object:",method->name->str,
        method->method);
    StrBufAppendStrBuf(buf,&temp);
    StrBufAppendStrLen(buf,")",1);
    StrBufDestroy(&temp);
  } else if(Vis_udata(&v)) {
    struct ObjUdata* udata = Vget_udata(&v);
    if(udata->mops) {
      int r;
      INVOKE_METAOPS(udata->name.str,sth->runtime,
          udata->mops,
          print,
          r,
          sth,
          v,
          buf);
      if(r) goto udata_print;
    } else {
udata_print:
      /* If the provided one cannot do print, we just do it by ourself */
      StrBufAppendF(buf,"udata(%s)",udata->name.str);
    }
  } else if(Vis_str(&v)) {
    StrBufAppendStrLen(buf,Vget_str(&v)->str,Vget_str(&v)->len);
  } else if(Vis_iterator(&v)) {
    struct ObjIterator* itr = Vget_iterator(&v);
    struct StrBuf temp;
    StrBufInit(&temp,128);
    ValuePrint(sth,&temp,itr->obj);
    StrBufAppendStrLen(buf,"iterator(",9);
    StrBufAppendStrBuf(buf,&temp);
    StrBufAppendStrLen(buf,")",1);
    StrBufDestroy(&temp);
  } else if(Vis_module(&v)) {
    struct ObjModule* mod = Vget_module(&v);
    StrBufAppendF(buf,"module(%s)",mod->source_path.str);
  } else if(Vis_component(&v)) {
    struct ObjComponent* comp = Vget_component(&v);
    StrBufAppendF(buf,"component(%s)",comp->module->source_path.str);
  } else if(Vis_loop(&v)) {
    struct ObjLoop* loop = Vget_loop(&v);
    StrBufAppendF(buf,"loop(start:%d,end:%d,step:%d)",
        loop->start,loop->end,loop->step);
  } else if(Vis_loop_iterator(&v)) {
    struct ObjLoopIterator* litr = Vget_loop_iterator(&v);
    StrBufAppendF(buf,"loop-iterator(index:%d,end:%d,step:%d,loop:%p)",
        litr->index,
        litr->end,
        litr->step,
        litr->loop);
  } else {
    assert(!"unreachable!");
  }
}

/* function wrapper for adapting __call to intrinsic function
 * prototype */
static int ifunc_wrapper( struct Sparrow* sparrow ,
    struct ObjUdata* udata , Value* ret ) {
  IntrinsicCall func = (IntrinsicCall)(udata->udata);
  int fail;
  assert(func);
  assert(sparrow->runtime);

  func(sparrow->runtime,ret,&fail);
  if( fail ) {
    return -1;
  } else {
    return 0;
  }
}

/* Wrapper Udata for global intrinsic function. Those functions also
 * needs to be exported as a Udata since this allows uer to modify
 * their value and capture them in variable */
static struct ObjUdata* create_ifunc_udata( struct Sparrow* sparrow ,
    IntrinsicCall func , const char* name ) {
  struct ObjUdata* udata = ObjNewUdataNoGC(sparrow,name,func,NULL,NULL,NULL);
  udata->mops = NewMetaOps();
  udata->call = ifunc_wrapper;
  return udata;
}

/* global env initialization */
static void global_env_init( struct Sparrow* sparrow ,
    struct GlobalEnv* genv ) {
  init_static_map_gcstate(&(genv->env),VALUE_MAP);
  ObjMapInit(&genv->env,NextPowerOf2Size(SIZE_OF_IFUNC+4));

#define ADD(NAME,FUNC) \
  do { \
    struct ObjStr* name = ObjNewStrNoGC(sparrow,#NAME,STRING_SIZE(#NAME)); \
    struct ObjUdata* udata = FUNC(sparrow); \
    Value v; \
    Vset_udata(&v,udata); \
    ObjMapPut(&(genv->env),name,v); \
  } while(0)

  ADD(list,GCreateListUdata);
  ADD(map ,GCreateMapUdata);
  ADD(string,GCreateStringUdata);
  ADD(gc,GCreateGCUdata);

  /* TODO :: Add other cached object here */

#undef ADD /* ADD */

#define __(A,B,C) \
  do { \
    struct ObjStr* name = IFUNC_NAME(sparrow,B); \
    Value v; \
    genv->icall[IFUNC_##A] = Builtin_##B; \
    Vset_udata(&v,create_ifunc_udata(sparrow,Builtin_##B,"function::" C)); \
    ObjMapPut(&(genv->env),name,v); \
  } while(0);

  INTRINSIC_FUNCTION(__)

#undef __ /* __ */
}

/* only used here for SparrowDestroy */
void SparrowDestroy( struct Sparrow* sth ) {
  struct GCRef* start , *temp;
  size_t i;
#ifndef NDEBUG
  i = 0;
#endif

  start = sth->gc_start;
  while( start ) {
    temp = start->next;
    GCFinalizeObj(sth,start);
    start = temp;
#ifndef NDEBUG
    ++i;
#endif /* NDEBUG */
  }
  assert( i == sth->gc_sz );
  free(sth->str_arr);
  sth->str_arr = NULL;
  sth->str_size = sth->str_cap = 0;
  ListInit(sth,mod);

  ObjMapDestroy(&(sth->global_env.env));
}

void SparrowInit( struct Sparrow* sth ) {
  sth->runtime = NULL;
  sth->max_funccall = SPARROW_DEFAULT_FUNCCALL_SIZE;
  sth->max_stacksize= SPARROW_DEFAULT_STACK_SIZE;
  sth->gc_start = NULL;
  sth->gc_prevsz = 0;
  sth->gc_sz = 0;
  sth->gc_active = 0;
  sth->gc_inactive = 0;
  sth->gc_generation = 0;
  sth->gc_threshold = SPARROW_DEFAULT_GC_THRESHOLD;
  sth->gc_ratio = SPARROW_DEFAULT_GC_RATIO;
  sth->gc_ps_threshold = 0;
  sth->gc_penalty_ratio = SPARROW_DEFAULT_GC_PENALTY_RATIO;
  sth->gc_adjust_threshold = sth->gc_threshold;
  sth->gc_penalty_times = 0;
  sth->str_arr = calloc(sizeof(struct ObjStr*),STRING_POOL_SIZE);
  sth->str_cap = STRING_POOL_SIZE;
  sth->str_size = 0;
  ListInit(sth,mod);

  /* Initialize global builtin function name lists */
#define __(A,B,C) \
  do { \
    ObjInitTempStrLen(&sth->BuiltinFuncName_##B,C,STRING_SIZE(C)); \
  } while(0);

  INTRINSIC_FUNCTION(__)

#undef __ /* __ */

  /* Initialize intrinsic attribute name lists */
#define __(A,B) \
    do { \
      ObjInitTempStrLen(&sth->IAttrName_##A,B,STRING_SIZE(B)); \
    } while(0);

  INTRINSIC_ATTRIBUTE(__)

#undef __ /* __ */
  global_env_init(sth,&(sth->global_env)); /* initialize global environment */
}

struct ObjStr* IAttrGetObjStr( struct Sparrow* sparrow,
    enum IntrinsicAttribute iattr ) {

  switch(iattr) {

#define __(A,B) case IATTR_##A: return &(sparrow->IAttrName_##A);

    INTRINSIC_ATTRIBUTE(__)

    default:
      assert(!"unreachable!");
      return NULL;
  }

#undef __ /* __ */
}

/* ========================================
 * String pool implementation
 * ======================================*/
static void objstr_insert( struct Sparrow* sth ,
    struct ObjStr* str,
    struct ObjStr* hint ) {
  int idx;
  struct ObjStr** slot;
  if( sth->str_size == sth->str_cap ) {
    /* do a rehashing */
    size_t ncap = sth->str_cap * 2;
    struct ObjStr** new_entry = calloc(sizeof(struct ObjStr*),ncap);
    size_t i;
    for( i = 0 ; i < sth ->str_cap ; ++i ) {
      struct ObjStr* s;
      struct ObjStr** e;

      s = sth->str_arr[i];
      idx = s->hash & (ncap-1);
      e = new_entry + idx;

      if(!*e) {
        *e = s;
        s->more = 0;
        s->next = 0;
      } else {
        /* find where we should chain */
        struct ObjStr** n;
        uint32_t h = s->hash;
        while((*e)->more) {
          e = new_entry + (*e)->next;
        }
        n = e;
        /* probing for an empty slot */
        do {
          e = new_entry + ((++h) & (ncap-1));
        } while(*e);
        *e = s;
        s->more = 0;
        s->next = 0;
        (*n)->more = 1;
        (*n)->next = (e - new_entry);
      }
    }
    free(sth->str_arr);
    sth->str_arr = new_entry;
    sth->str_cap = ncap;
    hint = NULL; /* invalidate HINT */
  }
  /* real insertion */
  idx = str->hash & ( sth->str_cap - 1);
  slot = sth->str_arr + idx;
  if(*slot) {
    int h = str->hash;
    if(!hint) {
      while((*slot)->more) {
        assert(!string_equal(*slot,str->hash,str->str,str->len));
        slot = sth->str_arr + (*slot)->next;
      }
      hint = *slot;
    }
    /* do a probing */
    do {
      slot = sth->str_arr + ( ++h & (sth->str_cap-1) );
    } while(*slot);
    *slot = str;
    str->more = 0;
    hint->next = (slot - sth->str_arr);
    hint->more = 1;
  } else {
    *slot = str;
  }
  ++sth->str_size;
}

struct ObjStr* ObjNewStrNoGC( struct Sparrow* sth ,
    const char* str , size_t len ) {
  uint32_t hash;
  struct ObjStr* hint = NULL;
  if(len < LARGE_STRING_SIZE) {
    int idx;
    struct ObjStr* slot;
    hash = StringHash(str,len);
    idx = hash & ( sth->str_cap -1 );
    slot = sth->str_arr[idx];
    while(slot && !string_equal(slot,hash,str,len)) {
      if(slot->more) slot = sth->str_arr[slot->next];
      else {
        hint = slot;
        slot = NULL;
        break;
      }
    }
    if(slot) return slot;
    else {
      struct ObjStr* new_str = malloc(sizeof(*new_str)+len+1);
      new_str->str = ((char*)new_str+sizeof(*new_str));
      new_str->len = len;
      new_str->hash = hash;
      new_str->more = 0;
      new_str->next = 0;

      /* do not treat str as a C string */
      memcpy((void*)new_str->str,str,len);

      /* buf we still append a null terminator for internal
       * print or error handling stuff */
      ((char*)new_str->str)[len] = 0;

      objstr_insert(sth,new_str,hint);
      add_gcobject(sth,new_str,VALUE_STRING);
      return new_str;
    }
  } else {
    struct ObjStr* lstr;
    hash = LargeStringHash(str,len);
    lstr = malloc( sizeof(*lstr) + len + 1 );
    lstr->str = ((char*)lstr + sizeof(*lstr));
    lstr->len = len;
    lstr->hash = hash;
    lstr->next = 0;
    lstr->more = 0;
    /* do not treat str as a C string */
    memcpy((void*)lstr->str,str,len);
    ((char*)lstr->str)[len] = 0;

    add_gcobject(sth,lstr,VALUE_STRING);
    return lstr;
  }
}

struct ObjStr* ObjNewStr( struct Sparrow* sth ,
    const char* str, size_t len ) {
  GCTry(sth);
  return ObjNewStrNoGC(sth,str,len);
}

struct ObjStr* ObjNewStrFromCharNoGC( struct Sparrow* sth,
    char c ) {
  char buf[1] = { c };
  return ObjNewStrNoGC(sth,buf,1);
}

struct ObjStr* ObjNewStrFromChar( struct Sparrow* sth,
    char c ) {
  GCTry(sth);
  return ObjNewStrFromCharNoGC(sth,c);
}

/* String iterator */
static int
str_iter_has_next( struct Sparrow* sth ,
    struct ObjIterator* itr ) {
  struct ObjStr* str = Vget_str(&(itr->obj));
  if( itr->u.index >= str->len )
    return -1;
  else
    return 0;
}

static void
str_iter_deref( struct Sparrow* sth ,
    struct ObjIterator* itr ,
    Value* key,
    Value* val ) {
  struct ObjStr* str = Vget_str(&(itr->obj));
  assert(itr->u.index < str->len);
  if(key) Vset_number(key,itr->u.index);
  if(val) Vset_str(val,ObjNewStrFromChar(sth,str->str[itr->u.index]));
}

static void
str_iter_move( struct Sparrow* sth ,
    struct ObjIterator* itr ) {
  ++itr->u.index;
}

void ObjStrIterInit( struct ObjStr* str , struct ObjIterator* itr ) {
  Vset_str(&(itr->obj),str);
  itr->u.index = 0;
  itr->has_next = str_iter_has_next;
  itr->deref = str_iter_deref;
  itr->move = str_iter_move;
  itr->destroy = NULL;
}

const char* ValueGetTypeString( Value v ) {
  if(Vis_number(&v)) {
    return "number";
  } else if(Vis_true(&v)) {
    return "true";
  } else if(Vis_false(&v)) {
    return "false";
  } else if(Vis_null(&v)) {
    return "null";
  } else if(Vis_list(&v)) {
    return "list";
  } else if(Vis_map(&v)) {
    return "map";
  } else if(Vis_proto(&v)) {
    return "proto";
  } else if(Vis_closure(&v)) {
    return "closure";
  } else if(Vis_method(&v)) {
    return "method";
  } else if(Vis_udata(&v)) {
    return Vget_udata(&v)->name.str;
  } else if(Vis_str(&v)) {
    return "string";
  } else if(Vis_iterator(&v)) {
    return "iterator";
  } else if(Vis_module(&v)) {
    return "module";
  } else if(Vis_component(&v)) {
    return "component";
  } else if(Vis_loop(&v)) {
    return "loop";
  } else if(Vis_loop_iterator(&v)) {
    return "loop_iterator";
  } else {
    assert(!"unreachable!");
    return NULL;
  }
}

double ValueToNumber( struct Runtime* rt , Value obj ,
    int* fail ) {
  double ret = 0;
  if(Vis_str(&obj)) {
    char* pend;
    ret = strtod( Vget_str(&obj)->str , &pend );
    if(ret == 0 && errno != 0) {
      goto fail;
    }
  } else if(Vis_number(&obj)) {
    ret = Vget_number(&obj);
  } else if(Vis_udata(&obj)) {
    struct ObjUdata* udata = Vget_udata(&obj);
    Value v;
    int r;
    INVOKE_METAOPS(udata->name.str,
        rt,
        udata->mops,
        to_number,
        r,
        RTSparrow(rt),
        obj,
        &v);
    if(r == 0) ret = Vget_number(&v);
    else {
      *fail = 1;
      return ret;
    }
  } else goto fail;
  *fail = 0;
  return ret;
fail:
  RuntimeError(rt,PERR_CONVERSION_ERROR,ValueGetTypeString(obj),"number");
  *fail = 1;
  return ret;
}

struct ObjStr* ValueToString( struct Runtime* rt , Value obj ,
    int* fail ) {
  struct ObjStr* ret = NULL;
  if(Vis_str(&obj)) {
    ret = Vget_str(&obj);
  } else if(Vis_udata(&obj)) {
    struct ObjUdata* udata = Vget_udata(&obj);
    Value v;
    int r;
    INVOKE_METAOPS(udata->name.str,
        rt,
        udata->mops,
        to_str,
        r,
        RTSparrow(rt),
        obj,
        &v);
    if(r == 0) ret = Vget_str(&v);
    else {
      *fail = 1;
      return ret;
    }
  } else {
    double num = ValueConvNumber(obj,fail);
    char buf[256];
    int len;
    if(*fail) {
      goto fail;
    } else {
      len = NumPrintF(num,buf,256);
      assert(len > 0);
      ret = ObjNewStr(RTSparrow(rt),buf,len);
    }
  }
  *fail = 0;
  return ret;
fail:
  *fail = 1;
  RuntimeError(rt,PERR_CONVERSION_ERROR,ValueGetTypeString(obj),"string");
  return ret;
}

int ValueToBoolean( struct Runtime* runtime , Value v ) {
  if(Vis_true(&v)) {
    return 1;
  } else if(Vis_false(&v) || Vis_null(&v)) {
    return 0;
  } else if(Vis_number(&v)) {
    return v.num ? 1 : 0;
  } else if(Vis_udata(&v)) {
    struct ObjUdata* udata = Vget_udata(&v);
    int r;
    Value val;

    INVOKE_METAOPS(udata->name.str,
        runtime,
        udata->mops,
        to_boolean,
        r,
        RTSparrow(runtime),
        v,
        &val);
    if(r == 0) return Vget_boolean(&val);
    else return 1; /* Even if we fail at calling to_boolean,
                    * our default strategy is just ignore the
                    * boolean operation and just return 1 for
                    * user data */
  } else if(Vis_map(&v)) {
    struct ObjMap* map = Vget_map(&v);
    if(map->mops) {
      int r;
      Value val;
      INVOKE_METAOPS("map",runtime,
          map->mops,
          to_boolean,
          r,
          RTSparrow(runtime),
          v,
          &val);
      if(r == 0 && Vis_boolean(&val))
        return Vget_boolean(&val);
    }

    /* fallback */
    return 1;
  } else {
    return 1;
  }
}

struct ObjStr* ObjStrCat( struct Sparrow* sth ,
    const struct ObjStr* left ,
    const struct ObjStr* right ) {
  GCTry(sth);
  return ObjStrCatNoGC(sth,left,right);
}

/* MetaOps wrapper functions ----------------------------- */
int MetaOps_geti( Value func , struct Sparrow* sparrow ,
    Value object , enum IntrinsicAttribute iattr , Value* ret ) {
  Value index;
  Vset_number(&index,iattr);
  if(PushArg(sparrow,object)) return -1;
  if(PushArg(sparrow,index)) return -1;
  if(CallFunc(sparrow,func,2,ret)) return -1;
  if(Vis_null(ret)) {
    RuntimeError(sparrow->runtime,PERR_HOOKED_METAOPS_ERROR,
        ValueGetTypeString(object),METAOPS_NAME(geti));
    return -1;
  }
  return 0;
}

int MetaOps_get( Value func , struct Sparrow* sparrow ,
    Value object , Value key , Value* ret ) {
  if(PushArg(sparrow,object)) return -1;
  if(PushArg(sparrow,key)) return -1;
  if(CallFunc(sparrow,func,2,ret)) return -1;
  if(Vis_null(ret)) {
    RuntimeError(sparrow->runtime,PERR_HOOKED_METAOPS_ERROR,
        ValueGetTypeString(object),METAOPS_NAME(get));
    return -1;
  }
  return 0;
}

int MetaOps_set( Value func , struct Sparrow* sparrow ,
    Value object , Value key , Value value ) {
  Value ret;
  if(PushArg(sparrow,object)) return -1;
  if(PushArg(sparrow,key)) return -1;
  if(PushArg(sparrow,value)) return -1;
  if(CallFunc(sparrow,func,3,&ret)) return -1;
  if(Vis_false(&ret)) {
    RuntimeError(sparrow->runtime,PERR_HOOKED_METAOPS_ERROR,
        ValueGetTypeString(object),METAOPS_NAME(set));
    return -1;
  }
  return 0;
}

int MetaOps_seti( Value func , struct Sparrow* sparrow ,
    Value object , enum IntrinsicAttribute iattr , Value value ) {
  Value index;
  Value ret;
  if(PushArg(sparrow,object)) return -1;
  Vset_number(&index,iattr);
  if(PushArg(sparrow,index)) return -1;
  if(PushArg(sparrow,value)) return -1;
  if(CallFunc(sparrow,func,3,&ret)) return -1;
  if(Vis_false(&ret)) {
    RuntimeError(sparrow->runtime,PERR_HOOKED_METAOPS_ERROR,
        ValueGetTypeString(object),METAOPS_NAME(seti));
    return -1;
  }
  return 0;
}

int MetaOps_hash( Value func , struct Sparrow* sparrow ,
    Value object , int32_t* ret ) {
  Value r;
  if(PushArg(sparrow,object)) return -1;
  if(CallFunc(sparrow,func,1,&r)) return -1;
  if(Vis_number(&r)) {
    /* Not really safe here .... */
    *ret = (int32_t)Vget_number(&r);
    return 0;
  } else {
    RuntimeError(sparrow->runtime,PERR_HOOKED_METAOPS_ERROR,
        ValueGetTypeString(object),METAOPS_NAME(hash));
    return -1;
  }
  return 0;
}

int MetaOps_key( Value func , struct Sparrow* sparrow ,
    Value object ,Value* ret ) {
  if(PushArg(sparrow,object)) return -1;
  if(CallFunc(sparrow,func,1,ret)) return -1;
  if(Vis_null(ret)) {
    RuntimeError(sparrow->runtime,PERR_HOOKED_METAOPS_ERROR,
        ValueGetTypeString(object),METAOPS_NAME(key));
    return -1;
  }
  return 0;
}

int MetaOps_exist( Value func , struct Sparrow* sparrow ,
    Value object , Value key , int* ret ) {
  Value r;
  if(PushArg(sparrow,object)) return -1;
  if(PushArg(sparrow,key)) return -1;
  if(CallFunc(sparrow,func,2,&r)) return -1;
  if(!Vis_boolean(&r)) {
    RuntimeError(sparrow->runtime,PERR_HOOKED_METAOPS_ERROR,
        ValueGetTypeString(object),METAOPS_NAME(exist));
    return -1;
  }
  return 0;
}

int MetaOps_size( Value func , struct Sparrow* sparrow ,
    Value object , size_t* ret ) {
  Value r;
  if(PushArg(sparrow,object)) return -1;
  if(CallFunc(sparrow,func,1,&r)) return -1;
  if(!Vis_number(&r) || ToSize(Vget_number(&r),ret) ) {
    RuntimeError(sparrow->runtime,PERR_HOOKED_METAOPS_ERROR,
        ValueGetTypeString(object),METAOPS_NAME(exist));
    return -1;
  }
  return 0;
}

int MetaOps_print( Value func , struct Sparrow* sparrow ,
    Value object , struct StrBuf* sbuf ) {
  Value r;
  if(PushArg(sparrow,object)) return -1;
  if(CallFunc(sparrow,func,1,&r)) return -1;
  if(Vis_str(&r)) {
    struct ObjStr* str = Vget_str(&r);
    StrBufAppendStrLen( sbuf , str->str , str->len );
    return 0;
  } else {
    return -1;
  }
}

int MetaOps_to_str( Value func , struct Sparrow* sparrow ,
    Value object , Value* ret ) {
  if(PushArg(sparrow,object)) return -1;
  if(CallFunc(sparrow,func,1,ret)) return -1;
  if(Vis_str(ret)) return 0;
  else {
    RuntimeError(sparrow->runtime,PERR_HOOKED_METAOPS_ERROR,
        ValueGetTypeString(object),METAOPS_NAME(to_str));
    return -1;
  }
}

int MetaOps_to_boolean( Value func , struct Sparrow* sparrow ,
    Value object , Value* ret ) {
  if(PushArg(sparrow,object)) return -1;
  if(CallFunc(sparrow,func,1,ret)) return -1;
  if(Vis_boolean(ret)) return 0;
  else {
    RuntimeError(sparrow->runtime,PERR_HOOKED_METAOPS_ERROR,
        ValueGetTypeString(object),METAOPS_NAME(to_boolean));
    return -1;
  }
}

int MetaOps_to_number( Value func , struct Sparrow* sparrow ,
    Value object , Value* ret ) {
  if(PushArg(sparrow,object)) return -1;
  if(CallFunc(sparrow,func,1,ret)) return -1;
  if(Vis_number(ret)) return 0;
  else {
    RuntimeError(sparrow->runtime,PERR_HOOKED_METAOPS_ERROR,
        ValueGetTypeString(object),METAOPS_NAME(to_number));
    return -1;
  }
}
