"""Verify attacking-move guarantee without replacing existing damaging moves."""
import subprocess,tempfile
from pathlib import Path
from check_factory_party_copy import function
ROOT=Path(__file__).resolve().parent.parent
def main():
 source=function((ROOT/'src/wild_attack_move.c').read_text(encoding='utf-8'),'void WildMon_EnsureAttackMove(struct Pokemon* mon)')
 fixture=r"""
typedef unsigned char u8;typedef unsigned short u16;
#define NULL ((void*)0)
#define MAX_MON_MOVES 4
#define MOVE_NONE 0
#define MOVE_TACKLE 3
#define MOVES_COUNT 4
#define SPLIT_STATUS 2
#define MON_DATA_MOVE1 0
#define MON_DATA_PP1 4
struct Pokemon {u16 moves[4];u8 pp[4];};
static const struct {u8 split;} gBattleMoves[4]={{2},{2},{0},{0}};
static unsigned writes;
static unsigned GetMonData(struct Pokemon* m,unsigned field,void* unused){return field<4?m->moves[field]:m->pp[field-4];}
static void SetMonMoveSlot(struct Pokemon* m,u16 move,u8 slot){m->moves[slot]=move;m->pp[slot]=35;writes++;}
PRODUCTION
int main(void){
for(unsigned shape=0;shape<256;shape++){
 struct Pokemon mon,old;unsigned digits=shape,hasAttack=0;
 for(unsigned i=0;i<4;i++){unsigned state=digits%4;digits/=4;mon.moves[i]=state==0?0:state==1?1:2;mon.pp[i]=state==2?0:10;hasAttack|=state>=2;}
 old=mon;writes=0;WildMon_EnsureAttackMove(&mon);unsigned usable=0;
 for(unsigned i=0;i<4;i++)usable|=mon.moves[i]>=2&&mon.pp[i]>0;
 if(!usable||writes>1)return 1;
 if(hasAttack)for(unsigned i=0;i<4;i++)if(mon.moves[i]!=old.moves[i])return 2;
 WildMon_EnsureAttackMove(&mon);if(writes>1)return 3;
}return 0;}
""".replace('PRODUCTION',source)
 with tempfile.TemporaryDirectory() as directory:
  path=Path(directory);c=path/'m.c';obj=path/'m.obj';exe=path/'m.exe';c.write_text(fixture,encoding='utf-8')
  subprocess.run(['clang','--target=x86_64-pc-windows-msvc','-O2','-fno-builtin','-fno-stack-protector','-c',str(c),'-o',str(obj)],check=True)
  subprocess.run(['lld-link','/entry:main','/subsystem:console','/nodefaultlib','/out:'+str(exe),str(obj)],check=True)
  subprocess.run([str(exe)],check=True)
 print('Wild attack moves: all 256 move/PP layouts and repeat calls pass.')
if __name__=='__main__':main()
