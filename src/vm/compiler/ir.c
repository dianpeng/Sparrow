#include "ir.h"
#include <math.h>

/* Linked list related stuff */
#define link_node( GRAPH , IRNODE , ELEMENT , TYPE ) \
  do { \
    struct IrUse* use = ArenaAllocatorAlloc(&((GRAPH)->arena),sizeof(*use)); \
    struct IrUse* tail = (struct IrUse*)(&(RNODE)->TYPE##_tail); \
    use->node = (ELEMENT); \
    use->prev = tail->prev; \
    tail->prev->next = use; \
    tail->prev = use; \
    use->next = tail; \
    ++((RNODE)->TYPE##_size); \
  } while(0)

#define remove_node(IRNODE,USE,TYPE) \
  do { \
    (USE)->prev->next = (USE)->next; \
    (USE)->next->prev = (USE)->prev; \
    --((IRNODE)->TYPE##_size); \
  } while(0)

#define init_node(IRNODE,TYPE) \
  do { \
    struct IrUse* tail = (struct IrUse*)(&((IRNODE)->TYPE_##tail)); \
    tail->next = tail; \
    tail->prev = tail; \
    (IRNODE)->TYPE_##size = 0; \
  } while(0)

#define IRNODE_GET_DATA(IRNODE,TYPE) (TYPE)IrNodeGetData(IRNODE)

static void 
add_region( struct IrGraph* graph , struct IrNode* region , struct IrNode* node ) {
  assert(IrIsControl(region->op));
  IrNodeAddInput(graph,region,node);
}

static void add_def_use( struct IrGraph* graph ,
                         struct IrNode* use ,
                         struct IrNode* def ) {
  assert( node->input_max < 0 || node->input_size < node->input_max );
  /* Add the def to Use-Def chain , which is the input chain */
  link_node(graph,use,def,input);
  /* Add the use to Def-Use chain , which is the output chain */
  link_node(graph,def,use,output);
}

void IrNodeAddInput( struct IrGraph* graph , struct IrNode* node , struct IrNode* target ) {
  assert(node->input_max <0 || node->input_size < node->input_max);
  if( IrNodeFindInput(node,target) != NULL )
    link_node(graph,node,target,input);
}

void IrNodeAddOutput(struct IrGraph* graph , struct IrNode* node , struct IrNode* target ) {
  assert(node->output_max <0 || node->output_size < node->output_max);
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
#ifdef SPARROW_COMPILER_DEBUG
  assert( IrNodeFindInput(node,use->node) == use );
#endif /* SPARROW_COMPILER_DEBUG */
  remove_node(node,use,input);
}

struct IrUse* IrNodeRemoveOutput(struct IrNode* node , struct IrUse* use ) {
#ifdef SPARROW_COMPILER_DEBUG
  assert( IrNodeFindOutput(node,use->node) == use );
#endif /* SPARROW_COMPILER_DEBUG */
  remove_node(node,use,output);
}

void IrNodeClearInput( struct IrNode* node ) {
  init_node(node,input);
}

void IrNodeClearOutput(struct IrNode* node) {
  init_node(node,output);
}

struct IrNode* IrNodeNewBinary( struct IrGraph* graph , int op , struct IrNode* left ,
                                                                 struct IrNode* right,
                                                                 struct IrNode* region ) {
  struct IrNode* bin = ArenaAllocatorAlloc(&(graph->arena), sizeof(*bin));
  assert(IrIsHighIr(op));
  bin->op = op;
  bin->effect = 0;
  bin->prop_effect = IrNodeHasEffect(left) || IrNodeHasEffect(right);
  bin->mark_state = 0;
  bin->id = graph->node_id++;

  /* Binary operation, only allow 2 input */
  bin->input_max = 2;

  /* This expression can be used by other with unlimited times */
  bin->output_max = -1;

  init_node(bin,input);
  init_node(bin,output);

  add_def_use(graph,bin,left);
  add_def_use(graph,bin,right);

  if(IrNodeHasEffect(bin)) {
    add_region(graph,region,bin);
  }

  return bin;
}

struct IrNode* IrNodeNewUnary( struct IrGraph* graph , int op , struct IrNode* operand ,
                                                                struct IrNode* region ) {
  struct IrNode* bin = ArenaAllocatorAlloc(&(graph->arena),sizeof(*bin));
  assert(IrIsHighIr(op));
  bin->op = op;
  bin->effect = 0;
  bin->prop_effect = IrNodeHasEffect(operand);
  bin->mark_state = 0;
  bin->id = graph->node_id++;

  bin->input_max = 1;
  bin->output_max= -1;

  init_node(bin,input);
  init_node(bin,output);

  add_def_use(graph,bin,operand);

  if(IrNodeHasEffect(bin)) {
    add_region(graph,region,bin);
  }
  return bin;
}

struct IrNode* IrNodeGetConstNumber( struct IrGraph* graph , int32_t number ) {
  struct IrNode* bin = ArenaAllocatorAlloc(&(graph->arena),sizeof(*bin));
  bin->op = IR_CONST_INT32;
  bin->effect = 0;
  bin->prop_effect = 0;
  bin->mark_state = 0;
  bin->id = graph->node_id++;

  bin->input_max = 0;
  bin->output_max= -1;

  init_node(bin,input);
  init_node(bin,output);

  return bin;
}

int IrNumberGetType( double number ) {
}

struct IrNode* IrNodeNewConstNumber( struct IrGraph* graph , uint32_t index ,
                                                             const struct ObjProto* proto ) {
  double num = proto->num_arr[index];
  int op = IrNumberGetType(num);
  struct IrNode* bin;
  size_t size;

  switch(op) {
    case IR_CONST_INT32:
      size = sizeof(*bin) + 4;
      break;
    case IR_CONST_INT64:
      size = sizeof(*bin) + 8;
      break;
    case IR_CONST_REAL32:
      size = sizeof(*bin) + 4;
      break;
    case IR_CONST_INT64:
      size = sizeof(*bin) + 8;
      break;
    default:
      assert(0); return NULL;
  }

  bin = ArenaAllocatorAlloc(&(graph->arena),size);
  bin->op = op;
  bin->effect = 0;
  bin->prop_effect = 0;
  bin->mark_state = 0;
  bin->id = graph->node_id++;

  switch(op) {
    case IR_CONST_INT32:
      *IRNODE_GET_DATA(bin,int32_t) = (int32_t)(num);
      break;
    case IR_CONST_INT64:
      *IRNODE_GET_DATA(bin,int64_t) = (int64_t)(num);
      break;
    case IR_CONST_REAL32:
      *IRNODE_GET_DATA(bin,float) = (float)(num);
      break;
    case IR_CONST_REAL64:
      *IRNODE_GET_DATA(bin,double)= num;
      break;
    default:
      assert(0); return NULL;
  }

  bin->input_max = 0;
  bin->output_max=-1;

  init_node(bin,input);
  init_node(bin,output);

  return bin;
}

struct IrNode* IrNodeNewConstString( struct IrGraph* graph , uint32_t index ,
                                                             const struct ObjProto* proto ) {
  struct IrNode* node = ArenaAllocatorAlloc(&(graph->arena),sizeof(*node) + sizeof(void*));
  node->op = IR_CONST_STRING;
  node->effect = 0;
  node->prop_effect = 0;
  node->mark_state = 0;
  node->id = graph->node_id++;
  node->input_max = 0;
  node->output_max=-1;
  init_node(node,input);
  init_node(node,output);
  *IRNODE_GET_DATA(node,struct ObjProto**) = proto->str_arr[index];
  return bin;
}

struct IrNode* IrNodeNewConstBoolean( struct IrGraph* graph , int value ) {
  struct IrNode* node = ArenaAllocatorAlloc(&(graph->arena),sizeof(*node) + sizeof(int));
  node->op = IR_CONST_BOOLEAN;
  node->effect = 0;
  node->prop_effect = 0;
  node->mark_state = 0;
  node->id = graph->node_id++;
  node->input_max = 0;
  node->output_max=-1;
  init_node(node,input);
  init_node(node,output);
  *IRNODE_GET_DATA(node,int*) = value;
  return bin;
}

struct IrNode* IrNodeNewConstNull( struct IrGraph* graph ) {
  struct IrNode* node = ArenaAllocatorAlloc(&(graph->arena),sizeof(*node));
  node->op = IR_CONST_NULL;
  node->effect = 0;
  node->prop_effect = 0;
  node->id = graph->node_id++;
  node->input_max = 0;
  node->output_max = -1;
  init_node(node,input);
  init_node(node,output);
  return bin;
}

struct IrNode* IrNodeNewList( struct IrGraph* graph ) {
  struct IrNode* node = ArenaAllocatorAlloc(&(graph->arena),sizeof(*node));
  node->op = IR_PRIMITIVE_LIST;
  node->effect = 0;
  node->prop_effect = 0;
  node->mark_state = 0;
  node->id = graph->node_id++;
  node->input_max = -1;
  node->output_max = -1;
  init_node(node,input);
  init_node(node,output);
  return node;
}

void IrNodeListAddArgument( struct IrGraph* graph , struct IrNode* list ,
                                                    struct IrNode* argument,
                                                    struct IrNode* region ) {
  add_def_use(graph,list,argument);
  node->prop_effect = IrNodeHasEffect(argument);
}

void IrNodeListAddRegion( struct IrGraph* graph , struct IrNode* list ,
                                                  struct IrNode* argument ) {
  if(IrNodeHasEffect(list)) {
    add_region(graph,list,argument);
  }
}

struct IrNode* IrNodeNewMap( struct IrGraph* graph ) {
  struct IrNode* node = ArenaAllocatorAlloc(&(graph->arena),sizeof(*node));
  node->op = IR_PRIMITIVE_MAP;
  node->effect = 0;
  node->prop_effect = 0;
  node->mark_state = 0;
  node->id = graph->node_id++;
  node->input_max = -1;
  node->output_max= -1;
  init_node(node,input);
  init_node(node,output);
  return node;
}

void IrNodeMapAddArgument( struct IrGraph* graph , struct IrNode* map ,
                                                   struct IrNode* key ,
                                                   struct IrNode* val ) {
  struct IrNode* pair = ArenaAllocatorAlloc(&(graph->arena),sizeof(*node));
  pair->op = IR_PRIMITIVE_PAIR;
  pair->effect = 0;
  pair->prop_effect = IrNodeHasEffect(key) || IrNodeHasEffect(val);
  pair->mark_state = 0;
  pair->id = graph->node_id++;
  pair->input_max = -1;
  pair->output_max = -1;
  map->prop_effect = pair->prop_effect;

  init_node(pair,input);
  init_node(pair,output);
  add_def_use(graph,pair,key);
  add_def_use(graph,pair,val);
  add_def_use(graph,map,pair);
}

void IrNodeMapAddRegion( struct IrGraph* graph , struct IrNode* map ,
                                                 struct IrNode* region ) {
  if(IrNodeHasEffect(map)) {
    add_region(graph,region,map);
  }
}

struct IrNode* IrNodeNewClosure( struct IrGraph* graph , int index ) {
  struct IrNode* bin = ArenaAllocatorAlloc(&(graph->arena),sizeof(*bin)+sizeof(int));
  bin->op = IR_PRIMITIVE_CLOSURE;
  bin->effect = 0;
  bin->prop_effect = 0;
  bin->mark_state = 0;
  bin->id = graph->node_id++;
  bin->input_max = 0;
  bin->output_max= -1;
  init_node(bin,input);
  init_node(bin,output);
  *IRNODE_GET_DATA(bin,int*) = index;
  return bin;
}

struct IrNode* IrNodeNewCall( struct IrGraph* graph , struct IrNode* function ,
                                                      struct IrNode* region ) {
  struct IrNode* bin = ArenaAllocatorAlloc(&(graph->arena),sizeof(*bin));
  bin->op = IR_H_CALL;
  bin->effect = 1;
  bin->prop_effect = 0;
  bin->mark_state = 0;
  bin->id = graph->node_id++;
  bin->input_max = -1;
  bin->output_max= -1;
  init_node(bin,input);
  init_node(bin,output);
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
  struct IrNode* bin = ArenaAllocatorAlloc(&(graph->arena),sizeof(*bin),sizeof(enum IntrinsicFunction));
  bin->op = IR_H_CALL_INTRINSIC;
  bin->effect = 1;
  bin->prop_effect = 0;
  bin->mark_state = 0;
  bin->id = graph->node_id++;
  bin->input_max = -1;
  bin->output_max= -1;
  init_node(bin,input);
  init_node(bin,output);

  *IRNODE_GET_DATA(bin,enum IntrinsicFunction*) = func;
  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewAGet( struct IrGraph* graph , struct IrNode* tos,
                                                      struct IrNode* component ,
                                                      struct IrNode* region ) {
  struct IrNode* bin = ArenaAllocatorAlloc(&(graph->arena),sizeof(*bin));
  bin->op = IR_H_AGET;
  bin->effect = 1;
  bin->prop_effect = 0;
  bin->mark_state = 0;
  bin->id = graph->node_id++;
  bin->input_max = 2;
  bin->output_max = -1;
  init_node(bin,input);
  init_node(bin,output);

  add_def_use(graph,bin,tos);
  add_def_use(graph,bin,component);
  add_region(graph,region,bin);

  return bin;
}

struct IrNode* IrNodeNewASet( struct IrGraph* graph , struct IrNode* tos,
                                                      struct IrNode* component,
                                                      struct IrNode* value,
                                                      struct IrNode* region ) {
  struct IrNode* bin = ArenaAllocatorAlloc(&(graph->arena),sizeof(*bin));
  bin->op = IR_H_ASET;
  bin->effect = 1;
  bin->prop_effect = 0;
  bin->mark_stats = 0;
  bin->id = graph->node_id++;
  bin->input_max = 3;
  bin->output_max = 0;
  init_node(bin,input);
  init_node(bin,output);

  add_def_use(graph,bin,tos);
  add_def_use(graph,bin,component);
  add_def_use(graph,bin,value);

  add_region(graph,region,bin);

  return bin;
}

struct IrNode* IrNodeNewAGetIntrinsic( struct IrGraph* graph , struct IrNode* tos,
                                                               enum IntrinsicAttribute attr ,
                                                               struct IrNode* region ) {
  struct IrNode* bin = ArenaAllocatorAlloc(&(graph->arena),sizeof(*bin) +
                                                           sizeof(enum IntrinsicAttribute));

  bin->op = IR_H_AGET_INTRINSIC;
  bin->effect = 1;
  bin->prop_effect = 0;
  bin->mark_state = 0;
  bin->id = graph->node_id++;
  bin->input_max = 1;
  bin->output_max= -1;
  init_node(bin,input);
  init_node(bin,output);

  add_def_use(graph,bin,tos);

  *IRNODE_GET_DATA(bin,enum IntrinsicAttribute*) = attr;

  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewASetIntrinsic( struct IrGraph* graph , struct IrNode* tos ,
                                                               enum IntrinsicAttribute attr ,
                                                               struct IrNode* value ,
                                                               struct IrNode* region ) {
  struct IrNode* bin = ArenaAllocatorAlloc(&(graph->arena),sizeof(*bin) +
                                                           sizeof(enum IntrinsicAttribute));
  bin->op = IR_H_ASET_INTRINSIC;
  bin->effect = 1;
  bin->prop_effect = 0;
  bin->mark_state = 0;
  bin->id = graph->node_id++;
  bin->input_max = 2;
  bin->output_max=-1;

  init_node(bin,input);
  init_node(bin,output);

  add_def_use(graph,bin,tos);
  add_def_use(graph,bin,value);

  *IRNODE_GET_DATA(bin,enum IntrinsicAttribute*) = attr;

  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewUGet( struct IrGraph* graph , uint32_t index ,
                                                      struct IrNode* region ) {
}
