#ifndef BUILTIN_H_
#define BUILTIN_H_
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "error.h"
#include "object.h"
#include "vm.h"
#include "../util.h"

#define BUILTIN_CHECK_ARGUMENT(...) \
  do { \
    if(RuntimeCheckArg(__VA_ARGS__)) { \
      *fail = 1; \
      return; \
    } else *fail = 0; \
  } while(0);

static SPARROW_INLINE
void Builtin_TypeOf( struct Runtime* rt , Value* ret , int* fail ) {
  BUILTIN_CHECK_ARGUMENT(rt,"typeof",1,ARG_ANY) {
    Value obj = RuntimeGetArg(rt,0);
    const char* type_name = ValueGetTypeString(obj);
    struct ObjStr* type = ObjNewStr(RTSparrow(rt) ,
        type_name,strlen(type_name));
    Vset_str(ret,type);
  }
}

static SPARROW_INLINE
void Builtin_IsBoolean( struct Runtime* rt , Value* ret , int* fail ) {
  BUILTIN_CHECK_ARGUMENT(rt,"is_boolean",1,ARG_ANY) {
    Value obj = RuntimeGetArg(rt,0);
    UNUSE_ARG(rt);
    Vset_boolean(ret,Vis_boolean(&obj));
  }
}

static SPARROW_INLINE
void Builtin_IsString( struct Runtime* rt , Value* ret , int* fail ) {
  BUILTIN_CHECK_ARGUMENT(rt,"is_string",1,ARG_ANY) {
    Value obj = RuntimeGetArg(rt,0);
    UNUSE_ARG(rt);
    Vset_boolean(ret,Vis_str(&obj));
  }
}

static SPARROW_INLINE
void Builtin_IsNumber( struct Runtime* rt , Value* ret , int* fail ) {
  BUILTIN_CHECK_ARGUMENT(rt,"is_number",1,ARG_ANY) {
    Value obj = RuntimeGetArg(rt,0);
    UNUSE_ARG(rt);
    Vset_boolean(ret,Vis_number(&obj));
  }
}

static SPARROW_INLINE
void Builtin_IsNull( struct Runtime* rt , Value* ret , int* fail ) {
  BUILTIN_CHECK_ARGUMENT(rt,"is_null",1,ARG_ANY) {
    Value obj = RuntimeGetArg(rt,0);
    UNUSE_ARG(rt);
    Vset_boolean(ret,Vis_null(&obj));
  }
}

static SPARROW_INLINE
void Builtin_IsList( struct Runtime* rt , Value* ret , int* fail ) {
  BUILTIN_CHECK_ARGUMENT(rt,"is_list",1,ARG_ANY) {
    Value obj = RuntimeGetArg(rt,0);
    UNUSE_ARG(rt);
    Vset_boolean(ret,Vis_list(&obj));
  }
}

static SPARROW_INLINE
void Builtin_IsMap( struct Runtime* rt , Value* ret , int* fail ) {
  BUILTIN_CHECK_ARGUMENT(rt,"is_map",1,ARG_ANY) {
    Value obj = RuntimeGetArg(rt,0);
    UNUSE_ARG(rt);
    Vset_boolean(ret,Vis_map(&obj));
  }
}

static SPARROW_INLINE
void Builtin_IsClosure( struct Runtime* rt , Value* ret , int* fail ) {
  BUILTIN_CHECK_ARGUMENT(rt,"is_closure",1,ARG_ANY) {
    Value obj = RuntimeGetArg(rt,0);
    UNUSE_ARG(rt);
    Vset_boolean(ret,Vis_closure(&obj));
  }
}

static SPARROW_INLINE
void Builtin_ToString( struct Runtime* rt , Value* ret , int* fail ) {
  BUILTIN_CHECK_ARGUMENT(rt,"to_string",1,ARG_ANY) {
    struct ObjStr* str = ValueToString(rt,RuntimeGetArg(rt,0),fail);
    if(!*fail) {
      Vset_str(ret,str);
    }
  }
}

static SPARROW_INLINE
void Builtin_ToBoolean( struct Runtime* rt, Value* ret , int* fail ) {
  BUILTIN_CHECK_ARGUMENT(rt,"to_boolean",1,ARG_ANY) {
    Value obj = RuntimeGetArg(rt,0);
    Vset_boolean(ret,ValueToBoolean(rt,obj));
  }
}

