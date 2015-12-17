# This script can be imported from a python build script to access the
# tup.config variables.
#
# Usage:
#
# import tup_client
# value = tup_client.config_var('FOO')
#
# This will store the value of CONFIG_FOO into 'value'. If the variable does
# not exist, 'value' will be None. In either case, a dependency will be created
# on the @(FOO) variable.
import os
import mmap
import struct
import string

tup_vardict_map = None
tup_vardict_num = None
tup_entry_size = 0

def config_read_byte(vardict_map):
    p = tup_vardict_map.read_byte()
    if(type(p) == int):
        return chr(p)
    return p

def config_var(key):
    global tup_vardict_map
    global tup_vardict_num
    global tup_entry_size

    try:
        f = open("@tup@/" + key, "r");
    except IOError:
        pass

    if tup_vardict_num is None:
        path = os.getenv('tup_vardict');
        if path is None:
            raise Exception('tup error: This python module can only be used as a sub-process from tup')

        if os.path.exists(path):
            with open(path) as f:
                tup_vardict_map = mmap.mmap(f.fileno(), 0, mmap.MAP_PRIVATE, mmap.PROT_READ);
                tup_vardict_num = struct.unpack("@i", tup_vardict_map.read(4))[0];
                tup_entry_size = (tup_vardict_num + 1) * 4
        else:
            tup_vardict_num = 0

    left = -1
    right = tup_vardict_num

    while(True):
        cur = int((right - left) / 2)
        if(cur <= 0):
            return None
        cur += left
        if(cur >= tup_vardict_num):
            return None
        # +1 for the number of entries at the start of the file
        tup_vardict_map.seek((cur+1) * 4)
        offset = struct.unpack("@i", tup_vardict_map.read(4))[0];
        tup_vardict_map.seek(offset + tup_entry_size)
        keylen = len(key)
        bytesleft = keylen

        p = '\0'
        while(bytesleft > 0):
            p = config_read_byte(tup_vardict_map)
            k = key[keylen - bytesleft]
            if(p == '=' or p < k):
                left = cur
                break
            elif(p > k):
                right = cur
                break
            bytesleft -= 1

        if(bytesleft == 0):
            p = config_read_byte(tup_vardict_map)
            if(p == '='):
                rc = []
                while(1):
                    p = config_read_byte(tup_vardict_map)
                    if(p == '\0'):
                        return "".join(rc)
                    rc.append(p)
            else:
                right = cur
