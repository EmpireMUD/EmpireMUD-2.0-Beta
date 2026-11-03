# Docker installation

This setup compiles and runs EmpireMUD entirely in Docker. It does not install
a compiler, PHP, or game binary on the host. Before building, read
[`license.doc`](license.doc); building or using EmpireMUD means that you accept
its EmpireMUD, CircleMUD, and DikuMUD license terms and preserve their credits.

## Start

From the repository root:

```sh
docker compose up --build -d
docker compose logs -f mud
```

The first start generates the upstream-default 1800x1000 world. This can take
several minutes. Later starts detect `lib/world/wld/binary_map` in the named
volume and never regenerate the world automatically.

Connect a MUD client to `127.0.0.1:4000`. The map viewer is available at
<http://127.0.0.1:8081/map-viewer.php>. Both ports bind only to loopback. To
select different host ports without changing the container ports:

```sh
EMPIRE_HOST_PORT=4444 EMPIRE_MAP_HOST_PORT=8088 docker compose up -d
```

The private Compose network defaults to `10.253.240.64/28`, an unused subnet
selected on this machine because Docker's automatic address pools were full.
Set `EMPIRE_DOCKER_SUBNET` to another unused private subnet if needed elsewhere.

The first generated PNG is the terrain map. After the game has completed its
first startup, refresh the normal, political, and city map files with:

```sh
docker compose exec mud render-maps
```

Run that command whenever you want to publish a fresh map snapshot.

## Persistence and shutdown

Compose stores the game library and saves, generated map text, logs, and web
maps in the `mud_lib`, `mud_runtime`, `mud_logs`, and `mud_maps` named volumes.
`docker compose down` preserves all four. Do not add host bind mounts for these
paths unless you deliberately want to manage runtime ownership and backups.

Stop the game with:

```sh
docker compose stop mud
```

Docker sends SIGTERM directly to EmpireMUD. Its built-in shutdown handler saves
players, empires, and the world; Compose allows up to five minutes for that save.
The default large world can spend several minutes rewriting individual room
files, so wait for the stop command to finish before restarting or upgrading.
Do not use `docker compose kill` for a routine shutdown.

Back up the named volumes before upgrades or world changes. Removing volumes
(for example, `docker compose down --volumes`) permanently removes the saved
world and player data.

## Upgrade note

Rebuilding the image updates the game binary, but it does not overwrite an
existing `mud_lib` volume. That volume intentionally owns the live world,
players, empires, OLC changes, and other library state. It also means newly
bundled upstream library files are not imported automatically after an image
rebuild.

Before upgrading, back up all four volumes and review the upstream update notes.
Apply any required data or player-file conversion as a deliberate migration;
do not delete or replace `mud_lib` merely to make it match the new image.

## Deliberately replace the world

World replacement is never automatic after a save exists. To intentionally
replace it, stop the game, back up `mud_lib`, and run the generator explicitly:

```sh
docker compose stop mud
docker compose run --rm --entrypoint new-world mud --force
docker compose up -d
```

`--force` is destructive: it replaces the world map in the persistent library
volume. It does not remove player or empire files, so use it only when following
EmpireMUD's world-regeneration guidance and with a verified backup.
