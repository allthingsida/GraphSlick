"""
This module contains IDA specific basic block datatypes and helper functions


10/08/2013 - eliasb - Initial version
                    - Added hash_node utility function
					- Added read_analysis() and addr_to_bb()

10/15/2013 - eliasb - Changed output format so it is compatible with the visualization plugin


TODO:
- sort the stats by min_match and min_icount
"""
try:
    import idaapi
except:
    print "Standalone mode"

import bb_ida
from   bb_ida import *

# ------------------------------------------------------------------------------
# Define global variables
print_out = False
bm = None

# ------------------------------------------------------------------------------
def print_hash_stats(bm, hashes, caption='', min_match=2, min_icount=2):
    print "%sStats for %d matched block(s) out of %d total; min_match=%d, min_icount=%d" % (
            caption, 
            len(hashes),
            len(bm.items()),
            min_match,
            min_icount)

    crit_match = 0
    for hash in hashes:
        v = hashes[hash]
        
        # Get count of matches
        match_count = len(v)
        if match_count < min_match:
            continue

        # Get instruction count
        inst_count = bm[v[0][0]].ctx.inst_count
        if inst_count < min_icount:
            continue

        crit_match = crit_match + 1
        print "ID:%s;IC:%d;MC:%d;NODESET:%s;" % (
            hash, 
            inst_count, 
            match_count, 
            ", ".join(["(%s : %s : %s)" % (nid, start, end) for nid, start, end in v])
        )

    print "Found %d results" % crit_match

    return

# ------------------------------------------------------------------------------
def hash_node(id, hid=1):
    if bm is None:
        print "main() was not called!"
    else:
        return bb_ida.HashNode(bm, id, hid)


# ------------------------------------------------------------------------------
def read_analysis(fn):
    try:
        f = open(fn);
        print "".join(f.readlines());
        f.close()    
    except:
        pass

# ------------------------------------------------------------------------------
def addr_to_bb(addr):
    return bm.find_by_addr(addr)

# ------------------------------------------------------------------------------
def main(min_match=1, min_icount=1):
    global bm
    global same_hashes
    global print_out
    global same_hashes1
    global same_hashes2

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

    ok, err = bm.FromFlowchart(
                addr, 
                use_cache = True,
                get_bytes = True, 
                get_hash_itype1 = True,
                get_hash_itype2 = True)

    if not ok:
        print "Error: %r" % (err)
        return False

    for bb in bm.items():
        # Stats for H1
        h = bb.ctx.hash_itype1
        try:
            v = (bb.id, "%x" % bb.start, "%x" % bb.end)
            L = same_hashes1[h]
            L.append(v)
        except:
            L = [v]
            same_hashes1[h] = L

        # Stats for H2
        h = bb.ctx.hash_itype2
        try:
            v = (bb.id, "%x" % bb.start, "%x" % bb.end)
            L = same_hashes2[h]
            L.append(v)
        except:
            L = [v]
            same_hashes2[h] = L

        if print_out:
            print "[%d] %x (%d); ctx=(bytes_len=%d, hash_itype1=%s hash_itype2=%s)" % (
                        bb.id,
                        bb.start,
                        bb.end - bb.start,
                        len(bb.ctx.bytes),
                        bb.ctx.hash_itype1,
                        bb.ctx.hash_itype2)

        for sid in bb.succs:
            bb0 = bm[sid]
            if print_out:
                print "  SUCC: [%d] %x (%d)" % (bb0.id, bb0.start, bb0.end - bb0.start)

        for pid in bb.preds:
            bb1 = bm[pid]
            if print_out:
                print "  PREDS: [%d] %x (%d)" % (bb1.id, bb1.start, bb1.end - bb1.start)

    # Display stats
    #print_hash_stats(bm, same_hashes1, 'H1) ')
    print_hash_stats(bm, same_hashes2, 'H2) ', min_match=0, min_icount=0)

# ------------------------------------------------------------------------------
main()