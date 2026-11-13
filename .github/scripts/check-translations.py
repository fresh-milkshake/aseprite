"""Validate bundled UI catalogues without starting Aseprite."""
import configparser
import hashlib
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]

def load(language):
    parser = configparser.ConfigParser(interpolation=None, strict=True)
    parser.optionxform = str
    parser.read(ROOT / f'data/strings/{language}.ini', encoding='utf-8')
    return {f'{s}.{k}': v for s in parser.sections() for k, v in parser[s].items()}

def fields(text):
    return sorted(re.findall(r'(?<!\{)\{([^{}]*)\}(?!\})', text))

en, ru = load('en'), load('ru')
assert en.keys() == ru.keys(), f'Russian key mismatch: {en.keys() ^ ru.keys()}'
# These are technical names/abbreviations, not untranslated interface prose.
technical = {'RGB', 'RGBA', 'HSV', 'HSL', 'RotSprite', 'UUID:', 'CIEXYZ', 'CIELAB',
             'Wintab', 'Ctrl', 'Cmd', 'Alt', 'Shift', 'Win', 'Java', 'Minecraft'}
for key, source in en.items():
    target = ru[key]
    assert target or not source, f'Empty Russian string: {key}'
    assert fields(source) == fields(target), f'Format fields differ: {key}'
    assert re.findall(r'%[ds]', source) == re.findall(r'%[ds]', target), key
    if re.search(r'[A-Za-z]{3}', source) and source == target:
        assert source.replace('&', '') in technical, f'Untranslated: {key}'

extension = ROOT / 'data/extensions/minecraft-tools'
catalogue = json.loads((extension / 'translations.json').read_text(encoding='utf-8'))
for source, target in catalogue.items():
    key = 'minecraft.t_' + hashlib.sha1(source.encode()).hexdigest()[:12]
    assert en[key] == source.strip() and ru[key] == target.strip(), key
for path in extension.glob('*.lua'):
    if path.stem in ('minecraft_tools_i18n', 'minecraft_tools_catalog'):
        continue
    for source in re.findall(r'"([^"\n]*)"', path.read_text(encoding='utf-8')):
        if ' ' in source and re.search(r'[A-Z]', source):
            assert source in catalogue, f'Uncatalogued UI/error text in {path.name}: {source}'

# Matching EN/RU key counts cannot detect a key absent from both catalogues.
# Validate the actual native widget references as well.
for path in (ROOT / 'data/widgets').glob('*.xml'):
    tree = ET.parse(path)
    for root in tree.getroot():
        prefix = root.get('id', '')
        for element in root.iter():
            for value in element.attrib.values():
                if value.startswith('@'):
                    key = prefix + value[1:] if value.startswith('@.') else value[1:]
                    assert key in en, f'Missing native widget translation in {path.name}: {key}'

print(f'Validated {len(en)} English/Russian UI keys and {len(catalogue)} Minecraft entries')
