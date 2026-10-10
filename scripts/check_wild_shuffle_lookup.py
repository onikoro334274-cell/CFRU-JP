"""Compare indexed wild lookup with full search for all current generated entries."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def block(source, name):
    return re.search(name + r'\[\]\s*=\s*\{(.*?)\};', source, re.S)[1]


def main():
    tables = (ROOT / 'src/Tables/wild_encounter_tables.c').read_text(encoding='utf-8')
    pairs = [tuple(part.strip() for part in row.split(','))
             for row in re.findall(r'\{([^{}]+)\}', block(tables, 'gHackWildShufflePairs'))]
    index = [tuple(int(part.strip(), 0) for part in row.split(','))
             for row in re.findall(r'\{([^{}]+)\}', block(tables, 'gHackWildShuffleIndex'))]
    assert len(pairs) == int(re.search(r'gHackWildShufflePairCount\s*=\s*(\d+)', tables)[1])
    assert len(index) == int(re.search(r'gHackWildShuffleIndexCount\s*=\s*(\d+)', tables)[1])
    cursor = 0
    for i, (key, start, count) in enumerate(index):
        assert start == cursor and count > 0 and start + count <= len(pairs)
        assert i == 0 or index[i - 1][0] < key
        cursor += count
    assert cursor == len(pairs)
    tokens = {value for row in pairs for value in row[:2]}
    identities = {value: i + 1 for i, value in enumerate(sorted(tokens))}
    rows = []
    for original, shuffled, region, variant in pairs:
        assert int(region) in (0, 1) and 0 <= int(variant) < 4
        rows.append('{(const struct WildPokemonInfo*)' + str(identities[original]) +
                    ',(const struct WildPokemonInfo*)' + str(identities[shuffled]) + ',' + region + ',' + variant + '}')
    source = (ROOT / 'src/wild_shuffle.c').read_text(encoding='utf-8')
    source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
    # Select the non-profile implementation and remove conditional declaration lines.
    tail = source[source.rfind('const struct WildPokemonInfo* HackWild_Shuffle('):]
    tail = re.sub(r'^#.*$', '', tail, flags=re.M)
    production = function(source, 'FindShuffled') + '\n' + function(tail, 'HackWild_Shuffle')
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
#define NULL ((void*)0)
#define TRUE 1
#define FALSE 0
#define HACK_FLAG_HABITAT_ORIGINAL 1
#define WILD_SHUFFLE_REGION_KANTO 0
#define WILD_SHUFFLE_REGION_HOENN 1
struct WildPokemonInfo {int unused;};
struct HackWildShufflePair {const struct WildPokemonInfo* orig;const struct WildPokemonInfo* shuffled;u8 region,variant;};
struct HackWildShuffleIndex {u16 map,start,count;};
static const struct HackWildShufflePair gHackWildShufflePairs[]={PAIRS};
static const struct HackWildShuffleIndex gHackWildShuffleIndex[]={INDEX};
static const u16 gHackWildShufflePairCount=PAIR_COUNT,gHackWildShuffleIndexCount=INDEX_COUNT;
static struct {struct {u8 mapGroup,mapNum;} location;} save;
#define gSaveBlock1 (&save)
static int habitat,variant;
static int FlagGet(int flag){(void)flag;return habitat;}
static u8 KantoVariant(void){return variant;}
PRODUCTION
int main(void){
 if(HackWild_Shuffle(NULL)!=NULL)return 1;
 for(u32 map=0;map<INDEX_COUNT;map++){
  save.location.mapGroup=gHackWildShuffleIndex[map].map>>8;
  save.location.mapNum=gHackWildShuffleIndex[map].map&255;
  for(u32 i=gHackWildShuffleIndex[map].start;i<(u32)gHackWildShuffleIndex[map].start+gHackWildShuffleIndex[map].count;i++)
  for(variant=0;variant<4;variant++)for(habitat=0;habitat<2;habitat++){
   const struct WildPokemonInfo* info=gHackWildShufflePairs[i].orig,*expected=info;
   if(!habitat)FindShuffled(info,variant,TRUE,TRUE,0,PAIR_COUNT,&expected);
   if(HackWild_Shuffle(info)!=expected)return 2;
  }
 }
 /* Unknown map forces whole-table fallback; unknown table preserves its pointer. */
 save.location.mapGroup=255;save.location.mapNum=255;habitat=0;
 for(u32 i=0;i<PAIR_COUNT;i++)for(variant=0;variant<4;variant++){
  const struct WildPokemonInfo* info=gHackWildShufflePairs[i].orig,*expected=info;
  FindShuffled(info,variant,TRUE,TRUE,0,PAIR_COUNT,&expected);
  if(HackWild_Shuffle(info)!=expected)return 3;
 }
 const struct WildPokemonInfo* absent=(const struct WildPokemonInfo*)999999;
 if(HackWild_Shuffle(absent)!=absent)return 4;
 return 0;}
'''.replace('PRODUCTION', production).replace('PAIRS', ',\n'.join(rows))
    fixture = fixture.replace('INDEX_COUNT', str(len(index))).replace('PAIR_COUNT', str(len(pairs)))
    fixture = fixture.replace('INDEX', ','.join('{%d,%d,%d}' % row for row in index))
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('lookup' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'result': 'passed', 'pair_entries': len(pairs), 'map_index_entries': len(index),
              'lookup_cases': len(pairs) * 12 + 2,
              'scope': 'Actual lookup functions and generated pair/index metadata; contiguous sorted index coverage, all variants, original-habitat bypass, indexed/full-search equality and unknown-map fallback.',
              'limitations': 'Symbol identities represented by opaque tokens; linked aliases, generated encounter contents, date/time variant choice and GBA runtime unverified.'}
    (ROOT / 'audit/wild_shuffle_lookup_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'{len(pairs)} pairs / {len(index)} maps: indexed and full wild search match')


if __name__ == '__main__':
    main()
