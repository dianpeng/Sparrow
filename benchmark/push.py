import time

t = []

start = time.time()
for i in xrange(1,10000):
    t.append({})
end = time.time()

print((end-start)*1000000)

