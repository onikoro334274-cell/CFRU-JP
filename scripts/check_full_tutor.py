"""Execute production tutor filtering with constrained heap and page boundaries."""

import argparse
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', choices=('memory', 'page'))
    args = parser.parse_args()
    source = (subprocess.check_output(['git', 'show', '61e1b61653c1c87077860400a995ff3a36b81689:src/hack_full_tutor.c'], cwd=ROOT).decode('utf-8')
              if args.baseline else (ROOT / 'src/hack_full_tutor.c').read_text(encoding='utf-8'))
    functions = []
    for name in ('FullTutorAllMoves', 'HackFullTutor_GetMoves', 'HackFullTutor_CountPages'):
        match = re.search(r'(?:static )?(?:u16|u8|void) ' + name + r'\([^;{}]*\)\s*\{', source)
        end, depth = match.end(), 1
        while depth:
            depth += (source[end] == '{') - (source[end] == '}')
            end += 1
        functions.append(source[match.start():end])
    original = subprocess.check_output(['git', 'show', '61e1b61653c1c87077860400a995ff3a36b81689:src/hack_full_tutor.c'], cwd=ROOT).decode('utf-8')
    match = re.search(r'static u16 FullTutorAllMoves\([^;{}]*\)\s*\{', original)
    end, depth = match.end(), 1
    while depth:
        depth += (original[end] == '{') - (original[end] == '}')
        end += 1
    functions.append(original[match.start():end].replace('FullTutorAllMoves', 'OriginalFullTutorAllMoves'))
    fixture = r'''
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
#define NULL ((void*)0)
#define FULL_TUTOR_BUFFER 64
#define MAX_MON_MOVES 4
#define MAX_LEARNABLE_MOVES 40
#define MOVE_NONE 0
#define MON_DATA_MOVE1 0
#define HACK_VAR_FULL_TUTOR_PAGE 0x4009
#define PARTY_SIZE 6
struct Pokemon { u16 moves[4]; };
static struct Pokemon gPlayerParty[6];
static u16 Var8004, gSpecialVar_LastResult, page;
static u16 heap[3][64];
static u32 allocations, frees, allowance, largeList;
static u16 custom[64], customCount;
static u8 useCustom;
void *Malloc(u32 size) {
    ++allocations;
    if (allocations > allowance || allocations > 3 || size != sizeof(heap[0])) return NULL;
    return heap[allocations - 1];
}
void Free(void *ptr) { ++frees; }
void Memset(void *dst, u32 value, u32 size) {
    volatile u8 *p = dst;
    for (u32 i = 0; i < size; ++i) p[i] = value;
}
u16 VarGet(u16 id) { return page; }
u16 GetMonData(struct Pokemon *mon, u32 field, void *dst) { return mon->moves[field]; }
u16 BuildLearnableMoveset(struct Pokemon *mon, u16 *out) {
    if (useCustom) {
        for (u16 i = 0; i < customCount; ++i) out[i] = custom[i];
        return customCount;
    }
    if (largeList) {
        for (u16 i = 0; i < 60; ++i) out[i] = i + 1;
        return 60;
    }
    const u16 list[] = {1, 2, 1, 3, 4, 2, 5, 0};
    for (u16 i = 0; i < 8; ++i) out[i] = list[i];
    return 8;
}
''' + '\n'.join(functions) + '\n#define PAGE_BASELINE ' + str(int(args.baseline == 'page')) + r'''
int main(void) {
    u16 out[40];
    struct Pokemon mon = {{2, 5, 0, 0}};
    allowance = 1;
    if (!PAGE_BASELINE) {
        if (HackFullTutor_GetMoves(&mon, out) != 3 || out[0] != 1 || out[1] != 3 || out[2] != 4)
            return 1;
        if (allocations != 1 || frees != 1) return 2;
        allocations = frees = 0; allowance = 0;
        out[0] = 999;
        if (HackFullTutor_GetMoves(&mon, out) || out[0] != 999 || frees) return 3;
    }
    allocations = frees = 0; allowance = 3; largeList = 1;
    page = 1639; mon.moves[0] = mon.moves[1] = 0; out[0] = 999;
    if (HackFullTutor_GetMoves(&mon, out) || out[0] != 999) return 4;
    allocations = frees = 0; page = 1;
    if (HackFullTutor_GetMoves(&mon, out) != 20 || out[0] != 41 || out[19] != 60) return 5;
    allocations = frees = 0; allowance = 1;
    HackFullTutor_CountPages();
    if (gSpecialVar_LastResult != 2 || allocations != 1 || frees != 1) return 6;
    allocations = frees = 0; Var8004 = 6;
    HackFullTutor_CountPages();
    if (gSpecialVar_LastResult || allocations || frees) return 7;
    useCustom = 1; allowance = 3;
    u32 seed = 1234567;
    for (u32 trial = 0; trial < 1000; ++trial) {
        u16 current[64], original[64];
        seed = seed * 1664525u + 1013904223u;
        customCount = seed % 65;
        for (u16 i = 0; i < customCount; ++i) {
            seed = seed * 1664525u + 1013904223u;
            custom[i] = seed % 32;
        }
        for (u16 i = 0; i < 4; ++i) {
            seed = seed * 1664525u + 1013904223u;
            mon.moves[i] = seed % 32;
        }
        allocations = frees = 0;
        u16 actual = FullTutorAllMoves(&mon, current);
        u16 expected = OriginalFullTutorAllMoves(&mon, original);
        if (actual != expected) return 8;
        for (u16 i = 0; i < actual; ++i)
            if (current[i] != original[i]) return 9;
    }
    return 0;
}
'''
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c_file, obj, exe = (path / name for name in ('tutor.c', 'tutor.obj', 'tutor.exe'))
        c_file.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c_file), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        result = subprocess.run([str(exe)])
        if args.baseline:
            expected = 1 if args.baseline == 'memory' else 4
            if result.returncode != expected:
                raise RuntimeError('Original tutor failure was not reproduced')
            print('Original HEAD tutor failure reproduced: ' + args.baseline)
        else:
            result.check_returncode()
            print('Production tutor passed one-buffer heap, filtering, allocation failure, '
                  'page overflow/boundaries, page count, party index and 1000 original-function comparisons.')


if __name__ == '__main__':
    main()
