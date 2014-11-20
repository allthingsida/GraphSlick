"""
This module contains IDA specific basic block datatypes and helper functions


09/12/2013 - eliasb - Initial version
09/16/2013 - eliasb - Added IDABBman subclass of BBMan
                    - Added hash_itype1() and IDABBContext() class
10/07/2013 - eliasb - Added hash2 algorithm
10/08/2013 - eliasb - Refactored code and fixed some bugs
                    - Fixed a bug in hashing bounds
                    - Added operand type into consideration in hash_itype2()
                    - Added utility HashNode()
                    - Added InstructionCount to IDABBContext structure
10/15/2013 - eliasb - Added get_cmd_prime_characteristics() and use it in hash_itype2()
                    - Added get_block_frequency()

10/16/2013 - eliasb - Finished match_block_frequencies()
                    - get_block_frequency() now returns the total instruction count as well
                    - Block frequency table keys are now rekeyed with RekeyDictionary()
10/25/2013 - eliasb - Integrated Ali's changes:
                      - Made Rekeying optional and off by default (since it will mess up relative comparison)
					  - Enforce division by floats where needed
					  - Avoid division by zero

TODO:
------

"""

stdalone = False
"""Desginates whether this module is running inside IDA or in stand alone mode"""

# ------------------------------------------------------------------------------
try:
    import idaapi
    import idautils
    import hashlib

    from idaapi import UA_MAXOP, o_last, o_void
except:
    stdalone = True

    UA_MAXOP  = 6
    o_last    = 14
    o_void    = 0

from   bb_types import *
import bb_utils

# ------------------------------------------------------------------------------
def _get_cache_filename(addr):
    # Form the cache file name
    return "%016X.cache" % addr


# ------------------------------------------------------------------------------
def InstructionCount(start, end):
    """Count the number of instructions"""
    icount = 0
    while start < end:
        cmd = idautils.DecodeInstruction(start)
        if cmd is None:
            break
        icount += 1
        start  += cmd.size

    return icount


# ------------------------------------------------------------------------------
def get_cmd_prime_characteristics(cmd):
    """
    Compute the characteristics of an instruction and return a prime number
    """

    # Compute the itype's prime number
    r = _CachedPrimes[cmd.itype]

    # Compute operands prime number
    ro = 1
    for op in cmd.Operands:
        if op.type == o_void:
            break

        # Take a unique prime from the operands prime offset for the operand type and index
        ro = ro * _CachedPrimes[_OP_P_OFFS + ((op.n * o_last) + op.type)]

    return r * ro


# ------------------------------------------------------------------------------
def hash_itype1(start, end):
    """Hash a block based on the instruction sequence"""
    sh  = hashlib.sha1()
    buf = []    
    while start < end:
        cmd = idautils.DecodeInstruction(start)
        if cmd is None:
            break

        buf.append(str(cmd.itype))
        start += cmd.size

    sh.update("".join(buf))

    return sh.hexdigest()


# ------------------------------------------------------------------------------
def hash_itype2(start, end):
    """
    Hash a block based on the instruction sequence.
    Take into consideration the operands
    """
    sh  = hashlib.sha1()
    r = 1
    while start < end:
        cmd = idautils.DecodeInstruction(start)
        if cmd is None:
            break

        r = r * get_cmd_prime_characteristics(cmd)

        # Advance decoder
        start += cmd.size

    sh.update(str(r))
    return sh.hexdigest()


# ------------------------------------------------------------------------------
def get_block_frequency(start, end, rekey=False):
    """
    Compute a table of instruction characteristic freqency
    Returns a tuple containing the total instruction count and freqency table
    """
    d  = {}
    t  = 0
    while start < end:
        # Decode the instruction
        cmd = idautils.DecodeInstruction(start)
        if cmd is None:
            break

        # Get the prime characteristics
        r = get_cmd_prime_characteristics(cmd)

        # See if this characteristic has been seen already
        try:
            # Yes, take its frequency count
            v = d[r]
        except:
            # No, zero frequency
            v = 0

        # Add up the frequency count
        d[r] = v + 1

        # Add the instruction count
        t += 1

        # Advance decoder
        start += cmd.size

    # Return a simpler dictionary
	if rekey:
		d = bb_utils.RekeyDictionary(d)
    
    return (t, d)


# ------------------------------------------------------------------------------
def match_block_frequencies(ft1, ft2, p1, p2):
    """
    Compute the percentage of match between two frequency tables
    @param p1: ...
    @param p2: ...
    """

    # Extract the total and frequency tables from the parameters
    t1, f1 = ft1
    t2, f2 = ft2

    # Identify big and small frequency tables
    if len(f1) > len(f2):
        fs = f2
        fb = f1
    else:
        fs = f1
        fb = f2

    # Clear common percentage
    tp  = 0
    comm_count  = 0
    total_count = 0
    ct1 = ct2 = 0
    
    # Walk the dictionary looking for common characteristics
    for k in fs:
        # Is this key common to both tables?
        if k not in fb:
            continue

        comm_count += 1

        # Get the values
        v1, v2 = fs[k], fb[k]

        # Add the common instructions that have different frequencies
        # in f1 and f2
        ct1 += v1
        ct2 += v2

        # Get the percentage
        v      = float((min(v1, v2)) * 100) / float(max(v1, v2))

        # Accumulate the result to the total percentage
        tp     = tp + v

    # Compute how much the common match in each frequency table
    cp1 = (100 * ct1) / float(t1)
    cp2 = (100 * ct2) / float(t2)

    ok1 = (cp1 > p1) and (cp2 > p1)

    # Compute the percent of the common match
    if comm_count == 0:
        ok2 = False
    else:   
        mp2 = tp / comm_count
        ok2 = mp2 > p2

    return (ok1, ok2)


