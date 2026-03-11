"""
agent-probe web server — FastAPI backend for D3.js visualization.

Runs the agent-probe C++ binary and serves results via REST API.
Also serves the static D3.js frontend.
"""

import json
import subprocess
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
    )

    if result.returncode == 2:
        raise HTTPException(status_code=400, detail=result.stderr.strip())

    if not result.stdout.strip():
        raise HTTPException(status_code=500, detail="No output from agent-probe")

    return json.loads(result.stdout)


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
