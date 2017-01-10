import time

t = []

start = time.time()
for i in xrange(1,10000):
    t.append({})
end = time.time()

print("10000:{0}".format((end-start)*1000000))

t = []
start = time.time()
for i in xrange(1,100000):
    t.append({})
end = time.time()
print("100000:{0}".format((end-start)*1000000))


