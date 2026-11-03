#!/bin/sh

set -eu

mud_root=${EMPIRE_DIR:-/opt/empiremud}
cd "${mud_root}"

new-world --if-empty

# Refresh persisted PNGs from the latest game-generated map data on restarts.
# On the first boot, new-world already rendered the generated terrain map.
if [ -s data/map.txt ]; then
    render-maps
fi

# Do not put the legacy autorun shell between Docker and the game. EmpireMUD's
# SIGTERM handler performs its own graceful save before exiting.
exec "$@"
