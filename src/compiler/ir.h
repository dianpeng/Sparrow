#ifndef IR_H_
#define IR_H_
#include <sparrow.h>
#include <shared/util.h>
#include <shared/debug.h>
#include <vm/bc.h>
#include <vm/object.h>
#include <compiler/arch.h>

struct IrNode;
struct IrLink;
struct IrGraph;

/* IR ---> Intermediate Representation
 * IR in Sparrow is model with Cliff Click's doctor thesis, ie sea-of-nodes.
 * I personally like the simplicity of this IR, especially there's no 2 layer
 * architecutures like in traditional CFG. Same as most of sea-of-nodes
 * mplementation, we have all layer of IR ( HIR , MIR , LIR ) in one single
 * graph. And the graph is enough for all stages of optimization.
 *
 * The IR is basically a SSA form and it is derived directly from bytecode.
 * We will have a bytecode ir builder , and also a high-level assembly like
 * ir builder which helps us to write stub assembly , similar with V8's interpreter.
 *
 * All the IR is composed of node and edge. Node carries all the information and
 * have different type/category. Edge is *unlabled* but impose constraints. It
 * is easy to generate 1) control flow graph and also 2) visit expression just by
 * using this single IR.
 *
 * Additionally information regarding IR is stored parallel in other bundled
 * data structure. Like dominator tree and liveless map are stored in an object
 * called JitContext. This object is per module based and serve as the central
 * data structure for any methods are been jitting .
 *
 *
 * IR Lowering is in space. The optimization algorithm will just modify the IR
 * in place . Since IR just a loosely connected graph, modify the graph is just
 * changing some pointers.
 *
 * For debug purpose, a IR serializer will be provided to serialize the IR into
 * dot format, then user is able to use graphviz to visualize it.
 *
 * IR doesn't have a fixed format but could have *multiple* information based on
 * type of the IR. All IR node has a common header IrNode , inside of the IrNode
 * we have a type field to help user *cast* the IrNode pointer to correct sub
 * type. It is really just traditional way to do single inheritance in C.
 */

#define CONTROL_IR_LIST(X) \
  X(CTL_START,"ctl_start") \
  /* loop header region */ \
  X(CTL_LOOP,"ctl_loop") \
  X(CTL_LOOP_EXIT,"ctl_loop_exit") \
  /* merge region , the only region that can have PHI */ \
  X(CTL_MERGE,"ctl_merge") \
  /* place holder for a bunch of value */ \
  X(CTL_REGION,"ctl_region") \
  /* branch node */ \
  X(CTL_IF,"ctl_if") \
  X(CTL_IF_TRUE,"ctl_if_true") \
  X(CTL_IF_FALSE,"ctl_if_false") \
  X(CTL_RET,"ctl_ret") \
  X(CTL_END,"ctl_end")

/* Shared IR list . IR here is shared throughout the optimization stage */
#define SHARED_IR_LIST(X) \
  X(PHI,"c_phi") \
  X(PROJECTION,"c_projection")

/* Constant IR list. */
#define CONSTANT_IR_LIST(X) \
  X(CONST_INT32,"const_int32") \
  X(CONST_INT64,"const_int64") \
  X(CONST_REAL64,"const_real64") \
  X(CONST_STRING,"const_string") \
  X(CONST_BOOLEAN,"const_boolean") \
  X(CONST_NULL,"const_null")

/* Primitive IR list */
#define PRIMITIVE_IR_LIST(X) \
  X(PRIMITIVE_LIST,"primitive_list") \
  X(PRIMITIVE_MAP,"primitive_map") \
  X(PRIMITIVE_PAIR,"primitive_pair") \
  X(PRIMITIVE_CLOSURE,"primitive_closure") \
  X(PRIMITIVE_UPVALUE_DETACH,"primitive_upvalue_detach") \
  X(PRIMITIVE_ARGUMENT,"primitive_argument")

/* High level IR , the IR is mostly a one-2-one mapping of the bytecode and with
 * some simplicity. It is the IR that is used to build IR graph */
