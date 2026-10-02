#!/usr/bin/env python3
"""Check tracked files and reachable history without printing credential values."""
import hashlib
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PUBLIC_DEVELOPMENT_DIGEST = "e57311281ea0478a80be57036768ef71481804eab73182b2b70255311a2ded6a"
TOKEN = re.compile(rb"mgst_[A-Za-z0-9_-]{43}")
OTHER_SECRET = re.compile(
    rb"(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|"
    rb"AKIA[0-9A-Z]{16}|-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----)"
)
GENERATED = re.compile(
    r"(?:^|/)(?:artifacts|private|managed_components|\.tools|\.references|build[^/]*)/"
    r"|(?:^|/)(?:sdkconfig(?:\.old)?|\.env(?:\..*)?)$"
    r"|\.(?:bin|elf|map|tar\.gz|zip|log)$"
)


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, check=True).stdout


def inspect(path, data, where):
    problems = []
    if GENERATED.search(path):
        problems.append(f"{where}: generated/private artifact at {path}")
    tokens = TOKEN.findall(data)
    if tokens:
        # This exact synthetic fixture is intentionally public, not a credential.
        allowed = path == "esp32/tests/link_pairing_handshake_harness.c" and all(
            value == b"mgst_" + b"A" * 43 for value in tokens
        )
        if not allowed:
            problems.append(f"{where}: possible Muse SDK token in {path}")
    if OTHER_SECRET.search(data):
        allowed = path == "esp32/dev_signing_key.pem" and hashlib.sha256(data).hexdigest() == PUBLIC_DEVELOPMENT_DIGEST
        if not allowed:
            problems.append(f"{where}: possible credential/key in {path}")
    if re.search(rb"/(?:Users|home)/[A-Za-z0-9_.-]+/", data):
        problems.append(f"{where}: machine-specific home path in {path}")
    return problems


def main():
    paths = git("ls-files", "-z").decode().split("\0")
    problems = []
    count = 0
    for path in filter(None, paths):
        full = ROOT / path
        if full.is_symlink():
            problems.append(f"tree: review symlink at {path}")
        elif full.exists():
            count += 1
            problems.extend(inspect(path, full.read_bytes(), "tree"))
        else:
            problems.append(f"tree: missing tracked file at {path}")

    # Scan every reachable version of each blob, not just the latest commit.
    entries = git("rev-list", "--objects", "--all").decode().splitlines()
    objects = [entry.split(" ", 1) for entry in entries if " " in entry]
    history_count = 0
    if objects:
        # One Git process avoids starting hundreds of processes on large trees.
        batch = subprocess.run(
            ["git", "cat-file", "--batch"], cwd=ROOT,
            input="".join(oid + "\n" for oid, _ in objects).encode(),
            capture_output=True, check=True).stdout
        cursor = 0
        for _, path in objects:
            end = batch.index(b"\n", cursor)
            _, kind, size = batch[cursor:end].split()
            start = end + 1
            cursor = start + int(size) + 1
            if kind != b"blob":
                continue
            history_count += 1
            problems.extend(inspect(path, batch[start:cursor - 1], "history"))
    if problems:
        for problem in sorted(set(problems)):
            print(problem, file=sys.stderr)
        return 1
    if not count:
        print("No tracked files: stage the publication tree before checking.", file=sys.stderr)
        return 1
    print(f"Public-tree checks passed: {count} tracked files, {history_count} historical blobs.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
