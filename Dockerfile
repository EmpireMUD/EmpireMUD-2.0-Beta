###########################################################
# 1) TOOLCHAIN STAGE — compiler + apt caches + ccache     #
###########################################################
FROM debian:12-slim AS toolchain

# Writable apt caches (packages + lists) — only in this stage
RUN --mount=type=cache,id=apt-cache,target=/var/cache/apt \
    --mount=type=cache,id=apt-lists,target=/var/lib/apt/lists \
    mkdir -p /var/lib/apt/lists/partial && \
    apt-get update && \
    DEBIAN_FRONTEND=noninteractive \
    apt-get install -y --no-install-recommends \
        build-essential autoconf pkg-config \
        libncurses5-dev libssl-dev \
        cron php-cli php-gd \
        ccache vim less && \
    rm -rf /var/lib/apt/lists/*

# GCC flags for the legacy codebase
ENV PATH="/usr/lib/ccache:${PATH}"
ENV CFLAGS="-fcommon -fgnu89-inline -I.."

###########################################################
# 2) BUILD STAGE — compile EmpireMUD + helpers            #
###########################################################
FROM toolchain AS build
WORKDIR /opt/empiremud

# Copy source  (BuildKit uses zero-copy links, keeps cache)
COPY . .

# Ensure map helper output dir exists, then build everything
RUN mkdir -p lib/world/wld && \
    ./configure

WORKDIR /opt/empiremud/src
RUN make all
RUN cp /opt/empiremud/lib/world/wld/map /opt/empiremud

###########################################################
# 3) RUNTIME STAGE — slim, final image                    #
###########################################################
FROM debian:12-slim
ENV EMPIRE_DIR=/opt/empiremud

# Writable package cache (no lists cache ⇒ avoid lock conflicts)
RUN --mount=type=cache,id=apt-cache,target=/var/cache/apt \
    apt-get update && \
    DEBIAN_FRONTEND=noninteractive \
    apt-get install -y --no-install-recommends \
        cron telnet php-cli php-gd && \
    rm -rf /var/lib/apt/lists/*

# Bring compiled game tree into the runtime image
COPY --from=build ${EMPIRE_DIR} ${EMPIRE_DIR}

# ------------------------------------------------------------------
# Symlinks so auxiliary scripts resolve hard-coded paths
# ------------------------------------------------------------------
RUN mkdir -p /path/to/empireMUD \
            /var/www/html && \
    ln -sf /opt/empiremud/lib/world          /path/to/empireMUD/data && \
    ln -sf /opt/empiremud/php/map-viewer.php /var/www/html/map-viewer.php

# World-generation helper script (writes PNGs to /var/www/html)
COPY scripts/new-world.sh /usr/local/bin/new-world
RUN chmod +x /usr/local/bin/new-world

WORKDIR ${EMPIRE_DIR}
EXPOSE 4000/tcp
VOLUME ["/opt/empiremud/lib", "/opt/empiremud/log"]

CMD ["./autorun"]

