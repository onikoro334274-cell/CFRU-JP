"""Definition imports and conditional directives used by insertion tables/strings."""
import re


def TryProcessFileInclusion(line, definesDict):
    if not line.lstrip().startswith('#include'):
        return False
    match = re.fullmatch(r'\s*#include\s+"([^"]+)"\s*(?://.*)?', line.rstrip())
    if match is None:
        raise ValueError('Malformed definition include: ' + line.strip())
    path = match[1]
    with open(path, 'r', encoding='utf-8-sig') as file:
        for number, definition in enumerate(file, 1):
            words = definition.strip().split()
            if words and words[0] == '#define':
                if len(words) < 2:
                    raise ValueError('Missing definition name in {}:{}'.format(path, number))
                value = True if len(words) == 2 or words[2].startswith(('//', '/*')) else words[2]
                definesDict[words[1]] = value
    return True


def TryProcessConditionalCompilation(line, definesDict, conditionals):
    words = line.split('//', 1)[0].strip().split()
    directive = words[0].upper() if words else ''
    if directive in ('#IF', '#ELIF'):
        raise ValueError('Use #ifdef/#ifndef for insertion-table conditions')
    if directive in ('#IFDEF', '#IFNDEF'):
        if len(words) != 2:
            raise ValueError('Conditional directive requires one definition name')
        enabled = words[1] in definesDict
        if directive == '#IFNDEF':
            enabled = not enabled
        conditionals.append({'enabled': enabled, 'else_seen': False})
        return True
    if directive in ('#ELSE', '#ENDIF'):
        if len(words) != 1 or not conditionals:
            raise ValueError('Unmatched or malformed ' + directive)
        if directive == '#ENDIF':
            conditionals.pop()
        else:
            frame = conditionals[-1]
            if frame['else_seen']:
                raise ValueError('Duplicate #else')
            frame['enabled'] = not frame['enabled']
            frame['else_seen'] = True
        return True
    return any(not frame['enabled'] for frame in conditionals)


def validate_conditionals(conditionals):
    if conditionals:
        raise ValueError('Missing #endif')


def validate_definition_file(path):
    defines = {}
    conditionals = []
    with open(path, 'r', encoding='utf-8-sig') as file:
        for line in file:
            if not TryProcessConditionalCompilation(line, defines, conditionals):
                TryProcessFileInclusion(line, defines)
    validate_conditionals(conditionals)
