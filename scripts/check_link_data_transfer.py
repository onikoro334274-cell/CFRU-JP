"""Execute controller data transfers at every u16 length and buffer boundary."""
import argparse
import re
import subprocess
import tempfile
from pathlib import Path
from check_factory_party_copy import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline')
    args = parser.parse_args()
    source = (ROOT / 'src/link.c').read_text(encoding='utf-8')
    battle = (ROOT / 'include/battle.h').read_text(encoding='utf-8')
    assert int(re.search(r'gBattleBuffersTransferData\[(0x[0-9A-Fa-f]+|\d+)\]', battle)[1], 0) == 256
    assert int(re.search(r'gBattleBufferA\[MAX_BATTLERS_COUNT\]\[(0x[0-9A-Fa-f]+|\d+)\]', battle)[1], 0) == 512
    if args.baseline:
        source = subprocess.run(['git', 'show', args.baseline + ':src/link.c'], cwd=ROOT,
                                capture_output=True, encoding='utf-8', check=True).stdout
    header = (ROOT / 'include/battle_controllers.h').read_text(encoding='utf-8')
    controller_enum = re.search(r'enum\s*\{[^}]*CONTROLLER_DATATRANSFER[^}]*\};', header)[0]
    constants = (ROOT / 'include/constants/battle.h').read_text(encoding='utf-8')
    defines = '\n'.join(re.search(r'^#define ' + name + r'\s+[^\n]*', constants, re.M)[0]
                        for name in ('MAX_BATTLERS_COUNT', 'B_SIDE_OPPONENT', 'BATTLE_TYPE_LINK'))
    defines += '\n' + '\n'.join(re.findall(r'^#define DATA_TRANSFER_HEADER_SIZE[^\n]*', source, re.M))
    production = function(source, 'void EmitDataTransfer(u8 bufferId, void* dst, u16 size, void* data)') + '\n' + function(source, 'void HandleDataTransfer(void)')
    fixture = r'''
#include <stddef.h>
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef int s32;
#define TRUE 1
DEFINES
CONTROLLER_ENUM
static struct {u8 before;u8 payload[0x100];u8 after[65536];} transfer;
#define gBattleBuffersTransferData transfer.payload
static u8 gBattleBufferA[MAX_BATTLERS_COUNT][0x200],gActiveBattler;
static u32 gBattleTypeFlags;
static u8 input[65536],output[512];
static int prepares,copies,players,opponents,links,errors;
static unsigned expectedSize;static u8 expectedBuffer;
static void PrepareBufferDataTransfer(u8 id,void *data,u16 length){
    prepares++;if(id!=expectedBuffer||data!=transfer.payload||length!=expectedSize+7) errors++;
}
static void Memcpy(void *dst,const void *src,u32 size){
    copies++;if(dst!=(void*)0x02023400||src!=gBattleBufferA[gActiveBattler]+7||size!=expectedSize||size>505){errors++;return;}
    for(unsigned i=0;i<size;i++) output[i]=((const u8*)src)[i];
}
static u8 GetBattlerSide(u8 bank){return bank&1;}
#define SIDE(bank) GetBattlerSide(bank)
static void PlayerBufferExecCompleted(void){players++;}
static void OpponentBufferExecCompleted(void){opponents++;}
static void LinkOpponentBufferExecCompleted(void){links++;}
PRODUCTION
static void reset(void){prepares=copies=players=opponents=links=errors=0;}
int main(void){
    for(unsigned i=0;i<65536;i++) input[i]=(u8)(i*37+11);
    transfer.before=transfer.after[0]=0xa5;
    for(unsigned size=0;size<65536;size++) for(unsigned bank=0;bank<2;bank++){
        reset();for(unsigned i=0;i<256;i++) transfer.payload[i]=0xcc;
        expectedSize=size;expectedBuffer=bank;
        EmitDataTransfer(bank,(void*)0x02023400,size,input);
        int valid=size<=249;
        if(prepares!=valid||errors||transfer.before!=0xa5||transfer.after[0]!=0xa5) return 1;
        if(valid){
            const u8 head[]={CONTROLLER_DATATRANSFER,0,0x34,0x02,0x02,(u8)size,(u8)(size>>8)};
            for(unsigned i=0;i<7;i++) if(transfer.payload[i]!=head[i]) return 2;
            for(unsigned i=0;i<size;i++) if(transfer.payload[i+7]!=input[i]) return 3;
            for(unsigned i=size+7;i<256;i++) if(transfer.payload[i]!=0xcc) return 4;
        }else for(unsigned i=0;i<256;i++) if(transfer.payload[i]!=0xcc) return 5;
    }
    for(unsigned zero=0;zero<2;zero++) for(unsigned nullDst=0;nullDst<2;nullDst++)
    for(unsigned nullSrc=0;nullSrc<2;nullSrc++){
        reset();expectedSize=zero?0:1;expectedBuffer=0;
        EmitDataTransfer(0,nullDst?NULL:(void*)0x02023400,expectedSize,nullSrc?NULL:input);
        if(prepares!=(zero||(!nullDst&&!nullSrc))||errors) return 6;
    }
    for(unsigned battler=0;battler<4;battler++) for(unsigned link=0;link<2;link++)
    for(unsigned size=0;size<65536;size++){
        reset();gActiveBattler=battler;gBattleTypeFlags=link?BATTLE_TYPE_LINK:0;expectedSize=size;
        u8 *packet=gBattleBufferA[battler];packet[1]=0;packet[2]=0x34;packet[3]=2;packet[4]=2;
        packet[5]=size;packet[6]=size>>8;
        for(unsigned i=0;i<505;i++) packet[7+i]=(u8)(i*17+3);
        for(unsigned i=0;i<512;i++) output[i]=0xcc;
        HandleDataTransfer();int valid=size>0&&size<=505;
        if(copies!=valid||errors) return 7;
        if(players!=(!(battler&1))||opponents!=((battler&1)&&!link)||links!=((battler&1)&&link)) return 8;
        for(unsigned i=0;i<512;i++) if(output[i]!=(valid&&i<size?packet[7+i]:0xcc)) return 9;
    }
    for(unsigned id=4;id<256;id++){
        reset();gActiveBattler=id;HandleDataTransfer();if(copies||players||opponents||links) return 10;
    }
    reset();gActiveBattler=0;gBattleBufferA[0][1]=gBattleBufferA[0][2]=gBattleBufferA[0][3]=gBattleBufferA[0][4]=0;
    gBattleBufferA[0][5]=1;gBattleBufferA[0][6]=0;HandleDataTransfer();if(copies||players!=1) return 11;
    return 0;
}
'''.replace('DEFINES', defines).replace('CONTROLLER_ENUM', controller_enum).replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('link_transfer' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-Wno-pointer-to-int-cast', '-Wno-int-to-pointer-cast',
                        '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Link transfer: all u16 lengths, headers, buffer canaries, NULLs, battler bounds and completion routes pass.')


if __name__ == '__main__':
    main()
