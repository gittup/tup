# .ycm_extra_conf.py - Tup commandline generator for YouCompleteMe
#
# Copyright (C) 2021  Mike Shal <marfey@gmail.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# The views and conclusions contained in the software and documentation are those
# of the authors and should not be interpreted as representing official policies,
# either expressed or implied, of the FreeBSD Project.

import os
import subprocess
import json
import shlex

# Get the flags from a command string. We use shlex.split and pull out anything
# that starts with '-' but isn't the standard -c/-o flags.
def get_flags(cmd, directory):
    flags = shlex.split(cmd)
    newflags = []
    for f in flags:
        if f.startswith('-I/'):
            newflags.append(f)
        elif f.startswith('-I'):
            # YCM doesn't seem to work with relative paths, so convert those to
            # full paths based on the working directory of the command
            part = f[2:]
            full_path = os.path.realpath(os.path.join(directory, part))
            newflags.append('-I%s' % full_path)
        elif f.startswith('-') and f != '-c' and f != '-o':
            newflags.append(f)

    return newflags

def Settings(**kwargs):
    filename = kwargs['filename']

    # Tup returns a JSON structure with command/directory/file entries.
    out = subprocess.check_output(['tup', 'commandline', filename], text=True)
    dat = json.loads(out)

    flags = get_flags(dat[0]['command'], dat[0]['directory'])
    return {
        'flags': flags,
    }
