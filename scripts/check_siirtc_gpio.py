"""Execute SII RTC source with traced register access instead of physical GPIO."""
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/siirtc.c').read_text(encoding='utf-8')
    header = (ROOT / 'include/siirtc.h').read_text(encoding='utf-8')
    header = re.sub(r'^#include[^\n]*\n', '', header, flags=re.M)
    source = re.sub(r'^#include[^\n]*\n', '', source, flags=re.M)
    source = re.sub(r'^#define GPIO_PORT_(?:DATA|DIRECTION|READ_ENABLE)[^\n]*\n', '', source, flags=re.M)
    # Instrument only register accesses. All protocol/byte/lock code is retained.
    source = re.sub(r'GPIO_PORT_(DATA|DIRECTION|READ_ENABLE)\s*=\s*([^;]+);',
                    lambda m: 'WriteRegister(REG_' + m[1] + ', ' + m[2] + ');', source)
    source = source.replace('GPIO_PORT_DATA', 'ReadRegister(REG_DATA)')
    fixture = r'''
#include <stddef.h>
typedef unsigned char u8;typedef unsigned short u16;typedef volatile u16 vu16;typedef u8 bool8;
#define TRUE 1
#define FALSE 0
HEADER
enum {REG_DATA,REG_DIRECTION,REG_READ_ENABLE};
bool8 sLocked;
static struct {int read,reg,value;} trace[1024];
static int count,pos,errors,bit,bytes,requireLock;
static u8 input[7];
static void WriteRegister(int reg,int value){
    if(count>=1024){errors++;return;}
    if(requireLock && sLocked!=TRUE) errors++;
    trace[count].read=0;trace[count].reg=reg;trace[count++].value=value;
}
static int ReadRegister(int reg){
    if(count>=1024||reg!=REG_DATA||bit>=bytes*8){errors++;return 0;}
    if(requireLock && sLocked!=TRUE) errors++;
    int value=((input[bit/8]>>(bit%8))&1)*2;bit++;
    trace[count].read=1;trace[count].reg=reg;trace[count++].value=value;return value;
}
SOURCE
static void reset(int locked){count=pos=errors=bit=bytes=0;requireLock=locked;sLocked=FALSE;}
static void expected(int read,int reg,int value){
    if(pos>=count){errors++;return;}
    if(trace[pos].read!=read||trace[pos].reg!=reg||trace[pos].value!=value) errors++;
    pos++;
}
static void write(int reg,int value){expected(0,reg,value);}
static void command(u8 byte){
    for(int i=7;i>=0;i--){int low=4+((byte>>i)&1)*2;for(int n=0;n<3;n++) write(REG_DATA,low);write(REG_DATA,low+1);}
}
static void data(u8 byte){
    for(int i=0;i<8;i++){int low=4+((byte>>i)&1)*2;for(int n=0;n<3;n++) write(REG_DATA,low);write(REG_DATA,low+1);}
}
static void readByte(u8 byte){
    for(int i=0;i<8;i++){for(int n=0;n<5;n++) write(REG_DATA,4);write(REG_DATA,5);expected(1,REG_DATA,((byte>>i)&1)*2);}
}
static void begin(u8 byte){write(REG_DATA,1);write(REG_DATA,5);write(REG_DIRECTION,7);command(byte);}
static void end(void){write(REG_DATA,1);write(REG_DATA,1);}
static int finished(void){return !errors&&pos==count&&bit==bytes*8&&sLocked==FALSE;}
int main(void){
    for(unsigned value=0;value<256;value++){
        reset(0);WriteCommand(value);command(value);if(!finished()) return 1;
        reset(0);WriteData(value);data(value);if(!finished()) return 2;
        reset(0);input[0]=value;bytes=1;if(ReadData()!=value) return 3;readByte(value);if(!finished()) return 4;
        struct SiiRtcInfo rtc={0};
        reset(1);input[0]=value;bytes=1;if(!SiiRtcGetStatus(&rtc)) return 5;
        begin(0x63);write(REG_DIRECTION,5);readByte(value);end();
        unsigned decoded=(value&0xc0)|((value&0x20)>>3)|((value&8)>>2)|((value&2)>>1);
        if(!finished()||rtc.status!=decoded) return 6;
        reset(1);rtc.status=value;if(!SiiRtcSetStatus(&rtc)) return 7;
        begin(0x62);data(0x40|((value&4)<<3)|((value&2)<<2)|((value&1)<<1));end();if(!finished()) return 8;
    }
    for(int date=0;date<2;date++) for(unsigned index=0;index<(date?7:3);index++)
    for(unsigned value=0;value<256;value++){
        struct {u8 before;struct SiiRtcInfo rtc;u8 after;} guarded;
        guarded.before=0x5a;guarded.after=0xa5;
        for(unsigned i=0;i<sizeof(guarded.rtc);i++) ((u8*)&guarded.rtc)[i]=0xcc;
        reset(1);bytes=date?7:3;
        for(int i=0;i<bytes;i++) input[i]=(u8)(i*37+11);
        input[index]=value;
        if(!(date?SiiRtcGetDateTime(&guarded.rtc):SiiRtcGetTime(&guarded.rtc))) return 9;
        begin(date?0x65:0x67);write(REG_DIRECTION,5);
        for(int i=0;i<bytes;i++) readByte(input[i]);end();
        if(!finished()||guarded.before!=0x5a||guarded.after!=0xa5) return 10;
        unsigned start=date?0:4;
        for(unsigned i=0;i<sizeof(guarded.rtc);i++){
            u8 expectedValue=i>=start&&i<start+bytes?input[i-start]:0xcc;
            if(i==4) expectedValue&=0x7f;
            if(((u8*)&guarded.rtc)[i]!=expectedValue) return 11;
        }
    }
    reset(1);if(!SiiRtcReset()) return 12;
    /* This port disables the physical RESET command; its reset wrapper only
     * sets 24-hour mode. Verify that behavior without inventing extra writes. */
    begin(0x62);data(0x40);end();if(!finished()) return 13;
    struct SiiRtcInfo rtc={0};
    reset(1);sLocked=TRUE;
    if(SiiRtcGetStatus(&rtc)||SiiRtcSetStatus(&rtc)||SiiRtcGetDateTime(&rtc)||SiiRtcGetTime(&rtc)||SiiRtcReset()||count||sLocked!=TRUE) return 14;
    reset(0);SiiRtcUnprotect();write(REG_READ_ENABLE,1);if(!finished()) return 15;
    return 0;
}
'''.replace('HEADER', header).replace('SOURCE', source)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('rtc_gpio' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('SII GPIO: all byte values, 2560 date/time buffers, commands, register direction, lock exits and reset sequence pass.')


if __name__ == '__main__':
    main()
