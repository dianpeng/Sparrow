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
      struct IrNode* num;
      struct IrNode* val;
      struct IrNode* bin;
      DECODE_ARG();
      num = IrNodeNewConstNumber(graph,opr,proto);
      val = ss_top(stack,0);
      bin = IrNodeNewBinary(graph,IR_H_ADD,num,val);
      IrNodeAddInput( region , bin );
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_ADDVN) {
      struct IrNode* val;
      struct IrNode* num;
      struct IrNode* bin;
      DECODE_ARG();
      num = IrNodeNewConstNumber(graph,opr,proto);
      val = ss_top(stack,0);
      bin = IrNodeNewBinary(graph,IR_H_ADD,val,num);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_ADDSV) {
      struct IrNode* left;
      struct IrNode* right;
      struct IrNode* bin;
      DECODE_ARG();
      left = ss_top(stack,0); assert(left->type == IR_CONST_STRING);
      right= IrNodeNewConstString(graph,opr,proto);
      bin = IrNodeNewBinary(graph,IR_H_ADD,left,right);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_ADDVS) {
      struct IrNode* left;
      struct IrNode* right;
      struct IrNode* bin;
      DECODE_ARG();
      left = IrNodeNewConstString(graph,opr,proto);
      right = ss_top(stack,0); assert(right->type == IR_CONST_STRING);
      bin = IrNodeNewBinary(graph,IR_H_ADD,left,right);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_ADDVV) {
      struct IrNode* left = ss_top(stack,1);
      struct IrNode* right= ss_top(stack,0);
      struct IrNode* bin = IrNodeNewBinary(graph,IR_H_ADD,left,right);
      ss_pop(stack,2);ss_push(stack,bin);
      DISPATCH();
    }

    CASE(BC_SUBNV) {
      struct IrNode* left;
      struct IrNode* right;
      struct IrNode* bin;
      DECODE_ARG();
      left = IrNodeNewConstNumber(graph,opr,proto);
      right= ss_top(stack,0);
      bin = IrNodeNewBinary(graph,IR_H_ADD,left,right);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_SUBVN) {
      struct IrNode* left;
      struct IrNode* right;
      struct IrNode* bin;
      DECODE_ARG();
      left = IrNodeNewConstNumber(graph,opr,proto);
      right= ss_top(stack,0);
      bin = IrNodeNewBinary(graph,IR_H_ADD,left,right);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_SUBVV) {
      struct IrNode* left;
      struct IrNode* right;
      struct IrNode* bin;
      left = ss_top(stack,1);
      right= ss_top(stack,0);
      bin = IrNodeNewBinary(graph,IR_H_ADD,left,right);
      ss_pop(stack,2);
      ss_push(stack,bin);
      DISPATCH();
    }
    
    CASE(BC_MULNV) {
      struct IrNode* left;
      struct IrNode* right;
      struct IrNode* bin;
      DECODE_ARG();
      left = IrNodeNewConstNumber(graph,opr,proto);
      right= ss_top(stack,0);
      bin = IrNodeNewBinary(graph,IR_H_ADD,left,right);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_MULVN) {
      struct IrNode* left;
      struct IrNode* right;
      struct IrNode* bin;
      DECODE_ARG();
      left = ss_top(stack,0);
      right= IrNodeNewConstNumber(graph,opr,proto);
      bin = IrNodeNewBinary(graph,IR_H_ADD,left,right);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_MULVV) {
      struct IrNode* left;
      struct IrNode* right;
      struct IrNode* bin;
      left = ss_top(stack,1);
      right= ss_top(stack,0);
      bin = IrNodeNewBinary(graph,IR_H_ADD,left,right);
      ss_pop(stack,2);
      ss_push(stack,bin);
      DISPATCH();
    }

    CASE(BC_DIVNV) {
      struct IrNode* left;
      struct IrNode* right;
      struct IrNode* bin;
      DECODE_ARG();
      left = IrNodeNewConstNumber(graph,opr,proto);
      right = ss_top(stack,0);
      bin = IrNodeNewBinary(graph,IR_H_ADD,left,right);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_DIVVN) {
      struct IrNode* left;
      struct IrNode* right;
      struct IrNode* bin;
      DECODE_ARG();
      left = ss_top(stack,0);
      right= IrNodeNewConstNumber(graph,opr,proto);
      bin = IrNodeNewBinary(graph,IR_H_ADD,left,right);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_DIVV) {
      struct IrNode* left = ss_top(stack,1);
      struct IrNode* right= ss_top(stack,0);
      struct IrNode* bin = IrNodeNewBinary(graph,IR_H_ADD,left,right);
      ss_pop(stack,2);
      ss_push(stack,bin);
      DISPATCH();
    }

    CASE(BC_MODVN) {
      struct IrNode* left;
      struct IrNode* right;
      struct IrNode* bin;
      DECODE_ARG();
      left = IrNodeNewConstNumber(graph,opr,proto);
      right= ss_top(stack,0);
      bin = IrNodeNewBinary(graph,IR_H_ADD,left,right);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_MODNV) {
      struct IrNode* left;
      struct IrNode* right;
      struct IrNode* bin;
      DECODE_ARG();
      left = ss_top(stack,0);
      right= IrNodeNewConstNumber(graph,opr,proto);
      bin = IrNodeNewBinary(graph,IR_H_ADD,left,right);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_MODVV) {
      struct IrNode* left = ss_top(stack,1);
      struct IrNode* right= ss_top(stack,0);
      struct IrNode* bin = IrNodeNewBinary(graph,IR_H_ADD,left,right);
      ss_pop(stack,2);
      ss_push(stack,bin);
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






















