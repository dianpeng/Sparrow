#include <compiler/text-ir-builder.h>
#include <compiler/ir.h>
#include <compiler/ir-helper.h>
#include <shared/debug.h>

#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

/* Grammar:
 * @@start := graph.
 * graph := node-section edge-section
 * node-section := NODE LBRA node-decalaration-list RBRA
 * node-declaration-list := EMPTY | (node-declaration)+
 * node-declaration := string LSQR attr-list RSQR
 * attr-list := EMPTY | attr*
 * attr := ( OPCODE ASSIGN STRING ) |
 *         ( EFFECT ASSIGN INTEGER) |
 *         ( PROP_EFFECT ASSIGN INTEGER) |
 *         ( DEAD ASSIGN INTEGER ) |
 *         ( INPUT_SIZE ASSIGN INTEGER ) |
 *         ( INPUT_MAX ASSIGN INTEGER ) |
 *         ( OUTPUT_SIZE ASSIGN INTEGER ) |
 *         ( OUTPUT_MAX ASSIGN INTEGER ) |
 *         ( BOUNDED ASSIGN INTEGER ) |
 *         ( VARIABLE ASSIGN value )
 *
 * edge-section := edge LBRA edge-declaration-list RBRA
 * edge-declaration-list := EMPTY | edge-decalaration*
 * edge-declaration := STRING arrow STRING
 * arrow := INPUT_ARROW | OUTPUT_ARROW
 * value := STRING | INTEGER */

#define IG_TOKEN_LIST(__) \
  __(LBRA,"{" ) \
  __(RBRA,"}" ) \
  __(NODE,"node") \
  __(EDGE,"edge") \
  __(LSQR,"[") \
  __(RSQR,"]") \
  __(OPCODE,"opcode") \
  __(EFFECT,"effect") \
  __(PROP_EFFECT,"prop_effect") \
  __(DEAD,"dead") \
  __(INPUT_SIZE,"input_size") \
  __(OUTPUT_SIZE,"output_size") \
  __(INPUT_MAX,"input_max") \
  __(OUTPUT_MAX,"output_max") \
  __(BOUNDED,"bouded") \
  __(VARIABLE,"<variable>") \
  __(ASSIGN,"=") \
  __(COMMA,",") \
  __(INTEGER,"<integer>") \
  __(STRING,"<string>") \
  __(INPUT_ARROW,"<-") \
  __(OUTPUT_ARROW,"->") \
  __(BIDIR_ARROW,"<>") \
  __(ERROR,"<error>") \
  __(EOF,"<eof>")

enum ig_token {
#define __(A,B) IG_TOKEN_##A,
  IG_TOKEN_LIST(__)
  SIZE_OF_TOKENS
#undef __ /* __ */
};

static const char* ig_token_name( enum ig_token tk ) {
#define __(A,B) case IG_TOKEN_##A : return B;
  switch(tk) {
    IG_TOKEN_LIST(__)
    default: return NULL;
  }
#undef __ /* __ */
}

struct ig_lexeme {
  int int_value;
  const char* str_value;
  enum ig_token tk;
  size_t tk_len;
};

struct ig_lexer {
  struct IrGraph* graph;
  const char* source;
  size_t pos;
  struct ig_lexeme lexeme;
  int line;
  int ccount;
};

static void ig_lexer_init( struct ig_lexer* lexer , struct IrGraph* graph ,
                                                    const char* source ) {
  lexer->source = source;
  lexer->pos = 0;
  lexer->lexeme.tk = IG_TOKEN_ERROR;
  lexer->lexeme.tk_len = 0;
  lexer->graph = graph;
  lexer->line = 1;
  lexer->ccount=1;
}

static int ig_lexer_error( struct ig_lexer* lexer ) {
  SPARROW_DBG(ERROR,"IrGraph lexer: unrecognized character %c near %d and %d",
      lexer->source[lexer->pos],
      lexer->line,
      lexer->ccount);
  lexer->lexeme.tk = IG_TOKEN_ERROR;
  lexer->lexeme.tk_len = 0;
  return IG_TOKEN_ERROR;
}

