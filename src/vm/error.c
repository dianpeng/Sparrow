#include <vm/error.h>

void ReportErrorV( struct StrBuf* buf , const char* spath ,
    size_t line , size_t ccnt ,
    const char* fmt , va_list vl ) {
  StrBufAppendF(buf,"Source file:%s at line:%zu and position:%zu::",spath,line,ccnt);
  StrBufVAppendF(buf,fmt,vl);
}
