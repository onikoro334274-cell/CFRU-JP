"""Check user-requested boss percentages and exact HP thresholds."""
import subprocess,tempfile,re
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
def main():
 source=(ROOT/'src/wild_boss.c').read_text(encoding='utf-8')
 source=re.sub(r'^#include.*$', '',source,flags=re.M)
 fixture=r"""
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
#define FALSE 0
#define DIFFICULTY_NORMAL 0
#define DIFFICULTY_EASY 1
#define DIFFICULTY_MEDIUM 2
#define DIFFICULTY_HARD 3
#define DIFFICULTY_MOMO_ICE 4
#define DIFFICULTY_COUNT 5
struct WildBossPolicy {u16 statBonusPercent;u8 allyCallPercent,catchHpPercent,spawnPercent,allyCallLimit;};
PRODUCTION
int main(void){
const unsigned bonus[]={50,10,30,100,300},call[]={50,10,30,70,100},hpLimit[]={40,90,60,20,5},spawn[]={25,1,20,30,40},limits[]={2,1,2,3,3};
for(unsigned d=0;d<5;d++){
 for(unsigned stat=0;stat<65536;stat++){
 unsigned expected=stat+stat*bonus[d]/100;if(expected>65535)expected=65535;
 if(WildBoss_BoostStat(stat,d)!=expected)return 1;
 }
 for(unsigned roll=0;roll<256;roll++)if(WildBoss_ShouldCallAlly(roll,d)!=(roll<call[d]))return 2;
 for(unsigned roll=0;roll<256;roll++)if(WildBoss_ShouldSpawn(roll,d)!=(roll<spawn[d]))return 5;
 for(unsigned count=0;count<256;count++)for(unsigned roll=0;roll<256;roll++)if(WildBoss_CanCallAlly(roll,count,d)!=(count<limits[d]&&roll<call[d]))return 6;
 for(unsigned max=0;max<=500;max++)for(unsigned hp=0;hp<=501;hp++){
 unsigned expected=hp&&max&&hp<=max&&hp<=max*hpLimit[d]/100;
 if(WildBoss_CanCatch(hp,max,d)!=expected)return 3;
 }
}
for(unsigned d=5;d<65536;d++)if(WildBoss_GetPolicy(d)!=WildBoss_GetPolicy(0))return 4;
return 0;}
""".replace('PRODUCTION',source)
 with tempfile.TemporaryDirectory() as directory:
  path=Path(directory);c=path/'policy.c';obj=path/'policy.obj';exe=path/'policy.exe';c.write_text(fixture,encoding='utf-8')
  subprocess.run(['clang','--target=x86_64-pc-windows-msvc','-O2','-fno-builtin','-fno-stack-protector','-c',str(c),'-o',str(obj)],check=True)
  subprocess.run(['lld-link','/entry:main','/subsystem:console','/nodefaultlib','/out:'+str(exe),str(obj)],check=True)
  subprocess.run([str(exe)],check=True)
 print('Wild boss policy: stat saturation, all roll values, HP thresholds and invalid difficulty fallback pass.')
if __name__=='__main__':main()
