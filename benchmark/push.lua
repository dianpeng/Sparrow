local t = {}

local start = os.clock()
for i = 1,10000 do
  t[i] = {}
end
local e = os.clock()

print("10000:",(e-start)*1000000)

t = {}

start = os.clock()
for i = 1,10000000 do
  t[i] = {}
end
e = os.clock();
print("100000:",(e-start)*1000000)
