# SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
"""Release packaging must consume the model from the matching finished build."""
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
BASH = shutil.which('bash')


@unittest.skipUnless(BASH, 'Release packaging requires Bash')
class WakeResourcePackagingTests(unittest.TestCase):
    def package(self, model):
        with tempfile.TemporaryDirectory(prefix='betta-package-test-') as tmp:
            root = Path(tmp).resolve()
            scripts = root / 'scripts'; scripts.mkdir()
            script = scripts / 'package_release.sh'
            script.write_bytes((ROOT / 'scripts/package_release.sh').read_bytes())
            build = root / 'build/panel10'; build.mkdir(parents=True)
            (build / 'project_description.json').write_text(json.dumps({'project_version': 'test', 'target': 'esp32p4'}))
            (build / 'flash_args').write_text('0x20000 app.bin\n')
            (build / 'app.bin').write_bytes(b'app')
            if model is not None:
                (build / 'xiaozhi-custom-wake.bin').write_bytes(model)
            # There are deliberately no managed_components or model generator.
            # Stub only the external image merger and portable SHA CLI.
            bin_dir = root / 'bin'; bin_dir.mkdir()
            helpers = {
                'python3': '#!/bin/sh\nexec ' + shlex.quote(Path(sys.executable).as_posix()) + ' "$@"\n',
                'esptool': '#!/usr/bin/env python3\nfrom pathlib import Path\nimport sys\nPath(sys.argv[sys.argv.index("-o")+1]).write_bytes(b"factory")\n',
                'shasum': '#!/usr/bin/env python3\nimport hashlib,sys\nfrom pathlib import Path\nfor p in sys.argv[3:]: print(hashlib.sha256(Path(p).read_bytes()).hexdigest()+"  "+p)\n',
            }
            for name, contents in helpers.items():
                helper = bin_dir / name
                helper.write_text(contents, encoding='utf-8', newline='\n'); helper.chmod(0o755)
            out = root / 'release/out/test'
            self.assertTrue(out.resolve().is_relative_to(root))
            # Set PATH after Bash starts: Git Bash can reorder an inherited
            # Windows PATH and otherwise find an installed esptool first.
            launch = ('helpers=$1; '
                      'if command -v cygpath >/dev/null 2>&1; then helpers=$(cygpath -u "$helpers"); fi; '
                      'export PATH="$helpers:$PATH"; exec bash "$2" test panel10')
            result = subprocess.run([BASH, '-c', launch, 'package-test', str(bin_dir), str(script)],
                                    capture_output=True, text=True, timeout=30)
            if model is None:
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('wake model missing', result.stderr)
            else:
                self.assertEqual(result.returncode, 0, result.stderr)
                artifact = out / 'betta86-ha-panel-test-panel10.wake-model.bin'
                self.assertEqual(artifact.read_bytes(), model)
                self.assertIn(artifact.name, (out / 'SHA256SUMS').read_text())

    def test_finished_resource_is_copied_without_current_dependencies(self):
        self.package(b'matching build resource')

    def test_missing_finished_resource_is_rejected(self):
        self.package(None)
