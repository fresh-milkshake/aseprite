"""Regenerate native string IDs and the Lua source-to-ID map from translations.json."""
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
EXTENSION = ROOT / 'data/extensions/minecraft-tools'
catalogue = json.loads((EXTENSION / 'translations.json').read_text(encoding='utf-8'))
ids = {source: 't_' + hashlib.sha1(source.encode()).hexdigest()[:12] for source in catalogue}
assert len(set(ids.values())) == len(ids), 'Translation ID collision'
for language in ('en', 'ru'):
    path = ROOT / f'data/strings/{language}.ini'
    text = path.read_text(encoding='utf-8')
    section = '[minecraft]\n' + ''.join(
        f'{ids[source]} = ' + (source if language == 'en' else catalogue[source]).strip().replace('\n', '\\n') + '\n'
        for source in sorted(catalogue))
    text, count = re.subn(r'(?ms)^\[minecraft\]\n.*?(?=^\[|\Z)', lambda _: section + '\n', text)
    assert count == 1, 'Expected a single Minecraft section'
    path.write_text(text, encoding='utf-8')
lines = ['-- English source text maps to native localization IDs.',
         '-- Generated from translations.json; CC BY 4.0 (see docs/LICENSES.md).',
         'return {']
for source in sorted(catalogue):
    lines.append('  [' + json.dumps(source, ensure_ascii=False) + ']=' + json.dumps('minecraft.' + ids[source]) + ',')
lines.append('}')
(EXTENSION / 'minecraft_tools_catalog.lua').write_text('\n'.join(lines) + '\n', encoding='utf-8')