static int ig_lexer_num ( struct ig_lexer* lexer ) {
  int c = lexer->source[lexer->pos];
  int start = lexer->pos;
  int num;
  char *endp;
  if(c == '-') ++start;
  while(isdigit(lexer->source[start++]));
  errno = 0;
  num = strtol(lexer->source+lexer->pos,&endp,10);
  if(endp == lexer->source + (start-1) && !errno) {
    lexer->lexeme.int_value = num;
    lexer->lexeme.tk = IG_TOKEN_INTEGER;
    lexer->lexeme.tk_len = (start - lexer->pos - 1);
    return IG_TOKEN_INTEGER;
  } else {
    lexer->lexeme.tk_len = 0;
    lexer->lexeme.tk = IG_TOKEN_ERROR;
    /* error during parsing the number */
    if(errno) {
      lexer->lexeme.str_value = strerror(errno);
    } else {
      SPARROW_DBG(ERROR,"IrGraph lexer: cannot parse number starting near %d and %d\n",
          lexer->line,
          lexer->ccount);
    }
    return IG_TOKEN_ERROR;
  }
}

static void ig_lexer_skip_cmt( struct ig_lexer* lexer ) {
  ++lexer->pos;
  do {
    int c = lexer->source[lexer->pos];
    if(c == '\n') {
      ++lexer->pos;
      break;
    } else if(c == 0) {
      break;
    } else {
      ++lexer->pos;
    }
  } while(1);
}

static int ig_lexer_str( struct ig_lexer* lexer ) {
  int start = lexer->pos + 1;
  char* buf = ArenaAllocatorAlloc(lexer->graph->arena,32);
  size_t sz = 0;
  size_t cap= 32;
  do {
    int c = lexer->source[start];
    if(c == '\\') {
      int nc = lexer->source[start+1];
      switch(nc) {
        case 't': c = '\t'; ++start; break;
        case 'b': c = '\b'; ++start; break;
        case 'n': c = '\n'; ++start; break;
        case 'v': c = '\v'; ++start; break;
        case 'r': c = '\r'; ++start; break;
        case '\\':c = '\\'; ++start; break;
        case '\"':c = '\"'; ++start; break;
        default: break;
      }
    } else if(c == '\"') {
      ++start;
      break;
    }
    if(sz ==cap) {
      buf = ArenaAllocatorRealloc(lexer->graph->arena,buf,cap,cap*2);
      cap *= 2;
    }
    buf[sz++] = c;
    ++start;
  } while(1);

  if(sz == cap) {
    buf = ArenaAllocatorRealloc(lexer->graph->arena,buf,cap,cap+1);
  }
  buf[sz++] = 0;
  lexer->lexeme.str_value = buf;
  lexer->lexeme.tk_len = (start-lexer->pos);
  lexer->lexeme.tk = IG_TOKEN_STRING;
  return IG_TOKEN_STRING;
}

static int SPARROW_INLINE lexer_is_id_rest_char( int c ) {
  return isalnum(c) || c == '_';
}

static int ig_lexer_var( struct ig_lexer* lexer ) {
  int c = lexer->source[lexer->pos];
  if(isalpha(c) || c == '_') {
    int start = lexer->pos + 1;
    for( c = lexer->source[start] ;
         lexer_is_id_rest_char(c) ;
         ++start )
      c = lexer->source[start];
    lexer->lexeme.tk = IG_TOKEN_VARIABLE;
    lexer->lexeme.tk_len = (start - lexer->pos);
    lexer->lexeme.str_value = ArenaStrDupLen(lexer->graph->arena,
        lexer->source + lexer->pos , start - lexer->pos );
    return IG_TOKEN_VARIABLE;
  } else {
    return ig_lexer_error(lexer);
  }
}

#define MAXIMUM_KEYWORD_LENGTH 14

static int kw_compare( const char* L , const char* R ) {
  int i;
  for( i = 0 ; i < MAXIMUM_KEYWORD_LENGTH ; ++i ) {
    char lc = L[i];
    char rc = R[i];
    if(!rc)
      return i; /* Success path , consumed *all* R string */
    else if( lc == rc )
      continue;
    else
      return -1;
  }
  SPARROW_UNREACHABLE(); return -1;
}

