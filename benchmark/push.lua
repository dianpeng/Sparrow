local t = {}

local start = os.clock()
for i = 1,10000 do
  t[i] = {}
end
local e = os.clock()

print((e-start)*1000000)
