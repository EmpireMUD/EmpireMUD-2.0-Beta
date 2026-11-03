#!/bin/sh
# Render the current text map data into the named volume served by Apache.

set -eu

mud_root=${EMPIRE_DIR:-/opt/empiremud}
output_dir=${EMPIRE_MAP_DIR:-/srv/maps}
php_map="${mud_root}/php/map.php"
viewer_source="${mud_root}/php/map-viewer.php"

mkdir -p "${output_dir}"

render_atomic() {
    map_type=$1
    destination=$2
    temporary="${destination}.tmp"

    php "${php_map}" ${map_type:+"${map_type}"} > "${temporary}"
    mv "${temporary}" "${destination}"
}

if [ -s "${mud_root}/data/map.txt" ]; then
    render_atomic "" "${output_dir}/map.png"
else
    render_atomic generated "${output_dir}/map.png"
fi

if [ -s "${mud_root}/data/map-political.txt" ]; then
    render_atomic political "${output_dir}/map-political.png"
else
    # Before the first game boot there is no political map yet. Use the
    # generated terrain map so the viewer remains functional.
    render_atomic generated "${output_dir}/map-political.png"
fi

if [ -s "${mud_root}/data/map-cities.txt" ]; then
    cp "${mud_root}/data/map-cities.txt" "${output_dir}/map-cities.txt"
else
    printf '$\n' > "${output_dir}/map-cities.txt"
fi

# The stock viewer deliberately contains a deployment placeholder. Configure
# only the copy in the web volume and leave the upstream PHP source untouched.
sed "s#\$pathToData = '/path/to/empireMUD/data/';#\$pathToData = '/var/www/html/';#" \
    "${viewer_source}" > "${output_dir}/map-viewer.php.tmp"
mv "${output_dir}/map-viewer.php.tmp" "${output_dir}/map-viewer.php"

echo "Rendered maps in ${output_dir}."
