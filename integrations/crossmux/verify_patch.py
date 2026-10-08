#!/usr/bin/env python3
"""Validate the pinned CrossMux integration; --apply modifies a clean checkout."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('checkout', type=Path)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    repo = args.checkout.resolve()
    manifest = json.loads((ROOT / 'upstream.json').read_text())
    patch = ROOT / 'coexistence.patch'

    def git(*arguments):
        return subprocess.check_output(['git', *arguments], cwd=repo, text=True).strip()

    if git('rev-parse', 'HEAD') != manifest['commit']:
        parser.error('Checkout is not the pinned upstream commit')
    if git('rev-parse', 'HEAD:freeink-sdk') != manifest['sdk_commit']:
        parser.error('SDK gitlink differs from the pinned upstream SDK')
    if git('status', '--porcelain', '--untracked-files=normal'):
        parser.error('Use a clean checkout; existing changes will not be overwritten')
    if hashlib.sha256(patch.read_bytes()).hexdigest() != manifest['patch_sha256']:
        parser.error('Patch SHA256 does not match upstream.json')
    subprocess.run(['git', 'apply', '--check', str(patch)], cwd=repo, check=True)
    if args.apply:
        subprocess.run(['git', 'apply', str(patch)], cwd=repo, check=True)
        for name, expected in manifest['changed_files'].items():
            if hashlib.sha256((repo / name).read_bytes()).hexdigest() != expected:
                parser.error('Patched file differs from the reviewed source: ' + name)
    print('Pinned CrossMux patch verified' + (' and applied' if args.apply else ''))


if __name__ == '__main__':
    main()
