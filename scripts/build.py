#!/usr/bin/env python3

from datetime import datetime
from glob import glob
import hashlib
import itertools
import os
from pathlib import Path
import subprocess
import sys
from string_converter import StringFileConverter
from make import ChangeFileLine
from build_dependencies import invalidate_command, needs_rebuild, prerequisites, record_command, record_prerequisites
import platform

on_wsl = "microsoft" in platform.uname()[3].lower()

from toolchain import tool_path

AS = tool_path('as')
CC = tool_path('gcc')
LD = tool_path('ld')
OBJCOPY = tool_path('objcopy')
if sys.platform.startswith('win') or on_wsl:
    GR = 'deps/grit.exe'
    WAV2AGB = 'deps/wav2agb.exe'
    MID2AGB = 'deps/mid2agb.exe'
else:
    GR = 'grit'
    WAV2AGB = 'wav2agb'
    MID2AGB = 'mid2agb'

SRC = './src'
GRAPHICS = './graphics'
ASSEMBLY = './assembly'
STRINGS = './strings'
AUDIO = './audio'
BUILD = './build'
IMAGES = './Images'
ASFLAGS = ['-mthumb', '-I', ASSEMBLY]
LDFLAGS = ['BPRJ.ld', '-T', 'linker.ld']
CFLAGS = ['-mthumb', '-mno-thumb-interwork', '-mcpu=arm7tdmi', '-mtune=arm7tdmi',
          '-mno-long-calls', '-march=armv4t', '-Wall', '-Wextra', '-O2', '-fira-loop-pressure', '-fipa-pta']


class Master:
    @staticmethod
    def init():
        Master.printedCompilingImages = False
        Master.printedCompilingAudio = False
        Master.printedCompilingMusic = False

    @staticmethod
    def printCompilingImages():
        if not Master.printedCompilingImages:
            # Used to tell the script whether or not the string 'Compiling Images' has been printed
            Master.printedCompilingImages = True
            print('Compiling Images')

    @staticmethod
    def printCompilingAudio():
        if not Master.printedCompilingAudio:
            # Used to tell the script whether or not the string 'Compiling Audio' has been printed
            Master.printedCompilingAudio = True
            print('Compiling Audio')

    @staticmethod
    def printCompilingMusic():
        if not Master.printedCompilingMusic:
            # Used to tell the script whether or not the string 'Compiling Music' has been printed
            Master.printedCompilingMusic = True
            print('Compiling Music')


def RunCommand(cmd: [str]):
    """Runs the command line command."""
    try:
        subprocess.check_output(cmd)
    except subprocess.CalledProcessError as e:
        try:
            print(e.output.decode(), file=sys.stderr)
        except:
            print(e)
        sys.exit(1)


def CreateOutputFile(fileName: str, newFileName: str) -> [str, bool]:
    """Helper function to produce object file output."""
    if not os.path.isfile(fileName):
        return [newFileName, False]

    fileExists = os.path.isfile(newFileName)

    # If the object file was created after the file was last modified
    if fileExists and os.path.getmtime(newFileName) > os.path.getmtime(fileName):
        return [newFileName, False]

    return [newFileName, True]


def MakeGeneralOutputFile(fileName: str) -> [str, bool]:
    """Return hash of filename to use as object filename."""
    m = hashlib.md5()
    m.update(fileName.encode())
    newFileName = os.path.join(BUILD, m.hexdigest() + '.o')

    return CreateOutputFile(fileName, newFileName)


def MakeOutputImageFile(assemblyFile: str) -> [str, bool]:
    """Return 'IMG_' + hash of filename to use as object filename."""
    m = hashlib.md5()
    m.update(assemblyFile.encode())
    objectFile = os.path.join(BUILD, 'IMG_' + m.hexdigest() + '.o')

    return CreateOutputFile(assemblyFile, objectFile)


def MakeAssetOutputFile(assemblyFile: str, prefix: str):
    name = hashlib.md5(os.path.normpath(assemblyFile).encode()).hexdigest()
    return CreateOutputFile(assemblyFile, os.path.join(BUILD, prefix + name + '.o'))


def MakeOutputAudioFile(assemblyFile: str) -> [str, bool]:
    return MakeAssetOutputFile(assemblyFile, 'SND_')


def MakeOutputMusicFile(assemblyFile: str) -> [str, bool]:
    return MakeAssetOutputFile(assemblyFile, 'MUS_')


