#include "lexer.h"
#include <assert.h>
#include <float.h>
#include <stdio.h>

static void test_operators() {
  {
    struct Lexer l;
    enum Token expected[] = {
      TK_ADD,
      TK_SUB,
      TK_MUL,
      TK_DIV,
      TK_MOD,
      TK_POW,
      TK_NOT,
      TK_LEN,
      TK_LT,
      TK_LE,
      TK_GT,
      TK_GE,
      TK_EQ,
      TK_NE,
      TK_AND,
      TK_OR,
      TK_ASSIGN,
      TK_DOT,
      TK_COLON,
      TK_COMMA,
      TK_SEMICOLON,
      TK_LPAR,
      TK_RPAR,
      TK_LBRA,
      TK_RBRA,
      TK_LSQR,
      TK_RSQR,
      TK_IF,
      TK_ELSE,
      TK_ELIF,
      TK_FOR,
      TK_IN,
      TK_WHILE,
      TK_BREAK,
      TK_CONTINUE,
      TK_FUNCTION,
      TK_RETURN,
      TK_IMPORT,
      TK_VARIABLE,
      TK_VAR,
      TK_EOF
    };
    size_t cnt = 0;
    struct Lexeme* lm;

    LexerInit(&l,"+ - * / % ^ ! # < <= > >= == != && || = . : , ; () {} []" \
                 "if else elif for in while break continue function return" \
                 " import avar var ",0);
    do {
      lm = LexerNext(&l);
      assert(lm->tk == expected[cnt]);
      ++cnt;
    } while(lm->tk != TK_EOF);
    LexerDestroy(&l);
  }
}

static int sbufeq( const struct StrBuf* sbuf , const char* str ) {
  return memcmp(sbuf->buf,str,sbuf->size) == 0;
}

static void test_keyword() {
  {
    struct Lexer l;
    struct Lexeme* lm;
    LexerInit(&l,"var _var var_ true _true true_ false false_ _false",0);
    lm = LexerNext(&l);
    assert(lm->tk == TK_VAR);
    lm = LexerNext(&l);
    assert(lm->tk == TK_VARIABLE);
    assert(sbufeq(&lm->str,"_var"));
    lm = LexerNext(&l);
    assert(lm->tk == TK_VARIABLE);
    assert(sbufeq(&lm->str,"var_"));
    lm = LexerNext(&l);
    assert(lm->tk == TK_TRUE);
    lm = LexerNext(&l);
    assert(lm->tk == TK_VARIABLE);
    assert(sbufeq(&lm->str,"_true"));
    lm = LexerNext(&l);
    assert(lm->tk == TK_VARIABLE);
    assert(sbufeq(&lm->str,"true_"));
    lm = LexerNext(&l);
    assert(lm->tk == TK_FALSE);
    lm = LexerNext(&l);
    assert(lm->tk == TK_VARIABLE);
    assert(sbufeq(&lm->str,"false_"));
    lm = LexerNext(&l);
    assert(lm->tk == TK_VARIABLE);
    assert(sbufeq(&lm->str,"_false"));
    assert(LexerNext(&l)->tk == TK_EOF);
    LexerDestroy(&l);
  }
  {
    struct Lexer l;
    struct Lexeme* lm;
    LexerInit(&l,"null _null null_",0);
    lm = LexerNext(&l);
    assert(lm->tk == TK_NULL);
    lm = LexerNext(&l);
    assert(lm->tk == TK_VARIABLE);
    assert(sbufeq(&lm->str,"_null"));
    lm = LexerNext(&l);
    assert(lm->tk == TK_VARIABLE);
    assert(sbufeq(&lm->str,"null_"));
    lm = LexerNext(&l);
    assert(lm->tk == TK_EOF);
    LexerDestroy(&l);
  }
}

