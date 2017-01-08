#include "util.h"
#include <stdio.h>

struct CStr CStrVPrintF( const char* format , va_list vl ) {
  struct StrBuf sbuf;
  struct CStr ret;
  StrBufInit(&sbuf,128);
  (void)StrBufVPrintF(&sbuf,format,vl);
  ret = StrBufToCStr(&sbuf);
  StrBufDestroy(&sbuf);
  return ret;
}

size_t StrBufVPrintF( struct StrBuf* sbuf , const char* fmt , va_list vl ) {
  size_t bufsz = sbuf->cap - sbuf->size;
  size_t oldsz = sbuf->size;
  int ret;
  va_list vlbackup;
  va_copy(vlbackup,vl);
  if(!bufsz) {
    MemGrow((void**)&(sbuf->buf),&(sbuf->cap),1);
    bufsz = sbuf->cap - sbuf->size;
  }
  ret = vsnprintf(sbuf->buf+sbuf->size,bufsz,fmt,vl);
  if(ret >= bufsz) {
    size_t newsize = (size_t)ret + 1 + sbuf->cap;
    sbuf->buf = realloc(sbuf->buf,newsize);
    bufsz = newsize - sbuf->cap;
    ret = vsnprintf(sbuf->buf+sbuf->size,bufsz,fmt,vlbackup);
    assert( (size_t)ret <= bufsz );
    sbuf->size = oldsz + (size_t)(ret);
    sbuf->cap = newsize;
    return (size_t)(ret);
  } else if(ret <0) {
    return 0; /* Do nothing */
  } else {
    sbuf->size = oldsz + (size_t)(ret);
    return (size_t)(ret);
  }
}

struct CStr StrBufToCStr( struct StrBuf* sbuf ) {
  size_t freesz = sbuf->cap - sbuf->size;
  struct CStr ret;
  if(freesz == 0) {
    ret.str = malloc(sbuf->size+1);
    memcpy((void*)ret.str,sbuf->buf,sbuf->size);
    ((char*)(ret.str))[sbuf->size] = 0;
    ret.len = sbuf->size;
    return ret;
  } else {
    if(freesz < 32 || (freesz < 1024 && freesz < sbuf->cap/2)) {
      sbuf->buf[sbuf->size] = 0;
      ret.str = sbuf->buf;
      ret.len = sbuf->size;
      sbuf->buf = NULL;
      sbuf->cap = sbuf->size = 0;
      return ret;
    } else {
      /* no matter what, we don't do anything */
      ret.str = malloc(sbuf->size+1);
      memcpy((void*)ret.str,sbuf->buf,sbuf->size);
      ((char*)ret.str)[sbuf->size] = 0;
      ret.len = sbuf->size;
      return ret;
    }
  }
}


void MemGrow( void** buf , size_t* ocap , size_t objsz ) {
  if(ocap == NULL || *ocap == 0 || *buf == NULL) {
    *buf = malloc(MEMORY_INITIAL_OBJ_SIZE*objsz);
    *ocap = MEMORY_INITIAL_OBJ_SIZE;
  } else {
    size_t nsize = *ocap * 2;
    if(nsize > MEMORY_MAX_OBJ_SIZE) {
      nsize = MEMORY_MAX_OBJ_SIZE;
    }
    *ocap = nsize;
    *buf = realloc(*buf,nsize*objsz);
  }
}

char* ReadFile( const char* fpath , size_t* size ) {
  FILE* f;
  long sz;
  size_t rd_sz;
  char* ret = NULL;
  f = fopen(fpath,"r");
  if(!f) return NULL;
  fseek(f,0,SEEK_END);
  sz = ftell(f);
  fseek(f,0,SEEK_SET);
  ret = malloc(sz + 1);
  rd_sz = fread(ret,1,sz,f);
  assert( rd_sz <= sz );
  ret[rd_sz] = 0;
  if(size) *size = sz;
  fclose(f);
  return ret;
}
