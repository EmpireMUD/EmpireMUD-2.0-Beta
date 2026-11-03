# syntax=docker/dockerfile:1.7

FROM debian:12-slim AS build

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential \
        libcrypt-dev

WORKDIR /opt/empiremud
COPY . .

# Building or using EmpireMUD means accepting doc/license.doc. The normal
# build prompt cannot be answered in an image build, so record acceptance
# after copying the license into the image and keep the full license in the
# runtime stage.
RUN test -s doc/license.doc && \
    touch src/.accepted && \
    CFLAGS="-O2 -fcommon -fgnu89-inline" ./configure && \
    make -C src -j4 all && \
    cp lib/world/wld/map bin/map


FROM debian:12-slim AS runtime

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        libcrypt1 \
        php-cli \
        php-gd && \
    groupadd --gid 10001 empire && \
    useradd --uid 10001 --gid empire --home-dir /opt/empiremud --no-create-home empire

ENV EMPIRE_DIR=/opt/empiremud
WORKDIR ${EMPIRE_DIR}

COPY --from=build --chown=empire:empire /opt/empiremud/bin ./bin
COPY --from=build --chown=empire:empire /opt/empiremud/lib ./lib
COPY --from=build --chown=empire:empire /opt/empiremud/data ./data
COPY --from=build --chown=empire:empire /opt/empiremud/log ./log
COPY --from=build --chown=empire:empire /opt/empiremud/php ./php
COPY --from=build --chown=empire:empire /opt/empiremud/doc/license.doc ./doc/license.doc
COPY --from=build --chown=empire:empire /opt/empiremud/README ./README
COPY --chown=empire:empire scripts/new-world.sh /usr/local/bin/new-world
COPY --chown=empire:empire scripts/render-maps.sh /usr/local/bin/render-maps
COPY --chown=empire:empire scripts/docker-entrypoint.sh /usr/local/bin/docker-entrypoint

RUN chmod 0755 \
        /usr/local/bin/new-world \
        /usr/local/bin/render-maps \
        /usr/local/bin/docker-entrypoint && \
    mkdir -p /path/to /srv/maps && \
    ln -s /opt/empiremud /path/to/empireMUD && \
    chown empire:empire /srv/maps

USER empire

EXPOSE 4000/tcp
VOLUME ["/opt/empiremud/lib", "/opt/empiremud/data", "/opt/empiremud/log", "/srv/maps"]

ENTRYPOINT ["docker-entrypoint"]
CMD ["bin/empire", "-q", "4000"]
