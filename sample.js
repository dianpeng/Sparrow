var a = 0;
var b = a;
var c = a + 1;

var foo = function( a ) {
  var sum = 0;
  for ( x in a ) {
    sum = sum + x;
  }
  return sum;
};

foo([1,2,3,4,5]);
