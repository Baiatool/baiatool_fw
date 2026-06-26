set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
VENV="$WORKSPACE_DIR/.venv"

cd "$WORKSPACE_DIR"

info()  { echo "[setup] $*"; }
die()   { echo "[setup] ERROR: $*" >&2; exit 1; }

step_done() {
    local marker="$WORKSPACE_DIR/.west/setup_markers/$1"
    [ -f "$marker" ]
}

mark_done() {
    local marker="$WORKSPACE_DIR/.west/setup_markers/$1"
    mkdir -p "$(dirname "$marker")"
    touch "$marker"
}

PYTHON=$(command -v python3 || die "python3 not found")
PY_VER=$("$PYTHON" -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')")
PY_MIN="3.12"
if ! "$PYTHON" -c "import sys; sys.exit(0 if sys.version_info >= (3,12) else 1)"; then
    die "Python $PY_MIN+ required, found $PY_VER"
fi
info "Python $PY_VER OK"

if [ ! -x "$VENV/bin/python" ]; then
    info "Creating .venv ..."
    "$PYTHON" -m venv "$VENV"
else
    info ".venv already exists, skipping"
fi

PIP="$VENV/bin/pip"
WEST="$VENV/bin/west"

if ! "$VENV/bin/python" -c "import west" 2>/dev/null; then
    info "Installing west ..."
    "$PIP" install --quiet west
else
    info "west already installed, skipping"
fi

if [ ! -d "$WORKSPACE_DIR/.west" ]; then
    info "Running west init ..."
    "$WEST" init -l baiatool_fw/
else
    info "west already initialized, skipping"
fi

if ! step_done "west_update"; then
    info "Running west update (this may take a while) ..."
    "$WEST" update
    mark_done "west_update"
else
    info "west update already done, skipping"
fi

info "Installing Python requirements ..."
"$PIP" install --quiet -r baiatool_fw/requirements.txt

if ! step_done "sdk_install"; then
    info "Installing Zephyr SDK ..."
    "$WEST" sdk install
    mark_done "sdk_install"
else
    info "Zephyr SDK already installed, skipping"
fi

if ! step_done "packages_pip"; then
    info "Installing board tools (esptool, ...) ..."
    "$WEST" packages pip --install -- --quiet
    mark_done "packages_pip"
else
    info "Board tools already installed, skipping"
fi

if ! step_done "blobs_espressif"; then
    info "Fetching hal_espressif blobs (WiFi/BT) ..."
    "$WEST" blobs fetch hal_espressif
    mark_done "blobs_espressif"
else
    info "hal_espressif blobs already fetched, skipping"
fi

echo ""
echo "Setup complete. To build:"
echo "  source .venv/bin/activate"
echo "  west build -p always baiatool_fw -b esp32_devkitc/esp32/procpu"
