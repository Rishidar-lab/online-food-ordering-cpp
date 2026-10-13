#!/bin/sh
# Container entrypoint.
# Runs as root only long enough to prepare the (possibly freshly-mounted,
# root-owned) persistent data volume, then drops to the unprivileged `app`
# user to run the server.
#
# The SQLite database lives at $OFOS_DB (on the volume when one is mounted),
# and uploaded images are served from a symlink into the volume so they also
# survive redeploys. The baked-in schema stays at /app/data/schema.sql, which
# is NOT hidden because the volume mounts at $OFOS_PERSIST_DIR (default /data),
# a different path.
set -e

PERSIST="${OFOS_PERSIST_DIR:-/data}"

mkdir -p "$PERSIST/uploads"
chown -R app:app "$PERSIST"

# Serve uploads from the persistent location.
if [ ! -L /app/public/uploads ]; then
    rm -rf /app/public/uploads
    ln -s "$PERSIST/uploads" /app/public/uploads
fi

# Drop privileges and exec the server (PID 1 signal handling preserved).
exec setpriv --reuid=app --regid=app --init-groups \
    ./ofos --host "$OFOS_HOST" --port "$PORT" --dir "$OFOS_DIR" --db "$OFOS_DB"