static int kw_check( struct ig_lexer* lex , const char* R ) {
  int i;
  if((i=kw_compare(lex->source+ lex->pos+1,R)) > 0 &&
      !lexer_is_id_rest_char(lex->source[1+i+lex->pos]))
    return 1;
  else
    return 0;
}

#define yield( TK , LEN ) \
  do { \
    lexer->lexeme.tk = (TK); \
    lexer->lexeme.tk_len = (LEN); \
    return (TK); \
  } while(0)

static int ig_lexer_var_or_keyword( struct ig_lexer* lexer ) {
  int c = lexer->source[lexer->pos];
  switch(c) {
    case 'o':
      if(kw_check(lexer,"pcode")) yield(IG_TOKEN_OPCODE,6);
      else if(kw_check(lexer,"utput_size")) yield(IG_TOKEN_OUTPUT_SIZE,11);
      else if(kw_check(lexer,"utput_max")) yield(IG_TOKEN_OUTPUT_MAX,10);
      else return ig_lexer_var(lexer);
    case 'e':
      if(kw_check(lexer,"ffect")) yield(IG_TOKEN_EFFECT,6);
      else if(kw_check(lexer,"dge")) yield(IG_TOKEN_EDGE,4);
      else return ig_lexer_var(lexer);
    case 'p':
      if(kw_check(lexer,"rop_effect")) yield(IG_TOKEN_PROP_EFFECT,11);
      else return ig_lexer_var(lexer);
    case 'd':
      if(kw_check(lexer,"ead")) yield(IG_TOKEN_DEAD,4);
      else return ig_lexer_var(lexer);
    case 'i':
      if(kw_check(lexer,"nput_size")) yield(IG_TOKEN_INPUT_SIZE,10);
      else if(kw_check(lexer,"nput_max")) yield(IG_TOKEN_INPUT_MAX,9);
      else return ig_lexer_var(lexer);
    case 'b':
      if(kw_check(lexer,"ounded")) yield(IG_TOKEN_BOUNDED,7);
      else return ig_lexer_var(lexer);
    case 'n':
      if(kw_check(lexer,"ode")) yield(IG_TOKEN_NODE,4);
      else return ig_lexer_var(lexer);
    default:
      return ig_lexer_var(lexer);
  }
}

static int ig_lexer_next( struct ig_lexer* lexer ) {
  lexer->pos += lexer->lexeme.tk_len;
  lexer->ccount += lexer->lexeme.tk_len;
  do {
    int c = lexer->source[lexer->pos];
    switch(c) {
      case '{': yield(IG_TOKEN_LBRA,1);
      case '}': yield(IG_TOKEN_RBRA,1);
      case '=': yield(IG_TOKEN_ASSIGN,1);
      case ',': yield(IG_TOKEN_COMMA,1);
      case '[': yield(IG_TOKEN_LSQR,1);
      case ']': yield(IG_TOKEN_RSQR,1);
      case '#': ig_lexer_skip_cmt(lexer); break;
      case '<': {
        int nc = lexer->source[lexer->pos+1];
        if(nc == '-') {
          yield(IG_TOKEN_INPUT_ARROW,2);
        } else if(nc == '>') {
          yield(IG_TOKEN_BIDIR_ARROW,2);
        } else {
          return ig_lexer_error( lexer );
        }
      }
      case '-': {
        int nc = lexer->source[lexer->pos+1];
        if(nc == '>') {
          yield(IG_TOKEN_OUTPUT_ARROW,2);
        } else if( isdigit(nc) ) {
          return ig_lexer_num(lexer);
        } else {
          return ig_lexer_error( lexer );
        }
      }
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        return ig_lexer_num( lexer );
      case '"': return ig_lexer_str(lexer);
      case ' ': case '\t': case '\v': case '\r': case '\b':
        ++lexer->pos;
        ++lexer->ccount;
        break;
      case '\n':
        ++lexer->pos;
        ++lexer->line;
        lexer->ccount = 1;
        break;
      case 0:
        yield(IG_TOKEN_EOF,0);
      default:
        return ig_lexer_var_or_keyword(lexer);
    }
  } while(1);
  SPARROW_UNREACHABLE();
  return IG_TOKEN_ERROR;
}

