"""Exercise production friendship OT dispatch with controlled Pokemon API stubs."""
from pathlib import Path
import json
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/hack_friend_ot.c').read_text(encoding='utf-8')
    source = re.sub(r'^#include[^\n]*', '', source, flags=re.M)
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
#define NULL ((void*)0)
#define TRUE 1
#define FALSE 0
#define EOS 255
#define OT_NAME_LENGTH 7
#define PARTY_SIZE 6
#define SPECIES_NONE 0
#define FLAG_BATTLE_FACILITY 1
enum {MON_DATA_SPECIES,MON_DATA_IS_EGG,MON_DATA_FRIENDSHIP,MON_DATA_OT_NAME,MON_DATA_OT_ID,MON_DATA_OT_GENDER};
struct Pokemon {u32 ot;u8 name[8],gender,species,egg,friendship,ownId,partner;};
struct BoxPokemon {u32 unused;};
static struct Pokemon gPlayerParty[6];
static struct {u32 vblankCounter2;} gMain;
static struct {u8 playerTrainerId[4],playerName[8],playerGender;} save;
#define gSaveBlock2 (&save)
static int facility,reads,writes,ensures,vars,errors,selected;static u16 varLo,varHi;
static int FlagGet(int flag){if(flag!=1) errors++;return facility;}
static u32 GetMonData(struct Pokemon* m,int field,void* dst){
 if(m!=&gPlayerParty[selected]) errors++;reads++;
 switch(field){case MON_DATA_SPECIES:return m->species;case MON_DATA_IS_EGG:return m->egg;
 case MON_DATA_FRIENDSHIP:return m->friendship;case MON_DATA_OT_ID:return m->ot;
 case MON_DATA_OT_GENDER:return m->gender;case MON_DATA_OT_NAME:
 for(int i=0;i<8;i++) ((u8*)dst)[i]=m->name[i];return 0;}errors++;return 0;}
bool8 HackPartner_IsMon(const struct Pokemon* m){if(writes||ensures) errors++;return m->partner;}
static void HackMonId_Ensure(struct BoxPokemon* b){struct Pokemon* m=(struct Pokemon*)b;
 if(writes||m->ot!=0x11223344u) errors++;m->ownId=1;ensures++;}
static void SetMonData(struct Pokemon* m,int field,const void* p){
 if(ensures!=1||!m->ownId) errors++;writes++;
 if(field==MON_DATA_OT_ID)m->ot=*(const u32*)p;
 else if(field==MON_DATA_OT_NAME){for(int i=0;i<8;i++)m->name[i]=((const u8*)p)[i];}
 else if(field==MON_DATA_OT_GENDER)m->gender=*(const u8*)p;else errors++;}
static void VarSet(u16 id,u16 val){if(writes!=3) errors++;vars++;if(id==0x50F2)varLo=val;else if(id==0x50F3)varHi=val;else errors++;}
PRODUCTION
int main(void){
 save.playerTrainerId[0]=0xEF;save.playerTrainerId[1]=0xCD;save.playerTrainerId[2]=0xAB;save.playerTrainerId[3]=0x89;
 save.playerGender=1;for(int i=0;i<8;i++)save.playerName[i]=(u8)(10+i);save.playerName[7]=EOS;
 for(u32 tick=0;tick<96;tick++)for(int f=0;f<2;f++)for(int state=0;state<5;state++)for(int partner=0;partner<2;partner++){
  facility=f;selected=(tick/8)%6;gMain.vblankCounter2=tick;reads=writes=ensures=vars=errors=0;
  for(int i=0;i<6;i++){struct Pokemon* m=&gPlayerParty[i];m->species=1;m->egg=0;m->friendship=255;m->ot=0x11223344u;m->gender=0;m->ownId=0;m->partner=partner;for(int j=0;j<8;j++)m->name[j]=0;}
  struct Pokemon* m=&gPlayerParty[selected];
  if(state==1)m->species=0;if(state==2)m->egg=1;if(state==3)m->friendship=254;
  if(state==4){m->ot=0x89ABCDEFu;m->gender=1;for(int j=0;j<8;j++)m->name[j]=save.playerName[j];}
  HackFriendOt_FieldTick();int change=tick%8==0&&!f&&state==0;
  if(errors||writes!=change*3||ensures!=change||vars!=change*partner*2)return 1;
  if((tick%8||f)&&reads)return 2;
  if(change&&(m->ot!=0x89ABCDEFu||m->gender!=1||!SameName(m->name,save.playerName)))return 3;
  if(change&&partner&&(varLo!=0xCDEF||varHi!=0x89AB))return 4;
  for(int i=0;i<6;i++)if(i!=selected&&(gPlayerParty[i].ownId||gPlayerParty[i].ot!=0x11223344u))return 5;
 }
 for(int end=0;end<8;end++)for(int mismatch=0;mismatch<8;mismatch++){
  u8 a[8],b[8];for(int i=0;i<8;i++)a[i]=b[i]=(u8)(i+1);a[end]=b[end]=EOS;b[mismatch]^=1;
  int expected=mismatch>=7||mismatch>end;if(SameName(a,b)!=expected)return 6;
 }
 return 0;
}
'''.replace('PRODUCTION', source)
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('friend_ot' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'dispatch_cases': 1920, 'name_cases': 64, 'result': 'passed',
              'scope': 'Production friendship OT source with controlled API stubs; eligibility, selected slot, update ordering, partner variables, EOS comparison.',
              'limitations': 'Does not verify actual encrypted Pokemon API, persistent individual IDs, stat recalculation, or GBA execution.'}
    (ROOT / 'audit/friend_ot_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Friendship OT: 1920 dispatch cases and 64 name cases passed')


if __name__ == '__main__':
    main()
