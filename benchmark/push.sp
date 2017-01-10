var times = 10000;
var l = [];

var start = msec();
for( i in loop(0,times,1) ) {
  l[i] = {};
}
var end = msec();

print("No GC:",(end-start),"usec\n");

// The above loop will never trigger GC since our bottom GC trigger is
// when it reaches 25000 objects on the fly.
assert( gc.generation == 0 , "No GC triggered!");

l = [];
// No we have GC triggered version
times = 100000;

start = msec();
for( i in loop(0,times,1) ) {
  l[i] = {};
}
end = msec();

print("GC:",(end-start),"usec\n");
print(gc.stat(),"\n");
