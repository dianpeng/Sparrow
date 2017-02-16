#include "util.h"

#define STRING(X) X , ARRAY_SIZE(X)-1
#define STRLEN(X) (ARRAY_SIZE(X)-1)

static void test_cstr() {
  {
    struct CStr str = CStrDupLen( STRING("HelloWorld") );
    assert(strcmp(str.str,"HelloWorld")==0);
    assert(str.len == STRLEN("HelloWorld"));
    CStrDestroy(&str);
  }
  {
    struct CStr str = CStrDup("HelloWorld");
    assert(strcmp(str.str,"HelloWorld")==0);
    assert(str.len == STRLEN("HelloWorld"));
    CStrDestroy(&str);
  }
  {
    struct CStr str = CStrDupLen("",0);
    assert(strcmp(str.str,"")==0);
    assert(str.len==0);
    CStrDestroy(&str);
  }
  {
    struct CStr const_str = CONST_CSTR("HelloWorld");
    struct CStr heap_str = CStrDup("HelloWorld");
    assert( CStrCmp(&const_str,&heap_str) == 0);
    CStrDestroy(&heap_str);
  }
  {
    struct CStr const_str = CONST_CSTR("");
    struct CStr heap_str = CStrDup("");
    assert( CStrCmp(&const_str,&heap_str) ==0);
    CStrDestroy(&heap_str);
  }
  {
    struct CStr const_str = CONST_CSTR("A");
    struct CStr const_str2 = CONST_CSTR("B");
    struct CStr expect = CONST_CSTR("AB");
    struct CStr result = CStrCat(&const_str,&const_str2);
    assert(CStrCmp(&result,&expect)==0);
    CStrDestroy(&result);
  }
}

static void test_strbuf() {
  {
    struct StrBuf sbuf;
    StrBufInit(&sbuf,1024);
    assert(sbuf.buf != NULL);
    assert(sbuf.cap == 1024);
    assert(sbuf.size == 0);
    StrBufDestroy(&sbuf);
    assert(sbuf.buf == NULL);
    assert(sbuf.cap == 0);
    assert(sbuf.size == 0);
  }
  {
    struct StrBuf sbuf;
    StrBufInit(&sbuf,2);
    StrBufPush(&sbuf,'a');
    StrBufPush(&sbuf,'b');
    assert(sbuf.buf[0]=='a');
    assert(sbuf.buf[1]=='b');
    assert(sbuf.cap == 2);
    assert(sbuf.size== 2);
    StrBufPush(&sbuf,'c');
    assert(sbuf.buf[2] == 'c');
    assert(sbuf.cap == 4);
    assert(sbuf.size == 3);
    StrBufDestroy(&sbuf);
  }
  {
    struct StrBuf sbuf;
    StrBufInit(&sbuf,2);
    StrBufPush(&sbuf,'a');
    assert(sbuf.buf[0] == 'a');
    assert(sbuf.size == 1);
    assert(sbuf.cap == 2);
    StrBufPop(&sbuf);
    assert(sbuf.size == 0);
    StrBufClear(&sbuf);
    assert(sbuf.size == 0);
    assert(sbuf.cap == 2);
    StrBufDestroy(&sbuf);
  }
  // Appending functions
  {
    struct StrBuf sbuf;
    StrBufInit(&sbuf,2);
    assert(sbuf.size == 0);
    assert(sbuf.cap == 2);
    StrBufAppendStrLen(&sbuf,"HelloWorld",STRLEN("HelloWorld"));
    StrBufAppendStr(&sbuf,"UU");
    assert(sbuf.size == STRLEN("UU") + STRLEN("HelloWorld"));
    assert(memcmp(sbuf.buf,"HelloWorldUU",STRLEN("HelloWorldUU"))==0);
    StrBufDestroy(&sbuf);
  }
  {
    struct StrBuf sbuf;
    struct CStr const_str = CONST_CSTR("World");
    struct StrBuf abuf;
    StrBufInit(&sbuf,2);
    StrBufInit(&abuf,2);
    assert(sbuf.size == 0);
    assert(sbuf.cap == 2);
    assert(abuf.size == 0);
    assert(abuf.cap == 2);
    StrBufPush(&sbuf,'a');
    StrBufAppendStr(&sbuf,"Hello");
    StrBufAppendCStr(&sbuf,&const_str);
    assert(sbuf.size == STRLEN("aHelloWorld"));
    assert(memcmp(sbuf.buf,"aHelloWorld",STRLEN("aHelloWorld"))==0);
    StrBufAppendStr(&abuf,"UU");
    StrBufAppendStrBuf(&sbuf,&abuf);
    assert(sbuf.size == STRLEN("aHelloWorldUU"));
    assert(memcmp(sbuf.buf,"aHelloWorldUU",STRLEN("aHelloWorldUU"))==0);
    StrBufDestroy(&sbuf);
    StrBufDestroy(&abuf);
  }
  {
    struct StrBuf sbuf;
    StrBufInit(&sbuf,16);
    StrBufPrintF(&sbuf,"Hello%s","World");
    assert(sbuf.size == STRLEN("HelloWorld"));
    assert(memcmp(sbuf.buf,"HelloWorld",STRLEN("HelloWorld"))==0);
    StrBufDestroy(&sbuf);
  }
  {
    struct StrBuf sbuf;
    StrBufInit(&sbuf,2);
    StrBufPrintF(&sbuf,"%s","Hello");
    StrBufAppendF(&sbuf,"%s","World");
    assert(memcmp(sbuf.buf,"HelloWorld",STRLEN("HelloWorld"))==0);
    StrBufDestroy(&sbuf);
  }
}

int main() {
  test_cstr();
  test_strbuf();
  return 0;
}
