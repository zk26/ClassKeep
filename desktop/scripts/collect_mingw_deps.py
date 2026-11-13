#!/usr/bin/env python3
"""Copy the complete MinGW/Qt runtime DLL closure into a payload directory.

A hand-written whitelist of DLL prefixes is unreliable: Qt6 pulls in libbz2,
libbrotli, libmd4c, libpcre2, libharfbuzz, ICU, etc. through indirect imports,
and any omission makes the installed EXE fail with "DLL not found" dialogs.

Instead we ask ntldd (or ldd as a fallback) what every PE file in the payload
actually imports, copy every DLL that exists under /ucrt64/bin, and iterate
until no new dependency appears. System DLLs (KERNEL32.dll, api-ms-win-*) are
not present under /ucrt64/bin and are therefore skipped - Windows supplies them.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys

DLL_NAME = re.compile(r"([A-Za-z0-9_.+-]+\.dll)\b", re.IGNORECASE)


def pe_files(root):
    for folder, _, names in os.walk(root):
        for name in names:
            if name.lower().endswith((".exe", ".dll")):
                yield os.path.join(folder, name)


def imported_names(path, tool):
    try:
        result = subprocess.run(
            [tool, path],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=60,
        )
    except (OSError, subprocess.SubprocessError):
        return set()
    return {match.group(1) for match in DLL_NAME.finditer(result.stdout)}


def resolve_tool(preferred):
    found = shutil.which(preferred)
    if found:
        return found
    fallback = shutil.which("ldd")
    if fallback:
        print(f"{preferred} not found, falling back to ldd", file=sys.stderr)
        return fallback
    raise SystemExit("Neither ntldd nor ldd is available; install mingw-w64-ucrt-x86_64-ntldd")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--payload", required=True, help="directory containing the built exe/dll files")
    parser.add_argument("--ucrt", default="/ucrt64", help="MSYS2 UCRT64 prefix")
    parser.add_argument("--tool", default="ntldd")
    parser.add_argument("--max-rounds", type=int, default=10)
    args = parser.parse_args()

    tool = resolve_tool(args.tool)
    runtime_bin = os.path.join(args.ucrt, "bin")
    if not os.path.isdir(runtime_bin):
        raise SystemExit(f"Runtime bin directory not found: {runtime_bin}")

    copied = set()
    for round_no in range(1, args.max_rounds + 1):
        needed = set()
        for pe in pe_files(args.payload):
            needed.update(imported_names(pe, tool))

        added = 0
        for name in sorted(needed):
            source = os.path.join(runtime_bin, name)
            target = os.path.join(args.payload, name)
            if os.path.isfile(target) or not os.path.isfile(source):
                continue
            shutil.copy2(source, target)
            copied.add(name)
            added += 1

        print(f"round {round_no}: imports={len(needed)} newly copied={added}", file=sys.stderr)
        if added == 0:
            break

    for name in sorted(copied):
        print(f"  + {name}")
    print(f"Total runtime DLLs copied: {len(copied)}", file=sys.stderr)


if __name__ == "__main__":
    main()