def DoMiddleManAssembly(originalFile: str, assemblyFile: str, flagFile: str, flags: [str],
                        cmd: [str], func, printingFunc, isMusic: bool) -> str:
    """Convert an asset and cache only a successfully assembled result."""
    objectFile = func(assemblyFile)[0]
    assemble = [AS] + ASFLAGS + ['--MD', objectFile + '.d', '-c', assemblyFile, '-o', objectFile]
    signature = cmd + ['--asset-assembly--'] + assemble + [flagFile, str(isMusic)]
    if not needs_rebuild(originalFile, objectFile, signature):
        return objectFile

    printingFunc()
    invalidate_command(objectFile)
    RunCommand(cmd)
    if isMusic and '-G' in flags:
        index = flags.index('-G')
        if index + 1 >= len(flags):
            raise ValueError('Music -G flag requires a voicegroup name')
        with open(assemblyFile, 'r') as file:
            for counter, line in enumerate(file, 1):
                if '_grp,' in line:
                    ChangeFileLine(assemblyFile, counter, line.split('voicegroup')[0] + flags[index + 1] + '\n')
                    break

    RunCommand(assemble)
    dependencies = [name for name in prerequisites(objectFile + '.d')
                    if Path(name).resolve() != Path(assemblyFile).resolve()]
    dependencies += [originalFile, __file__]
    if flagFile:
        dependencies.append(flagFile)
    record_prerequisites(objectFile, dependencies)
    record_command(objectFile, signature)
    os.remove(assemblyFile)
    return objectFile


def ProcessAssembly(assemblyFile: str) -> str:
    """Assemble."""
    objectFile, regenerateObjectFile = MakeGeneralOutputFile(assemblyFile)
    cmd = [AS] + ASFLAGS + ['--MD', objectFile + '.d', '-c', assemblyFile, '-o', objectFile]
    if not needs_rebuild(assemblyFile, objectFile, cmd):
        return objectFile  # No point in recompiling file

    try:
        print('Assembling %s' % assemblyFile)
        invalidate_command(objectFile)
        RunCommand(cmd)
        record_command(objectFile, cmd)

    except FileNotFoundError:
        print('Error! The assembler could not be located.\n'
              + 'Are you sure you set up your path to devkitPro/devkitARM/bin correctly?')
        sys.exit(1)

    return objectFile


def ProcessC(cFile: str) -> str:
    """Compile C."""
    objectFile, regenerateObjectFile = MakeGeneralOutputFile(cFile)
    cmd = [CC] + CFLAGS + ['-MMD', '-MF', objectFile + '.d', '-c', cFile, '-o', objectFile]
    if not needs_rebuild(cFile, objectFile, cmd):
        return objectFile  # No point in recompiling file

    try:
        print('Compiling %s' % cFile)
        invalidate_command(objectFile)
        RunCommand(cmd)
        record_command(objectFile, cmd)

    except FileNotFoundError:
        print('Error! The C compiler could not be located.\n'
              + 'Are you sure you set up your path to devkitPro/devkitARM/bin correctly?')
        sys.exit(1)

    return objectFile


def ProcessString(stringFile: str) -> str:
    """Build and assemble strings."""
    assemblyFile = stringFile.split('.string')[0] + '.s'
    objectFile = MakeGeneralOutputFile(assemblyFile)[0]
    cmd = [AS] + ASFLAGS + ['-c', assemblyFile, '-o', objectFile]

    if not needs_rebuild(stringFile, objectFile, cmd):
        # If the .o file was created after the string file was last modified
        return objectFile

    print('Building Strings %s' % stringFile)
    invalidate_command(objectFile)
    dependencies = StringFileConverter(stringFile)

    RunCommand(cmd)
    record_prerequisites(objectFile, dependencies)
    record_command(objectFile, cmd)
    os.remove(assemblyFile)
    return objectFile


def ProcessImage(imageFile: str) -> str:
    """Compile image."""
    if '.bmp' in imageFile:
        assemblyFile = imageFile.split('.bmp')[0] + '.s'
    else:
        assemblyFile = imageFile.split('.png')[0] + '.s'

    if sys.platform.startswith('win'):
        nameList = imageFile.split('\\')  # Get path of grit flags
        nameList.pop(len(nameList) - 1)
        flagFile = ''.join(str(i) + '\\' for i in nameList)
    else:  # Linux, OSX, etc.
        nameList = imageFile.split('/')  # Get path of grit flags
        nameList.pop(len(nameList) - 1)
        flagFile = ''.join(str(i) + '//' for i in nameList)

    flagFile += 'gritflags.txt'

    try:
        with open(flagFile, 'r') as file:
            line = file.readline()  # Only needs the first line
            flags = line.strip().split()
            cmd = [GR, imageFile] + flags + ['-o', assemblyFile]

    except FileNotFoundError:
        print('Error: No gritflags.txt found in directory with ' + imageFile + '.')
        sys.exit(1)

    return DoMiddleManAssembly(imageFile, assemblyFile, flagFile, flags, cmd,
                               MakeOutputImageFile, Master.printCompilingImages, False)


