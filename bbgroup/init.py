"""
BB matcher initialization script.

This script ensures that it runs once per Python runtime.


11/07/2013 - eliasb - Initial version
"""

import os
import sys

# Get the script path
script_path = os.path.dirname(__file__)

# Run this code once by checking if the required scripts
# are in the path
if script_path not in sys.path:
    sys.path.append(script_path)

    # Import the matcher module
    import bb_match

    #print "Imported"
else:
    #print "Already imported"
    pass