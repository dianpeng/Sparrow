#include "ir.h"
#include <math.h>

/* Linked list related stuff */
#define link_node( GRAPH , IRNODE , ELEMENT , TYPE ) \
  do { \
    struct IrUse* use = ArenaAllocatorAlloc(&((GRAPH)->arena),sizeof(*use)); \
    struct IrUse* tail = (struct IrUse*)(&(IRNODE)->TYPE##_tail); \
    use->node = (ELEMENT); \
    use->prev = tail->prev; \
    tail->prev->next = use; \
    tail->prev = use; \
    use->next = tail; \
    ++((IRNODE)->TYPE##_size); \
  } while(0)

#define remove_node(IRNODE,USE,TYPE) \
  do { \
    (USE)->prev->next = (USE)->next; \
    (USE)->next->prev = (USE)->prev; \
    --((IRNODE)->TYPE##_size); \
  } while(0)

#define init_node(IRNODE,TYPE) \
  do { \
    struct IrUse* tail = (struct IrUse*)(&((IRNODE)->TYPE##_tail)); \
    tail->next = tail; \
    tail->prev = tail; \
    (IRNODE)->TYPE##_size = 0; \
  } while(0)

#define IRNODE_GET_DATA(IRNODE,TYPE) (TYPE)IrNodeGetData(IRNODE)

static void
add_region( struct IrGraph* graph , struct IrNode* region , struct IrNode* node ) {
  SPARROW_ASSERT(IrIsControl(region->op));
  IrNodeAddInput(graph,region,node);
}

static void add_def_use( struct IrGraph* graph ,
                         struct IrNode* use ,
                         struct IrNode* def ) {
  SPARROW_ASSERT( node->input_max < 0 || node->input_size < node->input_max );
  /* Add the def to Use-Def chain , which is the input chain */
  link_node(graph,use,def,input);
  /* Add the use to Def-Use chain , which is the output chain */
  link_node(graph,def,use,output);
}

void IrNodeAddInput( struct IrGraph* graph , struct IrNode* node , struct IrNode* target ) {
  SPARROW_ASSERT(node->input_max <0 || node->input_size < node->input_max);
  if( IrNodeFindInput(node,target) != NULL )
    link_node(graph,node,target,input);
}

void IrNodeAddOutput(struct IrGraph* graph , struct IrNode* node , struct IrNode* target ) {
  SPARROW_ASSERT(node->output_max <0 || node->output_size < node->output_max);
  if( IrNodeFindOutput(node,target) != NULL )
    link_node(graph,node,target,output);
}

struct IrUse* IrNodeFindInput(struct IrNode* node , struct IrNode* target) {
  struct IrUse* begin = IrNodeInputBegin(node);
  for( ; begin  != IrNodeInputEnd(node) ; begin = begin->next ) {
    if(target == begin->node) return begin;
  }
  return NULL;
}

struct IrUse* IrNodeFindOutput(struct IrNode* node , struct IrNode* target) {
  struct IrUse* begin = IrNodeOutputBegin(node);
  for( ; begin != IrNodeOutputEnd(node) ; begin = begin->next ) {
    if(target == begin->node) return begin;
  }
  return NULL;
}

struct IrUse* IrNodeRemoveInput(struct IrNode* node , struct IrUse* use) {
  SPARROW_ASSERT( IrNodeFindInput(node,use->node) == use );
  remove_node(node,use,input);
}

struct IrUse* IrNodeRemoveOutput(struct IrNode* node , struct IrUse* use ) {
  SPARROW_ASSERT( IrNodeFindOutput(node,use->node) == use  );
  remove_node(node,use,output);
}

void IrNodeClearInput( struct IrNode* node ) {
  init_node(node,input);
}

void IrNodeClearOutput(struct IrNode* node) {
  init_node(node,output);
}

static struct IrNode* new_node( struct IrGraph* graph , int op ,
                                                 int effect,
                                                 int prop_effect,
                                                 int input_max ,
                                                 int output_max ,
                                                 size_t extra ) {
  struct IrNode* bin = ArenaAllocatorAlloc(&(graph->arena), sizeof(*bin) + extra);
  bin->op = op;
  bin->effect = effect;
  bin->prop_effect = prop_effect;
  bin->mark_state = 0;
  bin->id = graph->node_id++;
  bin->input_max = input_max;
  bin->output_max= output_max;
  init_node(bin,input);
  init_node(bin,output);
  return bin;
}

struct IrNode* IrNodeNewBinary( struct IrGraph* graph , int op , struct IrNode* left ,
                                                                 struct IrNode* right,
                                                                 struct IrNode* region ) {
  struct IrNode* bin;
  SPARROW_ASSERT(IrIsHighIr(op));
  bin = new_node(graph,op,
                       0,
                       IrNodeHasEffect(left) || IrNodeHasEffect(right),
                       2,
                       -1,
                       0);

  add_def_use(graph,bin,left);
  add_def_use(graph,bin,right);

  if(IrNodeHasEffect(bin)) {
    add_region(graph,region,bin);
  }

  return bin;
}

struct IrNode* IrNodeNewUnary( struct IrGraph* graph , int op , struct IrNode* operand ,
                                                                struct IrNode* region ) {
  struct IrNode* bin;
  SPARROW_ASSERT(IrIsHighIr(op));
  bin = new_node(graph,op,
                       0,
                       IrNodeHasEffect(operand),
                       1,
                       -1,
                       0);
  add_def_use(graph,bin,operand);
  if(IrNodeHasEffect(bin)) {
    add_region(graph,region,bin);
  }
  return bin;
}

static
struct IrNode* new_number_node( struct IrGraph* graph , double num ) {
  int op = IrNumberGetType(num);
  struct IrNode* bin;
  size_t size;

  switch(op) {
    case IR_CONST_INT32:
      size = 4;
      break;
    case IR_CONST_INT64:
      size = 8;
      break;
    case IR_CONST_REAL32:
      size = 4;
      break;
    case IR_CONST_REAL64:
      size = 8;
      break;
    default:
      SPARROW_UNREACHABLE(); return NULL;
  }

  bin = new_node(graph,op,
                        0,
                        0,
                        0,
                        -1,
                        size);

  switch(op) {
    case IR_CONST_INT32:
      *IRNODE_GET_DATA(bin,int32_t*) = (int32_t)(num);
      break;
    case IR_CONST_INT64:
      *IRNODE_GET_DATA(bin,int64_t*) = (int64_t)(num);
      break;
    case IR_CONST_REAL32:
      *IRNODE_GET_DATA(bin,float*) = (float)(num);
      break;
    case IR_CONST_REAL64:
      *IRNODE_GET_DATA(bin,double*)= num;
      break;
    default:
      SPARROW_UNREACHABLE(); return NULL;
  }

  return bin;
}

int IrNumberGetType( double number ) {
}

struct IrNode* IrNodeGetConstNumber( struct IrGraph* graph , int32_t number ) {
  struct IrNode* bin = new_node(graph,IR_CONST_INT32,0,0,0,-1,sizeof(int32_t));
  *IRNODE_GET_DATA(bin,int32_t*) = number;
  return bin;
}

struct IrNode* IrNodeNewConstNumber( struct IrGraph* graph , uint32_t index ,
                                                             const struct ObjProto* proto ) {
  double num = proto->num_arr[index];
  return new_number_node(graph,num);
}

struct IrNode* IrNodeNewConstString( struct IrGraph* graph , uint32_t index ,
                                                             const struct ObjProto* proto ) {
  struct IrNode* node = new_node(graph,IR_CONST_STRING,0,0,0,-1,sizeof(void*));
  *IRNODE_GET_DATA(node,struct ObjStr**) = proto->str_arr[index];
  return node;
}

struct IrNode* IrNodeNewConstBoolean( struct IrGraph* graph , int value ) {
  struct IrNode* node = new_node(graph,IR_CONST_BOOLEAN,0,0,0,-1,sizeof(int));
  *IRNODE_GET_DATA(node,int*) = value;
  return node;
}

struct IrNode* IrNodeNewConstNull( struct IrGraph* graph ) {
  return new_node(graph,IR_CONST_NULL,0,0,0,-1,0);
}

struct IrNode* IrNodeNewList( struct IrGraph* graph ) {
  return new_node( graph , IR_PRIMITIVE_LIST , 0 , 0 , -1, -1 , 0 );
}

void IrNodeListAddArgument( struct IrGraph* graph , struct IrNode* list ,
                                                    struct IrNode* argument ) {
  add_def_use(graph,list,argument);
  list->prop_effect = IrNodeHasEffect(argument);
}

void IrNodeListSetRegion( struct IrGraph* graph , struct IrNode* list ,
                                                  struct IrNode* argument ) {
  if(IrNodeHasEffect(list)) {
    add_region(graph,list,argument);
  }
}

struct IrNode* IrNodeNewMap( struct IrGraph* graph ) {
  return new_node(graph,IR_PRIMITIVE_MAP,0,0,-1,-1,0);
}

void IrNodeMapAddArgument( struct IrGraph* graph , struct IrNode* map ,
                                                   struct IrNode* key ,
                                                   struct IrNode* val ) {
  struct IrNode* pair;
  pair = new_node( graph , IR_PRIMITIVE_PAIR , 0 ,
                                               IrNodeHasEffect(key) || IrNodeHasEffect(val),
                                               -1,
                                               -1,
                                               0);
  map->prop_effect = pair->prop_effect;
  add_def_use(graph,pair,key);
  add_def_use(graph,pair,val);
  add_def_use(graph,map,pair);
}

void IrNodeMapSetRegion( struct IrGraph* graph , struct IrNode* map ,
                                                 struct IrNode* region ) {
  if(IrNodeHasEffect(map)) {
    add_region(graph,region,map);
  }
}

struct IrNode* IrNodeNewClosure( struct IrGraph* graph , int index ) {
  struct IrNode* bin;
  bin = new_node( graph , IR_PRIMITIVE_CLOSURE , 0 , 0 , 0 , -1 , sizeof(int));
  *IRNODE_GET_DATA(bin,int*) = index;
  return bin;
}

struct IrNode* IrNodeNewCall( struct IrGraph* graph , struct IrNode* function ,
                                                      struct IrNode* region ) {
  struct IrNode* bin = new_node( graph , IR_H_CALL , 1 , 0 , -1 , -1 , 0 );
  add_def_use(graph,bin,function);
  add_region(graph,region,bin);
  return bin;
}

void IrNodeCallAddArg( struct IrGraph* graph , struct IrNode* call ,
                                               struct IrNode* arg ) {
  IrNodeAddInput(graph,call,arg);
}

struct IrNode* IrNodeNewCallIntrinsic( struct IrGraph* graph , enum IntrinsicFunction func ,
                                                               struct IrNode* region ) {
  struct IrNode* bin =new_node( graph , IR_H_CALL_INTRINSIC , 1 ,
                                                              0 ,
                                                              -1 ,
                                                              -1 ,
                                                              sizeof(enum IntrinsicFunction));
  *IRNODE_GET_DATA(bin,enum IntrinsicFunction*) = func;
  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewAGet( struct IrGraph* graph , struct IrNode* tos,
                                                      struct IrNode* component ,
                                                      struct IrNode* region ) {
  struct IrNode* bin = new_node(graph,IR_H_AGET,1,0,2,-1,0);
  add_def_use(graph,bin,tos);
  add_def_use(graph,bin,component);
  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewASet( struct IrGraph* graph , struct IrNode* tos,
                                                      struct IrNode* component,
                                                      struct IrNode* value,
                                                      struct IrNode* region ) {
  struct IrNode* bin = new_node(graph,IR_H_ASET,1,0,3,0,0);
  add_def_use(graph,bin,tos);
  add_def_use(graph,bin,component);
  add_def_use(graph,bin,value);
  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewAGetIntrinsic( struct IrGraph* graph , struct IrNode* tos,
                                                               enum IntrinsicAttribute attr ,
                                                               struct IrNode* region ) {
  struct IrNode* bin = new_node(graph,IR_H_AGET_INTRINSIC,1,0,1,-1,
                                                                sizeof(enum IntrinsicAttribute));
  add_def_use(graph,bin,tos);
  *IRNODE_GET_DATA(bin,enum IntrinsicAttribute*) = attr;
  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewASetIntrinsic( struct IrGraph* graph , struct IrNode* tos ,
                                                               enum IntrinsicAttribute attr ,
                                                               struct IrNode* value ,
                                                               struct IrNode* region ) {
  struct IrNode* bin = new_node(graph, IR_H_ASET_INTRINSIC,1,0,2,-1,
                                                                 sizeof(enum IntrinsicAttribute));
  add_def_use(graph,bin,tos);
  add_def_use(graph,bin,value);
  *IRNODE_GET_DATA(bin,enum IntrinsicAttribute*) = attr;
  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewUGet( struct IrGraph* graph , uint32_t index ,
                                                      struct IrNode* region ) {
  struct IrNode* bin = new_node(graph, IR_H_UGET , 0 , 0 , 0 , -1 , sizeof(uint32_t));
  *IRNODE_GET_DATA(bin,uint32_t*) = index;
  (void)region;
  return bin;
}

struct IrNode* IrNodeNewUSet( struct IrGraph* graph , uint32_t index ,
                                                      struct IrNode* value ,
                                                      struct IrNode* region ) {
  struct IrNode* bin = new_node(graph , IR_H_USET , 1 , 0 , 1 , 0 , sizeof(uint32_t));
  *IRNODE_GET_DATA(bin,uint32_t*) = index;
  add_def_use(graph,bin,value);
  add_region(graph,region,bin);
  return bin;
}

/* Control flow nodes ========================================== */
struct IrNode* IrNodeNewRegion( struct IrGraph* graph ) {
  return new_node(graph , IR_CTL_REGION , 0 , 0 , -1 , 1 , 0 );
}

struct IrNode* IrNodeNewMerge( struct IrGraph* graph , struct IrNode* if_true ,
                                                       struct IrNode* if_false ) {
  struct IrNode* bin = new_node( graph , IR_CTL_MERGE , 0 , 0 , -1 , 1 , 0 );
  SPARROW_ASSERT(if_true->op == IR_CTL_IF_TRUE);
  SPARROW_ASSERT(if_false->op == IR_CTL_IF_FALSE);
  IrNodeAddOutput(graph,if_true,bin);
  IrNodeAddOutput(graph,if_false,bin);
  return bin;
}

struct IrNode* IrNodeNewIf( struct IrGraph* graph , struct IrNode* predicate ,
                                                    struct IrNode* parent ) {
  struct IrNode* node = new_node( graph , IR_CTL_IF , 0 , 0 , 1 , 2 , 0 );
  /* link the control flow chain */
  IrNodeAddOutput(graph,parent,node);
  /* link the expression that is bounded in this node */
  IrNodeAddInput (graph,node,predicate);
  return node;
}

struct IrNode* IrNodeNewIfTrue( struct IrGraph* graph , struct IrNode* parent ) {

  struct IrNode* node = new_node( graph , IR_CTL_IF_TRUE , 0 , 0 , -1 , 1 , 0 );
  IrNodeAddOutput(graph,parent,node);
  return node;
}

struct IrNode* IrNodeNewIfFalse(struct IrGraph* graph , struct IrNode* parent ) {
  struct IrNode* node = new_node( graph , IR_CTL_IF_FALSE , 0 , 0 , -1 , 1 , 0 );
  IrNodeAddOutput(graph,parent,node);
  return node;
}

struct IrNode* IrNodeNewLoop( struct IrGraph* graph , struct IrNode* parent ) {
  struct IrNode* node = new_node( graph , IR_CTL_LOOP , 0 , 0 , -1 , 1 , 0 );
  IrNodeAddOutput(graph,parent,node);
  return node;
}

struct IrNode* IrNodeNewLoopExit( struct IrGraph* graph , struct IrNode* parent ) {
  struct IrNode* node = new_node( graph , IR_CTL_LOOP_EXIT , 0 , 0 , -1 , 2 , 0 );
  IrNodeAddOutput(graph,parent,node);
  return node;
}

struct IrNode* IrNodeNewReturn( struct IrGraph* graph, struct IrNode* value ,
                                                       struct IrNode* parent ) {
  struct IrNode* node = new_node( graph , IR_CTL_RET , 0 , 0 , 1 , 1 , 0 );
  IrNodeAddOutput(graph,parent,node);
  IrNodeAddInput (graph,node,value);
  return node;
}

/* Iterator ============================== */
struct IrNode* IrNodeNewIterTest( struct IrGraph* graph , struct IrNode* value ,
                                                          struct IrNode* region) {
  struct IrNode* node = new_node( graph , IR_H_ITER_TEST , 1 ,
                                                           IrNodeHasEffect(value),
                                                           1 ,
                                                           -1 ,
                                                           0 );
  add_def_use(graph,node,value);
  add_region(graph,region,node);
  return node;
}

struct IrNode* IrNodeNewIterNew ( struct IrGraph* graph , struct IrNode* value ,
                                                          struct IrNode* region) {
  struct IrNode* node = new_node( graph , IR_H_ITER_NEW , 1 ,
                                                          IrNodeHasEffect(value),
                                                          1 ,
                                                          -1 ,
                                                          0 );
  add_def_use(graph,node,value);
  add_region(graph,region,node);
  return node;
}

struct IrNode* IrNodeNewIterDrefKey( struct IrGraph* graph , struct IrNode* iter ,
                                                             struct IrNode* region ) {
  struct IrNode* node = new_node( graph , IR_H_ITER_DREF, 1 ,
                                                          IrNodeHasEffect(iter),
                                                          1,
                                                          -1,
                                                          0);
  add_def_use(graph,node,iter);
  add_region(graph,region,node);
  return node;
}

/* Misc ================================== */
struct IrNode* IrNodeNewPhi( struct IrGraph* graph , struct IrNode* left ,
                                                     struct IrNode* right ) {
  struct IrNode* node = new_node( graph , IR_PHI , 0 , 0 , 2 , -1 , 0 );
  /* order matters !!! */
  add_def_use(graph,node,left);
  add_def_use(graph,node,right);
  return node;
}

struct IrNode* IrNodeNewProjection( struct IrGraph* graph , struct IrNode* target,
                                                            uint32_t index ,
                                                            struct IrNode* region ) {
  struct IrNode* node = new_node( graph , IR_PROJECTION , 1 , 0 , 1 , -1 , sizeof(uint32_t));
  add_def_use(graph,node,target);
  *IRNODE_GET_DATA(node,uint32_t*) = index;
  add_region(graph,region,node);
  return node;
}