def ProcessAudio(audioFile: str) -> str:
    """Compile audio."""
    assemblyFile = audioFile.split('.wav')[0] + '.s'

    flags = []
    flagFile = audioFile.split('.wav')[0] + '_flags.txt'

    try:
        with open(flagFile, 'r') as file:
            line = file.readline()  # Only needs the first line
            flags = line.strip().split()
    except FileNotFoundError:
        flagFile = ""

    cmd = [WAV2AGB, audioFile, assemblyFile] + flags

    return DoMiddleManAssembly(audioFile, assemblyFile, flagFile, flags, cmd,
                               MakeOutputAudioFile, Master.printCompilingAudio, False)


def ProcessMusic(midiFile: str) -> str:
    """Compile audio."""
    assemblyFile = midiFile.split('.mid')[0] + '.s'

    flags = []
    flagFile = midiFile.split('.mid')[0] + '_flags.txt'

    try:
        with open(flagFile, 'r') as file:
            line = file.readline()  # Only needs the first line
            flags = line.strip().split()
    except FileNotFoundError:
        pass

    cmd = [MID2AGB, midiFile, assemblyFile] + flags

    return DoMiddleManAssembly(midiFile, assemblyFile, flagFile, flags, cmd,
                               MakeOutputMusicFile, Master.printCompilingMusic, True)


def LinkObjects(objects: itertools.chain) -> str:
    """Link objects into one binary."""
    linked = 'build/linked.o'

    obj_list_path = 'object_files.txt'
    with open(obj_list_path, 'w', encoding='utf-8') as f:
        for obj in objects:
            f.write(obj.replace("\\", "/") + '\n')

    cmd = [LD] + LDFLAGS + ['-o', linked, f'@{obj_list_path}']
    RunCommand(cmd)
    return linked


def Objcopy(binary: str):
    """Run the objcopy."""
    cmd = [OBJCOPY, '-O', 'binary', binary, 'build/output.bin']
    RunCommand(cmd)


def RunGlob(globString: str, fn) -> map:
    """Glob recursively and run the processor function on each file in result."""
    if globString == '**/*.png' or globString == '**/*.bmp':  # Search the GRAPHICS location
        directory = GRAPHICS
    elif globString == '**/*.s':
        directory = ASSEMBLY
    elif globString == '**/*.string':
        directory = STRINGS
    elif globString == '**/*.wav' or globString == '**/*.mid':
        directory = AUDIO
    else:
        directory = SRC

    if sys.version_info > (3, 4):
        try:
            files = glob(os.path.join(directory, globString), recursive=True)
            return map(fn, files)

        except TypeError:
            print('Error compiling. Please make sure Python has been updated to the latest version.')
            sys.exit(1)
    else:
        files = Path(directory).glob(globString)
        return map(fn, map(str, files))


def main():
    Master.init()
    startTime = datetime.now()
    globs = {
            '**/*.s': ProcessAssembly,
            '**/*.c': ProcessC,
            '**/*.string': ProcessString,
            '**/*.png': ProcessImage,
            '**/*.bmp': ProcessImage,
            '**/*.wav': ProcessAudio,
            '**/*.mid': ProcessMusic,
    }

    if sys.version_info.major >= 3 and sys.version_info.minor >= 8:
        print("Warning! Python 3.8 may not be able to build this engine.\nPlease downgrade to Python 3.7.4")

    # Create output directory
    try:
        os.makedirs(BUILD)
    except FileExistsError:
        pass

    try:
        # Gather source files and process them
        objects = itertools.starmap(RunGlob, globs.items())

        # Link and extract raw binary
        linked = LinkObjects(itertools.chain.from_iterable(objects))
        Objcopy(linked)

    except Exception as e:
        print("There was an error compiling the engine: {}".format(e))
        sys.exit(1)

    # Build special_inserts.asm
    if os.path.isfile('special_inserts.asm'):
        if not os.path.isfile('build/special_inserts.bin') \
                or os.path.getmtime('build/special_inserts.bin') < os.path.getmtime('special_inserts.asm'):
            print('Assembling special_inserts.asm')
            cmd = [AS] + ASFLAGS + ['-c', 'special_inserts.asm', '-o', 'build/special_inserts.o']
            RunCommand(cmd)

            cmd = [OBJCOPY, '-O', 'binary', 'build/special_inserts.o', 'build/special_inserts.bin']
            RunCommand(cmd)

    print('Built in ' + str(datetime.now() - startTime) + '.')


if __name__ == '__main__':
    main()
