"""Locate ARM tools without launching them when a module is imported."""
import os
import shutil
from pathlib import Path


def tool_path(name):
    command = 'arm-none-eabi-' + name
    found = shutil.which(command)
    if found:
        return found
    locations = []
    if os.environ.get('DEVKITARM'):
        locations.append(Path(os.environ['DEVKITARM']) / 'bin')
    if os.name == 'nt':
        locations.append(Path('C:/devkitPro/devkitARM/bin'))
    for location in locations:
        found = shutil.which(command, path=str(location))
        if found:
            return found
    return command
