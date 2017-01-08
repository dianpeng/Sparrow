var lib2 = import("test.sp");
return {
  "sum" : function(list) {
     var sum = 0;
     for( _, v in list ) {
       sum = sum + v;
     }
     return sum;
  },
  "calc" : lib2.calc
};