#undef yield /* yield */

/* =================================================
 * ig_parser for the irgraph textual representation
 * =================================================*/

/* Internal data structure to keep the node records */
struct node_record {
  struct IrNode* node; /* related ir_node */
  const char* name;       /* unique name for this ir_node in text  */
  int input_size;
  int input_max ;
  int output_size;
  int output_max;
  int bounded;
};

struct ig_parser {
  struct ig_lexer lexer;
  struct node_record* nr_arr;
  size_t nr_cap;
  size_t nr_size;
};

#define G(PARSER) ((PARSER)->lexer.graph)
#define TOKEN(PARSER) ((PARSER)->lexer.lexeme.tk)
#define LEXER(PARSER) (&((PARSER)->lexer))
#define LEXEME(PARSER) (&(LEXER(PARSER)->lexeme))

static SPARROW_INLINE void ig_parser_init( struct ig_parser* p ,
                                           struct IrGraph* graph ,
                                           const char* source ) {
  ig_lexer_init(&(p->lexer),graph,source);
  p->nr_arr = NULL;
  p->nr_cap = 0;
  p->nr_size= 0;

  /* try to push the start and end node as defualt into the nr_arr */
  {
    struct node_record nr;
    nr.node = graph->start;
    nr.name = "start";
    nr.input_size = 0;
    nr.input_max  = 0;
    nr.output_size= 1;
    nr.output_max = 1;
    nr.bounded = 0;
    DynArrPush(p,nr,nr);
  }

  {
    struct node_record nr;
    nr.node = graph->end;
    nr.name = "end";
    nr.input_size = -1;
    nr.input_max  = -1;
    nr.output_size= 0;
    nr.output_max = 0;
    nr.bounded = 0;
    DynArrPush(p,nr,nr);
  }
}

#define ig_parser_expect(PARSER,TK) \
  do { \
    if(TOKEN(PARSER) == (TK)) { \
      ig_lexer_next(LEXER(PARSER)); \
    } else { \
      SPARROW_DBG(ERROR,"IrGraph parser: unrecognized token %s, expect %s!",\
          ig_token_name(TOKEN(PARSER)), \
          ig_token_name((TK))); \
      return -1; \
    } \
  } while(0)

#define ig_parser_try(PARSER,TK) \
  do { \
    if(TOKEN(PARSER) != (TK)) { \
      SPARROW_DBG(ERROR,"IrGraph parser: unrecognized token %s, expect %s!", \
          ig_token_name(TOKEN(PARSER)), \
          ig_token_name((TK))); \
    } \
  } while(0)


static SPARROW_INLINE struct node_record* ig_parser_get_node( struct ig_parser* p,
    const char* name ) {
  size_t i;
  for( i = 0 ; i < p->nr_size ; ++i ) {
    if(strcmp(p->nr_arr[i].name,name) ==0)
      return p->nr_arr + i;
  }
  return NULL;
}

static SPARROW_INLINE struct node_record* ig_parser_new_node( struct ig_parser* p,
    const char* name ,
    int op ,
    int effect ,
    int prop_effect,
    int dead ,
    int input_size,
    int input_max ,
    int output_size,
    int output_max ,
    int bounded ) {
  if(ig_parser_get_node(p,name)) {
    SPARROW_DBG(ERROR,"IrGraph parser: node %s already existed!",name);
    return NULL;
  } else {
    struct node_record nr;
    nr.input_max = input_max;
    nr.output_max= output_max;
    nr.input_size= input_size;
    nr.output_size=output_size;
    nr.bounded = bounded;
    nr.node = IrNodeNewRaw(G(p),op,effect,prop_effect,dead);
    nr.name = name;
    DynArrPush(p,nr,nr);
    return p->nr_arr + p->nr_size - 1;
  }
}

