#ifndef ERROR_H_
#define ERROR_H_
#include "../util.h"

#define PERR_OK  "no error!\n"
/* Parser error */
#define PERR_INVALID_MAP_KEY  "Invalid key for map object with type \"%s\"!\n"
#define PERR_UNEXPECTED_TOKEN  "Unexpected token here, expect \"%s\"!\n"
#define PERR_UNKNOWN_TOKEN  "Unknown token here!\n"
#define PERR_UNKNOWN_STMT  "Unknown statment!\n"
#define PERR_TOO_MANY_NUMBER_LITERALS  "Too many number literals,only allow 2^24!\n"
#define PERR_TOO_MANY_STRING_LITERALS  "Too many string literals,only allow 2^24!\n"
#define PERR_TOO_MANY_LOCAL_VARIABLES  "Too many local variables!\n"
#define PERR_NULL_INDEX  "NULL cannot be used as index!\n"
#define PERR_TOO_MANY_UNARY_OPERATORS  "Too many unary operators,only allow 128!\n"
#define PERR_NULL_UNARY  "negative sign(-) or length sign(#) cannot work with NULL!\n"
#define PERR_NULL_ARITH  "NULL operator cannot work with operator \"%s\"!\n"
#define PERR_STRING_UNARY  "string cannot work with unary operator negative(-)!\n"
#define PERR_BOOLEAN_UNARY  "boolean cannot work with unary operator length(#)!\n"
#define PERR_NUMBER_UNARY  "number cannot work with unary operator length(#)!\n"
#define PERR_DIVIDE_ZERO  "right hand side is 0,devide 0!\n"
#define PERR_STRING_ARITHMATIC  "string cannot work with arithmatic operator +!\n"
#define PERR_CONST_ARITHMATIC  "constant arithmatic operation not allowed with left hand type \"%s\" and right hand type \"%s\"!\n"
#define PERR_TOO_MANY_LOGIC_OP  "too many logic operation!\n"
#define PERR_FOR_LOOP_KEY "expect a key variable here in for loop!\n"
#define PERR_FOR_LOOP_VALUE "expect a value variable here in for loop!\n"
#define PERR_CONTINUE  "continue can only be in a for loop!\n"
#define PERR_BREAK  "break can only be in a for loop!\n"
#define PERR_TOO_MANY_BREAK  "too many breaks in a for loop!\n"
#define PERR_TOO_MANY_CONTINUE  "too many continue in a for loop!\n"
#define PERR_TOO_MANY_CLOSURE_ARGUMENT  "too many closure argument!\n"
#define PERR_TOO_MANY_BRANCH  "too many condition branch!\n"
#define PERR_CLOSURE_ARGUMENT_DUP  "argument \"%s\" in closure is duplicated, this name is shown before!\n"
#define PERR_DECL_VARIABLE  "\"var\" keyword means decalartion of a variable but no variable followed!\n"
#define PERR_DECL_DUP  "duplicate variable defintion via var!\n"
#define PERR_FUNCCALL_BRACKET  "function call here expects a \",\" or a \")\"!\n"

/* VM error */
#define PERR_TYPE_MISMATCH "%s hand side expect a %s,but got %s!"
#define PERR_MOD_OUT_OF_RANGE "%s hand side number is too large for mod(%) operator!"
#define PERR_TOS_TYPE_MISMATCH "top of stack element type is %s,but expect %s!"
#define PERR_OPERATOR_TYPE_MISMATCH "for operator \"%s\",left hand type %s and right hand type %s cannot work!"
#define PERR_MAP_KEY_NOT_STRING "map's key is not string but type %s!"
#define PERR_UDATA_NO_INDEX "user data %s doesn't have index attribute %d!"
#define PERR_TYPE_NO_ATTRIBUTE "type %s doesn't have attribute %s!"
#define PERR_ATTRIBUTE_TYPE "type %s doesn't support attribute with type %s!"
#define PERR_INDEX_OUT_OF_RANGE "index out of range!"
#define PERR_TYPE_NO_INDEX "type %s doesn't have index!"
#define PERR_TYPE_NO_SET_INDEX "type %s doesn't support set value by index!"
#define PERR_TYPE_NO_SET_ATTRIBUTE "type %s doesn't support set value by key %s!"
#define PERR_TYPE_NO_SET_OPERATOR "type %s doesn't support set operation!"
#define PERR_TOO_MANY_FUNCCALL "too many nested function call!"
#define PERR_NOT_FUNCCALL "type %s doesn't support function call!"
#define PERR_FUNCCALL_FAILED "function call failed!"
#define PERR_FUNCCALL_ARG_SIZE_MISMATCH "function %s' needs %d argument, but got %d!"
#define PERR_FUNCCALL_ARG_TYPE_MISMATCH "function %s' %d'th argument type requires %s, but got %s!"
#define PERR_TYPE_ITERATOR "type %s doesn't support iterator or failed at creating iterator!"
#define PERR_ASSERTION_ERROR "assert: Assertion failed!"
#define PERR_ARGUMENT_OUT_OF_RANGE "argument %s is out of range!"
#define PERR_SIZE_OVERFLOW "number %d is too large to use as size!"
#define PERR_CONVERSION_ERROR "type %s doesn't support conversion to %s!"
#define PERR_METAOPS_ERROR  "type %s doesn't support or not define meta operation %s!"
#define PERR_METAOPS_DEFAULT "type %s doesn't have default meta operation %s!"

#ifndef SPARROW_DEBUG
void ReportErrorV( struct StrBuf* , const char* ,
    size_t , size_t , const char* , va_list );

static SPARROW_INLINE
void ReportError( struct StrBuf* buf , const char* spath,
    size_t line , size_t ccnt ,const char* fmt , ... ) {
  va_list vl;
  va_start(vl,fmt);
  ReportErrorV(buf,spath,line,ccnt,fmt,vl);
}
# else

void ReportErrorV( struct StrBuf* , const char* ,
    size_t , size_t , const char* , int ,
    const char* , va_list );

static SPARROW_INLINE
void ReportError( struct StrBuf* buf , const char* spath,
    size_t line , size_t ccnt ,
    const char* file , int l ,
    const char* fmt , ... ) {
  va_list vl;
  va_start(vl,fmt);
  ReportErrorV(buf,spath,line,ccnt,file,l,fmt,vl);
}
#endif /* SPARROW_DEBUG */

#endif /* ERROR_H_ */
