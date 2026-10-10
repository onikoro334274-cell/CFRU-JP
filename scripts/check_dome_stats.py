"""Check extracted Dome stats against the standard integer stat formula."""
import argparse
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASELINE = '61e1b61653c1c87077860400a995ff3a36b81689'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    name = 'src/hoenn_dome.c'
    source = (subprocess.check_output(['git', 'show', BASELINE + ':' + name], cwd=ROOT).decode('utf-8')
              if args.baseline else (ROOT / name).read_text(encoding='utf-8'))
    start = source.index('#define CALC_STAT(base, statIndex)')
    end = source.index('static void SwapDomeTrainers(', start)
    function = source[start:end]
    fixture = r'''
typedef unsigned char u8;
typedef unsigned short u16;
#define NUM_STATS 6
#define STAT_HP 0
#define STAT_ATK 1
#define STAT_DEF 2
#define STAT_SPEED 3
#define STAT_SPATK 4
#define STAT_SPDEF 5
#define SPECIES_SHEDINJA 3
#define MAX_TOTAL_EVS 510
struct BaseStats { u8 baseHP, baseAttack, baseDefense, baseSpeed, baseSpAttack, baseSpDefense; }
gBaseStats[4] = {{0}, {50,60,70,80,90,100}, {255,255,255,255,255,255}, {1,90,45,40,30,30}};
static u16 ModifyStatByNature(u8 nature, u16 stat, u8 index) {
    if (nature / 5 == nature % 5) return stat;
    if (index == nature / 5 + 1) return stat * 110 / 100;
    if (index == nature % 5 + 1) return stat * 90 / 100;
    return stat;
}
FUNCTION
int main(void) {
    int stats[NUM_STATS];
#if BASELINE_TEST
    CalcDomeMonStats(1, 50, 31, 3, 0, stats);
    int lostEvs = stats[STAT_HP] == 125 && stats[STAT_ATK] == 80;
    CalcDomeMonStats(2, 100, 31, 63, 0, stats);
    int truncatedStats = stats[STAT_ATK] == ((510 + 31 + 5) & 255);
    return lostEvs && truncatedStats ? 1 : 2;
#else
    for (int species=1; species<4; species++)
    for (int levelId=0; levelId<4; levelId++)
    for (int ivId=0; ivId<2; ivId++)
    for (int mask=0; mask<256; mask++)
    for (int nature=0; nature<25; nature++) {
        int level = levelId == 0 ? 1 : levelId == 1 ? 50 : levelId == 2 ? 100 : 200;
        int ivs = ivId ? 31 : 0;
        int count = 0;
        for (int bit=0; bit<6; bit++) if (mask & (1 << bit)) count++;
        const u8 *base = (const u8 *)&gBaseStats[species];
        CalcDomeMonStats(species, level, ivs, mask, nature, stats);
        for (int stat=0; stat<6; stat++) {
            int ev = count && (mask & (1 << stat)) ? 510 / count : 0;
            int expected = ((2 * base[stat] + ivs + ev / 4) * level) / 100;
            if (stat == STAT_HP) expected = species == SPECIES_SHEDINJA ? 1 : expected + level + 10;
            else {
                expected += 5;
                if (nature / 5 != nature % 5) {
                    if (stat == nature / 5 + 1) expected = expected * 110 / 100;
                    else if (stat == nature % 5 + 1) expected = expected * 90 / 100;
                }
            }
            if (stats[stat] != expected) return 3;
        }
    }
    return 0;
#endif
}
'''.replace('FUNCTION', function).replace('BASELINE_TEST', '1' if args.baseline else '0')
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c_file, obj, exe = (path / name for name in ('stats.c', 'stats.obj', 'stats.exe'))
        c_file.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c_file), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        result = subprocess.run([str(exe)])
        if args.baseline:
            if result.returncode != 1:
                raise RuntimeError('Original EV omission and stat truncation were not reproduced')
            print('Original Dome stats omit EV bonuses and truncate nature-adjusted stats to 8 bits.')
        else:
            result.check_returncode()
            print('Dome stats pass 153600 inputs / 921600 stat comparisons: all masks and natures, levels through 200, IV extremes, Shedinja, and values above 255.')


if __name__ == '__main__':
    main()