#define HIGH_IROP_LIST(X) \
  /* Numeric Operations */ \
  X(H_ADD,"h_add") \
  X(H_SUB,"h_sub") \
  X(H_MUL,"h_mul") \
  X(H_DIV,"h_div") \
  X(H_POW,"h_pow") \
  X(H_MOD,"h_mod") \
  /* Comparison */ \
  X(H_LT,"h_lt") \
  X(H_LE,"h_le") \
  X(H_GT,"h_gt") \
  X(H_GE,"h_ge") \
  X(H_EQ,"h_eq") \
  X(H_NE,"h_he") \
  /* Unary */ \
  X(H_NEG,"h_neg") \
  X(H_NOT,"h_not") \
  X(H_TEST,"h_test") \
  /* Upvalue */ \
  X(H_UGET,"h_uget") \
  X(H_USET,"h_uset") \
  /* Property acesss without information */ \
  /* Very suprising is that we *DROP* the high level information for */ \
  /* whether it is a index (via []) or a property (via "."). That is */ \
  /* because the language distinguish a list from a map here. Mike Pall */ \
  /* used to point out that PyPy omits several important information on its */ \
  /* low level constructs which make AA extreamly hard to progress. In our */ \
  /* case we do make those information stay. So a high level AA would be a */ \
  /* pass follow the type inference. We will know that whether the alias existed */ \
  /* or not. */ \
  X(H_AGET,"h_aget") \
  X(H_ASET,"h_aset") \
  X(H_AGET_INTRINSIC,"h_aget_intrinsic") \
  X(H_ASET_INTRINSIC,"h_aset_intrinsic") \
  /* Global */ \
  X(H_GGET,"h_gget") \
  X(H_GSET,"h_gset") \
  /* Iterator */ \
  X(H_ITER_TEST,"h_iter_test") \
  X(H_ITER_NEW ,"h_iter_new") \
  X(H_ITER_DREF,"h_iter_dref") \
  /* Function */ \
  X(H_CALL,"h_call") \
  X(H_CALL_INTRINSIC,"h_call_intrinsic")

#define ALL_IRS(X) \
  CONTROL_IR_LIST(X) \
  SHARED_IR_LIST(X) \
  CONSTANT_IR_LIST(X) \
  PRIMITIVE_IR_LIST(X) \
  HIGH_IROP_LIST(X)

/* Define each IR list */
#define X(A,B) IR_##A,
enum {
  CONTROL_IR__START = 0xff,
  CONTROL_IR_LIST(X)
  CONTROL_IR__END
};

enum {
  SHARED_IR_LIST__START = 0x01ff,
  SHARED_IR_LIST(X)
  SHARED_IR__END
};

enum {
  CONSTANT_IR_LIST__START = 0x02ff,
  CONSTANT_IR_LIST(X)
  CONSTANT_IR__END
};

enum {
  PRIMITIVE_IR_LIST__START = 0x03ff,
  PRIMITIVE_IR_LIST(X)
  PRIMITIVE_IR__END
};

enum {
  HIGH_IROP_LIST__START = 0x04ff,
  HIGH_IROP_LIST(X)
  HIGH_IROP_LIST__END
};
#undef X /* X */

enum IrKind {
  IRKIND_CONTROL = 1,
  IRKIND_SHARED,
  IRKIND_CONSTANT,
  IRKIND_PRIMITIVE,
  IRKIND_HIGH_IROP
};

/* Function to check whether a ir opcode belongs to a certain type */
static SPARROW_INLINE int IrGetKindCode( int opcode ) {
  return ((opcode & 0xff00)>>8);
}

static SPARROW_INLINE int IrIsControl( int opcode ) {
  return IrGetKindCode(opcode) == IRKIND_CONTROL;
}

static SPARROW_INLINE int IrIsStatment(int opcode ) {
  return !IrIsControl(opcode);
}

static SPARROW_INLINE int IrIsShared ( int opcode ) {
  return IrGetKindCode(opcode) == IRKIND_SHARED;
}

static SPARROW_INLINE int IrIsConstant(int opcode ) {
  return IrGetKindCode(opcode) == IRKIND_CONSTANT;
}

