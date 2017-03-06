#include <vm/gc.h>
#include <vm/object.h>
#include <vm/list.h>
#include <vm/map.h>
#include <vm/vm.h>

static void destroy_proto( struct ObjProto* cls ) {
  CodeBufferDestroy(&(cls->code_buf));
  free(cls->num_arr);
  free(cls->str_arr);
  free(cls->uv_arr);
  CStrDestroy(&(cls->proto));
  cls->num_arr = NULL;
  cls->num_size = cls->num_cap = 0;
  cls->str_arr = NULL;
  cls->str_size = cls->str_cap = 0;
  cls->uv_arr = NULL;
  cls->uv_size = cls->uv_cap = 0;
}

/* Real routine that *deletes* resource based on GC object's type */
void GCFinalizeObj( struct Sparrow* sth, struct GCRef* obj ) {
  UNUSE_ARG(sth);
  switch(obj->gtype) {
    case VALUE_STRING:
      free(obj);
      break;
    case VALUE_LIST:
      ObjListDestroy(gc2obj(obj,struct ObjList));
      free(obj);
      break;
    case VALUE_MAP:
      ObjMapDestroy(gc2obj(obj,struct ObjMap));
      free(obj);
      break;
    case VALUE_PROTO:
      destroy_proto(gc2obj(obj,struct ObjProto));
      free(obj);
      break;
    case VALUE_METHOD:
      free(obj);
      break;
    case VALUE_UDATA:
      {
        struct ObjUdata* udata = gc2obj(obj,struct ObjUdata);
        CStrDestroy(&(udata->name));
        if(udata->destroy) udata->destroy(udata->udata);
        free(udata->mops);
        free(obj);
      }
      break;
    case VALUE_ITERATOR:
      {
        struct ObjIterator* itr = gc2obj(obj,struct ObjIterator);
        if(itr->destroy) itr->destroy(itr->u.ptr);
        free(obj);
      }
      break;
    case VALUE_MODULE:
      ObjDestroyModule(gc2obj(obj,struct ObjModule));
      free(obj);
      break;
    case VALUE_COMPONENT:
      free(obj);
      break;
    case VALUE_CLOSURE:
      free(obj);
      break;
    case VALUE_LOOP:
      free(obj);
      break;
    case VALUE_LOOP_ITERATOR:
      free(obj);
      break;
    default:
      SPARROW_ASSERT(!"unreachable!");
      break;
  }
}

/* Mark phase */
#define gcstate(OBJ) ((OBJ)->gc.gc_state)
#define gcmarked(OBJ) (gcstate(OBJ) == GC_MARKED)
#define gcunmarked(OBJ) (!gcmarked(OBJ))
#define gcsetmark(OBJ) (gcstate(OBJ) = GC_MARKED)
#define gcsetunmark(OBJ) (gcstate(OBJ) = GC_UNMARKED)

static void mark_mops( struct MetaOps* mops ) {
#define __(A,B,C) GCMark(mops->hook_##B);
  METAOPS_LIST(__)
#undef __ /* __ */
}

SPARROW_INLINE void GCMarkString( struct ObjStr* str ) {
  if(gcunmarked(str)) {
    gcsetmark(str);
  }
}

void GCMarkList( struct ObjList* list ) {
  if(gcunmarked(list)) {
    size_t i;
    gcsetmark(list);
    for( i = 0 ; i < list->size ; ++i ) {
      GCMark(list->arr[i]);
    }
  }
}

void GCMarkMap( struct ObjMap* map ) {
  if(gcunmarked(map)) {
    size_t i;
    gcsetmark(map);
    for( i = 0 ; i < map->cap ; ++i ) {
      struct ObjMapEntry* e = map->entry + i;
      if(!e->used || e->del) continue;
      GCMark(e->value);
      GCMarkString( e->key );
    }
    if(map->mops) mark_mops(map->mops);
  }
}

void GCMarkMethod( struct ObjMethod* method ) {
  if(gcunmarked(method)) {
    method->gc.gc_state = GC_MARKED;
    GCMark( method->object );
    GCMarkString(method->name);
  }
}

void GCMarkUdata( struct ObjUdata* udata ) {
  if(gcunmarked(udata)) {
    if(udata->mark) udata->mark(udata);
    udata->gc.gc_state = GC_MARKED;
    if(udata->mops) mark_mops(udata->mops);
  }
}

void GCMarkIter( struct ObjIterator* itr ) {
  if(gcunmarked(itr)) {
    gcsetmark(itr);
    GCMark(itr->obj);
  }
}

void GCMarkProto( struct ObjProto* proto ) {
  if(gcunmarked(proto)) {
    size_t i;
    gcsetmark(proto);
    for(i = 0 ; i < proto->str_size ; ++i) {
      GCMarkString(proto->str_arr[i]);
    }
    GCMarkModule(proto->module);
  }
}

void GCMarkClosure( struct ObjClosure* closure ) {
  if(gcunmarked(closure)) {
    size_t i;
    const size_t usize = closure->proto->uv_size;
    gcsetmark(closure);
    GCMarkProto(closure->proto);
    for( i = 0 ; i < usize ; ++i ) {
      GCMark(closure->upval[i]);
    }
  }
}

void GCMarkModule( struct ObjModule* module ) {
  if(gcunmarked(module)) {
    size_t i;
    gcsetmark(module);
    for( i = 0 ; i < module->cls_size ; ++i ) {
      GCMarkProto(module->cls_arr[i]);
    }
  }
}

void GCMarkComponent(
    struct ObjComponent* component ) {
  if(gcunmarked(component)) {
    gcsetmark(component);
    GCMarkMap(component->env);
    GCMarkModule(component->module);
  }
}

