#include "bc-ir-builder.h"

struct BytecodeIrBuilder {
  struct IrGraph* graph;     /* Targeted graph */
  struct IrNode** num_table; /* Constant Number table, in our bytecode, we
                              * have specialized IR to load small integer,
                              * those integer will not be here, but instead
                              * they will be directly read it from the field */

  /* Special number table, hold number ranging at [-5,+5] */
  struct IrNode* spnum_table[BC_SPECIAL_NUMBER_SIZE];

  /* Other primitive node */
  struct IrNode* true_node;
  struct IrNode* false_node;
  struct IrNode* null_node;

  /* String IR node */
  struct IrNode** str_table; /* Constant string table */
};
