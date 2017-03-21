#ifndef UTIL_H_
#define UTIL_H_
#include <sparrow.h>
#include <shared/debug.h>

#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#define MEMORY_INITIAL_OBJ_SIZE 16
#define MEMORY_MAX_OBJ_SIZE 2048

static SPARROW_INLINE
size_t NextPowerOf2Size(size_t v) {
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  return v;
}

/* Misc helper */
void MemGrow( void** buffer, size_t* ncap, size_t objsz );

/* This is not safe but currently we just assume it works */
static SPARROW_INLINE
double NumMod( double left , double right ) {
  int64_t lint = (int64_t)left;
  int64_t rint = (int64_t)right;
  return (double) (lint % rint);
}

static SPARROW_INLINE
int ConvNum( double num , int* value ) {
  double ipart;
  double ret = modf(num,&ipart);
  if(ret != 0.0) return -1;
  if(ipart > SPARROW_INT_MAX ||
     ipart < SPARROW_INT_MIN ) return -1;
  *value = (int)(ipart);
  return 0;
}

/* Conversion function */
static SPARROW_INLINE
int ToInt( double num , int* output ) {
#ifdef SPARROW_CHECK_DOUBLE_TO_INT
  if( num > SPARROW_INT_MAX ||
      num < SPARROW_INT_MIN )
    return -1;
  else
#endif /* SPARROW_CHECK_DOUBLE_TO_INT */
  {
    *output = (int)(num);
    return 0;
  }
}

static SPARROW_INLINE
int ToSize( double num , size_t* value ) {
#ifdef SPARROW_CHECK_DOUBLE_TO_SIZE
  if(num > SPARROW_SIZE_MAX || num < 0)
    return -1;
  else
#endif /* SPARROW_CHECK_DOUBLE_TO_SIZE */
  {
    *value = (size_t)num;
    return 0;
  }
}

/* String helper */
struct CStr {
  const char* str;
  size_t len;
};

#define CONST_CSTR(STR) { (STR) , ARRAY_SIZE(STR)-1 }

static SPARROW_INLINE struct CStr
CStrDupLen( const char* str , size_t l ) {
  struct CStr res;
  SPARROW_ASSERT(str);
  res.str = strdup(str);
  res.len = l;
  return res;
}
static struct CStr SPARROW_INLINE
CStrDup( const char* str ) {
  return CStrDupLen(str,strlen(str));
}

static  SPARROW_INLINE
struct CStr CStrEmpty() {
  struct CStr ret = { NULL , 0 };
  return ret;
}

static SPARROW_INLINE
int CStrIsEmpty( const struct CStr* str ) {
  return str->len == 0;
}

static SPARROW_INLINE struct CStr
CStrDupCStr( const struct CStr* str ) {
  return CStrDupLen(str->str,str->len);
}

static SPARROW_INLINE void
CStrDestroy( struct CStr* str ) {
  free((void*)(str->str));
  str->str = NULL; str->len = 0;
}

static SPARROW_INLINE int
CStrCmp( const struct CStr* left , const struct CStr* right ) {
  if(left->str == NULL || right->str == NULL)
    return left->str == right->str ? 0 : -1;
  return strcmp(left->str,right->str);
}

static SPARROW_INLINE int
CStrEq( const struct CStr* left , const struct CStr* right ) {
  if(left->str == NULL || right->str == NULL)
    return left->str == right->str;
  else if(left->len == right->len)
    return strcmp(left->str,right->str) == 0;
  else
    return 0;
}

static SPARROW_INLINE struct CStr
CStrCat( const struct CStr* left , const struct CStr* right ) {
  struct CStr result;
  result.str = malloc(left->len + right->len + 1);
  memcpy((void*)result.str,left->str,left->len);
  memcpy((void*)result.str+left->len,right->str,right->len);
  ((char*)(result.str))[left->len+right->len] = 0;
  result.len = left->len + right->len;
  return result;
}

struct CStr CStrVPrintF( const char* , va_list );
static SPARROW_INLINE struct CStr
CStrPrintF( const char* fmt , ... ) {
  va_list vl;
  va_start(vl,fmt);
  return CStrVPrintF(fmt,vl);
}

/* String buffer helper */
struct StrBuf {
  char* buf;
  size_t size;
  size_t cap;
};