static SPARROW_INLINE int IrIsPrimitive(int opcode) {
  return IrGetKindCode(opcode) == IRKIND_PRIMITIVE;
}

static SPARROW_INLINE int IrIsHighIr(int opcode) {
  return IrGetKindCode(opcode) == IRKIND_HIGH_IROP;
}

/* Detailed opcode category function */
static SPARROW_INLINE int IrIsHighIrBinary(int opcode) {
  return IrGetKindCode(opcode) == IRKIND_HIGH_IROP &&
         ( opcode >= IR_H_ADD && opcode <= IR_H_NE );
}

static SPARROW_INLINE int IrIsHighIrUnary (int opcode) {
  return IrGetKindCode(opcode) == IRKIND_HIGH_IROP &&
         ( opcode >= IR_H_NEG && opcode <= IR_H_TEST);
}

const char* IrGetName( int opcode );

static SPARROW_INLINE int IrNumberGetType(double number) {
  double intpart;
  double rest = modf(number,&intpart);
  if(rest == 0.0) {
    /* Treat it as an integer and see which range it falls into */
    if(intpart > (double)SPARROW_INT64_MAX || intpart < (double)SPARROW_INT64_MIN)
      return IR_CONST_REAL64;
    else {
      int64_t intval = (int64_t)intpart; /* No overflow */
      if(intval > SPARROW_INT32_MAX || intval < SPARROW_INT32_MIN)
        return IR_CONST_INT64;
      else
        return IR_CONST_INT32;
    }
  } else {
    return IR_CONST_REAL64;
  }
}

/* IrUse. A use structure represents the def-use/use-def chain element. Since
 * the chain will be modified during IrGraph construction. We use a double
 * linked list here to ease the pain for removing a certain use from the IR .
 *
 * The following code uses a very ugly hack. The IrUseBase just has same structure
 * as the very first unnamed structure embedded inside of IrUse. Since we are
 * using double linked list so the tail element could just be declared as IrUseBase
 * which saves a pointer's space. Then we cast IrUseBase to IrUse to do pointer
 * comparison but we never use the node field.
 *
 * The embedded unnamed structure is simply to ease the pain for accessing the
 * field of prev and next */
struct IrUse;

struct IrUseBase {
  struct IrUse* prev;
  struct IrUse* next;
};

struct IrUse {
  struct {
    struct IrUse* prev;
    struct IrUse* next;
  };
  struct IrNode* node;
};

/* Node . A node just represents a node in an IR graph. It is the single element
 * that is used to build IR graph. Node are labeled with OPCODE which indicates
 * the semantic of the node itself. Each node has builtin storage that indicate
 * its 1) use-def 2) def-use chains. Apart from that any other information is
 * supposed to be stored inside of a out-of-index array by unique id of the node.
 * A node will get its unique ID when it is created and it is a handy identity
 * for user to use node to index to certain side information .
 *
 *
 * For a statment IrNode , (opcode != CONTROL_FLOW) , the IrNode's input and
 * output will always pointed to use-def and def-use chain. For a control flow
 * node. The Input and Output will be used as control follow sequences for
 * forward and backward purpose.
 *
 * For expression that is related to a certain region node. It is represented
 * by its private data structure which *only* used in ControlFlow node .
 *
 * Customized data is stored right after the IrNode's memory layout. And the
 * value will be retrieved based on the Op (label) of the nodes */

struct IrNode {
  uint16_t op;
  uint16_t effect : 1;       /* This is a coarsed side effect analyzing while
                              * the bytecode is translated into the IrGraph.
                              * This will impact whether the node will be added
                              * into the use chain of its belonging control flow
                              * node */

  uint16_t prop_effect:1;    /* Indicates this node is effect because one more
                              * its operand has prop_effect or effect bit set.
                              */

  uint16_t bounded: 1;       /* Whether this IrNode is bounded to a certain CF
                              * node or not. It is a helper bit to optimize remove
                              * operation on this node when this node is used by
                              * multiple expressions */

  uint32_t id;               /* Ir Unique ID . User could use it to index to
                              * a side array for associating information that
                              * is local to a certain optimization pass */

