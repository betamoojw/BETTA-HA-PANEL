# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
"""Prepare pinned Otto 1.4.0 GIFs; embed maintained assets without Pillow at build time."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / 'main/ui/assets/otto'
REVISION = '4279001383c2b1a39ed0457b14759e7c87537941'


def prepare(source):
    from PIL import Image, ImageSequence
    ASSETS.mkdir(parents=True, exist_ok=True)
    manifest = {'component': 'txp666/otto-emoji-gif-component', 'version': '1.4.0',
                'revision': REVISION, 'size': 96, 'assets': {}}
    palette = bytes([v for i in range(16) for v in [i * 17] * 3] + [0, 0, 0] * 16)
    for path in sorted((source / 'gifs').glob('*.gif')):
        frames, durations = [], []
        with Image.open(path) as gif:
            for frame in ImageSequence.Iterator(gif):
                durations.append(frame.info.get('duration', 80))
                frame = frame.convert('L').resize((96, 96), Image.Resampling.LANCZOS)
                indexed = frame.point(lambda value: round(value / 17))
                compact = Image.frombytes('P', indexed.size, indexed.tobytes())
                compact.putpalette(palette)
                frames.append(compact)
        target = ASSETS / path.name
        frames[0].save(target, save_all=True, append_images=frames[1:], duration=durations,
                       loop=0, disposal=1, optimize=True, palette=palette, transparency=16)
        manifest['assets'][path.stem] = {
            'source_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
            'sha256': hashlib.sha256(target.read_bytes()).hexdigest(),
            'duration_ms': sum(durations)}
    if len(manifest['assets']) != 21:
        raise ValueError('Expected all 21 upstream expressions')
    (ASSETS / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    (ASSETS / 'LICENSE').write_bytes((source / 'LICENSE').read_bytes())


def embed(target):
    manifest = json.loads((ASSETS / 'manifest.json').read_text(encoding='utf-8'))
    lines = ['/* Generated Otto assets, MIT, Copyright (c) 2025 小鹏. See assets/otto/LICENSE. */',
             '#include "xiaozhi/xiaozhi_emoji.h"', '#include <string.h>']
    for name, meta in manifest['assets'].items():
        data = (ASSETS / (name + '.gif')).read_bytes()
        if hashlib.sha256(data).hexdigest() != meta['sha256']:
            raise ValueError('Asset checksum mismatch: ' + name)
        lines.append(f'static const unsigned char data_{name}[] = {{')
        lines.extend(','.join(f'0x{b:02x}' for b in data[i:i+16]) + ',' for i in range(0, len(data), 16))
        lines.append('};')
        lines.append(f'static const lv_image_dsc_t img_{name} = {{.header = {{.magic=LV_IMAGE_HEADER_MAGIC, '
                     f'.cf=LV_COLOR_FORMAT_RAW, .w=96, .h=96}}, .data_size=sizeof(data_{name}), .data=data_{name}}};')
    lines.append('const lv_image_dsc_t *xiaozhi_otto_asset(const char *name) {')
    for name in manifest['assets']:
        lines.append(f'    if (name && !strcmp(name, "{name}")) return &img_{name};')
    lines.extend(['    return &img_neutral;', '}'])
    output = '\n'.join(lines) + '\n'
    if not target.exists() or target.read_text(encoding='utf-8') != output:
        target.write_text(output, encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, help='Extracted, pinned upstream component')
    parser.add_argument('--embed', type=Path, help='Generate build-only C arrays')
    args = parser.parse_args()
    if args.source:
        prepare(args.source)
    if args.embed:
        embed(args.embed)
