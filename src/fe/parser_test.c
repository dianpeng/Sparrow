#include "parser.h"
#include "bc.h"
#include "object.h"
#include <stdlib.h>

#define STRINGIFY(...) #__VA_ARGS__

static int COUNT = 0;

static void show_bc( const char* src ) {
  struct Sparrow sth;
  struct CStr err;
  struct ObjModule* mod;
  SparrowInit(&sth);
  mod = Parse(&sth,"test",src,&err);
  if(!mod) {
    fprintf(stderr,"Failed:%s!",err.str);
    abort();
  } else {
    ++COUNT;
    // ObjDumpModule(mod,stdout,"test");
  }
  SparrowDestroy(&sth);
}

static void test_list() {
  show_bc("a = [1,2,3,4];");
  show_bc("a = [];");
  show_bc("a = [1,2,3,4,5,6,7,8,9,10];");
  show_bc("a = [\"str\",b,3,true,false,null];");
  show_bc("a = [[]];");
  show_bc("a = [[],1,[2,3,4],true];");
  show_bc("a = [false,true,null];");
}

static void test_map() {
  show_bc("a = { \"a\": b };");
  show_bc("a = {};");
  show_bc("a = { uuv : ss , pp : 2 , vv : true , qq : null };");
  show_bc("a = {\"key\":{}};");
  show_bc("a = [{}];");
  show_bc("a = { \"u\": [1,2] };");
}


static void test_postfix() {
  show_bc("a = b.c.d;");
  show_bc("a = a[true].b.c[false]();");
  show_bc("a = a[3];");
  show_bc("a = a.b[3].c.d[1+100];");
  show_bc("a = {}.c.d;");
  show_bc("a = [1,2,3,4].length[10];");
}

static void test_branch() {
  show_bc("if(a) temp = 1;");
  show_bc("if(a) temp = 1; else temp = 2;");
  show_bc("if(a) temp = 1; elif(b) temp = 2;");
  show_bc("if(a) tmep = 1; elif(b) temp = 2; else temp = 3;");
  show_bc("if(a) temp = 1; elif(b) temp = 2; elif(c) temp = 3; else temp =4;");
}

static void test_loop_ctrl() {
  show_bc("for( i in a ) { if( i % 2 == 0 ) break; } u = a;");
  show_bc("for( i in a ) { if( i % 2 == 0 ) continue; } u = a;");
}

static void test_for() {
  show_bc("for(i in a){ p = i; }");
  show_bc("for(i in [1,2,3,4] ) { sum = sum + i; }");
  show_bc("var temp = 0; for(i in [1,2,3,4,5]) { temp = temp + i; }");
  show_bc("var sum = 0; for(i in {}) sum = sum + i.a;");
  show_bc("var list = [1,2,3,4]; for( i in list ) sum = sum +i;");
  show_bc("for( i in a ) { var l; l = l +i; }");
}

static void test_closure() {
  show_bc("var a = function() { return u + v; };");
  show_bc("var a = function() { return function() { return a + b ; }; };");
  show_bc("var a = function() { var u = 0 ; return u + v * 2; };");
  show_bc("var a = function() { for( i in a ) return i; };");
  show_bc("var a = function() { var u = 0; for( i in {} ) u = u + i; };");
  show_bc("var a = function(a,b,c,d) { return a + b + c + d; };");
  show_bc("var f = function(a,d) { var c = 0; return a+d; };");
}

static void test_assignment() {
  show_bc("a.b = 1;");
  show_bc("a[1]= 1;");
  show_bc("a[b]= 1;");
  show_bc("a.b.c.d=1;");
  show_bc("a[1][2][3]=1;");
  show_bc("a.b[3].d.e[1]=1;");
}

static void test_upvar() {
  show_bc("var a = 10; var b = function() { return a; };");
  show_bc("var a = 10; var b = function() { return function() { return a; }; };");
  show_bc("var fib = function(va) { if(va <= 2) return va; else return fib(va-1) + fib(va-2); };");
  show_bc("var a = 1; var b = 2; var c = 3; var d = function() { return function() { return function() { return a + b + c; }; }; };");
  show_bc("var a = 1; var b = function() { var c = a; return function() { return a; }; };");
  show_bc("var a = 1; var b = function() { a = 3; };");
  show_bc("var b = function() { a = 3; };");
}

