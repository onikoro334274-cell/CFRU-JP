"""Exercise production Dome round decisions, including absent opponents."""
import argparse
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASELINE = '61e1b61653c1c87077860400a995ff3a36b81689'


def function(source, name):
    start = source.index('static void ' + name + '(u8 roundId)\n{')
    end = source.index('\nstatic void CopyDomeTrainerName', start)
    return source[start:end]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    name = 'src/hoenn_dome.c'
    current = (ROOT / name).read_text(encoding='utf-8')
    original = subprocess.check_output(['git', 'show', BASELINE + ':' + name], cwd=ROOT).decode('utf-8')
    if '#define BUGFIX' not in current or '#define BUGFIX' not in original:
        raise RuntimeError('Fixture must match production BUGFIX configuration')
    fixture = r'''
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
#define BUGFIX
#define TRUE 1
#define DOME_TOURNAMENT_TRAINERS_COUNT 16
#define FRONTIER_PARTY_SIZE 3
#define MAX_MON_MOVES 4
#define TRAINER_PLAYER 1000
#define TRAINER_FRONTIER_BRAIN 1001
#define EFFECTIVENESS_MODE_AI_VS_AI 0
struct Trainer { int trainerId, isEliminated, eliminatedAt; };
/* Extra sentinel entries make the old invalid access reproducible without crashing.
   Only entries 0..15 are real tournament participants. */
struct Trainer trainers[256], initial[256], expected[256];
#define DOME_TRAINERS trainers
u16 mons[16][3];
#define DOME_MONS mons
struct Dome { u16 domeWinningMoves[16]; } dome, initialDome, expectedDome;
#define DOME (&dome)
struct BaseStats { int baseHP, baseAttack, baseDefense, baseSpeed, baseSpAttack, baseSpDefense; }
    gBaseStats[5] = {{0}, {50,80,90,100,110,120}, {80,10,20,30,40,50},
                    {70,130,120,110,100,90}, {20,30,40,50,60,70}};
int opponents[16], randomCalls, wins, bad;
u32 seed;
static u16 Random(void) { randomCalls++; seed = seed * 1664525u + 1013904223u; return seed >> 16; }
static int TournamentIdOfOpponent(int round, int trainerId) {
    (void)round;
    for (int i=0;i<16;i++) if (trainers[i].trainerId == trainerId) return opponents[i];
    bad = 1; return 255;
}
static int HoennFactory_MonMove(int mon, int slot) { return (mon + slot) % 4 + 1; }
static int HoennFactory_MonSpecies(int mon) { return mon % 4 + 1; }
static int GetTypeEffectivenessPoints(int move, int species, int mode) {
    (void)mode; return (move * 7 + species * 3) % 41;
}
static u16 GetWinningMove(int winner, int loser, u8 round) {
    wins++;
    if (winner < 0 || winner >= 16 || loser < 0 || loser >= 16) bad = 1;
    return (Random() % 600) + 1 + round;
}
ORIGINAL
CURRENT
static int sameBytes(const void *a, const void *b, int n) {
    for (int i=0;i<n;i++) if (((const u8*)a)[i] != ((const u8*)b)[i]) return 0;
    return 1;
}
static void copyBytes(void *a, const void *b, int n) {
    for (int i=0;i<n;i++) ((u8*)a)[i] = ((const u8*)b)[i];
}
int main(void) {
    /* No opponent must not use the sentinel as a trainer, eliminate a mon,
       select a winning move, or advance RNG. */
    for (int missing=255; missing>=16; missing-=239) {
        for (int i=0;i<256;i++) trainers[i] = (struct Trainer){i, 1, 0};
        for (int i=0;i<16;i++) dome.domeWinningMoves[i] = 0;
        trainers[0].isEliminated = 0;
        trainers[missing].trainerId = TRAINER_FRONTIER_BRAIN;
        opponents[0] = missing;
        randomCalls = wins = bad = 0;
        TEST_FUNCTION(2);
        if (trainers[0].isEliminated || randomCalls || wins || bad || dome.domeWinningMoves[0]) return 1;
    }
    /* Valid tournaments must produce exactly the baseline results and RNG state. */
    for (u32 sample=0; sample<1000; sample++) {
        for (int i=0;i<256;i++) trainers[i] = (struct Trainer){i, 1, 0};
        for (int i=0;i<16;i++) {
            trainers[i].isEliminated = 0;
            opponents[i] = i ^ 1;
            for (int j=0;j<3;j++) mons[i][j] = (sample * 17 + i * 3 + j) % 30;
            dome.domeWinningMoves[i] = 0;
        }
        trainers[sample % 16].trainerId = TRAINER_PLAYER;
        if (sample % 3 == 0) trainers[(sample + 1) % 16].trainerId = TRAINER_FRONTIER_BRAIN;
        copyBytes(initial, trainers, sizeof(trainers)); initialDome = dome;
        seed = sample + 123; randomCalls = wins = bad = 0;
        DecideRoundWinnersBaseline(sample % 4);
        copyBytes(expected, trainers, sizeof(trainers)); expectedDome = dome;
        u32 expectedSeed = seed;
        int expectedCalls = randomCalls, expectedWins = wins;
        if (bad) return 2;
        copyBytes(trainers, initial, sizeof(trainers)); dome = initialDome;
        seed = sample + 123; randomCalls = wins = bad = 0;
        DecideRoundWinners(sample % 4);
        if (!sameBytes(expected, trainers, sizeof(trainers)) || !sameBytes(&expectedDome, &dome, sizeof(dome))
            || seed != expectedSeed || randomCalls != expectedCalls || wins != expectedWins || bad) return 3;
    }
    return 0;
}
'''.replace('ORIGINAL', function(original, 'DecideRoundWinners').replace(
        'DecideRoundWinners(', 'DecideRoundWinnersBaseline(')).replace(
        'CURRENT', function(current, 'DecideRoundWinners')).replace(
        'TEST_FUNCTION', 'DecideRoundWinnersBaseline' if args.baseline else 'DecideRoundWinners')
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c_file, obj, exe = (path / name for name in ('rounds.c', 'rounds.obj', 'rounds.exe'))
        c_file.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c_file), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        result = subprocess.run([str(exe)])
        if args.baseline:
            if result.returncode != 1:
                raise RuntimeError('Original invalid-opponent access was not reproduced')
            print('Original baseline treats an out-of-range opponent sentinel as a real trainer.')
        else:
            result.check_returncode()
            print('Dome absent-opponent guards pass; 1000 valid rounds match original outcomes and RNG consumption.')


if __name__ == '__main__':
    main()
