var l = [];
var ten_million = 100000;
var sum = 0;

var start = msec();
for( i in loop(0,ten_million,1) ) {
  l[i] = {};
}
var end = msec();
print("ten million costs : ",(end-start),"usec\n");
print(gc.stat(),"\n");