static SPARROW_INLINE
int ig_parser_parse_attr( struct ig_parser* p , int* key , int* value ) {

#define DO(TK,TYPE,CONVERTER,CHECKER) \
    case (TK): do { \
      ig_lexer_next(LEXER(p)); \
      ig_parser_expect(p,IG_TOKEN_ASSIGN); \
      ig_parser_try(p,TYPE); \
      (*value) = CONVERTER(LEXEME(p)); \
      CHECKER((*value)); \
      ig_lexer_next(LEXER(p)); \
      *key = (TK); \
    } while(0); break

#define INTEGER_CONVERTER(X) ((X)->int_value)
#define BOOLEAN_CONVERTER(X) (((X)->int_value) ? 1 : 0)
#define NULL_CHECKER(X) (void)(X)
#define OPCODE_CONVERTER(X) IrNodeNameToOpcode((X)->str_value)
#define OPCODE_CHECKER(X)  \
    do { \
      if((X) < 0) { \
        SPARROW_DBG(ERROR,"IrGraph parser: unknown opcode %s!",LEXEME(p)->str_value); \
        return -1; \
      } \
    } while(0)

  switch(TOKEN(p)) {
    /* opcode */
    DO(IG_TOKEN_OPCODE,
       IG_TOKEN_STRING,
       OPCODE_CONVERTER,
       OPCODE_CHECKER);
    /* effect */
    DO(IG_TOKEN_EFFECT,
       IG_TOKEN_INTEGER,
       BOOLEAN_CONVERTER,
       NULL_CHECKER);
    /* prop_effect */
    DO(IG_TOKEN_PROP_EFFECT,
       IG_TOKEN_INTEGER,
       BOOLEAN_CONVERTER,
       NULL_CHECKER);
    /* dead */
    DO(IG_TOKEN_DEAD,
       IG_TOKEN_INTEGER,
       BOOLEAN_CONVERTER,
       NULL_CHECKER);
    /* input_size */
    DO(IG_TOKEN_INPUT_SIZE,
       IG_TOKEN_INTEGER,
       INTEGER_CONVERTER,
       NULL_CHECKER);
    /* input_max */
    DO(IG_TOKEN_INPUT_MAX,
       IG_TOKEN_INTEGER,
       INTEGER_CONVERTER,
       NULL_CHECKER);
    /* output_size */
    DO(IG_TOKEN_OUTPUT_SIZE,
       IG_TOKEN_INTEGER,
       INTEGER_CONVERTER,
       NULL_CHECKER);
    /* output_max */
    DO(IG_TOKEN_OUTPUT_MAX,
       IG_TOKEN_INTEGER,
       INTEGER_CONVERTER,
       NULL_CHECKER);
    /* bounded */
    DO(IG_TOKEN_BOUNDED,
       IG_TOKEN_INTEGER,
       BOOLEAN_CONVERTER,
       NULL_CHECKER);
    default:
      SPARROW_DBG(ERROR,"IrGraph parser: unknown token %s!",ig_token_name(TOKEN(p)));
      return -1;
  }
#undef DO
#undef OPCODE_CONVERTER
#undef OPCODE_CHECKER
#undef INTEGER_CONVERTER
#undef BOOLEAN_CONVERTER
#undef NULL_CHECKER

  return 0;
}

