"""Execute the production Substitute decision function with controlled battle inputs."""
import argparse
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', action='store_true', help='Verify that original HEAD fails')
    args = parser.parse_args()
    name = 'src/Battle_AI/ai_advanced.c'
    source = (subprocess.check_output(['git', 'show', '61e1b61653c1c87077860400a995ff3a36b81689:' + name], cwd=ROOT).decode('utf-8')
              if args.baseline else (ROOT / name).read_text(encoding='utf-8'))
    match = re.search(r'static bool8 ShouldUseSubstitute\([^;{]*\)\s*\{', source)
    start, end, depth = match.start(), match.end(), 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    function = source[start:end]
    call = 'ShouldUseSubstitute(0, 1, move)' if 'u16 move' in match[0] else 'ShouldUseSubstitute(0, 1)'
    fixture = '''
typedef unsigned char u8;
typedef unsigned char bool8;
typedef unsigned short u16;
typedef int s32;
#define TRUE 1
#define FALSE 0
#define NULL ((void*)0)
#define MOVE_NONE 0
#define MOVE_SUBSTITUTE 164
#define MOVE_SHEDTAIL 961
#define MathMax(a,b) ((a) > (b) ? (a) : (b))
static struct { u16 hp, maxHP; } gBattleMons[2];
static u16 prediction = 1, seen;
static bool8 incapacitated, first, switching;
static s32 damage;
static u16 IsValidMovePrediction(u8 a, u8 b) { return prediction; }
static bool8 IsBankIncapacitated(u8 a) { return incapacitated; }
static bool8 MoveWouldHitFirst(u16 move, u8 a, u8 b) { seen = move; return first; }
static s32 GetFinalAIMoveDamage(u16 m, u8 a, u8 b, u8 count, void* data) { return damage; }
static bool8 IsPredictedToSwitch(u8 a, u8 b) { return switching; }
''' + function + '''
static int check(u16 move, bool8 expected) {
    seen = 0xFFFF;
    bool8 result = CALL;
    return result != expected || (!incapacitated && prediction != MOVE_NONE && seen != move);
}
int main(void) {
    gBattleMons[0].hp = 100; gBattleMons[0].maxHP = 100;
    incapacitated = 1;
    if (check(MOVE_SUBSTITUTE, TRUE)) return 1;
    incapacitated = 0; prediction = MOVE_NONE;
    if (check(MOVE_SUBSTITUTE, FALSE)) return 1;
    prediction = 1; first = 1; damage = 24;
    if (check(MOVE_SUBSTITUTE, TRUE)) return 1;
    if (check(MOVE_SHEDTAIL, TRUE)) return 1;
    damage = 25;
    if (check(MOVE_SUBSTITUTE, FALSE)) return 1;
    first = 0; damage = 74;
    if (check(MOVE_SUBSTITUTE, TRUE)) return 1;
    damage = 75;
    if (check(MOVE_SUBSTITUTE, FALSE)) return 1;
    switching = 1; damage = 100;
    if (check(MOVE_SUBSTITUTE, TRUE)) return 1;
    return 0;
}
'''.replace('CALL', call)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c_file, obj, exe = (path / name for name in ('ai.c', 'ai.obj', 'ai.exe'))
        c_file.write_text(fixture)
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-stack-protector',
                        '-c', str(c_file), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        result = subprocess.run([str(exe)])
        if args.baseline:
            if result.returncode != 1:
                raise RuntimeError('Original behavior did not reproduce the expected failure')
            print('Original HEAD reproduces incorrect move forwarding.')
        else:
            result.check_returncode()
            print('Production Substitute decision passed move identity and HP/turn-order boundary cases.')


if __name__ == '__main__':
    main()
