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
        libsqlite3-0 ca-certificates curl \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --create-home --uid 10001 app
WORKDIR /app

# Application binary + runtime assets (views / public / schema).
COPY --from=build /src/ofos       /app/ofos
COPY --from=build /src/views      /app/views
COPY --from=build /src/public     /app/public
COPY --from=build /src/data/schema.sql /app/data/schema.sql

# Writable locations for the SQLite DB and uploaded images.
RUN mkdir -p /app/data /app/public/uploads && chown -R app:app /app
USER app

# Bind all interfaces; $PORT is supplied by the platform (default 8080 locally).
ENV OFOS_HOST=0.0.0.0 \
    PORT=8080 \
    OFOS_DIR=/app \
    OFOS_DB=data/ofos.db
EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s \
  CMD curl -fsS "http://127.0.0.1:${PORT}/healthz" || exit 1

CMD ["sh", "-c", "./ofos --host \"$OFOS_HOST\" --port \"$PORT\" --dir \"$OFOS_DIR\" --db \"$OFOS_DB\""]