static SPARROW_INLINE
void Builtin_ToNumber( struct Runtime* rt , Value* ret , int* fail ) {
  BUILTIN_CHECK_ARGUMENT(rt,"to_number",1,ARG_ANY) {
    double num = ValueToNumber(rt,RuntimeGetArg(rt,0),fail);
    if(!*fail) {
      Vset_number(ret,num);
    }
  }
}

void Builtin_Print( struct Runtime* , Value* , int* );
void Builtin_Error( struct Runtime* , Value* , int* );

static SPARROW_INLINE
void Builtin_Assert( struct Runtime* rt , Value* ret , int* fail ) {
  Vset_null(ret);
  BUILTIN_CHECK_ARGUMENT(rt,"assert",2,ARG_CONV_BOOLEAN,ARG_ANY) {
    Value first = RuntimeGetArg(rt,0);
    if(!ValueToBoolean(rt,first)) {
      struct StrBuf sbuf;
      struct CStr str;
      StrBufInit(&sbuf,128);
      *fail = 1;
      Value second = RuntimeGetArg(rt,1);
      (void) ValuePrint(RTSparrow(rt),&sbuf,second);
      str = StrBufToCStr(&sbuf);
      RuntimeError(rt,"Assertion failed: %s",str.str);
      StrBufDestroy(&sbuf);
      CStrDestroy(&str);
      return;
    }
    *fail = 0;
  }
}

void Builtin_Import(struct Runtime* , Value* , int* );

static SPARROW_INLINE
void Builtin_Size( struct Runtime* rt , Value* ret , int* fail ) {
  BUILTIN_CHECK_ARGUMENT(rt,"size",1,ARG_ANY) {
    size_t len;
    Value obj = RuntimeGetArg(rt,0);
    len = ValueSize(rt,obj,fail);
    if(!*fail) {
      Vset_number(ret,len);
    }
  }
}

void Builtin_Range( struct Runtime* rt , Value* ret , int* fail );

static SPARROW_INLINE
void Builtin_Loop ( struct Runtime* rt , Value* ret , int* fail ) {
  BUILTIN_CHECK_ARGUMENT(rt,"loop",3,ARG_CONV_NUMBER,
                                     ARG_CONV_NUMBER,
                                     ARG_CONV_NUMBER) {
    Value start = RuntimeGetArg(rt,0);
    Value end = RuntimeGetArg(rt,1);
    Value step= RuntimeGetArg(rt,2);
    int istart,iend,istep;

    if(ConvNum(Vget_number(&start),&istart)) {
      RuntimeError(rt,PERR_ARGUMENT_OUT_OF_RANGE,"start");
      goto fail;
    }
    if(ConvNum(Vget_number(&end),&iend)) {
      RuntimeError(rt,PERR_ARGUMENT_OUT_OF_RANGE,"end");
      goto fail;
    }
    if(ConvNum(Vget_number(&step),&istep)) {
      RuntimeError(rt,PERR_ARGUMENT_OUT_OF_RANGE,"step");
      goto fail;
    }
    Vset_loop(ret,ObjNewLoop( RTSparrow(rt) , istart,iend,istep));
    *fail = 0;
  }
  return;

fail:
  *fail = 1;
}

void Builtin_RunString( struct Runtime* rt , Value* ret , int* fail );
void Builtin_GCForce( struct Runtime* rt , Value* ret , int* fail );
void Builtin_GCTry( struct Runtime* rt , Value* ret, int* fail );
void Builtin_GCStat(struct Runtime* rt , Value* ret, int* fail );
void Builtin_GCConfig(struct Runtime* rt , Value* ret , int* fail );

void Builtin_Min( struct Runtime* rt , Value* ret , int* fail );
void Builtin_Max( struct Runtime* rt , Value* ret , int* fail );
void Builtin_Sort(struct Runtime* rt , Value* ret , int* fail );
void Builtin_Set( struct Runtime* rt , Value* ret , int* fail );
void Builtin_Get( struct Runtime* rt , Value* ret , int* fail );
void Builtin_Exist(struct Runtime* rt, Value* ret , int* fail );
void Builtin_MSec( struct Runtime* rt , Value* ret , int* fail );




/* Builtin global objects creation routine */
struct ObjUdata* GCreateListUdata( struct Sparrow* );

/*
struct ObjUdata* GCreateMapUdata( struct Sparrow* );
struct ObjUdata* GCreateStringUdata( struct Sparrow* );
struct ObjUdata* GCreateGCUdata( struct Sparrow* );
struct ObjUdata* GCreateMetaUdata( struct Sparrow* );
*/

#endif /* BUILTIN_H_ */