static void test_simple_arith() {
  show_bc("a=1;");
  show_bc("a=true;");
  show_bc("a=false;");
  show_bc("a=null;");
  show_bc("a=1+2;");
  show_bc("a=b;");
  show_bc("a=b+1+2;");
  show_bc("a=b+1+2*3;");
  show_bc("a=1+2*3+b;");
  show_bc("a=a*b+3;");
  show_bc("a=b*a*2+1;");
  show_bc("a=b>3;");
  show_bc("a=3>b;");
  show_bc("a=--3;");
  show_bc("a=--a;");
  show_bc("a=!!2;");
  show_bc("a=!!a;");
  show_bc("a=\"u\"+\"v\";");
  show_bc("a=\"u\"+a;");
  show_bc("a=\"u\"+\"v\"+a;");
  show_bc("a=a+\"u\"+\"v\";");
  show_bc("a=a/b;");
  show_bc("a=1%2;");
  show_bc("a=1;");
  show_bc("a=-3;");
  show_bc("a=2;");
  show_bc("a=-4;");
  show_bc("var a = 3; a = 4;");
  show_bc("var a; var b; var c; a = true; b = false; c = null;");
}

static void test_decl() {
  show_bc("var a;");
  show_bc("var a = 1;");
  show_bc("var b = a+3; var c = b;");
  show_bc("var a = 1 , b = 2;");
  show_bc("var a , b , c;");
}

static void test_return() {
  show_bc("return;");
  show_bc("return -1;");
  show_bc("return 0;");
  show_bc("return 1;");
  show_bc("return true;");
  show_bc("return false;");
  show_bc("return null;");
}

static void test_logic() {
  show_bc("a = a && (1+2);");
  show_bc("a = a && (2+b);");
  show_bc("a = a && true && b && c && d && 10;");
  show_bc("a = a && b && true;");
  show_bc("a = a && true;");
  show_bc("b = b || false;");
  show_bc("a = a && b && c && d;");
  show_bc("a = a || b;");
  show_bc("a = a || b || c || d;");
  show_bc("a = a && b || c;");
  show_bc("a = false && 3;");
  show_bc("a = 2 && 3;");
  show_bc("a = 2 && 3 && d;");
  show_bc("a = true && b;");
  show_bc("a = b && true;");
  show_bc("a = c && d;");
  show_bc("a = false && d;");
  show_bc("a = d && false;");
  show_bc("a = {} && true;");
  show_bc("a = [] && true;");
  show_bc("a = \"\" && true;");
  show_bc("a = function() { return; } && true;");
  show_bc("a = null && true;");
  show_bc("a = [] || false;");
  show_bc("a = [] || false || false || a;");
  show_bc("a = true || (false && ( [] || a || b || c || d));");
}

static void test_funccall() {
  show_bc("a = a();");
  show_bc("a = a.b.c();");
  show_bc("a = a(a);");
  show_bc("a = a(a,b,c);");
  show_bc("a = a(a(a(a())));");
  show_bc("a = a.b.c().d.e[10].g(true,false,1,null,\"str\");");
}

static void test_intrinsic() {
  show_bc("typeof();");
  show_bc("is_bool();");
  show_bc("is_string(a);");
  show_bc("a = typeof();");
  show_bc("c = is_string(a);");
}

static void test_fancy_loop() {
  show_bc(STRINGIFY(
        var sum = 0;
        for(i in x) {
          for(j in y) {
            sum = sum +j;
          }
          sum = sum *i;
        }
        ));
  show_bc(STRINGIFY(
        var sum = 0;
        for( j in y) {
          var b;
          if(b) {
            var c;
            break;
          }
        }
        ));
  show_bc(STRINGIFY(
        for( k,v in y ) {
          var b;
          b = b + v;
          if(b) {
            var c;
            var d;
            break;
            }
        }
        ));
}

int main() {
  test_list();
  test_map();
  test_postfix();
  test_branch();
  test_loop_ctrl();
  test_for();
  test_closure();
  test_assignment();
  test_upvar();
  test_simple_arith();
  test_decl();
  test_return();
  test_logic();
  test_funccall();
  test_intrinsic();
  test_for();
  test_fancy_loop();
  fprintf(stderr,"%d tests performed!\n",COUNT);
  return 0;
}
