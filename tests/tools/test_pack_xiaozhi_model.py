# SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
import struct
import sys
from pathlib import Path
import tempfile
import unittest
from unittest import mock
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from pack_xiaozhi_model import pack
from prepare_custom_wake_model import verified_resource, packed_resource
SOURCE = ROOT / 'managed_components/espressif__esp-sr/model/multinet_model/mn7_cn'

class WakeModelTests(unittest.TestCase):
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
