"""
agent-probe web server — FastAPI backend for D3.js visualization.

Runs the agent-probe C++ binary and serves results via REST API.
Also serves the static D3.js frontend.
"""

import json
import subprocess
import shutil
import tempfile
import re
import os
from pathlib import Path
from typing import Optional

from fastapi import FastAPI, HTTPException, Query
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse

app = FastAPI(
    title="agent-probe",
    description="Static analysis for agent integration points",
    version="0.1.0",
)

# Resolve paths relative to this file
SERVER_DIR = Path(__file__).parent
PROJECT_ROOT = SERVER_DIR.parent
BINARY_PATH = PROJECT_ROOT / "build" / "agent-probe.exe"

# Check for Linux/Mac binary name
if not BINARY_PATH.exists():
    BINARY_PATH = PROJECT_ROOT / "build" / "agent-probe"


def find_binary() -> Path:
    """Locate the agent-probe binary."""
    if BINARY_PATH.exists():
        return BINARY_PATH
    raise FileNotFoundError(
        f"agent-probe binary not found at {BINARY_PATH}. "
        "Build it first with: cmake --build build"
    )


def _build_env() -> dict:
    """Build environment with MSYS2/MinGW-w64 on PATH (Windows)."""
    env = os.environ.copy()
    msys_bin = r"C:\msys64\ucrt64\bin"
    if os.path.isdir(msys_bin) and msys_bin not in env.get("PATH", ""):
        env["PATH"] = msys_bin + os.pathsep + env.get("PATH", "")
    return env


def run_probe(target_path: str, fmt: str = "json", min_confidence: float = 0.0) -> dict:
    """Run agent-probe CLI and return parsed JSON output."""
    binary = find_binary()

    cmd = [
        str(binary),
        "--path", target_path,
        "--format", fmt,
        "--min-confidence", str(min_confidence),
    ]

    result = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        timeout=60,
        cwd=str(PROJECT_ROOT),
        env=_build_env(),
    )

    if result.returncode == 2:
        raise HTTPException(status_code=400, detail=result.stderr.strip())

    stdout = result.stdout.strip()
    if not stdout:
        detail = f"No output from agent-probe. stderr: {result.stderr.strip()}"
        raise HTTPException(status_code=500, detail=detail)

    try:
        return json.loads(stdout)
    except json.JSONDecodeError as e:
        raise HTTPException(
            status_code=500,
            detail=f"Invalid JSON from agent-probe: {e}. Output: {stdout[:200]}"
        )


@app.get("/api/health")
def health():
    """Health check — verifies the binary exists."""
    try:
        binary = find_binary()
        return {"status": "ok", "binary": str(binary)}
    except FileNotFoundError as e:
        raise HTTPException(status_code=503, detail=str(e))


@app.get("/api/scan")
def scan(
    path: str = Query(..., description="Path to repository or file to scan"),
    min_confidence: float = Query(0.0, ge=0.0, le=1.0),
):
    """Run analysis and return findings as JSON."""
    abs_path = Path(path)
    if not abs_path.is_absolute():
        abs_path = PROJECT_ROOT / path

    if not abs_path.exists():
        raise HTTPException(status_code=404, detail=f"Path not found: {path}")

    return run_probe(str(abs_path), fmt="json", min_confidence=min_confidence)


@app.get("/api/graph")
def graph(
    path: str = Query(..., description="Path to repository or file to scan"),
    min_confidence: float = Query(0.0, ge=0.0, le=1.0),
):
    """Run analysis and return full graph structure for D3.js visualization."""
    abs_path = Path(path)
    if not abs_path.is_absolute():
        abs_path = PROJECT_ROOT / path

    if not abs_path.exists():
        raise HTTPException(status_code=404, detail=f"Path not found: {path}")

    return run_probe(str(abs_path), fmt="graph", min_confidence=min_confidence)


# ── GitHub URL support ────────────────────────────────────────────

_GITHUB_RE = re.compile(
    r"^https?://github\.com/[\w.\-]+/[\w.\-]+(\.git)?(/.*)?$", re.IGNORECASE
)


def _clone_repo(url: str) -> Path:
    """Shallow-clone a GitHub repo to a temp directory."""
    tmp_dir = Path(tempfile.mkdtemp(prefix="agent-probe-"))
    try:
        subprocess.run(
            ["git", "clone", "--depth", "1", "--quiet", url, str(tmp_dir)],
            check=True,
            capture_output=True,
            text=True,
            timeout=120,
        )
    except subprocess.CalledProcessError as e:
        shutil.rmtree(tmp_dir, ignore_errors=True)
        raise HTTPException(
            status_code=400,
            detail=f"Failed to clone {url}: {e.stderr.strip()}"
        )
    except subprocess.TimeoutExpired:
        shutil.rmtree(tmp_dir, ignore_errors=True)
        raise HTTPException(status_code=408, detail="Clone timed out (120s limit)")
    return tmp_dir


@app.get("/api/github")
def github_scan(
    url: str = Query(..., description="GitHub repository URL"),
    min_confidence: float = Query(0.0, ge=0.0, le=1.0),
):
    """Clone a GitHub repo and return graph analysis."""
    if not _GITHUB_RE.match(url):
        raise HTTPException(status_code=400, detail="Invalid GitHub URL")

    tmp_dir = _clone_repo(url)
    try:
        return run_probe(str(tmp_dir), fmt="graph", min_confidence=min_confidence)
    finally:
        shutil.rmtree(tmp_dir, ignore_errors=True)


# Serve static files (D3.js frontend)
static_dir = SERVER_DIR / "static"
if static_dir.exists():
    app.mount("/static", StaticFiles(directory=str(static_dir)), name="static")


@app.get("/")
def index():
    """Serve the main visualization page."""
    index_path = static_dir / "index.html"
    if index_path.exists():
        return FileResponse(str(index_path))
    return {"message": "agent-probe API — visit /docs for API documentation"}