  uint32_t mark;             /* Marking states */

  /* The following use-def and def-use chain's memory is owned by the Arena
   * allocator inside of the IrGrpah */

  /* Input chain or use def chain */
  struct IrUseBase input_tail;
  int input_size;
  int input_max;

  /* Output chain or def use chain */
  struct IrUseBase output_tail;
  int output_size;
  int output_max ;
};

#define IrNodeHasEffect(IRNODE) ((IRNODE)->effect || (IRNODE)->prop_effect)

#define IrNodeGetData(IRNODE) (((char*)(IRNODE))+sizeof(*IRNODE))

/* Low level add input/output function. It won't take care of effect bit settings
 *
 * General use case for input/output chain
 *
 * 1) For control flow node , the output chain is used to represent the node's
 * control flow ; the input chain is used to represent the statement that is
 * related to this control flow node.
 *
 * 2) For none control flow node , the output node defines the def-use chains;
 * and the input node defines the use-def chains.
 */

void IrNodeAddInput (struct IrGraph* , struct IrNode* node , struct IrNode* input_node );
void IrNodeAddOutput(struct IrGraph* , struct IrNode* node , struct IrNode* output_node);

struct IrUse* IrNodeFindInput(struct IrNode* , struct IrNode* );
struct IrUse* IrNodeFindOutput(struct IrNode*, struct IrNode* );

struct IrUse* IrNodeRemoveInput(struct IrNode*,struct IrUse*);
struct IrUse* IrNodeRemoveOutput(struct IrNode*,struct IrUse*);

void IrNodeClearInput(struct IrNode*);
void IrNodeClearOutput(struct IrNode*);

#define IrNodeGetInputSize(IRNODE)  ((IRNODE)->input_size)
#define IrNodeGetOutputSize(IRNODE) ((IRNODE)->output_size)
#define IrNodeGetInputMax(IRNODE)   ((IRNODE)->input_max)
#define IrNodeGetOutputMax(IRNODE)  ((IRNODE)->output_max)

/* Helper macro for iterating the def-use/use-def chains */
#define IrNodeInputEnd(IRNODE)    ((struct IrUse*)(&((IRNODE)->input_tail)))
#define IrNodeOutputEnd(IRNODE)   ((struct IrUse*)(&((IRNODE)->output_tail)))
#define IrNodeInputBegin(IRNODE)  (((IRNODE)->input_tail).next)
#define IrNodeOutputBegin(IRNODE) (((IRNODE)->output_tail).next)

/* General purpose IrNode creation */
struct IrNode* IrNodeNewBinary( struct IrGraph*, int op , struct IrNode* left ,
                                                          struct IrNode* right,
                                                          struct IrNode* region);

struct IrNode* IrNodeNewUnary ( struct IrGraph* , int op, struct IrNode* operand,
                                                          struct IrNode* region);

/* Function to get intrinsice number constant node. Number must be in range
 * [-5,5] */
struct IrNode* IrNodeGetConstNumber( struct IrGraph* , int32_t number );
struct IrNode* IrNodeNewConstNumber( struct IrGraph* , uint32_t index , const struct ObjProto* );
struct IrNode* IrNodeNewConstString( struct IrGraph* , uint32_t index , const struct ObjProto* );
struct IrNode* IrNodeNewConstBoolean(struct IrGraph* , int value );
#define IrNodeNewConstTrue(GRAPH) IrNodeNewConstBoolean(GRAPH,1)
#define IrNodeNewConstFalse(GRAPH) IrNodeNewConstBoolean(GRAPH,0)
struct IrNode* IrNodeNewConstNull( struct IrGraph* );
struct IrNode* IrNodeNewArgument ( struct IrGraph* , uint32_t index );

/* Primitive */
struct IrNode* IrNodeNewList( struct IrGraph* );

/* For adding input into List/Map , do not use IrNodeAddInput/Output, but use
 * following functions which takes care of the effect */
void IrNodeListAddArgument( struct IrGraph* , struct IrNode*  , struct IrNode*  , struct IrNode* );

void IrNodeListSetRegion  ( struct IrGraph* , struct IrNode*  , struct IrNode*  );

