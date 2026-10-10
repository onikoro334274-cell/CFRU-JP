"""Exercise production time predicates and final encounter variant over a leap cycle."""
import datetime
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/wild_shuffle.c').read_text(encoding='utf-8')
    dns = (ROOT / 'src/dns.c').read_text(encoding='utf-8')
    config = (ROOT / 'src/config.h').read_text(encoding='utf-8')
    names = ['TIME_MORNING_START', 'TIME_DAY_START', 'TIME_EVENING_START', 'TIME_NIGHT_START']
    times = [int(re.search(r'#define\s+' + name + r'\s+(\d+)', config)[1]) for name in names]
    variants = int(re.search(r'#define\s+HACK_WILD_KANTO_VARIANTS\s+(\d+)', source)[1])
    assert 0 < times[0] < times[1] < times[2] < times[3] < 24 and variants > 0

    def expected(date, slot):
        key = date.year * 372 + date.month * 31 + date.day
        value = (((key * 4 + slot) ^ 0x5A17) * 0x9E3779B1) & 0xFFFFFFFF
        value ^= value >> 15
        value = (value * 0x2C1B3C6D) & 0xFFFFFFFF
        value ^= value >> 13
        return value % variants

    rows = []
    date = datetime.date(2000, 1, 1)
    while date < datetime.date(2400, 1, 1):
        values = [expected(date, slot) for slot in range(4)]
        values.append(expected(date - datetime.timedelta(days=1), 3))
        rows.append('{%d,%d,%d,{%s}}' % (date.year, date.month, date.day, ','.join(map(str, values))))
        date += datetime.timedelta(days=1)
    definitions = '\n'.join('#define %s %d' % pair for pair in zip(names, times))
    definitions += '\n#define HACK_WILD_KANTO_VARIANTS %d\n' % variants
    production = '\n'.join(function(dns[dns.index('bool8 ' + name + '(void)'):], name)
                           for name in ['IsNightTime', 'IsEvening', 'IsMorning'])
    production += '\n' + function(source, 'PreviousWildDate') + '\n' + function(source, 'KantoVariant')
    fixture = r'''
typedef unsigned char u8; typedef unsigned char bool8; typedef unsigned int u32;
struct {u32 year,month,day,hour;} gClock;
DEFINITIONS
PRODUCTION
static const struct {u32 year;u8 month,day,expected[5];} cases[]={CASES};
int main(void){
 for(u32 i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
  gClock.year=cases[i].year;gClock.month=cases[i].month;gClock.day=cases[i].day;
  for(u32 hour=0;hour<24;hour++){
   gClock.hour=hour;
   u32 slot=hour<TIME_MORNING_START?4:hour<TIME_DAY_START?0:hour<TIME_EVENING_START?1:hour<TIME_NIGHT_START?2:3;
   if(KantoVariant()!=cases[i].expected[slot])return 1;
  }
 }return 0;
}
'''.replace('DEFINITIONS', definitions).replace('PRODUCTION', production).replace('CASES', ',\n'.join(rows))
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('variant' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'result': 'passed', 'date_cases': len(rows), 'hour_cases': len(rows) * 24,
              'scope': 'Actual time predicates, previous-date helper and final KantoVariant hash against independent Gregorian/32-bit Python oracle for every hour of 2000..2399.',
              'limitations': 'Valid clock fields only. RTC acquisition, linked ROM layout and GBA runtime unverified.'}
    (ROOT / 'audit/wild_variant_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'{len(rows) * 24} date/hour variant cases passed')


if __name__ == '__main__':
    main()
