"""Compare production bag-page arrangement with and without temporary memory."""

import argparse
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    name = 'src/hack_training_pouch.c'
    source = (subprocess.check_output(['git', 'show', '61e1b61653c1c87077860400a995ff3a36b81689:' + name], cwd=ROOT).decode('utf-8')
              if args.baseline else (ROOT / name).read_text(encoding='utf-8'))
    match = re.search(r'u16 HackTrainingPouch_Arrange\([^;{}]*\)\s*\{', source)
    end, depth = match.end(), 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    production = source[match.start():end]
    fixture = r'''
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
#define NULL ((void*)0)
#define ITEM_NONE 0
#define ITEMS_COUNT 32
struct ItemSlot { u16 itemId, quantity; };
static struct ItemSlot temporary[64];
static u8 failAllocation, page;
static u32 frees;
void *Malloc(u32 size) { return failAllocation ? NULL : temporary; }
void Free(void *ptr) { ++frees; }
u8 CurrentPage(void) { return page; }
u8 HackBagPage_Of(u16 id) { return id % 6; }
''' + production + r'''
int main(void) {
    struct ItemSlot simple[4] = {{1, 10}, {2, 20}, {0, 0}, {3, 30}};
    page = 1; failAllocation = 1;
    if (HackTrainingPouch_Arrange(simple, 4) != 1) return 1;
    if (simple[0].itemId != 1 || simple[1].itemId != 2 || simple[2].itemId != 3
        || simple[3].itemId || simple[3].quantity || frees) return 2;
    u32 seed = 9876543;
    for (u32 trial = 0; trial < 1000; ++trial) {
        struct ItemSlot normal[64], fallback[64];
        seed = seed * 1664525u + 1013904223u;
        u16 capacity = seed % 65;
        page = (seed >> 12) % 6;
        for (u32 i = 0; i < capacity; ++i) {
            seed = seed * 1664525u + 1013904223u;
            normal[i].itemId = seed % 40;
            normal[i].quantity = seed >> 16;
            fallback[i] = normal[i];
        }
        failAllocation = 0; frees = 0;
        u16 expected = HackTrainingPouch_Arrange(normal, capacity);
        if (frees != 1) return 3;
        failAllocation = 1; frees = 0;
        u16 actual = HackTrainingPouch_Arrange(fallback, capacity);
        if (actual != expected || frees) return 4;
        for (u32 i = 0; i < capacity; ++i)
            if (normal[i].itemId != fallback[i].itemId || normal[i].quantity != fallback[i].quantity)
                return 5;
        for (u32 i = 0; i < actual; ++i)
            if (!fallback[i].itemId || HackBagPage_Of(fallback[i].itemId) != page) return 6;
    }
    return 0;
}
'''
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c_file, obj, exe = (path / name for name in ('pouch.c', 'pouch.obj', 'pouch.exe'))
        c_file.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c_file), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        result = subprocess.run([str(exe)])
        if args.baseline:
            if result.returncode != 1:
                raise RuntimeError('Original unfiltered page failure was not reproduced')
            print('Original HEAD allocation failure reports items outside the selected bag page.')
        else:
            result.check_returncode()
            print('Production pouch allocation-failure fallback matches normal output and item '
                  'quantities/order for 1000 cases including holes, invalid IDs and empty capacity.')


if __name__ == '__main__':
    main()
