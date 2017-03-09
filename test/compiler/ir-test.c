#include <compiler/ir.h>
#include <compiler/bc-ir-builder.h>
#include <compiler/ir-helper.h>

#include <vm/parser.h>
#include <vm/object.h>
#include <shared/util.h>

#include <assert.h>
#include <stdio.h>

static int show_graph( const char* path ) {
  struct ObjModule* mod;
  struct ObjProto* proto;
  struct CStr err;
  struct Sparrow sparrow;

  SparrowInit(&sparrow);
  mod = Parse(&sparrow,path,NULL,&err);
  if(!mod) {
    fprintf(stderr,"Cannot parse file %s due to reason %s!\n",path,err.str);
    CStrDestroy(&err);
    return -1;
  }

  proto = ObjModuleGetEntry(mod);

  { /* Build an IR graph */
    struct IrGraph graph;
    struct StrBuf output;
    struct IrGraphDisplayOption option;
    option.show_extra_info = 1;
    option.only_control_flow = 1;
    IrGraphInit(&graph,mod,proto,&sparrow);
    StrBufInit(&output,1024);
    assert(!BytecodeToIrGraph(&sparrow,&graph));
    IrGraphToDotFormat( &output , &graph , &option);
    fwrite(output.buf,1,output.size,stdout);
    StrBufDestroy(&output);
    IrGraphDestroy(&graph);
  }
  SparrowDestroy(&sparrow);
  return 0;
}

int main( int argc , char* argv[] ) {
  if(argc != 2) {
    fprintf(stderr,"Usage\n");
    return -1;
  } else {
    return show_graph(argv[1]);
  }
}
