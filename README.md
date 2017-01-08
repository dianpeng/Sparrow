Sparrow
=======================================
# What it is
Sparrow is a small compact script language similar to javascript. Its purpose
is to investigate new JIT technology and study related state of art paper. It
is used as a playground for me to try different things out.

# Current status

Currently the interpreter which consumes byte code has been done , which I call
front-end. The front-end features a one pass parser, the parser will run constant folding
and some other trivial optimizations in place. The parser directly spits out bytecode
and then interpreter consumes those bytecodes. The design is mostly influenced
by lua and luajit. However the interpreter currently is still written in C.
The GC is pretty simple , only a stop the world GC is provided currently, later
on I will revisit this part.
The script language is pretty usable now, you could just image it as a lua but wrapped
in a javascript like syntax. And its performance in most case is very good since there're
lots of optimizations are already performed on top of the VM. It is very early, so
everything is subject to change

# Performance

I didn't do too much benchmark yet but compared to python 2.7 , sparrow is 10%
faster on recursive call and loop related operations. Since many intrinsice call
trick is used in Sparrow to reduce dictionary lookup, I believe its performance
should not be sucky even compared to many other cool scripting engine. In long
run , Sparrow will feature a JIT compiler , the interpreter part is important but
I will not spend too much efforts to fine tune it.

# Blueprint

In long run, I will add a 2 tier JIT compiler to Sparrow , mostly for personal
studying purpose. Instead of using a tracing JIT, I will use a method JIT. The
reason is simply because a method JIT can allow me to implement more optimization
pass . It may not be a good choice for performance but a good choice for learning
those papers and algorithm. Tier 1 JIT will be a simple JIT which just do inline
cachine and machine code generation , and also a linear scan register allocation
will be provided. Tier 2 JIT will try to mimic hotspot server compiler ,if I can,
which I will try to add a heavy JIT compiler on top of it.
Many details are still unknown ight now , like native call support , which result in
pain in the ass for calling convention ; also stack layout is another painful thing.
Other things are that I need to add a better GC. I may not be able to add
a very cool one but at least a generational GC is needed otherwise the performance
will be totally screwed up.
I will support X86-64 at first since this is only 2 platforms I am familiar with.
For other platform like PPC , it may be fun to work with but I need time to make
myself know more about these platforms.

# Licence

MIT
