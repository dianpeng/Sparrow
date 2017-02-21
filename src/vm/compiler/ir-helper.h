#ifndef IR_HELPER_H_
#define IR_HELPER_H_
#include <stdio.h>

struct IrGraph;

/* Bunch of helper functions to do several interested things around the IR */
int IrDump( const char* type , FILE* output , const struct IrGraph* graph );

#endif /* IR_HELPER_H_ */
