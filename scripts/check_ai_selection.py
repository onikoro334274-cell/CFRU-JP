"""Execute production move/target selection and predictions with controlled scores."""
import argparse
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASELINE = '61e1b61653c1c87077860400a995ff3a36b81689'


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
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    name = 'src/Battle_AI/ai_master.c'
    source = (subprocess.check_output(['git', 'show', BASELINE + ':' + name], cwd=ROOT).decode('utf-8')
              if args.baseline else (ROOT / name).read_text(encoding='utf-8'))
    battle_header = (subprocess.check_output(['git', 'show', BASELINE + ':include/battle.h'], cwd=ROOT).decode('utf-8')
                     if args.baseline else (ROOT / 'include/battle.h').read_text(encoding='utf-8'))
    thinking = re.search(r'struct AI_ThinkingStruct\s*\{[^}]*\};', battle_header)[0]
    selected = ('ChooseMoveOrAction_Singles', 'ChooseMoveOrAction_Doubles', 'PredictMovesForBanks')
    helpers = (function(source, 'GetMaxByteIndexInList') if args.baseline
               else function(source, 'FindBestLegalMoves'))
    functions = '\n'.join(function(source, name) for name in selected)
    fixture = r'''
typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;
typedef u8 bool8;
#define NULL ((void*)0)
#define TRUE 1
#define FALSE 0
#define MAX_MON_MOVES 4
#define MAX_BATTLERS_COUNT 4
#define BIT_FLANK 2
#define BIT_SIDE 1
#define SIDE(bank) ((bank)&1)
#define MOVE_NONE 0
#define MOVE_STRUGGLE 20
#define SPLIT_STATUS 2
#define STATUS2_RECHARGE 1
#define STATUS2_MULTIPLETURNS 2
#define EFFECT_SLEEP_TALK 1
#define EFFECT_SNORE 2
#define AI_ACTION_FLEE 2
#define AI_ACTION_WATCH 4
#define AI_CHOICE_FLEE 4
#define AI_CHOICE_WATCH 5
#define AIState_SettingUp 0
THINKING
_Static_assert(__builtin_offsetof(struct AI_ThinkingStruct, score)==4, "Score offset changed");
_Static_assert(__builtin_offsetof(struct AI_ThinkingStruct, aiFlags)==12, "Flags offset changed");
_Static_assert(sizeof(struct AI_ThinkingStruct)==28, "AI memory layout changed");
struct AI_ThinkingStruct thinking;
#define AI_THINKING_STRUCT (&thinking)
struct AIScript { u8 dummy; };
struct DamageCalc { u8 bankAtk, bankDef; };
struct Mon { u16 hp, moves[4]; u8 pp[4]; u32 status2; } gBattleMons[4];
#define BATTLER_ALIVE(bank) (gBattleMons[bank].hp != 0)
struct Disable { u8 truantCounter; } gDisableStructs[4];
u16 gLockedMoves[4];
u32 gBitTable[4]={1,2,4,8};
u8 gBankAttacker=1, gBankTarget=0, gBattlersCount=4;
u8 limitations[4], targetScores[4][4], moveScores[40], split[40];
u32 targetDamage[4][40], rngCalls, badDamageCalls, scoringCalls, damageCalls;
u8 scoresByTarget;
u8 asleep[4], sleepMove[4];
struct New { struct { u16 movePredictions[4][4]; } ai; } newBS, *gNewBS=&newBS;
#define SPLIT(move) (split[move])
static u16 Random(void) { rngCalls++; return 0; }
static void Memset(void *p,int value,unsigned long long size) {
    for (unsigned long long i=0;i<size;i++) ((u8*)p)[i]=value;
}
static u8 CheckMoveLimitations(u8 bank,u8 ignore,u8 flags) {(void)ignore;(void)flags;return limitations[bank];}
static u8 AdjustMoveLimitationFlagsForAI(u8 a,u8 d) {(void)a;(void)d;return 255;}
static void RecordLastUsedMoveByTarget(void) {}
static void PopulateAIScriptStructWithBaseAttackerData(struct AIScript *data,u8 bank) {(void)data;(void)bank;}
static void PopulateAIScriptStructWithBaseDefenderData(struct AIScript *data,u8 bank) {
    (void)data;
    if (scoresByTarget) for(int i=0;i<4;i++) thinking.score[i]=targetScores[bank][i];
}
static void PopulateDamageCalcStructWithBaseAttackerData(struct DamageCalc *data) {(void)data;}
static void PopulateDamageCalcStructWithBaseDefenderData(struct DamageCalc *data) {(void)data;}
static void BattleAI_SetupAIData(u8 mask) {
    (void)mask;thinking.aiFlags=0;
    for (int i=0;i<4;i++) thinking.score[i]=targetScores[gBankTarget][i];
}
/* Setup in production chooses a preliminary random target; the target loop then
   explicitly assigns gBankTarget. Feed target-specific scores when data is loaded. */
static void BattleAI_DoAIProcessing(struct AIScript *data) {(void)data;}
static u16 TryReplaceMoveWithZMove(u8 a,u8 d,u16 move) {(void)a;(void)d;return move;}
static u32 GetFinalAIMoveDamage(u16 move,u8 a,u8 d,u8 hits,struct DamageCalc *data) {
    (void)a;(void)hits;(void)data;
    damageCalls++;
    if (move<10 || move>=40) badDamageCalls++;
    return targetDamage[d][move];
}
static bool8 IsBankAsleep(u8 bank) {return asleep[bank];}
static bool8 MoveEffectInMoveset(u8 effect,u8 bank) {(void)effect;return sleepMove[bank];}
static bool8 MoveInMoveset(u16 move,u8 bank) {
    for(int i=0;i<4;i++) if(gBattleMons[bank].moves[i]==move)return 1;
    return 0;
}
static void StoreMovePrediction(u8 a,u8 d,u16 move) {newBS.ai.movePredictions[a][d]=move;}
static void StoreSwitchPrediction(u8 a,u8 d) {newBS.ai.movePredictions[a][d]=65535;}
static u8 AIScript_Negatives(u8 a,u8 d,u16 move,u8 score,struct AIScript *data) {
    (void)a;(void)d;(void)score;(void)data;scoringCalls++;return moveScores[move];
}
static u8 AIScript_Positives(u8 a,u8 d,u16 move,u8 score,struct AIScript *data) {
    (void)a;(void)d;(void)move;(void)data;return score;
}
HELPERS
FUNCTIONS
static void Reset(void) {
    Memset(&thinking,0,sizeof(thinking));
    Memset(gBattleMons,0,sizeof(gBattleMons));
    Memset(targetScores,0,sizeof(targetScores));
    Memset(limitations,0,sizeof(limitations));
    for(int i=0;i<4;i++) gBattleMons[i].hp=100;
    for(int i=0;i<4;i++) gBattleMons[1].moves[i]=10+i;
    gBankTarget=0; badDamageCalls=rngCalls=damageCalls=scoresByTarget=0;
}
int main(void) {
    struct AIScript data={0};
    Reset();
    /* A zero-scored legal move must beat empty/disabled zero-scored slots. */
    gBattleMons[1].moves[0]=0;limitations[1]=12;
    if(ChooseMoveOrAction_Singles(&data)!=1) return 1;
    Reset();
    thinking.score[0]=100;thinking.score[1]=200;
    if(ChooseMoveOrAction_Singles(&data)!=1) return 2;
    for(int missing=0;missing<16;missing++)
    for(int disabled=0;disabled<16;disabled++) {
        Reset();limitations[1]=disabled;
        int best=-1;
        for(int i=0;i<4;i++) {
            if(missing&(1<<i))gBattleMons[1].moves[i]=0;
            thinking.score[i]=i*65;
            if(!(missing&(1<<i))&&!(disabled&(1<<i)))best=i;
        }
        if(ChooseMoveOrAction_Singles(&data)!=(best<0?0:best))return 3;
    }
    Reset();
    scoresByTarget=1;
    for(int target=0;target<4;target++)targetScores[target][0]=100;
    targetDamage[0][10]=20;targetDamage[2][10]=80;
    if(ChooseMoveOrAction_Doubles(&data)!=0 || gBankTarget!=2 || badDamageCalls) return 4;
    Reset();
    scoresByTarget=1;
    for(int target=0;target<4;target++)targetScores[target][0]=100;
    gBattleMons[2].hp=0;
    if(ChooseMoveOrAction_Doubles(&data)!=0 || gBankTarget!=0 || badDamageCalls)return 5;
    Reset();scoresByTarget=1;
    targetScores[0][0]=100;targetScores[2][0]=150;
    targetDamage[0][10]=90;targetDamage[2][10]=1;
    if(ChooseMoveOrAction_Doubles(&data)!=0 || gBankTarget!=2)return 13;
    Reset();scoresByTarget=1;
    targetScores[0][3]=150;targetScores[2][0]=150;split[13]=SPLIT_STATUS;
    if(ChooseMoveOrAction_Doubles(&data)!=3 || gBankTarget!=0 || damageCalls)return 14;
    Reset();scoresByTarget=1;
    targetScores[0][0]=100;targetScores[2][0]=100;targetScores[3][3]=110;
    if(ChooseMoveOrAction_Doubles(&data)!=3 || gBankTarget!=3 || damageCalls)return 15;
    Reset();
    thinking.aiAction=AI_ACTION_FLEE;
    if(ChooseMoveOrAction_Doubles(&data)!=AI_CHOICE_FLEE)return 6;
    Reset();
    thinking.aiAction=AI_ACTION_WATCH;
    if(ChooseMoveOrAction_Doubles(&data)!=AI_CHOICE_WATCH)return 7;
    Reset();
    /* Sparse movesets: predictions must evaluate every usable slot. */
    gBattleMons[1].moves[0]=0;gBattleMons[1].moves[1]=10;
    gBattleMons[1].moves[2]=0;gBattleMons[1].moves[3]=13;
    moveScores[10]=100;moveScores[13]=150;
    thinking.aiFlags=0x1234;
    PredictMovesForBanks();
    if(newBS.ai.movePredictions[1][0]!=13 || newBS.ai.movePredictions[1][2]!=13
       || newBS.ai.movePredictions[1][3]!=13 || thinking.aiFlags!=0x1234)return 8;
    limitations[1]=8;
    PredictMovesForBanks();
    if(newBS.ai.movePredictions[1][0]!=10)return 9;
    limitations[1]=15;
    PredictMovesForBanks();
    if(newBS.ai.movePredictions[1][0]!=MOVE_STRUGGLE)return 10;
    limitations[1]=0;asleep[1]=1;
    PredictMovesForBanks();
    if(newBS.ai.movePredictions[1][0]!=MOVE_NONE)return 11;
    asleep[1]=0;gBattleMons[1].status2=STATUS2_MULTIPLETURNS;gLockedMoves[1]=10;
    PredictMovesForBanks();
    if(newBS.ai.movePredictions[1][0]!=10)return 12;
    return 0;
}
'''.replace('\nTHINKING\n', '\n'+thinking+'\n').replace('\nHELPERS\n', '\n'+helpers+'\n').replace(
        '\nFUNCTIONS\n', '\n'+functions+'\n')
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c_file, obj, exe = (path / name for name in ('selection.c', 'selection.obj', 'selection.exe'))
        c_file.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c_file), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        result = subprocess.run([str(exe)])
        if args.baseline:
            if result.returncode!=1:
                raise RuntimeError('Original illegal zero-score choice was not reproduced: '+str(result.returncode))
            print('Original baseline chooses an empty slot over the remaining legal zero-scored move.')
        else:
            result.check_returncode()
            print('AI selection passes legal slots, high unsigned scores, correct damage move IDs, absent targets, special actions and sparse/locked/Struggle predictions.')


if __name__ == '__main__':
    main()