static SPARROW_INLINE void
StrBufInit( struct StrBuf* sbuf , size_t cap ) {
  if(cap) {
    sbuf->buf = malloc(cap);
    sbuf->cap = cap;
    sbuf->size = 0;
  } else {
    sbuf->buf = NULL;
    sbuf->cap = 0;
    sbuf->size= 0;
  }
}

static SPARROW_INLINE void
StrBufPush( struct StrBuf* sbuf , char c ) {
  if(sbuf->size == sbuf->cap) {
    MemGrow((void**)&(sbuf->buf),&(sbuf->cap),1);
  }
  sbuf->buf[sbuf->size] = c;
  ++sbuf->size;
}

static SPARROW_INLINE char
StrBufPop( struct StrBuf* sbuf ) {
  char ret;
  SPARROW_ASSERT(sbuf->size >0);
  ret = sbuf->buf[sbuf->size-1];
  --sbuf->size;
  return ret;
}

static SPARROW_INLINE void
StrBufReserve( struct StrBuf* sbuf , size_t cap ) {
  if(sbuf->cap >= cap) return;
  else {
    sbuf->buf = realloc(sbuf->buf,cap);
    sbuf->cap = cap;
  }
}

static SPARROW_INLINE void
StrBufResize( struct StrBuf* sbuf , size_t cap ) {
  StrBufReserve(sbuf,cap);
  sbuf->size = cap;
}

static SPARROW_INLINE void
StrBufClear( struct StrBuf* sbuf ) {
  sbuf->size = 0;
}

static SPARROW_INLINE void
StrBufDestroy( struct StrBuf* sbuf ) {
  free(sbuf->buf);
  sbuf->buf = NULL;
  sbuf->size = 0;
  sbuf->cap = 0;
}

static SPARROW_INLINE size_t
StrBufAppendStrLen(struct StrBuf* sbuf , const char* str , size_t len ) {
  if(sbuf->size + len > sbuf->cap) {
    size_t ncap = sbuf->cap*2 + len;
    sbuf->buf = realloc(sbuf->buf,ncap);
    sbuf->cap = ncap;
  }
  memcpy(sbuf->buf+sbuf->size,str,len);
  sbuf->size += len;
  return len;
}

static SPARROW_INLINE size_t
StrBufAppendStr( struct StrBuf* sbuf , const char* str ) {
  return StrBufAppendStrLen(sbuf,str,strlen(str));
}

static SPARROW_INLINE size_t
StrBufAppendCStr(struct StrBuf* sbuf , const struct CStr* str ) {
  return StrBufAppendStrLen(sbuf,str->str,str->len);
}

static SPARROW_INLINE size_t
StrBufAppendStrBuf( struct StrBuf* sbuf , const struct StrBuf* str ) {
  return StrBufAppendStrLen(sbuf,str->buf,str->size);
}

static SPARROW_INLINE size_t
StrBufAppendChar( struct StrBuf* sbuf , char c ) {
  char buf[2] = {c,0};
  return StrBufAppendStrLen(sbuf,buf,1);
}

struct CStr StrBufToCStr ( struct StrBuf* );

size_t StrBufVPrintF(struct StrBuf* , const char* fmt , va_list );
static SPARROW_INLINE size_t
StrBufPrintF( struct StrBuf* sbuf , const char* fmt , ... ) {
  va_list vl;
  va_start(vl,fmt);
  return StrBufVPrintF(sbuf,fmt,vl);
}
static SPARROW_INLINE size_t
StrBufVAppendF(struct StrBuf* sbuf , const char* fmt , va_list vl ) {
  struct StrBuf temp;
  size_t ret;
  StrBufInit(&temp,128);
  StrBufVPrintF(&temp,fmt,vl);
  StrBufAppendStrBuf(sbuf,&temp);
  ret = temp.size;
  StrBufDestroy(&temp);
  return ret;
}
static size_t SPARROW_INLINE
StrBufAppendF(struct StrBuf* sbuf, const char* fmt , ... ) {
  va_list vl;
  va_start(vl,fmt);
  return StrBufVAppendF(sbuf,fmt,vl);
}

/* Append a num in human readable string */
static SPARROW_INLINE
void StrBufAppendNumString( struct StrBuf* sbuf , double num ) {
  double ipart;
  double ret = modf(num,&ipart);
  if( ret == 0.0 ) {
    StrBufAppendF(sbuf,"%.0f",ipart);
  } else {
    StrBufAppendF(sbuf,"%." SPARROW_REAL_FORMAT_PRECISION "f",num);
  }
}