struct IrNode* IrNodeNewMap( struct IrGraph* );

void IrNodeMapAddArgument( struct IrGraph* , struct IrNode* map , struct IrNode* key ,
                                                                  struct IrNode* val ,
                                                                  struct IrNode* region );

void IrNodeMapSetRegion  ( struct IrGraph* , struct IrNode* , struct IrNode* ); 

/* A loaded closure is always no effect and not impcated by its upvalue */
struct IrNode* IrNodeNewClosure( struct IrGraph* , const struct ObjProto* , size_t upcnt );

void IrNodeClosureAddUpvalueEmbed(struct IrGraph* , struct IrNode* , struct IrNode* );

void IrNodeClosureAddUpvalueDetach(struct IrGraph* , struct IrNode*  , uint32_t index );

static SPARROW_INLINE
uint32_t IrNodeUpvalueDetachGetIndex( struct IrNode* node ) {
  SPARROW_ASSERT(node->op == IR_PRIMITIVE_UPVALUE_DETACH);
  return *((uint32_t*)(IrNodeGetData(node)));
}

static SPARROW_INLINE
const struct ObjProto* IrNodeClosureGetProto( struct IrNode* node ) {
  SPARROW_ASSERT(node->op == IR_PRIMITIVE_CLOSURE);
  return *((const struct ObjProto**)(IrNodeGetData(node)));
}

/* Function call */
struct IrNode* IrNodeNewCall( struct IrGraph* , struct IrNode* function ,
                                                struct IrNode* region );

void IrNodeCallAddArg( struct IrGraph* , struct IrNode* call ,
                                         struct IrNode* arg  ,
                                         struct IrNode* region );

struct IrNode* IrNodeNewCallIntrinsic( struct IrGraph* , enum IntrinsicFunction func ,
                                                         struct IrNode* region );

static SPARROW_INLINE
enum IntrinsicFunction IrNodeCallIntrinsicGetFunction( struct IrNode* node ) {
  SPARROW_ASSERT( node->op == IR_H_CALL_INTRINSIC );
  return *((enum IntrinsicFunction*)(IrNodeGetData(node)));
}

/* Attribute/Global/Upvalue */
struct IrNode* IrNodeNewAGet( struct IrGraph* , struct IrNode* tos ,
                                                struct IrNode* component,
                                                struct IrNode* region );

struct IrNode* IrNodeNewASet( struct IrGraph* , struct IrNode* tos ,
                                                struct IrNode* component,
                                                struct IrNode* value ,
                                                struct IrNode* region );

struct IrNode* IrNodeNewAGetIntrinsic( struct IrGraph* , struct IrNode* tos ,
                                                         enum IntrinsicAttribute attr ,
                                                         struct IrNode* region );

struct IrNode* IrNodeNewASetIntrinsic( struct IrGraph* , struct IrNode* tos,
                                                         enum IntrinsicAttribute attr ,
                                                         struct IrNode* value ,
                                                         struct IrNode* region );

static SPARROW_INLINE
enum IntrinsicAttribute IrNodeAGetIntrinsicGetIntrinsic( struct IrNode* node ) {
  SPARROW_ASSERT( node->op == IR_H_AGET_INTRINSIC );
  return *((enum IntrinsicAttribute*)(IrNodeGetData(node)));
}

struct IrNode* IrNodeNewUGet( struct IrGraph* , uint32_t index,
                                                struct IrNode* region );

struct IrNode* IrNodeNewUSet( struct IrGraph* , uint32_t index,
                                                struct IrNode* value ,
                                                struct IrNode* region );

struct IrNode* IrNodeNewGGet( struct IrGraph* , struct IrNode* name ,
                                                struct IrNode* region );

struct IrNode* IrNodeNewGSet( struct IrGraph* , struct IrNode* name ,
                                                struct IrNode* value ,
                                                struct IrNode* region );

static SPARROW_INLINE
uint32_t IrNodeUGetGetIndex( struct IrNode* node ) {
  SPARROW_ASSERT( node->op == IR_H_UGET );
  return *((uint32_t*)(IrNodeGetData(node)));
}