# ------------------------------------------------------------------------------
class IdaBBContext(object):
    """IDA Basic block context class"""

    def __init__(self):

        self.bytes = None
        """The bytes of the block"""

        self.hash_itype1 = None
        """Hash on itype v1"""

        self.hash_itype2 = None
        """Hash on itype v2"""

        self.inst_count = 0
        """Instruction count"""


    def get_context(
            self,
            bb,
            bytes=True, 
            itype1=True, 
            itype2=False,
            icount=True):
        """Compute the context of a basic block"""
        # Get the bytes
        if bytes:
            self.bytes = idaapi.get_many_bytes(bb.start, bb.end - bb.start)

        # Count instructions
        if icount:
            self.inst_count = InstructionCount(bb.start, bb.end)
        
        # Get the itype1 hash
        if itype1:
            self.hash_itype1 = hash_itype1(bb.start, bb.end)

        # Get the itype2 hash
        if itype2:
            self.hash_itype2 = hash_itype2(bb.start, bb.end)


# ------------------------------------------------------------------------------
class IDABBMan(BBMan):
    def __init__(self):
        BBMan.__init__(self)

    def add_bb_ctx(
            self, 
            bb, 
            get_bytes, 
            get_hash_itype1,
            get_hash_itype2):
        """Add a basic block to the manager with its context computed"""
    
        # Create the context object
        ctx = IdaBBContext()

        # Compute context
        ctx.get_context(
             bb, 
             get_bytes, 
             get_hash_itype1, 
             get_hash_itype2)

        # Assign context to the basic block object
        bb.ctx = ctx

        # Remember this basic block
        self.add(bb)


    def FromFlowchart(
            self, 
            func_addr, 
            use_cache = False, 
            get_bytes = False, 
            get_hash_itype1 = False,
            get_hash_itype2 = False):
        """
        Build a BasicBlock manager object from a function address
        """
       
        # Use cache?
        if use_cache:
            # Try to load cached items
            if self.load(_get_cache_filename(func_addr)):
                # Loaded successfully, return to caller...
                return (True, self)

        # Is it possible to operate in standalone mode?
        if stdalone:
            return (False, "Cannot compute IDA basic blocks in standalone mode!")

        # Get the IDA function object
        fnc = idaapi.get_func(func_addr)
        if fnc is None:
            return (False, "No function at %x" % func_addr)

        # Update function address to point to the start of the function
        func_addr = fnc.startEA

        # Main IDA BB loop
        fc = idaapi.FlowChart(fnc)
        for block in fc:
            # Try to get BB reference
            bb = self[block.id]
            if bb is None:
                bb = BBDef(id=block.id, 
                           start=block.startEA, 
                           end=block.endEA)
                
                # Add the current block
                self.add_bb_ctx(
                        bb, 
                        get_bytes, 
                        get_hash_itype1,
                        get_hash_itype2)

            # Add all successors
            for succ_block in block.succs():
                # Is the successor block already present?
                b0 = self[succ_block.id]
                if b0 is None:
                    b0 = BBDef(id=succ_block.id, 
                               start=succ_block.startEA, 
                               end=succ_block.endEA)

                    # Add successor
                    self.add_bb_ctx(
                            b0, 
                            get_bytes, 
                            get_hash_itype1, 
                            get_hash_itype2)

                # Link successor
                bb.add_succ(b0, link_pred = True)
                    
            # Add all predecessors
            for pred_block in block.preds():
                # Is the successor block already present?
                b0 = self[pred_block.id]
                if b0 is None:
                    b0 = BBDef(id=pred_block.id, 
                               start=pred_block.startEA, 
                               end=pred_block.endEA)
                    # Add predecessor
                    self.add_bb_ctx(
                            b0, 
                            get_bytes, 
                            get_hash_itype1,
                            get_hash_itype2)

                # Link predecessor
                bb.add_pred(b0, link_succ = True)

        # Save nodes if cache is enabled
        if use_cache:
            self.save(_get_cache_filename(func_addr))

        return (True, self)

# ------------------------------------------------------------------------------
def BuildBBFromIdaFlowchart(func_addr, use_cache=True):
    bm = IDABBMan()
    ok = bm.FromFlowchart(
                func_addr, 
                use_cache = use_cache, 
                get_bytes = not stdalone,
                get_hash_itype1 = not stdalone,
                get_hash_itype2 = not stdalone)


# ------------------------------------------------------------------------------
def HashNode(bm, node_id, hash_id=1):
    """
    Hash a node
    """
    bb = bm[node_id]
    if hash_id == 1:
        r = hash_itype1(bb.start, bb.end)
    elif hash_id == 2:
        r = hash_itype2(bb.start, bb.end)
    else:
        r = "unknown hash_id %d" % hash_id
    print "->%s" % r

# ------------------------------------------------------------------------------
# Precompute lots of prime numbers. This should be more than the count of 
# instructions for most processor modules in IDA

# Total prime numbers
_MAX_PRIMES   = 8117
# Prime number offsets offset in the primes pool:
#     We have UA_MAXOP operands, each may have 'o_last' operand types.
#     For each operand type we have to assure a unique prime number
_OP_P_OFFS    = _MAX_PRIMES - (UA_MAXOP * (o_last+1))

# Precompute primes
_CachedPrimes = bb_utils.CachedPrimes(_MAX_PRIMES)

# ------------------------------------------------------------------------------
if __name__ == '__main__':
    ft1 = (7, {21614129: 5, 4790013691321L: 1, 722682555311L: 1})
    ft2 = (3, {21614129: 1, 4790013691321L: 1, 722682555311L: 1})

    match_block_frequencies(ft1, ft2, 90, 90)