static SPARROW_INLINE
int NumPrintF( double num , char* buf , size_t len ) {
  double ipart;
  double ret = modf(num,&ipart);
  if(ret == 0.0) {
    return snprintf(buf,len,"%.0f",ipart);
  } else {
    return snprintf(buf,len,"%.6f",num);
  }
}

char* ReadFile( const char* filepath , size_t* length );

/* Arena allocator or bump allocator. Used in backend to generate IR,
 * sea of nodes */
struct ArenaAllocator;
struct ArenaAllocator* ArenaAllocatorCreate( size_t initial_size , size_t maximum_size ) ;
void* ArenaAllocatorAlloc( struct ArenaAllocator* aa , size_t size );
void* ArenaAllocatorRealloc(struct ArenaAllocator* arena , void* old ,
                                                           size_t old_size,
                                                           size_t new_size );
void ArenaAllocatorDestroy(struct ArenaAllocator* aa );

/* Some string manipulation routine but allocate memory via a ArenaAllocator */
const char* ArenaStrDupLen( struct ArenaAllocator* , const char* , size_t len );
static SPARROW_INLINE
const char* ArenaStrDup( struct ArenaAllocator* arena , const char* str ) {
  return ArenaStrDupLen(arena,str,strlen(str));
}
const char* ArenaVPrintF(struct ArenaAllocator* , const char* , va_list );
static SPARROW_INLINE
const char* ArenaPrintF( struct ArenaAllocator* arena , const char* fmt , ... ) {
  va_list vl;
  va_start(vl,fmt);
  return ArenaVPrintF(arena,fmt,vl);
}

#define SparrowAlign(SIZE,ALIGN) (((SIZE) + (ALIGN)-1) & ~((ALIGN)-1))

/* Macro to help handling common dynamic array pattern in C.
 * The decalaration must be :
 * NAME_arr;
 * NAME_size;
 * NAME_cap; */
#define DynArrPush(C,PREFIX,OBJ) \
  do { \
    if((C)->PREFIX##_size == (C)->PREFIX##_cap) { \
      MemGrow((void**)&((C)->PREFIX##_arr),&((C)->PREFIX##_cap),sizeof(OBJ)); \
    } \
    ((C)->PREFIX##_arr)[((C)->PREFIX##_size)] = (OBJ); \
    ++((C)->PREFIX##_size); \
  } while(0)

/* Linked list helper macro.
 * Used to work on double linked circular list, so user needs a dummy
 * object serves as header.
 * The definition of link list is :
 * PREFIX_next ( next pointer )
 * PREFIX_prev ( prev pointer )
 * The header is just a dummy object */
#define _LIST_HEADER(C,PREFIX) (&((C)->PREFIX##_list))
#define _LIST_NEXT(OBJ,PREFIX) ((OBJ)->PREFIX##_next)
#define _LIST_PREV(OBJ,PREFIX) ((OBJ)->PREFIX##_prev)

#define ListLink(C,PREFIX,OBJ) \
  do { \
    _LIST_PREV(OBJ,PREFIX) = _LIST_PREV(_LIST_HEADER(C,PREFIX),PREFIX); \
    _LIST_NEXT(_LIST_PREV(_LIST_HEADER(C,PREFIX),PREFIX),PREFIX) = (OBJ); \
    _LIST_NEXT(OBJ,PREFIX) = _LIST_HEADER(C,PREFIX); \
    _LIST_PREV(_LIST_HEADER(C,PREFIX),PREFIX) = (OBJ); \
  } while(0)

#define ListRemove(PREFIX,OBJ) \
  do { \
    _LIST_NEXT(_LIST_PREV(OBJ,PREFIX),PREFIX) = _LIST_NEXT(OBJ,PREFIX); \
    _LIST_PREV(_LIST_NEXT(OBJ,PREFIX),PREFIX) = _LIST_PREV(OBJ,PREFIX); \
  } while(0)

#define ListInit(C,PREFIX) \
  do { \
    _LIST_NEXT(_LIST_HEADER(C,PREFIX),PREFIX) = _LIST_HEADER(C,PREFIX); \
    _LIST_PREV(_LIST_HEADER(C,PREFIX),PREFIX) = _LIST_HEADER(C,PREFIX); \
  } while(0)

#define ListForeach(C,PREFIX,NAME) \
  for( NAME = _LIST_HEADER(C,PREFIX); \
       NAME != _LIST_HEADER(C,PREFIX); \
       NAME = _LIST_NEXT(mod,PREFIX) )

#endif /* UTIL_H_ */
