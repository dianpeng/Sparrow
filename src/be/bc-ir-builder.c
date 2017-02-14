#include "bc-ir-builder.h"
#include "../fe/object.h"

struct BytecodeIrBuilder {
  struct IrGraph* graph;     /* Targeted graph */
  /* Error buffer */
  struct CStr err;
};

struct StackStats {
  size_t stk_size;
  size_t stk_cap;
  struct IrNode** stk_arr;
};

static void ss_init( struct StackStats* ss ) {
  ss->stk_size = 0;
  ss->stk_cap = 8;
  ss->stk_arr = malloc( sizeof(struct IrNode*) * 8 );
}

static void ss_push( struct StackStats* ss, struct IrNode* node ) {
  DynArrPush(ss,stk,node);
}

static void ss_pop( struct StackStats* ss, size_t num ) {
  assert(num <= ss->stk_size);
  ss->stk_size -= num;
}

static struct IrNode* ss_top( struct StackStats* ss, size_t num ) {
  assert(num < ss->stk_size);
  return ss->stk_arr[ (ss->stk_size - 1) - num ];
}

static struct IrNode* ss_replace( struct StackStats* ss ,
    struct IrNode* node ) {
  assert(ss->stk_size > 0);
  ss->stk_arr[ (ss->stk_size-1) ] = node;
}

static void ss_copy( const struct StackStats* src , struct StackStats* dest ) {
  size_t i;
  dest->stk_size = src->stk_size;
  dest->stk_cap = src->stk_cap;
  dest->stk_arr = malloc(sizeof(struct IrNode*)*src->stk_size);
  memcpy(dest->stk_arr,src->stk_arr,dest->stk_size*sizeof(struct IrNode*));
}

/* IrBuilder ============================================== */
static int build_graph( struct Sparrow* sparrow , const struct ObjProto* proto,
                                                  struct IrGraph* graph ,
                                                  struct IrNode* region ,
                                                  struct BytecodeIrBuilder* builder, 
                                                  struct StackStats* stack ) {
  uint8_t op;
  uint32_t opr;
  size_t code_pos = 0;
  struct CodeBuffer* code_buffer = &(proto->code_buf);

  /* Similar with VM, we will use threading code if we can; otherwise default
   * to switch case code */

#define DECODE_ARG() \
  do { \
    opr = CodeBufferDecodeArg(code_buffer,code_pos); \
    code_pos += 3; \
  } while(0)

#ifndef SPARROW_VM_NO_THREADING
    static const void* jump_table[] = {
#define X(A,B,C) &&label_##A,
      BYTECODE(X)
      NULL
#undef X /* X */
    };

#define CASE(X) label_##X:
#define DISPATCH() \
    do { \
      if( code_pos == code_buffer->pos ) { \
        goto finish; \
      } \
      op = code_buffer->buf[code_pos++]; \
      goto *jump_table[op]; \
    } while(0)

    DISPATCH(); /* very first dispatch for the threading code */
#else
#define CASE(X) case X:
#define DISPATCH() break
  while( code_pos < code_buffer->pos ) {
    op = code_buffer->buf[code_pos++];
    switch(op) {
#endif /* SPARROW_VM_NO_THREADING */

    CASE(BC_ADDNV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_ADD ,
          IrNodeNewConstNumber(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_ADDVN) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_ADD ,
          ss_top(stack,0),
          IrNodeNewConstNumber(graph,opr,proto),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_ADDSV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          ss_top(stack,0),
          IrNodeNewConstString(graph,opr,proto),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_ADDVS) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          IrNodeNewConstString(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_ADDVV) {
      struct IrNode* bin = IrNodeNewBinary(graph,IR_H_ADD,
          ss_top(stack,1),
          ss_top(stack,0),
          region);
      ss_pop(stack,2);ss_push(stack,bin);
      DISPATCH();
    }

    CASE(BC_SUBNV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          IrNodeNewConstNumber(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_SUBVN) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          IrNodeNewConstNumber(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_SUBVV) {
      struct IrNode* bin;
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          ss_top(stack,1),
          ss_top(stack,0),
          region);
      ss_pop(stack,2); ss_push(stack,bin);
      DISPATCH();
    }
    
    CASE(BC_MULNV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          IrNodeNewConstNumber(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_MULVN) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          ss_top(stack,0),
          IrNodeNewConstNumber(graph,opr,proto),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_MULVV) {
      struct IrNode* bin;
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          ss_top(stack,1),
          ss_top(stack,0),
          region);
      ss_pop(stack,2);
      ss_push(stack,bin);
      DISPATCH();
    }

    CASE(BC_DIVNV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          IrNodeNewConstNumber(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_DIVVN) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          ss_top(stack,0),
          IrNodeNewConstNumber(graph,opr,proto),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_DIVV) {
      struct IrNode* bin = IrNodeNewBinary(graph,IR_H_ADD,
          ss_top(stack,1),
          ss_top(stack,0),
          region);
      ss_pop(stack,2); ss_push(stack,bin);
      DISPATCH();
    }

    CASE(BC_MODVN) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          IrNodeNewConstNumber(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_MODNV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          ss_top(stack,0),
          IrNodeNewConstNumber(graph,opr,proto),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_MODVV) {
      struct IrNode* bin = IrNodeNewBinary(graph,IR_H_ADD,
          ss_top(stack,1),
          ss_top(stack,0),
          region);
      ss_pop(stack,2); ss_push(stack,bin);
      DISPATCH();
    }

    CASE(BC_POWNV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          IrNodeNewConstNumber(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_POWVN) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          ss_top(stack,0),
          IrNodeNewConstNumber(graph,opr,proto),
          region);
      ss_replace(stack,0);
      DISPATCH();
    }

    CASE(BC_POWVV) {
      struct IrNode* bin;
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          ss_top(stack,1),
          ss_top(stack,0),
          region);
      ss_pop(stack,2); ss_push(stack,bin);
      DISPATCH();
    }

    CASE(BC_NEG) {
      struct IrNode* una = IrNodeNewUnary(graph,IR_H_NEG,
          ss_top(stack,0),
          region);
      ss_replace(stack,una);
      DISPATCH();
    }

    CASE(BC_NOT) {
      struct IrNode* una = IrNodeNewUnary(graph,IR_H_NOT,
          ss_top(stack,0),
          region);
      ss_replace(stack,una);
      DISPATCH();
    }

    CASE(BC_TEST) {
      struct IrNode* una = IrNodeNewUnary(graph,IR_H_TEST,
          ss_top(stack,0),
          region);
      ss_replace(stack,una);
      DISPATCH();
    }

    /* Constant loading instructions. For constant node we don't link
     * it to the region node since we don't need to. If this node's value
     * is not a dead value, then it will somehow be used in certain
     * expression. Then obviously it will linked the graph later. Otherwise
     * automatically it is DECed */
    CASE(BC_LOADN) {
      struct IrNode* n;
      DECODE_ARG();
      n = IrNodeNewConstNumber(graph,opr,proto);
      ss_push(stack,n);
      DISPATCH();
    }



#ifdef SPARROW_VM_NO_THREADING
      default:
        verify(!"Unknown instruction!");
    } /* switch */
  } /* while */
#else
finish:
#endif /* SPARROW_VM_NO_THREADING */







  return 0;
}






















