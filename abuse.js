var j = 0;

for( i in loop(1,100,1) ) {
  j = j + 1;
  j = j - 1;
  g = j;
}

var b = 0;
b = b + 1;
b = b - 1;

return j + b;
