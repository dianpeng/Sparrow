#ifndef IR_SET_H_
#define IR_SET_H_
#include <sparrow.h>
#include <compiler/ir.h>
#include <shared/util.h>

/* ===================================================
 * IrNode stack structure. Used to help visiting
 * =================================================*/

struct IrNodeStack {
  struct IrNode** ir_arr;
  size_t ir_size;
  size_t ir_cap;
};

static SPARROW_INLINE void IrNodeStackPush( struct IrNodeStack* stack ,
                                            struct IrNode* node ) {
  DynArrPush(stack,ir,node);
}

static SPARROW_INLINE void IrNodeStackInit( struct IrNodeStack* stack , size_t cap ) {
  stack->ir_arr = malloc(sizeof(struct IrNode*)*cap);
  stack->ir_size= 0;
  stack->ir_cap = cap;
}

static SPARROW_INLINE struct IrNode* IrNodeStackPop( struct IrNodeStack* stack ) {
  struct IrNode* ret;
  SPARROW_ASSERT( stack->ir_size > 0 );
  ret = stack->ir_arr[stack->ir_size-1];
  --stack->ir_size;
  return ret;
}

static SPARROW_INLINE struct IrNode* IrNodeStackTop( struct IrNodeStack* stack ) {
  SPARROW_ASSERT( stack->ir_size > 0 );
  return stack->ir_arr[stack->ir_size-1];
}

static SPARROW_INLINE int IrNodeStackIsEmpty( struct IrNodeStack* stack ) {
  return stack->ir_size == 0;
}

static SPARROW_INLINE size_t IrNodeStackGetSize( struct IrNodeStack* stack ) {
  return stack->ir_size;
}

#endif /* IR_SET_H_ */
