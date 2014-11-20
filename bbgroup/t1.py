from bb_types import *

bbs = BBMan()

bbs.load('test.bin')

for x in bbs[0].succs:
    print x.id
