#ifndef OPT_H_
#define OPT_H_

/* Optimization passes happened on our IrGraph */
enum OptPass {
  GLOBAL_EXPRESSION_REDUCE,
  SIZE_OF_OPT_PASS
};

enum OptResult {
  OPT_EXCEPTION = 0,
  OPT_NOTHING,
  OPT_SUCCESS
};

#include "opt-global-expr-reduce.h"

#endif /* OPT_H_ */
