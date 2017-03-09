#ifndef IR_HELPER_H_
#define IR_HELPER_H_
#include <shared/util.h>

struct IrGraph;

/*
 * To make our life easier for debugging the IR and compiler. We made a small
 * function to dump the IrGraph in *dot format* which means user could use
 * graphviz to visualize the IR graph. In hotspot VM , they have a fancy tool
 * to visualize and debug the IR graph. We don't have it but I guess graphviz
 * is also very cool :)
 */

struct IrGraphDisplayOption {
  int only_control_flow;
  int show_extra_info;
};

static SPARROW_INLINE void IrGraphDisplayOptionDefault( struct IrGraphDisplayOption* opt ) {
  opt->only_control_flow = 0;
  opt->show_extra_info = 0;
}

int IrGraphToDotFormat( struct StrBuf* buffer , struct IrGraph* graph ,
                                                const struct IrGraphDisplayOption* option );


#endif /* IR_HELPER_H_ */
