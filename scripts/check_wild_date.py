"""Compare prior-night date keys against the Gregorian calendar for a 400-year cycle."""
import datetime
import json
from pathlib import Path
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/wild_shuffle.c').read_text(encoding='utf-8')
    rows = []
    date = datetime.date(2000, 1, 1)
    end = datetime.date(2400, 1, 1)
    while date < end:
        previous = date - datetime.timedelta(days=1)
        expected = previous.year * 372 + previous.month * 31 + previous.day
        rows.append('{%d,%d,%d,%d}' % (date.year, date.month, date.day, expected))
        date += datetime.timedelta(days=1)
    fixture = r'''
typedef unsigned char u8;typedef unsigned int u32;
PRODUCTION
static const struct {u32 year,month,day,expected;} cases[]={CASES};
int main(void){
 for(u32 i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
  if(PreviousWildDate(cases[i].year,cases[i].month,cases[i].day)!=cases[i].expected)return 1;
 }return 0;}
'''.replace('PRODUCTION', function(source, 'PreviousWildDate')).replace('CASES', ',\n'.join(rows))
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('date' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'result': 'passed', 'date_cases': len(rows), 'years': '2000..2399',
              'scope': 'Production previous-night date key against Python Gregorian date subtraction; complete 400-year leap cycle including month/year boundaries.',
              'limitations': 'Date helper only; final variant hash/timeband predicates, invalid clock fields and GBA runtime unverified.'}
    (ROOT / 'audit/wild_date_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'{len(rows)} prior-night date cases passed across a 400-year calendar cycle')


if __name__ == '__main__':
    main()
