from bb_types import *
import pickle

d = {}
for i in xrange(1, 10000):
    b = RawBB(id=i, start=i, end=i*2)
    d[i] = b


f = open('t.bin', 'wb')
pickle.dump(d, f)
f.close()


f = open('t.bin', 'rb')
t = pickle.load(f)
f.close()

print t
print d