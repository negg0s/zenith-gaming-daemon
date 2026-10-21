#!/usr/bin/env bash
# packaging/build_reproducible.sh
# Deterministic & reproducible release packaging script
set -euo pipefail

# Pin epoch timestamp for reproducible archives (e.g. latest git commit date or fixed epoch)
export SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(git log -1 --format=%ct 2>/dev/null || echo 1761000000)}"

BUILD_DIR="build-release"
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

echo "[*] Configuring reproducible build with SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH}..."
cmake -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-ffile-prefix-map=$(pwd)=. -Wdate-time -fmacro-prefix-map=$(pwd)=." \
    -DCMAKE_INSTALL_PREFIX=/usr

echo "[*] Compiling binaries..."
cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

echo "[*] Generating release packages (.tar.gz, .deb, .rpm) via CPack..."
(cd "${BUILD_DIR}" && cpack)

DIST_DIR="dist"
rm -rf "${DIST_DIR}"
mkdir -p "${DIST_DIR}"

# Move built packages
cp "${BUILD_DIR}"/*.tar.gz "${DIST_DIR}/" 2>/dev/null || true
cp "${BUILD_DIR}"/*.deb "${DIST_DIR}/" 2>/dev/null || true
cp "${BUILD_DIR}"/*.rpm "${DIST_DIR}/" 2>/dev/null || true
cp packaging/sbom/cyclonedx.json "${DIST_DIR}/zenith-1.2.0-cyclonedx.sbom.json"

echo "[*] Generating SHA256SUMS manifest..."
(cd "${DIST_DIR}" && sha256sum * > SHA256SUMS)

echo "========================================================================"
echo "                   REPRODUCIBLE RELEASE ARTIFACTS                       "
echo "========================================================================"
ls -lh "${DIST_DIR}"
echo "------------------------------------------------------------------------"
cat "${DIST_DIR}/SHA256SUMS"
echo "========================================================================"
