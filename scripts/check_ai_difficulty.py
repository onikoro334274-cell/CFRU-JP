"""Exercise saved difficulty mappings, AI tiers and the new tactical scoring."""
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def function(source, name):
    match = re.search(r'[^\n;{}]*\b' + name + r'\([^;{}]*\)\s*\{', source)
    if not match:
        raise ValueError('Missing function ' + name)
    end, depth = match.end(), 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]


def main():
    header = (ROOT / 'src/hack_difficulty.h').read_text(encoding='utf-8')
    header = re.sub(r'^#(?:include|pragma).*$', '', header, flags=re.M)
    difficulty = (ROOT / 'src/hack_difficulty.c').read_text(encoding='utf-8')
    positives = (ROOT / 'src/Battle_AI/ai_positives.c').read_text(encoding='utf-8')
    master = (ROOT / 'src/Battle_AI/ai_master.c').read_text(encoding='utf-8')
    menu = (ROOT / 'src/bw_option_menu.c').read_text(encoding='utf-8')
    for registration in ('[10] = AIScript_Advanced', '[11] = AIScript_Expert', '[12] = AIScript_WildInstinct',
                         'return HackDifficulty_AIFlags(flags);'):
        if registration not in master:
            raise RuntimeError('Missing master AI integration: ' + registration)
    for integration in ('HackDifficulty_SelectionFromValue(VarGet(HOENN_VAR_DIFFICULTY))',
                        'HackDifficulty_ValueFromSelection(data[TD_DIFFICULTY])',
                        'CycleInput(selection, DIFFICULTY_COUNT, arrow)',
                        'gText_BwOpt_Hard, gText_BwOpt_MomoIce'):
        if integration not in menu:
            raise RuntimeError('Missing menu integration: ' + integration)
    flags = '\n'.join(line for line in (ROOT / 'include/battle.h').read_text(encoding='utf-8').splitlines()
                      if line.startswith('#define AI_SCRIPT_'))
    funcs = '\n'.join(function(difficulty, name) for name in
                      ('OrdinaryTrainerBattle', 'HackAI_GetRank', 'RankAIFlags', 'CommonBattlerRank',
                       'HackAI_GetTrainerRank', 'HackAI_GetWildRank', 'HackAI_GetBattlerRank',
                       'HackAI_TrainerFlags', 'HackAI_WildFlags', 'HackDifficulty_AIFlags'))
    config = '\n'.join(line for line in (ROOT / 'src/config.h').read_text(encoding='utf-8').splitlines()
                       if line.startswith(('#define AI_RANK_', '#define AI_WILD_INSTINCT_')))
    scores = '\n'.join(function(positives, name) for name in ('AIScript_Advanced', 'AIScript_Expert', 'AIScript_WildInstinct'))
    strategy = function((ROOT / 'include/new/ai_master.h').read_text(encoding='utf-8'), 'AIFlagsHaveStrategy')
    fixture = r'''
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed char s8;
typedef signed short s16;
typedef u8 bool8;
#define unusedArg
#define NULL ((void *)0)
#define TRUE 1
#define FALSE 0
#define BATTLE_TYPE_TRAINER 1
#define BATTLE_TYPE_FRONTIER 2
#define BATTLE_TYPE_SAFARI 4
#define BATTLE_TYPE_ROAMER 8
#define BATTLE_TYPE_OAK_TUTORIAL 16
#define BATTLE_TYPE_INGAME_PARTNER 32
#define B_SIDE_PLAYER 0
#define FLAG_BATTLE_FACILITY 1
#define HOENN_VAR_DIFFICULTY 0
#define MOVE_NONE 0
#define SPLIT_STATUS 2
#define EFFECT_RESTORE_HP 3
#define EFFECT_MORNING_SUN 4
#define SIDE(n) ((n)&1)
#define BATTLER_ALIVE(n) (gBattleMons[n].hp != 0)
#define MathMin(a,b) ((a)<(b)?(a):(b))
#define MathMax(a,b) ((a)>(b)?(a):(b))
#define STATUS1_ANY 63
#define STAT_ATK 1
#define STAT_SPATK 4
#define STAT_SPEED 3
#define CLASS_LEADER 1
#define CLASS_ELITE_FOUR 2
#define CLASS_CHAMPION 3
#define CLASS_LEADER_RS 4
#define CLASS_ELITE_FOUR_RS 5
#define CLASS_CHAMPION_RS 6
#define CLASS_AQUA_LEADER 7
#define CLASS_MAGMA_LEADER 8
FLAGS
STRATEGY
HEADER
CONFIG
u32 gBattleTypeFlags;
u16 savedDifficulty;
int facility;
struct BattlePokemon { u16 hp, maxHP; u8 level; u32 status1, personality; u8 statStages[7]; } gBattleMons[4];
struct Move { u8 effect; } gBattleMoves[40];
struct Trainer { u8 trainerClass; } gTrainers[1];
struct Results { u8 battleTurnCounter; } gBattleResults;
u8 gActiveBattler=1, gBattlersCount=2;
u16 gTrainerBattleOpponent_A;
static u16 VarGet(u16 var) { (void)var; return savedDifficulty; }
static bool8 FlagGet(u16 flag) { (void)flag; return facility; }
static bool8 IsFrontierTrainerId(u16 id) { return id >= 100; }
DIFFICULTY
struct AIScript { u8 intelligenceRank; };
u32 damage[40];
u16 accuracy[40], prediction;
u8 status[40], first[40], guaranteed[40];
static u16 TryReplaceMoveWithZMove(u8 a, u8 d, u16 move) { (void)a;(void)d;return move; }
static u8 CalcMoveSplit(u8 a, u16 move, u8 d) { (void)a;(void)d;return status[move] ? SPLIT_STATUS : 0; }
static u32 GetFinalAIMoveDamage(u16 move,u8 a,u8 d,u8 hits,void *data) {
    (void)a;(void)d;(void)hits;(void)data;return damage[move];
}
static u16 AccuracyCalc(u16 move,u8 a,u8 d) { (void)a;(void)d;return accuracy[move]; }
static bool8 MoveWillHit(u16 move,u8 a,u8 d) { (void)a;(void)d;return guaranteed[move]; }
static bool8 MoveWouldHitFirst(u16 move,u8 a,u8 d) { (void)a;(void)d;return first[move]; }
static bool8 MoveWouldHitBeforeOtherMove(u16 move,u8 a,u16 other,u8 d) {
    (void)a;(void)d;(void)other;return first[move];
}
static u16 IsValidMovePrediction(u8 a,u8 d) { (void)a;(void)d;return prediction; }
static u8 GetNatureFromPersonality(u32 personality) { return personality%25; }
static u16 GetAmountToRecoverBy(u8 a,u8 d,u16 move) { (void)d;(void)move;return gBattleMons[a].maxHP/2; }
SCORES
int main(void) {
    for(u32 flags=0;flags<8192;flags++)
        if(AIFlagsHaveStrategy(flags)!=((flags&(2|4|1024|2048))!=0))return 41;
    if(AIFlagsHaveStrategy(AI_SCRIPT_ROAMING|AI_SCRIPT_WILD_INSTINCT|1))return 42;
    for (u32 mode=0;mode<5;mode++)
    for (u32 level=0;level<256;level++)
    for (u32 conditions=0;conditions<32;conditions++) {
        struct AIRankContext context = {mode, level, !!(conditions&1), !!(conditions&2),
                                       !!(conditions&4), !!(conditions&8), !!(conditions&16)};
        u8 rank = HackAI_GetRank(&context);
        context.wild=1;
        if (HackAI_GetRank(&context)!=rank) return 30;
        context.wild=0;
        if (rank>24 || (mode==4&&rank!=24) || (mode==3&&rank<8)) return 15;
        if (mode!=4 && rank>=24) return 16;
        if (level<255) {
            context.level++;
            if (HackAI_GetRank(&context)<rank) return 17;
        }
        if (mode==0) {
            context.difficulty=2;
            u8 medium=HackAI_GetRank(&context);
            context.difficulty=1;
            if (HackAI_GetRank(&context)>medium) return 18;
            context.difficulty=0;
            if (HackAI_GetRank(&context)<medium) return 19;
        }
    }
    struct AIRankContext boundary = {0};
    for (u32 level=1;level<=200;level++) {
        boundary.level=level;
        if (HackAI_GetRank(&boundary)!=level/10) return 20;
    }
    boundary.level=19;
    if (HackAI_GetRank(&boundary)!=1) return 21;
    boundary.lowHp=1;
    if (HackAI_GetRank(&boundary)!=1) return 22;
    boundary.statused=1;
    if (HackAI_GetRank(&boundary)!=2) return 23;
    boundary.importantTrainer=boundary.boostedFoe=boundary.prolongedBattle=1;
    if (HackAI_GetRank(&boundary)!=2) return 24;
    for (u32 value=0;value<65536;value++) {
        u8 selection = HackDifficulty_SelectionFromValue(value);
        if (selection >= DIFFICULTY_COUNT) return 1;
        if (HackDifficulty_ValueFromSelection(selection) != (value<5 ? value : 0)) return 2;
        if (HackDifficulty_ValueFromSelection(value) != (value==0?1:value==1?2:value==2?0:value<5?value:0)) return 3;
    }
    gBattleMons[1].level=1; gBattleMons[1].hp=gBattleMons[1].maxHP=100;
    for (u32 mode=0;mode<5;mode++)
    for (u32 battle=0;battle<4;battle++)
    for (u32 original=0;original<8;original++) {
        savedDifficulty = mode;
        gBattleTypeFlags = battle & 1 ? BATTLE_TYPE_TRAINER : 0;
        facility = battle & 2;
        u32 expected = original;
        if (!(gBattleTypeFlags & BATTLE_TYPE_TRAINER) && original)
            expected = (mode>=3?7:1) | AI_SCRIPT_WILD_INSTINCT;
        if (gBattleTypeFlags & BATTLE_TYPE_TRAINER) {
            if (mode==3 || mode==4) expected |= 7 | AI_SCRIPT_ADVANCED | (mode==4?AI_SCRIPT_EXPERT:0);
            else if (!facility) expected = AI_SCRIPT_CHECK_BAD_MOVE;
        }
        if (HackDifficulty_AIFlags(original) != expected) return 4;
    }
    for (u32 mode=3;mode<=4;mode++) {
        savedDifficulty = mode;
        gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_FRONTIER;
        if ((HackDifficulty_AIFlags(7)&(AI_SCRIPT_ADVANCED|AI_SCRIPT_EXPERT)) !=
            (AI_SCRIPT_ADVANCED|(mode==4?AI_SCRIPT_EXPERT:0))) return 5;
        for (u32 special=BATTLE_TYPE_SAFARI;special<=BATTLE_TYPE_OAK_TUTORIAL;special<<=1) {
            gBattleTypeFlags = BATTLE_TYPE_TRAINER | special;
            if (HackDifficulty_AIFlags(0)!=0) return 6;
        }
    }
    facility=0; savedDifficulty=0; gBattleTypeFlags=BATTLE_TYPE_TRAINER;
    gBattleMons[1].level=19;
    gTrainers[0].trainerClass=CLASS_LEADER;
    if (HackDifficulty_AIFlags(7)!=3) return 25;
    gTrainers[0].trainerClass=0;
    gBattleMons[1].hp=30; gBattleMons[1].status1=1;
    if (HackDifficulty_AIFlags(7)!=3) return 26;
    gBattleMons[1].status1=0; gBattleResults.battleTurnCounter=4;
    if (HackDifficulty_AIFlags(7)!=3) return 27;
    gBattleMons[1].hp=100; gBattleResults.battleTurnCounter=0;
    gBattleMons[0].hp=50; gBattleMons[0].statStages[STAT_ATK]=8;
    if (HackDifficulty_AIFlags(7)!=3) return 28;
    gBattleMons[0].hp=0;
    if (HackDifficulty_AIFlags(7)!=1) return 29;
    gTrainers[0].trainerClass=CLASS_LEADER;
    gBattleTypeFlags=0;
    if (HackAI_GetWildRank(1)!=1 || HackDifficulty_AIFlags(7)!=(1|AI_SCRIPT_WILD_INSTINCT)) return 31;
    gBattleMons[0].hp=50;gBattleMons[0].level=40;
    if (HackAI_GetWildRank(1)!=2) return 32;
    savedDifficulty=4;
    if (HackAI_GetWildRank(1)!=24 || (HackDifficulty_AIFlags(7)&(AI_SCRIPT_EXPERT|AI_SCRIPT_ADVANCED))) return 33;
    struct AIScript data = {24};
    gBattleMons[0].hp=50; gBattleMons[1].hp=100; gBattleMons[2].hp=50;
    damage[10]=100; accuracy[10]=100; first[10]=1;
    damage[11]=100; accuracy[11]=70;
    damage[12]=70; accuracy[12]=100; first[12]=1;
    status[13]=1;
    if (AIScript_Advanced(0,1,10,100,&data)!=114 || AIScript_Advanced(0,1,11,100,&data)!=105
        || AIScript_Advanced(0,1,12,100,&data)!=105 || AIScript_Advanced(0,1,13,105,&data)!=105) return 7;
    prediction=30; damage[30]=60; accuracy[30]=100;
    if (AIScript_Expert(0,1,10,114,&data)!=120 || AIScript_Expert(0,1,11,105,&data)!=83
        || AIScript_Expert(0,1,12,105,&data)!=105 || AIScript_Expert(0,1,13,105,&data)!=105) return 8;
    for (int score=0;score<100;score++)
        if (AIScript_Advanced(0,1,10,score,&data)!=score || AIScript_Expert(0,1,10,score,&data)!=score) return 9;
    if (AIScript_Advanced(0,2,10,100,&data)!=100 || AIScript_Expert(0,2,10,100,&data)!=100) return 10;
    if (AIScript_Advanced(0,1,10,254,&data)!=255 || AIScript_Expert(0,1,10,254,&data)!=255) return 11;
    gBattleMons[1].hp=0;
    if (AIScript_Advanced(0,1,10,100,&data)!=100 || AIScript_Expert(0,1,10,100,&data)!=100) return 12;
    gBattleMons[1].hp=100;
    for (int dmg=0;dmg<=100;dmg++)
    for (int acc=0;acc<=100;acc++) {
        damage[10]=dmg; accuracy[10]=acc; first[10]=0;
        int expected=100+dmg*acc*8/10000+(dmg==100&&acc>=90?5:0);
        if (AIScript_Advanced(0,1,10,100,&data)!=expected) return 13;
    }
    gBattleMons[1].hp=65535; damage[10]=0xffffffffu; guaranteed[10]=1; first[10]=1;
    if (AIScript_Advanced(0,1,10,100,&data)!=114) return 14;
    gBattleTypeFlags=0; gBattleMons[0].hp=30;gBattleMons[0].maxHP=100;
    gBattleMons[1].hp=100;damage[10]=100;accuracy[10]=100;
    gBattleMons[0].personality=0;
    if (AIScript_WildInstinct(0,1,10,100,&data)!=110) return 34;
    gBattleMons[0].personality=1;
    if (AIScript_WildInstinct(0,1,10,100,&data)!=111) return 35;
    gBattleMoves[13].effect=EFFECT_RESTORE_HP;
    gBattleMons[0].personality=5;
    if (AIScript_WildInstinct(0,1,13,100,&data)!=110) return 36;
    gBattleMons[0].hp=100;
    if (AIScript_WildInstinct(0,1,13,100,&data)!=100) return 37;
    data.intelligenceRank=0;
    u8 instinct=AIScript_WildInstinct(0,1,12,100,&data);
    if (AIScript_WildInstinct(0,1,12,100,&data)!=instinct || instinct<96 || instinct>106) return 38;
    if (AIScript_WildInstinct(0,1,12,0,&data)!=0) return 39;
    gBattleTypeFlags=BATTLE_TYPE_TRAINER;
    if (AIScript_WildInstinct(0,1,12,100,&data)!=100) return 40;
    return 0;
}
'''.replace('\nFLAGS\n', '\n' + flags + '\n').replace('\nSTRATEGY\n', '\n' + strategy + '\n').replace(
        '\nHEADER\n', '\n' + header + '\n').replace(
        '\nCONFIG\n', '\n' + config + '\n').replace(
        '\nDIFFICULTY\n', '\n' + funcs + '\n').replace('\nSCORES\n', '\n' + scores + '\n')
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c_file, obj, exe = (path / name for name in ('difficulty.c', 'difficulty.obj', 'difficulty.exe'))
        c_file.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c_file), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Five difficulties; all saves; 40960 Lv200 rank/condition inputs; separate wild/trainer profiles; temperament/recovery instincts; Momo tactics and accuracy/damage boundaries passed.')


if __name__ == '__main__':
    main()
