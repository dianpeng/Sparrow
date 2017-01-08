#include "error.h"
#include "../util.h"


#ifndef SPARROW_DEBUG
void ReportErrorV( struct StrBuf* buf , const char* spath ,
    size_t line , size_t ccnt ,
    const char* fmt , va_list vl ) {
#else
void ReportErrorV( struct StrBuf* buf , const char* spath ,
    size_t line , size_t ccnt ,
    const char* file , int l, const char* fmt , va_list vl ) {
#endif

#ifdef SPARROW_DEBUG
  StrBufAppendF(buf,"{{DEBUG:%s|%d}}\n",file,l);
#endif /* SPARROW_DEBUG */

  StrBufAppendF(buf,"Source file:%s at line:%zu and position:%zu::",spath,line,ccnt);
  StrBufVAppendF(buf,fmt,vl);
}
