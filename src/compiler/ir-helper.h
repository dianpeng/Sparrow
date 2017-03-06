#ifndef IR_HELPER_H_
#define IR_HELPER_H_
struct IrGraph;
struct StrBuf;

/*
 * To make our life easier for debugging the IR and compiler. We made a small
 * function to dump the IrGraph in *dot format* which means user could use
 * graphviz to visualize the IR graph. In hotspot VM , they have a fancy tool
 * to visualize and debug the IR graph. We don't have it but I guess graphviz
 * is also very cool :)
 */
int IrGraphToDotFormat( struct StrBuf* buffer , struct IrGraph* graph );


#endif /* IR_HELPER_H_ */
