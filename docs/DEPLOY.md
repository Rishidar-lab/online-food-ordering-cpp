# Deploying the Online Food Ordering System (free hosting)

The app is a native C++ server, so it is deployed as a **Docker container**.
The included `Dockerfile` builds and runs it on any container host. It binds
`0.0.0.0` and uses the `$PORT` the platform injects, and exposes `/healthz`
for health checks.

> **Data note (all free tiers):** free plans use an *ephemeral* filesystem, so
> the SQLite database resets whenever the service redeploys or restarts, and
> the app automatically re-seeds the sample menu + the default admin
> (`admin` / `admin123`). This is ideal for a demo. To keep data permanently,
> attach a persistent disk/volume mounted at `/app/data` (see each platform).

> **This project is deployed to Koyeb** (Option A below) — its one free service
> stays awake, so the public demo URL is always live for examiners.

---

## Option A — Koyeb  *(chosen target: free, no credit card, stays awake)*

1. Push this project to GitHub (see "Pushing to GitHub" below).
2. Go to <https://app.koyeb.com> and sign in with GitHub (free).
3. **Create Web Service → GitHub**, pick the `online-food-ordering-cpp` repo.
4. Builder = **Dockerfile** (auto-detected). Set:
   - **Instance:** Free
   - **Port:** `8080`  (Koyeb maps it to public `443`)
   - **Health check:** HTTP path `/healthz`
5. **Deploy.** After ~3–5 min you get a public URL like
   `https://online-food-ordering-cpp-<org>.koyeb.app`.
   - Store: `/`  ·  Admin: `/admin/login`  (`admin` / `admin123`)

CLI alternative:

```bash
# install: curl -fsSL https://raw.githubusercontent.com/koyeb/koyeb-cli/master/install.sh | sh
koyeb login
koyeb app init online-food-ordering-cpp \
  --git github.com/Rishidar-lab/online-food-ordering-cpp \
  --git-branch main --git-builder docker \
  --ports 8080:http --routes /:8080 --instance-type free \
  --checks 8080:http:/healthz
```

---

## Option B — Render.com  *(alternative: free, no credit card)*

1. Push this project to a GitHub repository (see "Pushing to GitHub" below).
2. Go to <https://render.com> and sign in with GitHub (free).
3. **New +  →  Blueprint**, pick the repository. Render reads `render.yaml`,
   builds the `Dockerfile`, and deploys a **free web service**.
   (Or **New +  →  Web Service → Docker** and set Health Check Path `/healthz`.)
4. After ~3–5 min you get a public URL like
   `https://online-food-ordering-cpp.onrender.com`.
   - Store: `/`  ·  Admin: `/admin/login`  (`admin` / `admin123`)

Free tier sleeps after ~15 min idle and cold-starts in ~30–50 s on the next
visit — fine for a demo.

---

## Option C — Fly.io  *(CLI deploy, no GitHub needed; volumes for persistence)*

```bash
curl -L https://fly.io/install.sh | sh      # install flyctl
fly auth signup                             # or: fly auth login
fly launch --no-deploy --name my-ofos       # detects the Dockerfile
fly volumes create ofosdata --size 1        # optional: persistent SQLite
# then in fly.toml add a [mounts] entry: source="ofosdata" destination="/app/data"
fly deploy
```

Fly injects `$PORT=8080` via `internal_port`; set it in `fly.toml` if prompted.

---

## Option D — Google Cloud Run  *(generous free tier, scales to zero)*

```bash
gcloud auth login
gcloud run deploy ofos --source . --region us-central1 --allow-unauthenticated
```

Cloud Run builds the `Dockerfile`, injects `$PORT`, and returns an HTTPS URL.
Filesystem is ephemeral (re-seeds on cold start).

---

## Pushing to GitHub

```bash
cd food-ordering-cpp
git init -b main
git add .
git commit -m "Online Food Ordering System (C++)"
gh repo create online-food-ordering-cpp --public --source=. --push   # uses gh CLI
# or create the repo on github.com and:
#   git remote add origin https://github.com/<you>/online-food-ordering-cpp.git
#   git push -u origin main
```

`.gitignore` already excludes build output, the database, and uploaded files.

---

## Local container test (what was verified here)

```bash
docker build -t ofos .
docker run --rm -e PORT=8080 -p 8096:8080 ofos
# open http://127.0.0.1:8096
```
