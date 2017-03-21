#ifndef TYPE_INFERENCE_H_
#define TYPE_INFERENCE_H_

struct IrGraph;

enum IrTypeCategory {
  IR_TYPE_INT32,
  IR_TYPE_INT64,
  IR_TYPE_REAL ,
  IR_TYPE_STRING,
  IR_TYPE_BOOLEAN,
  IR_TYPE_NULL,
  IR_TYPE_MAP,
  IR_TYPE_LIST,
  IR_TYPE_CLOSURE,
  IR_TYPE_ANY,

  SIZE_OF_IR_TYPE
};

/* Wrape it as a structure to ease the pain for future extension. We may be
 * able to add more complicated IrType information inside of this structure */
struct IrTypeInfo {
  enum IrTypeCategory kind;
};

struct IrTypeMap {
};

/* Call this function to add TypeInfer information inside of the IrGraph */
int IrGraphAnaylze_DoTypeInfer( struct IrGraph* , int debug );

#endif /* TYPE_INFERENCE_H_ */
