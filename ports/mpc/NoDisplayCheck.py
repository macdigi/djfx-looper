#!/usr/bin/env python3
"""Guard the Native artifact against accidental display/worker dependencies."""
from pathlib import Path
import subprocess
import sys

module = Path(sys.argv[1])
data = module.read_bytes()
for value in [b'/dev/fb', b'/dev/dri', b'/proc/self', b'libdrm',
              b'rd-djfx-native-', b'QuotaScope', b'ScopeTap', b'FrameDisplay',
              b'DrmTargets', b'DJFXNativeScope', b'DJFXScopeCanvas']:
    assert value not in data, 'Native artifact includes retired display code: ' + str(value)
imports = subprocess.check_output(['nm', '-u', str(module)], text=True)
for line in imports.splitlines():
    symbol = line.split()[-1].split('@')[0].lstrip('_')
    assert symbol not in ['ioctl', 'mmap', 'open', 'open64', 'readlink',
                          'opendir', 'pthread_create', 'dlopen'], line
print('PASS: no native renderer, display-device I/O, worker/thread launch or private scope dependency')
