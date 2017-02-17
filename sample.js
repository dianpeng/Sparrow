var foo = function( str ) {
  print("This is foo:");
  print(str);
  print("\n");
};

var fib = function( x ) {
  if(x <= 2) return x;
  else {
    return fib(x-1) + fib(x-2);
  }
};

var sum = function ( list ) {
  var ret = 0;
  for( k,v in list ) {
    ret = ret + v;
  }
  return ret;
};

var start = msec();
var sum_all = 0;
for( i in loop(1,10000000,1)) {
  sum_all = sum_all + i;
}
print("SUM : ",sum_all,"\n");
var end = msec();

print("Use time :",end-start," micro seconds \n");
for( i in loop(1,10,1) ) {
  foo("Hello World");
}