void GCMark( Value v ) {
  if(Vis_str(&v)) {
    GCMarkString(Vget_str(&v));
  } else if(Vis_map(&v)) {
    GCMarkMap(Vget_map(&v));
  } else if(Vis_list(&v)) {
    GCMarkList(Vget_list(&v));
  } else if(Vis_proto(&v)) {
    GCMarkProto(Vget_proto(&v));
  } else if(Vis_closure(&v)) {
    GCMarkClosure(Vget_closure(&v));
  } else if(Vis_method(&v)) {
    GCMarkMethod(Vget_method(&v));
  } else if(Vis_udata(&v)) {
    GCMarkUdata(Vget_udata(&v));
  } else if(Vis_iterator(&v)) {
    GCMarkIter(Vget_iterator(&v));
  } else if(Vis_module(&v)) {
    GCMarkModule(Vget_module(&v));
  } else if(Vis_loop(&v)) {
    struct ObjLoop* loop = Vget_loop(&v);
    if(gcunmarked(loop)) {
      gcsetmark(loop);
    }
  } else if(Vis_loop_iterator(&v)) {
    struct ObjLoopIterator* itr = Vget_loop_iterator(&v);
    if(gcunmarked(itr)) {
      gcsetmark(itr);
      gcsetmark(itr->loop);
    }
  }
}

/* Swap phase */

/* swapping the state of Sparrow object. Inside of Sparrow object,
 * there're a lot of import static field and attributes and they
 * should not be collected by GC cycle but they should involve in
 * the mark cycle since the map/table may hold other object can be
 * GC collected. Afterwards, since they don't participate in swap
 * cycle for finalize, then their gc_state flag remains marked all
 * the time. We need this routine to flip them back.
 *
 * We only need to flip gc flag of those container objects, for
 * objects that doesn't hold other gc reference, we could just skip
 * them since they don't really gets collected */

static SPARROW_INLINE
void swap_sparrow( struct Sparrow* sparrow ) {
  gcsetunmark(&(sparrow->global_env.env));
}

static void swap( struct Sparrow* sparrow ,
    int64_t* active, int64_t* inactive ) {
  struct GCRef* ref = sparrow->gc_start;
  struct GCRef** prev = &(sparrow->gc_start);
  int64_t a = 0;
  int64_t i = 0;
  while(ref) {
    if(ref->gc_state == GC_MARKED) {
      ref->gc_state = GC_UNMARKED;
      prev = &(ref->next);
      ref = ref->next;
      ++a;
    } else {
      SPARROW_ASSERT(ref->gc_state == GC_UNMARKED);
      *prev = ref->next;
      GCFinalizeObj(sparrow,ref);
      ref = *prev;
      ++i;
    }
  }
  *prev = NULL;
  sparrow->gc_sz -= i;
  swap_sparrow(sparrow);
  if(active) *active = a;
  if(inactive) *inactive = i;
}

void GCForce( struct Sparrow* sparrow ) {
  double pr;
  size_t i;
  int64_t active = 0;
  int64_t inactive = 0;
  sparrow->gc_prevsz = sparrow->gc_sz;
  sparrow->gc_ps_threshold =
    sparrow->gc_prevsz * sparrow->gc_ratio;

  /* mark string pool */
  for( i = 0 ; i < sparrow->str_cap ; ++i ) {
    if(sparrow->str_arr[i])
      GCMarkString(sparrow->str_arr[i]);
  }

  /* mark the global environment */
  GCMarkMap(&(sparrow->global_env.env));

  /* mark the runtime virtual machine.
   * TODO:: If multiple sparrows are supported,
   *        then fix the following marking
   *        algorithm for runtime */
  if(sparrow->runtime) {
    struct Runtime* runtime = sparrow->runtime;
    do {
      struct CallThread* csparrow = runtime->cur_thread;
      /* mark the stack */
      for( i = 0 ; i < csparrow->stack_size ; ++i ) {
        GCMark(csparrow->stack[i]);
      }
      /* mark all active function call */
      for( i = 0 ; i < csparrow->frame_size ; ++i ) {
        struct CallFrame* cframe = csparrow->frame+i;
        if(cframe->closure) {
          GCMarkClosure(cframe->closure);
          GCMark(cframe->callable);
        }
      }
      GCMarkComponent(csparrow->component);
      runtime = runtime->prev;
    } while(runtime);
  }

  /* swap the memory away and free them */
  swap(sparrow,&active,&inactive);

  sparrow->gc_active = active;
  sparrow->gc_inactive = inactive;
  sparrow->gc_generation++;

  /* update adjust threshold */
  pr = sparrow->gc_inactive / sparrow->gc_prevsz;
  if(pr < sparrow->gc_penalty_ratio) {
    ++sparrow->gc_penalty_times;

    sparrow->gc_adjust_threshold = sparrow->gc_adjust_threshold +
      ((1 - pr) / (double)(sparrow->gc_penalty_times)) *
      sparrow->gc_adjust_threshold;

  } else {
    sparrow->gc_penalty_times = 0;
  }
}

/* This is actually the core of our GC */
static SPARROW_INLINE
int trigger_gc( struct Sparrow* sparrow ) {
  if(sparrow->runtime && /* If we don't have a runtime, just don't do GC */
     sparrow->gc_sz >= sparrow->gc_adjust_threshold &&
     sparrow->gc_sz >= sparrow->gc_ps_threshold)
    return 1;
  return 0;
}

int GCTry( struct Sparrow* sparrow ) {
  if(trigger_gc(sparrow)) {
    GCForce(sparrow);
    return 0;
  } else {
    return -1;
  }
}
