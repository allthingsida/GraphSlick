try:
    import idaapi
except:
    print "Standalone mode"

import bb_ida
from   bb_ida import *

# ------------------------------------------------------------------------------
def main():
    global bm
    global same_hashes

    same_hashes1 = {}
    same_hashes2 = {}

    bm = IDABBMan()
    if bb_ida.stdalone:
        addr = 0x40A730
        addr = 0xFFFFF803CC152F78
        addr = 0x40A494
        addr = 0xFFFFF803CC152F78
    else:
        addr = here()
        f = idaapi.get_func(addr)
        addr = f.startEA

    ok, _ = bm.FromFlowchart(
                addr, 
                use_cache = True, 
                get_bytes = True, 
                get_hash_itype1 = True,
                get_hash_itype2 = True)

    if not ok:
        print "Error: %r" % (bm)
        return False

    for bb in bm.items():
        h1 = bb.ctx.hash_itype1
        try:
            L = same_hashes1[h1]
            L.append(bb.id)
        except:
            L = [bb.id]
            same_hashes1[h1] = L
        continue

        if bb_ida.stdalone:
            print "[%d] %x (%d); ctx=(bytes_len=%d, hash_itype1=%s hash_itype2=%s)" % (bb.id, bb.start, bb.end - bb.start, len(bb.ctx.bytes), bb.ctx.hash_itype1, bb.ctx.hash_itype2)
        for sid in bb.succs:
            bb0 = bm[sid]
            if bb_ida.stdalone:
                print "  SUCC: [%d] %x (%d)" % (bb0.id, bb0.start, bb0.end - bb0.start)

        for pid in bb.preds:
            bb1 = bm[pid]
            if bb_ida.stdalone:
                print "  PREDS: [%d] %x (%d)" % (bb1.id, bb1.start, bb1.end - bb1.start)

    print "Stats for %d blocks" % len(same_hashes1)
    L = []
    for h1 in same_hashes1:
        v = same_hashes1[h1]
        c = len(v)
        if c < 2:
            continue
        L.append([c, v[0]])
        print "%s -> (%d)" % (h1, c)

    L = sorted(L, key = lambda x: x[0])

    for c, nid in L:
        bb = bm[nid]
        print "%x: count=%d" % (bb.start, c)

# ------------------------------------------------------------------------------
main()