static
int ig_parser_parse_attr_list( struct ig_parser* p , const char* name ,
                                                     struct node_record** node ) {
  int input_max = -1;
  int input_size= 0;
  int output_max= -1;
  int output_size=0;
  int bounded = 0;
  int opcode = -1;
  int effect = 0;
  int prop_effect = 0;
  int dead = 0;

  SPARROW_ASSERT( TOKEN(p) == IG_TOKEN_LSQR );
  ig_lexer_next(LEXER(p));

  do {
    int k , v;
    if(ig_parser_parse_attr(p,&k,&v)) return -1;
    switch(k) {
      case IG_TOKEN_OPCODE: opcode = v; break;
      case IG_TOKEN_EFFECT: effect = v; break;
      case IG_TOKEN_PROP_EFFECT: prop_effect = v; break;
      case IG_TOKEN_DEAD: dead = v; break;
      case IG_TOKEN_INPUT_SIZE: input_size = v; break;
      case IG_TOKEN_INPUT_MAX: input_max = v; break;
      case IG_TOKEN_OUTPUT_SIZE: output_size = v; break;
      case IG_TOKEN_OUTPUT_MAX: output_max = v; break;
      case IG_TOKEN_BOUNDED: bounded = v; break;
      default: SPARROW_UNREACHABLE(); break;
    }
    if(TOKEN(p) == IG_TOKEN_COMMA) {
      ig_lexer_next(LEXER(p));
    } else {
      ig_parser_expect(p,IG_TOKEN_RSQR);
      break;
    }
  } while(1);

  *node = ig_parser_new_node(p,name,opcode,
                                    effect,
                                    prop_effect,
                                    dead,
                                    input_size,
                                    input_max,
                                    output_size,
                                    output_max,
                                    bounded);
  return 0;
}

static SPARROW_INLINE
int ig_parser_parse_node( struct ig_parser* p ) {
  const char* name;
  struct node_record* nr;
  ig_parser_try(p,IG_TOKEN_STRING);
  name = LEXEME(p)->str_value;
  ig_lexer_next(LEXER(p));
  if(TOKEN(p) != IG_TOKEN_LSQR) {
    SPARROW_DBG(ERROR,"IrGraph parser: a node must follows a attribute list,"
                      "but right now we just got %s!",ig_token_name(TOKEN(p)));
    return -1;
  }
  if(ig_parser_parse_attr_list(p,name,&nr)) return -1;
  (void)nr;
  return 0;
}

static SPARROW_INLINE
int ig_parser_parse_node_list( struct ig_parser* p ) {
  int node_count = 0;
  while( TOKEN(p) != IG_TOKEN_RBRA &&
         TOKEN(p) != IG_TOKEN_EOF ) {
    if(ig_parser_parse_node(p)) return -1;
    ++node_count;
  }

  if(node_count == 0) SPARROW_DBG(WARNING,"IrGraph parser: empty node section?");
  return 0;
}

static SPARROW_INLINE
int ig_parser_parse_node_section( struct ig_parser* p ) {
  SPARROW_ASSERT( TOKEN(p) == IG_TOKEN_NODE );
  ig_lexer_next(LEXER(p));
  ig_parser_expect(p,IG_TOKEN_LBRA);
  if(ig_parser_parse_node_list(p)) return -1;
  ig_parser_expect(p,IG_TOKEN_RBRA);
  return 0;
}

static SPARROW_INLINE
int ig_parser_parse_edge ( struct ig_parser* p ) {
  struct node_record* start, *end;
  int op;

  if(TOKEN(p) != IG_TOKEN_STRING) {
    SPARROW_DBG(ERROR,"IrGraph parser: expect a string to start the link, got %s",
        ig_token_name(TOKEN(p)));
    return -1;
  }
  start = ig_parser_get_node(p,LEXEME(p)->str_value);
  if(!start) {
    SPARROW_DBG(ERROR,"IrGraph parser: node %s is not defined!",LEXEME(p)->str_value);
    return -1;
  }

  op = ig_lexer_next(LEXER(p));
  if(op != IG_TOKEN_INPUT_ARROW &&
     op != IG_TOKEN_OUTPUT_ARROW &&
     op != IG_TOKEN_BIDIR_ARROW ) {
    SPARROW_DBG(ERROR,"IrGraph parser: link needs input/output/bidir arrow connected, got %s",
        ig_token_name(TOKEN(p)));
    return -1;
  }

  if(ig_lexer_next(LEXER(p)) != IG_TOKEN_STRING) {
    SPARROW_DBG(ERROR,"IrGraph parser: expect a string to end the link, got %s",
        ig_token_name(TOKEN(p)));
    return -1;
  }
  end = ig_parser_get_node(p,LEXEME(p)->str_value);
  ig_lexer_next(LEXER(p));

  if(op == IG_TOKEN_INPUT_ARROW) {
    IrNodeRawAddInput(G(p),start->node,end->node);
  } else if(op == IG_TOKEN_OUTPUT_ARROW) {
    IrNodeRawAddOutput(G(p),start->node,end->node);
  } else {
    IrNodeRawAddInput(G(p),start->node,end->node);
    IrNodeRawAddOutput(G(p),start->node,end->node);
  }
  return 0;
}

