// First test , just some simple arithmatic stuff
assert( 1+2*3 == 7 , "1+2*3 == 7");
assert( 1+3*2 == 7 , "1+3*2 == 7");

var fib = function(a) {
  if(a == 0 ||
     a == 1 ||
     a == 2) return a;
  else
    return fib(a-1) + fib(a-2);
};

var fib2 = function(a) {
  if(a == 0 || a == 1 || a == 2)
    return a;
  else {
    var p1, p2;
    var sum = 0;
    p1 = 1;
    p2 = 2;
    for( i in loop(3,a+1,1) ) {
      sum = p1 + p2;
      p1 = p2;
      p2 = sum;
    }
    return sum;
  }
};

var l = [1,2,3,4,5];
var f = list.size;
print(f(l),"\n");

assert(fib(10) == fib2(10),"fib");
