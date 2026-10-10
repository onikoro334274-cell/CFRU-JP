"""Exercise actual save scrambling, partial reads and flash retry control flow."""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def key_at(i):
    x = (i * 0x9E3779B1 + 0x504B5354) & 0xFFFFFFFF
    x ^= x >> 15
    x = (x * 0x2C1B3C6D) & 0xFFFFFFFF
    x ^= x >> 12
    x = (x * 0x297A2D39) & 0xFFFFFFFF
    x ^= x >> 15
    return x >> 24


def main():
    path = ROOT / 'src/hack_save_crypt.c'
    source = re.sub(r'/\*.*?\*/|//[^\n]*', '', path.read_text(encoding='utf-8'), flags=re.S)
    production = '\n'.join(re.findall(r'^#define (?:SECTOR_SIZE|SIG_OFFSET|FILE_SIG|CRYPT_SEED)\b[^\n]*', source, re.M))
    names = ['HackSaveCrypt_KeyAt', 'Crypt', 'IsPlainSector', 'HackSaveCrypt_ReadFlash',
             'HackSaveCrypt_DoReadFlashWholeSection', 'HackSaveCrypt_ProgramFlashSectorAndVerify']
    production += '\n' + '\n'.join(function(source, name) for name in names)
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
#define TRUE 1
struct SaveSection {u8 bytes[4096];};
static const u8 key[4096]={KEY};
static u8 flash[4096],original[4096];static int errors,reads,programs,verifies,pattern;
static void ReadFlashVanilla(u16 sector,u32 off,void* dst,u32 n){
 if(sector!=17||off>4096||n>4096-off){errors++;return;}reads++;
 for(u32 i=0;i<n;i++)((u8*)dst)[i]=flash[off+i];}
static u16 ProgramFlashSectorPtr(u16 sector,void* src){
 if(sector!=17)errors++;
 for(int i=0;i<4096;i++)if(((u8*)src)[i]!=(original[i]^key[i]))errors++;
 int result=(pattern>>(programs*2))&1;programs++;
 if(!result)for(int i=0;i<4096;i++)flash[i]=((u8*)src)[i];
 return result?7:0;}
static u32 VerifyFlashSectorVanilla(u16 sector,u8* src){
 if(sector!=17)errors++;verifies++;
 for(int i=0;i<4096;i++)if(src[i]!=flash[i])errors++;
 return ((pattern>>((programs-1)*2+1))&1)?9:0;}
PRODUCTION
int main(void){
 for(u32 i=0;i<4096;i++)if(HackSaveCrypt_KeyAt(i)!=key[i])return 1;
 for(pattern=0;pattern<64;pattern++){
  static u8 src[4096];for(int i=0;i<4096;i++)src[i]=original[i]=(u8)(i*37+pattern);
  programs=verifies=errors=0;u32 result=HackSaveCrypt_ProgramFlashSectorAndVerify(17,src);
  int attempts=0,checks=0,wanted=0;
  for(int k=0;k<3;k++){attempts++;if((pattern>>(k*2))&1){wanted=7;continue;}
   checks++;wanted=((pattern>>(k*2+1))&1)?9:0;if(!wanted)break;}
  if(errors||result!=(u32)wanted||programs!=attempts||verifies!=checks)return 2;
  for(int i=0;i<4096;i++)if(src[i]!=original[i])return 3;
 }
 /* Legacy plain, scrambled and erased sectors; every possible starting offset. */
 for(int mode=0;mode<3;mode++){
  for(int i=0;i<4096;i++)original[i]=(u8)(i*17+3);
  original[4088]=0x25;original[4089]=0x20;original[4090]=1;original[4091]=8;
  for(int i=0;i<4096;i++)flash[i]=mode==0?original[i]:mode==1?(original[i]^key[i]):255;
  for(u32 off=0;off<=4096;off++){
   u8 out[66];for(int i=0;i<66;i++)out[i]=0xA5;
   u32 n=4096-off;if(n>64)n=64;reads=errors=0;
   HackSaveCrypt_ReadFlash(17,off,out+1,n);
   if(errors||reads!=2||out[0]!=0xA5)return 4;
   for(u32 i=0;i<n;i++)if(out[i+1]!=(mode==2?(255^key[off+i]):original[off+i]))return 5;
   for(u32 i=n+1;i<66;i++)if(out[i]!=0xA5)return 6;
  }
  static struct SaveSection section;reads=errors=0;
  if(HackSaveCrypt_DoReadFlashWholeSection(17,&section)!=TRUE||errors||reads!=2)return 7;
  for(int i=0;i<4096;i++)if(section.bytes[i]!=(mode==2?(255^key[i]):original[i]))return 8;
 }
 return 0;
}
'''.replace('PRODUCTION', production).replace('KEY', ','.join(str(key_at(i)) for i in range(4096)))
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('crypt' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'source_sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'result': 'passed',
              'key_positions': 4096, 'retry_patterns': 64, 'partial_read_cases': 12291, 'whole_read_cases': 3,
              'scope': 'Actual scrambling functions with controlled flash APIs; key parity, legacy/scrambled/erased reads, retry termination, error propagation and caller buffer restoration.',
              'limitations': 'Does not verify physical flash, power-loss recovery, banking, callers supplying out-of-range reads, linked ROM hooks, or GBA execution.'}
    (ROOT / 'audit/save_crypt_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Save scrambling: 4096 key positions, 64 retry patterns, 12291 partial and 3 whole reads passed')


if __name__ == '__main__':
    main()