static int ig_parser_parse_edge_list( struct ig_parser* p ) {
  int count = 0;
  while(TOKEN(p) != IG_TOKEN_RBRA &&
        TOKEN(p) != IG_TOKEN_EOF) {
    if(ig_parser_parse_edge(p)) return -1;
    ++count;
  }
  if(count ==0) SPARROW_DBG(WARNING,"IrGraph parser: empty edge section?");
  return 0;
}

static int ig_parser_parse_edge_section( struct ig_parser* p ) {
  SPARROW_ASSERT( TOKEN(p) == IG_TOKEN_EDGE );
  ig_lexer_next(LEXER(p));
  ig_parser_expect(p,IG_TOKEN_LBRA);
  if(ig_parser_parse_edge_list(p)) return -1;
  ig_parser_expect(p,IG_TOKEN_RBRA);
  return 0;
}

static int ig_parser_verify( struct ig_parser* p ) {
  size_t i;
  for( i = 0 ; i < p->nr_size ; ++i ) {
    struct node_record* nr = p->nr_arr + i;
    struct IrNode* node = nr->node;

#define CHECK(field) \
    do { \
      if(nr->field != -1 && node->field != nr->field) { \
        SPARROW_DBG(ERROR,"IrNode: name(%s)'s field (%s) doesn't match!" \
                          "expect (%d) , but got (%d)!", \
                          nr->name, \
                          #field, \
                          nr->field, \
                          node->field); \
        return -1; \
      } \
    } while(0)

    CHECK(input_max);
    CHECK(input_size);
    CHECK(output_max);
    CHECK(output_size);

#undef CHECK /* CHECK */

    {
      int b = node->bounded_node ? 1 : 0;
      if(b != nr->bounded) {
        SPARROW_DBG(ERROR,"IrNode: name(%s)'s field (%s) doesn't match!"
                          "expect (%d) , but got (%d)!",
                          nr->name,
                          "bounded",
                          nr->bounded,
                          b);
      }
    }
  }
  return 0;
}

static int ig_parser_parse( struct ig_parser* p ) {
  ig_lexer_next(LEXER(p));
  if(TOKEN(p) != IG_TOKEN_NODE) {
    SPARROW_DBG(ERROR,"IrGraph parser: expect node section!");
    return -1;
  }
  if(ig_parser_parse_node_section(p)) return -1;
  if(TOKEN(p) != IG_TOKEN_EDGE) {
    SPARROW_DBG(ERROR,"IrGraph parser: expect edge section!");
    return -1;
  }
  if(ig_parser_parse_edge_section(p)) return -1;
  if(ig_parser_verify(p)) return -1;
  return 0;
}

/* ====================================================
 * Public Interface
 * ==================================================*/
int TextToIrGraph( const char* text , struct IrGraph* output ) {
  struct ig_parser p;
  ig_parser_init(&p,output,text);
  return ig_parser_parse(&p);
}

/* ===================================================
 * Self Testing
 * =================================================*/
#ifdef TEXT_IR_BUILDER_UNIT_TEST
#include <assert.h>
#define STRINGIFY(...) #__VA_ARGS__

