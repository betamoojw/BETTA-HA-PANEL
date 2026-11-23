# SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
import struct
import sys
import random
import subprocess
import zlib
from pathlib import Path
import tempfile
import unittest
from unittest import mock
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from pack_xiaozhi_model import pack, compress_model
from prepare_custom_wake_model import verified_resource, packed_resource
SOURCE = ROOT / 'managed_components/espressif__esp-sr/model/multinet_model/mn7_cn'

class WakeModelTests(unittest.TestCase):
    def test_preset_compression_preserves_catalog_and_chooses_smaller_stream(self):
        rng = random.Random(27)
        fixtures = {
            'repeated': b'weights' * 2048,
            'small_signed': bytes(rng.choice((-3, -2, -1, 0, 1, 2, 3)) & 255 for _ in range(12000)),
            'full_signed': bytes(rng.randrange(-128, 128) & 255 for _ in range(12000)),
        }
        choices = set()
        for kind, weights in fixtures.items():
            with self.subTest(kind=kind), tempfile.TemporaryDirectory() as tmp:
                source = Path(tmp) / 'wn9_fixture'
                source.mkdir()
                payloads = {'_MODEL_INFO_': b'synthetic model', 'wn9_index': b'index', 'wn9_data': weights}
                for name, data in payloads.items():
                    (source / name).write_bytes(data)
                raw = pack(source)
                packed = compress_model(raw)
                self.assertEqual(packed, compress_model(raw))
                self.assertEqual(struct.unpack_from('<I', packed)[0], len(raw))
                decoder = zlib.decompressobj()
                decoded = decoder.decompress(packed[4:]) + decoder.flush()
                self.assertTrue(decoder.eof)
                self.assertEqual(decoder.unused_data, b'')
                self.assertEqual(decoded, raw)
                count, name, files = struct.unpack_from('<I32sI', decoded)
                self.assertEqual((count, name.rstrip(b'\0'), files), (1, b'wn9_fixture', 3))
                next_offset = 160
                for i, filename in enumerate(sorted(payloads)):
                    name, offset, size = struct.unpack_from('<32sII', decoded, 40 + 40 * i)
                    self.assertEqual(name.rstrip(b'\0').decode(), filename)
                    self.assertEqual(offset, next_offset)
                    self.assertEqual(decoded[offset:offset + size], payloads[filename])
                    next_offset += size
                self.assertEqual(next_offset, len(decoded))
                filtered = zlib.compressobj(level=9, memLevel=6, strategy=zlib.Z_FILTERED)
                candidates = (zlib.compress(raw, 9), filtered.compress(raw) + filtered.flush())
                chosen = min(range(2), key=lambda i: len(candidates[i]))
                choices.add(chosen)
                self.assertEqual(packed[4:], candidates[chosen])
        self.assertEqual(choices, {0, 1})

    def test_preset_cli_preserves_raw_output_and_compressed_header(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / 'wn9_fixture'
            source.mkdir()
            for name, data in {'_MODEL_INFO_': b'info', 'wn9_index': b'index', 'wn9_data': b'weights' * 256}.items():
                (source / name).write_bytes(data)
            output = Path(tmp) / 'model.bin'
            command = [sys.executable, str(ROOT / 'tools/pack_xiaozhi_model.py'), str(source), str(output)]
            subprocess.run(command, check=True)
            self.assertEqual(output.read_bytes(), pack(source))
            subprocess.run(command + ['--compress'], check=True)
            self.assertEqual(output.read_bytes(), compress_model(pack(source)))
            written = output.stat().st_mtime_ns
            subprocess.run(command + ['--compress'], check=True)
            self.assertEqual(output.stat().st_mtime_ns, written)

    def test_model_catalog_and_seed_offsets_without_idf_dependencies(self):
        payloads = {'_MODEL_INFO_': b'info', 'mn7_index': b'index', 'mn7_data': b'model', 'vocab': b'vocabulary'}
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / 'mn7_cn'
            source.mkdir()
            for name, data in payloads.items():
                (source / name).write_bytes(data)
            raw = packed_resource(source)
            # MultiNet must use the dedicated two-catalog packer, not a preset
            # blob missing the FST required by the vendor initializer.
            with self.assertRaises(ValueError):
                pack(source)
        count, name, files = struct.unpack_from('<I32sI', raw)
        self.assertEqual((count, name.rstrip(b'\0'), files), (2, b'mn7_cn', 4))
        next_offset = 276
        for i in range(files):
            name, offset, size = struct.unpack_from('<32sII', raw, 40 + 40 * i)
            self.assertEqual(offset, next_offset)
            self.assertEqual(raw[offset:offset + size], payloads[name.rstrip(b'\0').decode()])
            next_offset += size
        name, count = struct.unpack_from('<32sI', raw, 200)
        self.assertEqual((name.rstrip(b'\0'), count), (b'fst', 1))
        filename, offset, size = struct.unpack_from('<32sII', raw, 236)
        self.assertEqual(filename.rstrip(b'\0'), b'commands_cn.txt')
        self.assertEqual(offset, next_offset)
        self.assertEqual(raw[offset:offset + size], b'1,ni hao xiao zhi\n')
        self.assertEqual(offset + size, len(raw))

    def test_changed_resource_is_rejected(self):
        with mock.patch('prepare_custom_wake_model.packed_resource', return_value=b'wrong model'):
            with self.assertRaisesRegex(ValueError, 'differs from the pinned resource'):
                verified_resource()

    @unittest.skipUnless(SOURCE.is_dir(), 'Pinned ESP-SR model is checked after the panel10 dependencies are fetched')
    def test_installed_pinned_resource(self):
        raw, meta = verified_resource()
        self.assertEqual(len(raw), meta['bytes'])
        self.assertEqual(raw, packed_resource())

if __name__ == '__main__':
    unittest.main()