static void test_number() {
  {
    struct Lexer l;
    struct Lexeme* lm;
    LexerInit(&l,"12345 u8 0",0);
    lm = LexerNext(&l);
    assert(lm->tk == TK_NUMBER);
    assert(lm->num == 12345);
    lm = LexerNext(&l);
    assert(lm->tk == TK_VARIABLE);
    assert(sbufeq(&lm->str,"u8"));
    lm = LexerNext(&l);
    assert(lm->tk == TK_NUMBER);
    assert(lm->num == 0);
    assert(LexerNext(&l)->tk == TK_EOF);
    LexerDestroy(&l);
  }
  { // Testing *MAX* double number
    char buf[4096];
    struct Lexer l;
    struct Lexeme* lm;
    sprintf(buf,"%f",DBL_MAX);
    LexerInit(&l,buf,0);
    lm = LexerNext(&l);
    assert(lm->tk == TK_NUMBER);
    assert(lm->num == DBL_MAX);
    assert(LexerNext(&l)->tk == TK_EOF);
    LexerDestroy(&l);
  }
}

static void test_string() {
  {
    struct Lexer l;
    struct Lexeme* lm;
    LexerInit(&l,"\"string\"",0);
    lm = LexerNext(&l);
    assert(lm->tk == TK_STRING);
    assert(sbufeq(&lm->str,"string"));
    assert(LexerNext(&l)->tk == TK_EOF);
    LexerDestroy(&l);
  }
  {
    struct Lexer l;
    struct Lexeme* lm;
    LexerInit(&l,"\"a\\tb\"",0);
    lm = LexerNext(&l);
    assert(lm->tk == TK_STRING);
    assert(sbufeq(&lm->str,"a\tb"));
    assert(LexerNext(&l)->tk == TK_EOF);
    LexerDestroy(&l);
  }
  {
    struct Lexer l;
    struct Lexeme* lm;
    LexerInit(&l,"\"a\\vb\"",0);
    lm = LexerNext(&l);
    assert(lm->tk == TK_STRING);
    assert(sbufeq(&lm->str,"a\vb"));
    assert(LexerNext(&l)->tk == TK_EOF);
    LexerDestroy(&l);
  }
  {
    struct Lexer l;
    struct Lexeme* lm;
    LexerInit(&l,"\"a\\bb\"",0);
    lm = LexerNext(&l);
    assert(lm->tk == TK_STRING);
    assert(sbufeq(&lm->str,"a\bb"));
    assert(LexerNext(&l)->tk == TK_EOF);
    LexerDestroy(&l);
  }
  {
    struct Lexer l;
    struct Lexeme* lm;
    LexerInit(&l,"\"a\\nb\"",0);
    lm = LexerNext(&l);
    assert(lm->tk == TK_STRING);
    assert(sbufeq(&lm->str,"a\nb"));
    assert(LexerNext(&l)->tk == TK_EOF);
    LexerDestroy(&l);
  }
  {
    struct Lexer l;
    struct Lexeme* lm;
    LexerInit(&l,"\"a\\\\b\"",0);
    lm = LexerNext(&l);
    assert(lm->tk == TK_STRING);
    assert(sbufeq(&lm->str,"a\\b"));
    assert(LexerNext(&l)->tk == TK_EOF);
    LexerDestroy(&l);
  }
  {
    struct Lexer l;
    struct Lexeme* lm;
    LexerInit(&l,"\"a\\\"b\"",0);
    lm = LexerNext(&l);
    assert(lm->tk == TK_STRING);
    assert(sbufeq(&lm->str,"a\"b"));
    assert(LexerNext(&l)->tk == TK_EOF);
    LexerDestroy(&l);
  }
}

// Comments
static void test_comment() {
  {
    struct Lexer l;
    struct Lexeme* lm;
    LexerInit(&l,"// This is acomment \n Var",0);
    lm = LexerNext(&l);
    assert(lm->tk == TK_VARIABLE);
    assert(sbufeq(&lm->str,"Var"));
    assert(LexerNext(&l)->tk == TK_EOF);
    LexerDestroy(&l);
  }
  {
    struct Lexer l;
    LexerInit(&l,"//This is end",0);
    assert(LexerNext(&l)->tk == TK_EOF);
    LexerDestroy(&l);
  }
}

int main() {
  test_operators();
  test_keyword();
  test_number();
  test_string();
  test_comment();
}