#if 0
static void test_lexer() {
  {
    struct ig_lexer lexer;
    struct IrGraph graph;
    const char* source = STRINGIFY(
        { } node edge [ ] opcode effect prop_effect dead input_size
        output_size input_max output_max bounded var var2 _var _
        = , 123 "string" <- -> <>
        );
    int tokens[] = {
      IG_TOKEN_LBRA,
      IG_TOKEN_RBRA,
      IG_TOKEN_NODE,
      IG_TOKEN_EDGE,
      IG_TOKEN_LSQR,
      IG_TOKEN_RSQR,
      IG_TOKEN_OPCODE,
      IG_TOKEN_EFFECT,
      IG_TOKEN_PROP_EFFECT,
      IG_TOKEN_DEAD,
      IG_TOKEN_INPUT_SIZE,
      IG_TOKEN_OUTPUT_SIZE,
      IG_TOKEN_INPUT_MAX,
      IG_TOKEN_OUTPUT_MAX,
      IG_TOKEN_BOUNDED,
      IG_TOKEN_VARIABLE,
      IG_TOKEN_VARIABLE,
      IG_TOKEN_VARIABLE,
      IG_TOKEN_VARIABLE,
      IG_TOKEN_ASSIGN,
      IG_TOKEN_COMMA,
      IG_TOKEN_INTEGER,
      IG_TOKEN_STRING,
      IG_TOKEN_INPUT_ARROW,
      IG_TOKEN_OUTPUT_ARROW,
      IG_TOKEN_BIDIR_ARROW,
      IG_TOKEN_EOF
    };
    int i;
    IrGraphInit(&graph,NULL,NULL,NULL);
    ig_lexer_init(&lexer,&graph,source);
    ig_lexer_next(&lexer);
    for( i = 0 ; i < SPARROW_ARRAY_SIZE(tokens) ; ++i ) {
      assert(lexer.lexeme.tk == tokens[i]);
      if( tokens[i] != IG_TOKEN_EOF ) ig_lexer_next(&lexer);
    }
    IrGraphDestroy(&graph);
  }
  {
    struct ig_lexer lexer;
    struct IrGraph graph;
    const char* source = STRINGIFY( 123 41231 23 -3 -4 -11 -0 0);
    IrGraphInit(&graph,NULL,NULL,NULL);
    ig_lexer_init(&lexer,&graph,source);
    ig_lexer_next(&lexer);

#define DO(NUM) \
    do { \
      assert(lexer.lexeme.tk == IG_TOKEN_INTEGER); \
      assert(lexer.lexeme.int_value == (NUM)); \
      ig_lexer_next(&lexer); \
    } while(0)

    DO(123);
    DO(41231);
    DO(23);
    DO(-3);
    DO(-4);
    DO(-11);
    DO(0);
    DO(0);

    assert( lexer.lexeme.tk == IG_TOKEN_EOF );

#undef DO /* DO */
    IrGraphDestroy(&graph);
  }
  {
    struct ig_lexer lexer;
    struct IrGraph graph;
    const char* source = STRINGIFY( "name" "you" "hello" "a\t\n" );
    IrGraphInit(&graph,NULL,NULL,NULL);
    ig_lexer_init(&lexer,&graph,source);
    ig_lexer_next(&lexer);

#define DO(STR) \
    do { \
      assert(lexer.lexeme.tk == IG_TOKEN_STRING); \
      assert(strcmp(lexer.lexeme.str_value,(STR))==0); \
      ig_lexer_next(&lexer); \
    } while(0)

    DO("name");
    DO("you");
    DO("hello");
    DO("a\t\n");

    assert( lexer.lexeme.tk == IG_TOKEN_EOF );

#undef DO /* DO */
    IrGraphDestroy(&graph);
  }
}
#endif

static void test_parser() {
  {
    struct IrGraph graph;
    const char* source = STRINGIFY(
        node {
          "a"[opcode="ctl_region",output_size=1]
        }
        edge {
          "start" -> "a"
          "a" -> "end"
          "start" -> "end"
        }
    );
    IrGraphInit(&graph,NULL,NULL,NULL);
    TextToIrGraph(source,&graph);
    {
      struct StrBuf output;
      struct IrGraphDisplayOption option;
      option.show_extra_info = 1;
      option.only_control_flow = 0;
      StrBufInit(&output,1024);
      IrGraphToDotFormat( &output , &graph , &option);
      fwrite(output.buf,1,output.size,stdout);
      StrBufDestroy(&output);
    }
    IrGraphDestroy(&graph);
  }
}

int main() {
  test_parser();
  return 0;
}
#endif
