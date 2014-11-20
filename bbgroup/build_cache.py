"""
Module to build a cache of the current function
"""
try:
    import idaapi
except:
    print "Standalone mode"

import bb_ida
from   bb_ida import *

# ------------------------------------------------------------------------------
def main():
    global bm

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

    ok, _ = bm.FromFlowchart(addr, use_cache = True, get_bytes = True, get_hash_itype1 = True)

    if not ok:
        print "Error: %r" % (bm)
        return False

    for bb in bm.items():
        if bb_ida.stdalone:
            print "[%d] %x (%d); ctx=(bytes_len=%d, hash_itype1=%s)" % (bb.id, bb.start, bb.end - bb.start, len(bb.ctx.bytes), bb.ctx.hash_itype1)
        for sid in bb.succs:
            bb0 = bm[sid]
            if bb_ida.stdalone:
                print "  SUCC: [%d] %x (%d)" % (bb0.id, bb0.start, bb0.end - bb0.start)

        for pid in bb.preds:
            bb1 = bm[pid]
            if bb_ida.stdalone:
                print "  PREDS: [%d] %x (%d)" % (bb1.id, bb1.start, bb1.end - bb1.start)

# ------------------------------------------------------------------------------
main()