static SPARROW_INLINE
uint32_t IrNodeUSetGetIndex( struct IrNode* node ) {
  SPARROW_ASSERT( node->op == IR_H_USET );
  return *((uint32_t*)(IrNodeGetData(node)));
}

/* Control flow node.
 *
 * Each control flow node can have *unbounded* Input node but fixed number of
 * output node. EG: a binary branch will have to force you to add a If node
 * since it is the only node that is able have 2 branches/output */
struct IrNode* IrNodeNewRegion(struct IrGraph*);
struct IrNode* IrNodeNewMerge (struct IrGraph* , struct IrNode* if_true , struct IrNode* if_false );
struct IrNode* IrNodeNewIf( struct IrGraph* , struct IrNode* cond, struct IrNode* pred );
struct IrNode* IrNodeNewIfTrue(struct IrGraph*, struct IrNode* pred );
struct IrNode* IrNodeNewIfFalse(struct IrGraph*, struct IrNode* pred );
struct IrNode* IrNodeNewLoop(struct IrGraph* , struct IrNode* pred );
struct IrNode* IrNodeNewLoopExit(struct IrGraph* , struct IrNode* pred );

/* This function will link the Return node to end node in IrGraph automatically */
struct IrNode* IrNodeNewReturn(struct IrGraph* ,
                               struct IrNode* value ,
                               struct IrNode* pred);

/* Iterator */
struct IrNode* IrNodeNewIterTest(struct IrGraph*, struct IrNode* value, struct IrNode* region);
struct IrNode* IrNodeNewIterNew (struct IrGraph*, struct IrNode* value, struct IrNode* region);
struct IrNode* IrNodeNewIterDref(struct IrGraph* , struct IrNode* , struct IrNode* );

/* Misc */
struct IrNode* IrNodeNewPhi( struct IrGraph* , struct IrNode* left ,
                                               struct IrNode* right ,
                                               struct IrNode* region );

struct IrNode* IrNodeNewProjection( struct IrGraph* , struct IrNode* target ,
                                                      uint32_t index ,
                                                      struct IrNode* region );

/* IR graph ================================================================
 * IR graph is really just a high level name of a bundle that has lots of
 * information stored inside of it. It is just a piece of central data
 * structure to store information like , liveness analyze result , dominator
 * tree result or other temporary analyze result. Also user is able to find
 * the Start and End node inside of the IR graph as well.
 */
struct IrGraph {
  struct Sparrow* sparrow;     /* Sparrow instance */
  const struct ObjModule* mod ;/* Target module */
  const struct ObjProto* proto;/* Targeted function */
  struct ArenaAllocator* arena; /* Allocator for IR nodes */
  uint32_t node_id;            /* Current node ID */
  struct IrNode* start;        /* Start of the Node */
  struct IrNode* end  ;        /* End of the Node */
  uint32_t clean_state;        /* The current value for clean state of visiting
                                * the graph's node */
};

/* For DFS tri-color visiting usage. The mark filed in IrNode will be growing
 * monolithically. The previous visiting of Black will become next iteration of
 * visiting's whtie . Then we don't need to reset the mark field after we finish
 * visiting the graph. */
#define IrGraphWhiteMark(IRGRAPH) ((IRGRAPH)->clean_state)
#define IrGraphBlackMark(IRGRAPH) ((IRGRAPH)->clean_state+2)
#define IrGraphGrayMark(IRGRAPH) ((IRGRAPH)->clean_state+1)

static SPARROW_INLINE void IrGraphBumpCleanState( struct IrGraph* graph ) {
  /* unsigned integer's overflow is well defined */
  (graph)->clean_state += 2;
}

/* Initialize a IrGraph object with regards to a specific protocol object */
void IrGraphInit( struct IrGraph* , const struct ObjModule* module ,
                                    const struct ObjProto*  protocol,
                                    struct Sparrow* );

void IrGraphInitForInline( struct IrGraph* new_graph ,
                           struct IrGraph* parent_graph ,
                           uint32_t protocol_index );

#endif /* IR_H_ */
