"""
Utility class

This module contains the basic types and data structures

* 09/17/2013 - eliasb - Added the GenPrimes() utility function
* 10/07/2013 - eliasb - Added the Caching class
                      - Added Cached prime generator
* 10/15/2013 - eliasb - Added RekeyDictionary()

"""

import pickle
import sys

# ------------------------------------------------------------------------------
class Caching(object):
    @staticmethod
    def Save(filename, obj):
        """Serializes an object to a file"""
        try:
            f = open(filename, 'wb')
            pickle.dump(obj, f, protocol=pickle.HIGHEST_PROTOCOL)
            f.close()
            return (True, None)
        except:
            return (False, sys.exc_info()[0])

    @staticmethod
    def Load(filename):
        """Deserializes an object from a file"""
        try:
            f = open(filename, 'rb')
            ret = pickle.load(f)
            f.close()
            return (True, ret)
        except:
            return (False, sys.exc_info()[0])


# ------------------------------------------------------------------------------
def RekeyDictionary(d):
    """
    Reassigns simple numeric keys to a dictionary
    """

    # New mapping
    rkd = {}
    # New key numbering
    nk  = 0
    for k in d:
        rkd[nk] = d[k]
        nk += 1

    return rkd


# ------------------------------------------------------------------------------
def GenPrimes():
    """
    Generate an infinite sequence of prime numbers.
    http://stackoverflow.com/questions/567222/simple-prime-generator-in-python
    """
    # Maps composites to primes witnessing their compositeness.
    # This is memory efficient, as the sieve is not "run forward"
    # indefinitely, but only as long as required by the current
    # number being tested.
    #
    D = {}  

    # The running integer that's checked for primeness
    q = 2  

    while True:
        if q not in D:
            # q is a new prime.
            # Yield it and mark its first multiple that isn't
            # already marked in previous iterations
            # 
            yield q        
            D[q * q] = [q]
        else:
            # q is composite. D[q] is the list of primes that
            # divide it. Since we've reached q, we no longer
            # need it in the map, but we'll mark the next 
            # multiples of its witnesses to prepare for larger
            # numbers
            # 
            for p in D[q]:
                D.setdefault(p + q, []).append(p)
            del D[q]

        q += 1


# ------------------------------------------------------------------------------
class CachedPrimes(object):
    def __init__(self, count, fn_template = 'Primes%08d.cache'):
        # Format the file name
        fn = fn_template % count

        # Try to load the cache file
        ok, self.__primes = Caching.Load(fn)

        # No cache file?
        if not ok:
            # Pre-generate numbers
            L = []
            for p in GenPrimes():
                L.append(p)
                if count <= 1:
                    break
                # Decrement count
                count = count - 1

            # Save the pregenerated primes
            self.__primes = L

            # Cache them as well
            Caching.Save(fn, L)

    def __getitem__(self, key):
        return self.__primes[key]

    def __iter__(self):
        return iter(self.__primes)

    def __len__(self):
        return len(self.__primes)


# ------------------------------------------------------------------------------
if __name__ == '__main__':
    cp = CachedPrimes(5001)
    print "L=%d x2=%d" % (len(cp), cp[2])
    for x in xrange(5, 10):
        print "\t->", cp[x]

    for x in cp:
        print x

    pass