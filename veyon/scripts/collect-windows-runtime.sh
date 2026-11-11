#!/usr/bin/env bash
# Assemble the payload consumed by classkeep-installer.nsi.
# Run from the repository root inside an MSYS2 UCRT64 shell.
#
# Layout follows upstream nsis/veyon.nsi.in:
#   /                     executables, runtime DLLs, docs, icon
#   /plugins              Veyon plugins (classroom.dll included)
#   /platforms /styles /tls /imageformats /crypto   Qt & QCA plugins
set -euo pipefail

ROOT="$(pwd)"
BUILD="$ROOT/build"
PAYLOAD="$ROOT/payload"
UCRT="/ucrt64"

rm -rf "$PAYLOAD"
mkdir -p "$PAYLOAD/plugins" "$PAYLOAD/platforms" "$PAYLOAD/styles" \
         "$PAYLOAD/tls" "$PAYLOAD/imageformats" "$PAYLOAD/crypto"

echo "== built executables =="
find "$BUILD" -type f -name 'veyon-*.exe' ! -path '*CMakeFiles*' \
  -exec cp '{}' "$PAYLOAD/" \;

echo "== core DLL =="
find "$BUILD/core" -maxdepth 1 -type f -name '*.dll' -exec cp '{}' "$PAYLOAD/" \;

echo "== Veyon plugins =="
find "$BUILD/plugins" -type f -name '*.dll' ! -path '*CMakeFiles*' \
  -exec cp '{}' "$PAYLOAD/plugins/" \;

echo "== Qt plugins =="
for sub in platforms styles tls imageformats; do
  for base in "$UCRT/share/qt6/plugins/$sub" "$UCRT/lib/qt6/plugins/$sub"; do
    if [ -d "$base" ]; then
      cp "$base"/*.dll "$PAYLOAD/$sub/" 2>/dev/null || true
      break
    fi
  done
done

echo "== QCA crypto plugin =="
qca_ossl="$(find "$UCRT" -type f -name 'libqca-ossl.dll' 2>/dev/null | head -n 1 || true)"
if [ -n "$qca_ossl" ]; then
  cp "$qca_ossl" "$PAYLOAD/crypto/"
fi

echo "== full runtime DLL closure =="
# Fixes the "libbz2-1.dll / libmd4c.dll / libbrotli*.dll not found" failures:
# ntldd walks the real PE import graph instead of a hand-written whitelist.
PY="$(command -v python3 || command -v python)"
if [ -z "$PY" ]; then
  echo "python3/python not found; install mingw-w64-ucrt-x86_64-python" >&2
  exit 1
fi
"$PY" "$ROOT/veyon/scripts/collect_mingw_deps.py" --payload "$PAYLOAD" --ucrt "$UCRT"

echo "== documentation and icon =="
cp "$ROOT/LICENSE" "$PAYLOAD/LICENSE.TXT"
cp "$ROOT/veyon/COPYING" "$PAYLOAD/COPYING.TXT"
cp "$ROOT/veyon/scripts/windows-install-readme.txt" "$PAYLOAD/README.TXT"
cp "$ROOT/veyon/scripts/ClassKeep.ico" "$PAYLOAD/ClassKeep.ico"

echo "== verify =="
for f in veyon-core.dll veyon-master.exe veyon-configurator.exe veyon-cli.exe \
         veyon-wcli.exe veyon-service.exe veyon-server.exe veyon-worker.exe \
         plugins/classroom.dll platforms/qwindows.dll ClassKeep.ico; do
  if [ ! -f "$PAYLOAD/$f" ]; then
    echo "MISSING REQUIRED FILE: $f" >&2
    exit 1
  fi
done

echo "payload files: $(find "$PAYLOAD" -type f | wc -l)"
echo "plugin DLLs: $(find "$PAYLOAD/plugins" -type f -name '*.dll' | wc -l)"
du -sh "$PAYLOAD"
