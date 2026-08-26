#!/usr/bin/env bash
# Regenerates the EmpireMUD world, produces PNG maps, and copies them into the
# shared mud_maps volume served by the “map” (PHP/Apache) container.

set -euo pipefail

MUD_ROOT=/opt/empiremud
WLD_DIR="${MUD_ROOT}/lib/world/wld"     # where the C helper writes map.txt
OUT_DIR=/var/www/html                   # mud_maps volume (shared)
PHP_MAP="${MUD_ROOT}/php/map.php"

# Ensure the map helper exists
if [[ ! -x "${WLD_DIR}/map" ]]; then
  echo "[self-heal] copying map helper into ${WLD_DIR}"
  cp /opt/empiremud/bin/map "${WLD_DIR}/"
fi

echo ">> Regenerating map text + .wld files"
cd "${WLD_DIR}"
rm -f *.wld map.txt
./map                                    # builds terrain + map.txt in wld/

# ----------------------------------------------------------------------
# Move/copy data files so php/map.php finds what it expects
# ----------------------------------------------------------------------
mv map.txt ../map.txt                    #   …/lib/world/map.txt
cp ../map.txt ../map-political.txt       #   …/lib/world/map-political.txt

# ----------------------------------------------------------------------
# Produce PNGs for the viewer
# ----------------------------------------------------------------------
echo ">> Creating fresh PNGs for the viewer"
mkdir -p "${OUT_DIR}"

php "${PHP_MAP}"             > "${OUT_DIR}/map.png"
php "${PHP_MAP}" political   > "${OUT_DIR}/map-political.png"
cp -u /opt/empiremud/php/map-viewer.php /var/www/html/
echo "✓  Done – restart MUD to load new terrain if it’s running."

