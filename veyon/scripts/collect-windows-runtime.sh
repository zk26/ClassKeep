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

# PluginManager tries QPluginLoader on every *.dll in plugins/. Helper DLLs
# (lib*.dll, vnchooks.dll) belong next to the executables, not here.
for helper in "$PAYLOAD"/plugins/lib*.dll "$PAYLOAD"/plugins/vnchooks.dll; do
  if [ -f "$helper" ]; then mv "$helper" "$PAYLOAD/"; fi
done

echo "== Qt plugins =="
for sub in platforms styles tls imageformats; do
  for base in "$UCRT/share/qt6/plugins/$sub" "$UCRT/lib/qt6/plugins/$sub"; do
    if [ -d "$base" ]; then
      cp "$base"/*.dll "$PAYLOAD/$sub/" 2>/dev/null || true
      break
    fi
  done
done

echo "== QCA crypto plugin (Qt6 build ONLY) =="
# A blind `find ... | head -1` can pick the qca-qt5 plugin, which a Qt6 process
# cannot load: CryptoCore then qFatal("RSA not supported!") => __fastfail.
# Use the exact upstream WindowsInstaller.cmake path first.
qca_copied=0
for qca in "$UCRT/lib/qca-qt6/crypto/libqca-ossl.dll" \
           "$UCRT/share/qt6/plugins/crypto/libqca-ossl.dll" \
           "$UCRT/lib/qt6/plugins/crypto/libqca-ossl.dll"; do
  if [ -f "$qca" ]; then
    cp "$qca" "$PAYLOAD/crypto/"
    echo "  QCA from $qca"
    qca_copied=1
    break
  fi
done
if [ "$qca_copied" -eq 0 ]; then
  echo "WARNING: libqca-ossl.dll not found under $UCRT" >&2
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

echo "== Simplified Chinese translations =="
# Built by the CI step `lrelease veyon/translations/veyon_zh_CN.ts`.
# Without these the UI stays English even on a Chinese Windows install.
mkdir -p "$PAYLOAD/translations"
if [ -d "$ROOT/translations" ]; then
  cp "$ROOT"/translations/*.qm "$PAYLOAD/translations/" 2>/dev/null || true
fi

echo "== documentation and icon =="
cp "$ROOT/LICENSE" "$PAYLOAD/LICENSE.TXT"
cp "$ROOT/veyon/COPYING" "$PAYLOAD/COPYING.TXT"
cp "$ROOT/veyon/scripts/windows-install-readme.txt" "$PAYLOAD/README.TXT"
cp "$ROOT/veyon/scripts/ClassKeep.ico" "$PAYLOAD/ClassKeep.ico"

echo "== verify (startup-critical files included) =="
for f in veyon-core.dll veyon-master.exe veyon-configurator.exe veyon-cli.exe \
         veyon-wcli.exe veyon-service.exe veyon-server.exe veyon-worker.exe \
         plugins/classroom.dll plugins/windows-platform.dll \
         crypto/libqca-ossl.dll libqca-qt6.dll \
         platforms/qwindows.dll ClassKeep.ico translations/veyon_zh_CN.qm; do
  if [ ! -f "$PAYLOAD/$f" ]; then
    echo "MISSING REQUIRED FILE: $f" >&2
    exit 1
  fi
done
echo "windows-platform.dll: PRESENT (VeyonCore can start)"
echo "libqca-ossl.dll:      PRESENT (CryptoCore RSA available)"

echo "translation catalogs: $(find "$PAYLOAD/translations" -type f -name '*.qm' | wc -l)"
echo "payload files: $(find "$PAYLOAD" -type f | wc -l)"
echo "plugin DLLs: $(find "$PAYLOAD/plugins" -type f -name '*.dll' | wc -l)"
du -sh "$PAYLOAD"
