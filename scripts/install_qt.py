"""Narrow Qt 6.11 repository-layout compatibility for aqtinstall 3.3.0.

Matches the architecture-suffix handling in upstream aqtinstall metadata.py.
It changes only the official repository subdirectory, not download/hash checks.
"""
import os
import runpy
import sys
from aqt.archives import QtArchives

original=QtArchives._arch_ext
def architecture_suffix(self):
    if self.os_name=='windows' and str(self.version)=='6.11.3' and self.arch=='win64_msvc2022_64':
        return '_msvc2022_64'
    return original(self)
QtArchives._arch_ext=architecture_suffix
sys.argv=['aqt','install-qt','windows','desktop','6.11.3','win64_msvc2022_64','--modules','qtsvg','--outputdir',os.path.join(os.environ['RUNNER_TEMP'],'Qt')]
runpy.run_module('aqt',run_name='__main__')
