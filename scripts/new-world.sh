#!/bin/sh
# Generate the upstream-default EmpireMUD world inside the persistent lib
# volume. This is non-destructive unless the caller explicitly passes --force.

set -eu

mud_root=${EMPIRE_DIR:-/opt/empiremud}
world_dir="${mud_root}/lib/world/wld"
map_binary="${world_dir}/map"
saved_world="${world_dir}/binary_map"
mode=${1:---if-empty}

case "${mode}" in
    --if-empty)
        if [ -s "${saved_world}" ]; then
            echo "Existing world found; initialization skipped."
            exit 0
        fi
        ;;
    --force)
        echo "WARNING: replacing the saved world in ${world_dir}."
        ;;
    *)
        echo "Usage: new-world [--if-empty|--force]" >&2
        exit 2
        ;;
esac

if [ ! -x "${map_binary}" ]; then
    echo "Restoring the map generator to the persistent lib volume."
    cp "${mud_root}/bin/map" "${map_binary}"
    chmod 0755 "${map_binary}"
fi

echo "Generating the upstream-default 1800x1000 EmpireMUD world..."
(
    cd "${world_dir}"
    ulimit -s unlimited 2>/dev/null || true
    ./map
)

if [ ! -s "${saved_world}" ]; then
    echo "World generation did not create ${saved_world}." >&2
    exit 1
fi

render-maps
echo "World generation complete."
