#include <compiler/ir-helper.h>
#include <compiler/ir-set.h>
#include <compiler/ir.h>

#include <shared/debug.h>
#include <shared/util.h>

/* -----------------------------------------------------------------------
 * I am not an expert of dot format, so I will just use the basic feature
 * of it. The graph visiting is done by using a stack to perform a DFS
 * style visiting. Since dot format allow us to point to graph node later on
 * defined, it is mostly trivial to walk the ir node.
 * A node's lable is composed of the node opcode name plus its pointer address
 * which makes it totally unique.
 * Backedge is marked with different color, apart from this ,nothing special
 * -----------------------------------------------------------------------*/

struct DotFormatBuilder {
  struct StrBuf* output;
};

/* helper function to serialize certain feature of the node */
static SPARROW_INLINE void print_node( struct DotFormatBuilder* builder ,
                                       const struct IrGraph* graph ,
                                       const struct IrNode* node ) {
  const char* opcode_name = IrGetName(node->op);
  SPARROW_ASSERT( IrGraphGrayMark(graph) == node->mark );
  StrBufPrintF(builder->output,"%s_%d [shape=box,lable=\"%s:%p:%d:%d:%d\"];\n",
      opcode_name,
      node->id,
      opcode_name,
      node,
      node->effect,
      node->prop_effect,
      node->bounded);
}


static SPARROW_INLINE void print_node_link( struct DotFormatBuilder* builder ,
    const struct IrNode* from , const struct IrNode* to , const char* style ) {
  StrBufPrintF(builder->output,"%s_%d -> %s_%d %s;\n",
      IrGetName(from->op),from->id,
      IrGetName(to->op),to->id,
      style);
}

static SPARROW_INLINE void print_node_input( struct DotFormatBuilder* builder ,
                                             const struct IrGraph* graph ,
                                             const struct IrNode* node ) {
  const char* style;
  (void)graph;
  if(IrIsControl(node->op)) {
    style = "[style=dotted]";
  } else {
    style = "";
  }

  {
    struct IrUse* start = IrNodeInputBegin(node);
    struct IrUse* end = IrNodeInputEnd(node);
    while( start != end ) {
      print_node_link(builder,node,start->node,style);
      start = start->next;
    }
  }
}

static SPARROW_INLINE void print_node_output( struct DotFormatBuilder* builder ,
                                              const struct IrGraph* graph ,
                                              const struct IrNode* node ) {
  (void)graph;
  if(IrIsControl(node->op)) {
    struct IrUse* start = IrNodeOutputBegin(node);
    struct IrUse* end  = IrNodeOutputEnd(node);
    while( start != end ) {
      print_node_link(builder,node,start->node,"[style=bold]");
      start = start->next;
    }
  }
}

static SPARROW_INLINE void expand_node( struct IrNodeStack* stack ,
                                        struct IrUse* begin ,
                                        struct IrUse* end   ,
                                        const struct IrGraph* graph ) {
  while(begin != end) {
    if(begin->node->mark == IrGraphWhiteMark(graph)) {
      IrNodeStackPush(stack,begin->node);
      begin->node->mark = IrGraphGrayMark(graph);
    }
    begin = begin->next;
  }
}

#define AddNode(NODE) \
  do { \
    IrNodeStackPush(&work_set,(NODE)); \
    ((NODE)->mark) = IrGraphGrayMark(graph); \
  } while(0)

static SPARROW_INLINE void print_graph ( struct DotFormatBuilder* builder ,
                                         const struct IrGraph* graph ) {
  struct IrNodeStack work_set;
  IrNodeStackInit(&work_set,128);
  AddNode(graph->start);

  StrBufPrintF(builder->output,"digraph IrGraph {\n");

  while( !IrNodeStackIsEmpty(&work_set) ) {
    struct IrNode* top = IrNodeStackPop(&work_set);

    /* print the node's all information */
    print_node(builder,graph,top);
    print_node_input(builder,graph,top);
    print_node_output(builder,graph,top);

    /* mark it to black */
    top->mark = IrGraphBlackMark(graph);

    /* expands its sibling nodes */
    expand_node(&work_set,IrNodeInputBegin(top),IrNodeInputEnd(top),graph);
    if(IrIsControl(top->op)) {
      expand_node(&work_set,IrNodeOutputBegin(top),IrNodeOutputEnd(top),graph);
    }
  }

  StrBufPrintF(builder->output,"}\n");
}

#undef AddNode /* AddNode */

int IrGraphToDotFormat( struct StrBuf* buffer , struct IrGraph* graph ) {
  struct DotFormatBuilder builder;
  builder.output = buffer;
  print_graph(&builder,graph);
  IrGraphBumpCleanState(graph);
  return 0;
}
