# ===========================================================================
# Online Food Ordering System (C++) — production container
# Multi-stage: compile with the full toolchain, then ship a slim runtime image.
# Works on any Docker-based free host (Render, Koyeb, Fly.io, Google Cloud Run).
# ===========================================================================

# ---- build stage ----------------------------------------------------------
FROM debian:12-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
        g++ make libsqlite3-dev ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN make clean && make

# ---- runtime stage --------------------------------------------------------
FROM debian:12-slim AS runtime
RUN apt-get update && apt-get install -y --no-install-recommends \
        libsqlite3-0 ca-certificates curl util-linux \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --create-home --uid 10001 app
WORKDIR /app

# Application binary + runtime assets (views / public / schema).
COPY --from=build /src/ofos        /app/ofos
COPY --from=build /src/views       /app/views
COPY --from=build /src/public      /app/public
COPY --from=build /src/data/schema.sql /app/data/schema.sql
COPY docker-entrypoint.sh          /app/docker-entrypoint.sh
RUN chmod +x /app/docker-entrypoint.sh \
    && mkdir -p /app/public/uploads && chown -R app:app /app

# Bind all interfaces; $PORT is supplied by the platform (default 8080 locally).
# $OFOS_DB and the uploads symlink point at $OFOS_PERSIST_DIR (a volume on
# Fly.io; an ephemeral dir elsewhere) so data persists where a volume exists.
ENV OFOS_HOST=0.0.0.0 \
    PORT=8080 \
    OFOS_DIR=/app \
    OFOS_PERSIST_DIR=/data \
    OFOS_DB=/data/ofos.db
EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=3s --start-period=8s \
  CMD curl -fsS "http://127.0.0.1:${PORT}/healthz" || exit 1

# The entrypoint prepares the data volume as root, then drops to user `app`.
ENTRYPOINT ["/app/docker-entrypoint.sh"]
