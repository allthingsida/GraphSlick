"""
Basic Block base types module


This module contains the basic types and data structures
* 09/12/2013 - eliasb - Initial version
* 09/16/2013 - eliasb - Renamed RawBB class to BBDef class
                      - Added proper serialization
                      - Added BBDef.size() and context attribute
                      - Added Context attribute and size() to the BBDef class
                      - Added BBMan.clear(), .get_cache()
* 09/17/2013 - eliasb - Added method BBMan.is_using_cache() to see if the cache was used
* 10/07/2013 - eliasb - Use the Caching class from bb_util
                      - Added setitem/getitem helper methods to access the ctx object members of BBDef
* 10/08/2013 - eliasb - bugfix: import from Caching
                      - added BBMan.find_by_addr()
* 10/09/2013 - eliasb - bugfix: find_by_addr()
					  
"""

import pickle
import sys
from bb_utils import Caching

# ------------------------------------------------------------------------------
class BBDef(object):
    """Class to define a raw basic block"""

    def __init__(self, start=0, end=0, id=-1, label="", ctx=None):
        self.start = start
        """Start address of basic block"""

        self.end   = end
        """End address of basic block"""

        self.id    = id
        """The ID of the basic block"""

        self.label = label
        """The label of the basic block"""

        self.preds = list()
        """List of predecessor node IDs"""

        self.succs = list()
        """List of successor node IDs"""

        self.ctx   = ctx
        """Context associated with the BB"""


    def size(self):
        """Return the size of the BB"""
        return self.end - self.start


    def matchp(self, bb):
        """Match two BBs and return percentage of equality"""
        return 0.0


    def add_succ(self, bb, link_pred = False):
        """Add a successor basic block"""
        self.succs.append(bb.id)

        if link_pred:
            bb.add_pred(self, False)

        return self


    def add_pred(self, bb, link_succ = False):
        """Add predecessor basic block"""

        self.preds.append(bb.id)

        if link_succ:
            bb.add_succ(self, False)

        return self

    
    def __getitem__(self, key):
        """Shortcut method to access items in the context class"""
        return self.ctx.__dict__[key]


    def __setitem__(self, key, value):
        """Shortcut method to set item value in the context class"""
        self.ctx.__dict__[key] = value


# ------------------------------------------------------------------------------
class BBMan(object):
    """Class to manage basic blocks"""
    def __init__(self):
        self.__idcache = {}
        self.__lasterr = None
        self.__is_using_cache = False


    def is_using_cache():
        """Was the cache file used"""
        return self.__is_using_cache


    def clear(self):
        """Clear the basic blocks"""
        self.__idcache = {}
        self.__is_using_cache = False

        
    def last_error(self):
        """Return the last error string"""
        return self.__lasterr


    def save(self, filename):
        """Saves the basic blocks to a file (serialize)"""
        ok, self.__lasterr = Caching.Save(filename, self.__idcache)
        return ok


    def load(self, filename):
        """Load the basic blocks from a file (deserialize)"""
        ok, r = Caching.Load(filename)
        if not ok:
            self.__lasterr = r
            self.__is_using_cache = False
            return False

        self.__is_using_cache = True
        self.__idcache = r
        return True


    def add(self, bb):
        """Add a basic block"""
        
        # Cache the basic block by ID
        self.__idcache[bb.id] = bb


    def items(self):
        """Return all the raw basic block objects"""
        return self.__idcache.values()

   
    def __getitem__(self, index):
        try:
            return self.__idcache[index]
        except:
            return None


    def get_cache(self):
        return self.__idcache


    def gen_dot(self, filename):
        """Generate a DOT file"""

        # TODO
        pass


    def find_by_addr(self, addr):
        """Return the basic block that contains the given address"""
        for bb in self.items():
            if bb.start <= addr < bb.end:
                return bb

        return None

    def build_from_id_string(self, conn):
        # Failed to load or no cache was to be used?
        for couples in conn.split(';'):
            n0, n1 = couples.split(':')
            n0     = int(n0)

            bb0 = self[n0]
            if bb0 is None:
                bb0 = BBDef(id=n0)
                self.add(bb0)

            for child in n1.split(','):
                ni  = int(child)
                bbi = self[ni]
                if bbi is None:
                    bbi = BBDef(id=ni)
                    self.add(bbi)
                bb0.add_succ(bbi)

# ------------------------------------------------------------------------------
def __test():
    bm = BBMan()
    fn = 'test.bin'
    if not bm.load(fn):
        conn = "0:1,2,3,4,5;1:3;3:4;2:4"
        bm.build_from_id_string(conn)
        if not bm.save(fn):
            print "Could not save!"
            return

    # Process .....        
    bb0 = bm[0]
    for x in bb0.succs: 
        x = bm[x]
        print x.id, ",",

# ------------------------------------------------------------------------------
def __test2():
    class X(object):
        def __init__(self):
            self.a = 0
            self.b = 0

    b = BBDef(ctx = X())

    print "a=%d" % b['a']
    b['a'] = 1
    b['c'] = 3
    print "a=%d c=%d" % (b['a'], b['c'])


# ------------------------------------------------------------------------------
if __name__ == '__main__':
    __test2()
