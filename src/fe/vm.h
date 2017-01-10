#ifndef VM_H_
#define VM_H_
#include "object.h"
#include "bc.h"
#include "../util.h"

/* Currently we don't support multiple threads */
#define SIZE_OF_THREADS 1

/* Represents one function call */
struct CallFrame;
struct CallThread;

struct CallFrame {
  int base_ptr;   /* base pointer */
  size_t pc;      /* program counter */
  struct ObjClosure* closure; /* closure */
  size_t narg;    /* argument count */
  Value callable; /* callable value, can be ObjMethod or ObjUdata */
};

struct CallThread {
  struct Sparrow* sparrow;
  struct ObjComponent* component;
  struct Runtime* runtime;

  struct CallFrame* frame;
  size_t frame_size;
  size_t frame_cap;

  Value* stack;
  size_t stack_size;
  size_t stack_cap;
};

struct Runtime {
  struct Runtime* prev; /* Previous runtime */
  struct CallThread* cur_thread;
  struct CallThread ths[SIZE_OF_THREADS];
  size_t max_funccall;
  size_t max_stacksize;
  struct CStr error; /* error description */
};

/* Helper macro to make our life easier */
#define RTSparrow(RT) ((RT)->cur_thread->sparrow)
#define RTCallThread(RT) ((RT)->cur_thread)
#define RTCurFrame(RT) (RTCallThread(RT)->frame+RTCallThread(RT)->frame_size-1)

/* Helper functions for C function to retrieve argument passed from
 * script */
static SPARROW_INLINE
Value RuntimeGetArg( struct Runtime* rt , size_t narg ) {
  struct CallThread* thread = rt->cur_thread;
  struct CallFrame* frame =  thread->frame + thread->frame_size -1;
  size_t arg_idx;
  assert( narg < frame->narg );
  arg_idx = frame->base_ptr + narg;
  return thread->stack[arg_idx];
}

static SPARROW_INLINE
size_t RuntimeGetArgSize( struct Runtime* rt ) {
  return RTCurFrame(rt)->narg;
}

/* Argument checking type flag */
#define ARGTYPE(__) \
  __(ARG_NUMBER,"number") \
  __(ARG_TRUE,"true") \
  __(ARG_FALSE,"false") \
  __(ARG_NULL,"null") \
  __(ARG_LIST,"list") \
  __(ARG_MAP,"map") \
  __(ARG_PROTO,"proto") \
  __(ARG_CLOSURE,"closure") \
  __(ARG_METHOD,"method") \
  __(ARG_UDATA,"udata") \
  __(ARG_STRING,"string") \
  __(ARG_ITERATOR,"iterator") \
  __(ARG_MODULE,"module") \
  __(ARG_COMPONENT,"component") \
  __(ARG_ANY,"any") \
  __(ARG_CONV_NUMBER,"number") \
  __(ARG_BOOLEAN,"boolean") \
  __(ARG_CONV_BOOLEAN,"boolean or number") \
  __(ARG_GCOBJECT,"gc object")

enum ArgType {
#define __(A,B) A,
  ARGTYPE(__)
  SIZE_OF_ARGTYPES
#undef __
};

int RuntimeCheckArg( struct Runtime* rt , const char* , size_t , ... );
void RuntimeError( struct Runtime* rt , const char* format , ... );

/* Execute a ObjModule object's entry code and get the return value */
int Execute( struct Sparrow* , struct ObjComponent* , Value* ,
    struct CStr* error );

#endif /* VM_H_ */
