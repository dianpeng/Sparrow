// Unfortunately, our sparrow engine has a pretty basic STOP-THE-WORLD GC.
// This is in general not very cool since it may *pause* your application
// randomly especially when memory stress is high.
//
//
// What we want is to get a better GC stragety to make GC kicks in less and
// make the swap phase has more garbage to collect. Whenever a GC kicks in,
// it should expect to collect many garbage.
//
// It means when GC kicks in, if it cannot collects many objects, then it
// must have a penalty . This make sense since this pattern typically means
// our application has a pattern of holding those memory instead of releasing
// them. Our GC needs to be aware of this as well.


// 1. This is a pattern that we have lots of garbage since the map is just
// dropped right after it is created. (We don't have escape analyze).
// The GC in this situation is pretty performant and it should be since every
// times it kicks in it do collects some garbage.
var start = msec();
for( i in loop(1,10000000,1) ) {
  var u = {};
}
var end = msec();
print("Time:",end-start,"usec\n");

// Force a gc operation to make rest of our code faster
gc_force();

// 2. This is a pattern that currently we sucks at , of course we will fix it.
// This pattern is that it aggressively accumulates a lot of memory in to a
// single list and since this list is reachable so it is *not* garbage at all.
// But GC just kicks in very frequently because currently we don't have a penalty
// GC kicks in times. Because every time GC cannot swap out any garbage so the
// memory waterlevel is always high hence GC kicks in more frequently and make
// performance suck.

start = msec();
var l = [];
// We cannot use large loop times since it will make our code impossible to
// finish in current rookie GC strategy :)
for( i in loop(1,2000,1) ) {
  list.push(l,{});
}
end = msec();
print("Time:",end-start,"usec\n");

stat = gc_stat();
print("GC:\n",stat,"\n");
