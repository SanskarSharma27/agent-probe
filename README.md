# agent-probe

**Static analysis tool that discovers where AI agents should be integrated into existing codebases — without using AI.**

Most "AI code analysis" tools call an LLM to summarize code. agent-probe takes a fundamentally different approach: it builds a full call graph from source code using Tree-sitter AST parsing, runs graph algorithms (Brandes' betweenness centrality, PageRank), and applies pattern detectors to surface concrete integration points — functions that make external API calls, orchestrate many downstream services, implement retry/polling loops, or form CRUD clusters. The entire analysis runs locally in milliseconds with zero API keys, zero network calls, and zero token costs.

```
$ agent-probe -p ./my-project

agent-probe v0.1.0 — 3 files, 33 graph nodes

CONF    TYPE          LINE      FUNCTION                        FILE
------  ------------  --------  ------------------------------  --------------------
1.00    RETRY         59        sync_with_retry                 flask_app.py
0.81    CRUD          32        user (CRUD cluster)             flask_app.py
0.75    API_CALL      54        get_weather                     flask_app.py
0.65    API_CALL      59        sync_with_retry                 flask_app.py
0.54    FAN_OUT       10        DatabaseClient.getUser          express_app.js
0.50    FAN_OUT       78        orchestrate                     express_app.js

11 finding(s)
```

---

## Why This Exists

Integrating AI agents into production code requires understanding the dependency topology of a codebase:
- **Where** does the code call external services? (API boundaries)
- **What** functions orchestrate multiple downstream calls? (fan-out hubs)
- **Which** patterns already handle failure/retry? (retry loops)
- **How** is data managed? (CRUD clusters that could benefit from agent-driven workflows)

These questions are structural, not semantic. They don't need an LLM — they need a graph. agent-probe builds that graph and answers those questions deterministically.

## What Makes This Different

| Approach | agent-probe | LLM-based tools |
|---|---|---|
| **Deterministic** | Same input always produces same output | Stochastic, varies per run |
| **Speed** | ~20ms for 1000-line repo | Seconds to minutes per API call |
| **Cost** | Free, runs offline | Token costs per analysis |
| **Privacy** | Code never leaves your machine | Code sent to cloud APIs |
| **Explainable** | Every finding has traceable evidence | "The AI thinks..." |
| **Graph-aware** | PageRank + centrality boost confidence | No structural understanding |

---

## Architecture

```
Source Code (.py, .js, .ts, .jsx, .tsx, .mjs)
    |
    v
[Tree-sitter AST Parser] -----> [Language Profiles]
    |                             Python / JavaScript
    v
[AST Nodes: functions, classes, imports, calls]
    |
    v
[Graph Builder] -----> Weighted Directed Graph (adjacency list)
    |                   Edge types: CALLS, IMPORTS, INHERITS, CONTAINS
    v
[Graph Algorithms]
    |-- Brandes' Betweenness Centrality
    |-- Iterative PageRank (damping=0.85)
    |-- BFS / DFS traversal
    |-- In-degree / Out-degree
    v
[Pattern Detectors]
    |-- API Call Analyzer    (external service boundaries)
    |-- Fan-Out Analyzer     (orchestrator functions)
    |-- Retry Analyzer       (retry/polling/backoff loops)
    |-- CRUD Analyzer        (entity-based CRUD clusters)
    v
[Confidence Scorer] -----> type weight x evidence x PageRank boost
    v
[Output: table | json | summary | graph]
    |
    v
[Optional: FastAPI + D3.js visualization]
```

### Core Components

| Component | Location | What It Does |
|---|---|---|
| Tree-sitter parser wrapper | `src/parser/ts_parser.cpp` | Extracts functions, classes, imports, call expressions from AST |
| Language profiles | `src/parser/{python,javascript}_profile.h` | Abstract interface mapping language grammar to node types |
| Graph | `src/graph/graph.{h,cpp}` | Adjacency list with typed/weighted edges and name index |
| Graph builder | `src/graph/builder.cpp` | Converts AST nodes into graph nodes + edges |
| Algorithms | `src/graph/algorithms.cpp` | BFS, DFS, Brandes' centrality, iterative PageRank |
| API call detector | `src/analyzers/api_call_analyzer.cpp` | Finds functions calling known HTTP/DB/RPC libraries |
| Fan-out detector | `src/analyzers/fan_out_analyzer.cpp` | Identifies orchestrator functions via out-degree + centrality |
| Retry detector | `src/analyzers/retry_analyzer.cpp` | Detects retry/polling patterns via sleep calls + loops |
| CRUD detector | `src/analyzers/crud_analyzer.cpp` | Groups functions into entity-based CRUD clusters |
| Confidence scorer | `src/scoring/scorer.cpp` | Combines type weights, evidence count, and PageRank boost |
| CLI formatter | `src/cli/formatter.cpp` | Table, JSON, summary, graph output with ANSI colors |
| Web visualization | `server/` | FastAPI backend + D3.js force-directed graph |

---

## Build

### Prerequisites

- **C++17 compiler** (GCC 11+, Clang 14+, or MSVC 2019+)
- **CMake 3.20+**
- **Git** (for FetchContent to pull dependencies)

All C++ dependencies are fetched automatically at configure time:

| Dependency | Version | Purpose |
|---|---|---|
| [tree-sitter](https://github.com/tree-sitter/tree-sitter) | v0.24.6 | Core AST parsing engine (C library) |
| [tree-sitter-python](https://github.com/tree-sitter/tree-sitter-python) | v0.23.6 | Python grammar |
| [tree-sitter-javascript](https://github.com/tree-sitter/tree-sitter-javascript) | v0.23.1 | JavaScript/TypeScript grammar |
| [CLI11](https://github.com/CLIUtils/CLI11) | v2.3.2 | Command-line argument parsing |
| [nlohmann/json](https://github.com/nlohmann/json) | v3.11.3 | JSON serialization |
| [GoogleTest](https://github.com/google/googletest) | v1.14.0 | Unit testing framework |

### Build Steps

```bash
# Clone
git clone <repo-url> agent-probe
cd agent-probe

# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Run tests (73 tests across 12 suites)
cd build && ctest --output-on-failure && cd ..

# Run the tool
./build/agent-probe -p /path/to/your/repo
```

### Windows with MSYS2/MinGW-w64

```bash
# Install MSYS2, then from UCRT64 shell:
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake git

cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Make sure C:\msys64\ucrt64\bin is on PATH when running
./build/agent-probe.exe -p ./fixtures
```

---

## Usage

```
agent-probe [OPTIONS]

Options:
  -p, --path PATH           Path to repo, file, or GitHub URL to scan (default: ".")
  -f, --format FORMAT       Output format: table, json, summary, graph
  -c, --min-confidence N    Minimum confidence threshold 0.0-1.0 (default: 0.0)
  -e, --exclude DIR         Additional directory names to skip (repeatable)
  --no-color                Disable ANSI colored output
  --version                 Print version
  -h, --help                Print help
```

### Output Formats

**Table** (default) — human-readable with ANSI colors:
```bash
agent-probe -p ./src
```

**JSON** — structured output for CI/CD pipelines:
```bash
agent-probe -p ./src -f json
```

```json
{
  "files_scanned": 3,
  "findings": [
    {
      "type": "RETRY",
      "function": "sync_with_retry",
      "file": "flask_app.py",
      "line": 59,
      "confidence": 1.0,
      "reason": "Retry/polling pattern detected",
      "evidence": [
        "contains sleep/delay call",
        "calls API: requests.post",
        "name suggests retry/polling pattern"
      ]
    }
  ]
}
```

**Summary** — quick overview for dashboards:
```bash
agent-probe -p ./src -f summary
```
```
agent-probe v0.1.0
Scanned 3 files, 47 AST nodes
Graph: 33 nodes, 32 edges

Findings: 11
  API calls:      2
  Fan-out:        7
  Retry patterns: 1
  CRUD clusters:  1
```

**Graph** — full graph export for visualization:
```bash
agent-probe -p ./src -f graph
```

Returns JSON with `nodes` (name, file, PageRank, finding types), `edges` (source, target, type, weight), and `findings` — consumed by the D3.js frontend.

### Scan a GitHub Repo Directly

No need to clone manually — pass a GitHub URL and agent-probe handles the rest:

```bash
agent-probe -p https://github.com/pallets/flask
```

This runs `git clone --depth 1` to a temp directory, scans, and cleans up automatically. Works with any public repo.

```
$ agent-probe -p https://github.com/pallets/flask -f summary

Cloning https://github.com/pallets/flask ...
agent-probe v0.1.0
Scanned 83 files, 1468 AST nodes
Graph: 857 nodes, 1691 edges

Findings: 253
  Fan-out:        242
  Retry patterns: 9
  CRUD clusters:  2
```

### Directory Filtering

agent-probe automatically skips common non-source directories:

> `.git`, `.venv`, `venv`, `node_modules`, `__pycache__`, `build`, `dist`, `site-packages`, `.mypy_cache`, `.pytest_cache`, `.tox`, `.next`, `vendor`, `target`, `.idea`, `.vscode`, and all hidden directories (starting with `.`)

Exclude additional directories with `-e`:

```bash
# Skip tests and scripts
agent-probe -p ./my-project -e tests -e scripts

# Skip generated code
agent-probe -p ./my-project -e generated -e proto_out
```

### Filtering by Confidence

Show only high-confidence findings:
```bash
agent-probe -p ./src -c 0.7
```

Scan a single file:
```bash
agent-probe -p ./src/routes/api.py
```

### Exit Codes

| Code | Meaning |
|---|---|
| 0 | No findings (clean) |
| 1 | Findings detected |
| 2 | Error (path not found, no supported files) |

---

## Web Visualization

A FastAPI backend serves the C++ binary's graph output as a D3.js force-directed graph.

```bash
# Set up Python environment
cd server
uv venv
uv pip install -r requirements.txt

# Start the server
uv run uvicorn app:app --reload --port 8000

# Open http://localhost:8000 in your browser
```

The visualization provides:
- **Force-directed graph layout** showing call relationships
- **Node coloring** by finding type (blue=API, purple=fan-out, red=retry, green=CRUD)
- **Node sizing** scaled by PageRank score
- **Click-to-highlight** connected nodes and edges
- **Sidebar** with scan stats, findings list, and legend
- **Drag and zoom** for exploring large graphs
- **GitHub URL input** — paste a repo URL, the server clones and scans it (no local checkout needed)

The web UI has two input modes toggled by tabs:
- **Local Path** — scan a directory on the machine running the server
- **GitHub URL** — enter `https://github.com/owner/repo`, the backend clones it via `git clone --depth 1`, scans, and cleans up

---

## How Detection Works

### API Call Analyzer

Scans each function's `called_functions` for known HTTP/DB/RPC libraries:

- **Python**: `requests`, `httpx`, `aiohttp`, `urllib`, `boto3`, `sqlalchemy`, `redis`, `grpc`, `celery`
- **JavaScript**: `fetch`, `axios`, `got`, `node-fetch`, `superagent`, `http`, `grpc`, `redis`, `mongoose`, `prisma`, `pg`, `aws-sdk`

Confidence is boosted when the function has decorators (route handlers) or is a class method on a service object.

### Fan-Out Analyzer

Identifies orchestrator functions that coordinate multiple downstream calls:
1. Compute out-degree (number of outgoing CALLS edges)
2. Apply betweenness centrality — functions that sit on many shortest paths between other functions are structural hubs
3. Functions with high out-degree AND high centrality are likely orchestration points

### Retry/Polling Analyzer

Pattern-matches for retry loops by checking:
- Function name contains "retry", "poll", "backoff", "loop", or "wait"
- Function body calls `time.sleep` (Python) or `setTimeout`/`setInterval` (JS)
- Function also makes an API call (retry of external service)

### CRUD Cluster Analyzer

Groups functions by entity name (extracted from function names) and checks for Create/Read/Update/Delete coverage:
- `create_user`, `get_user`, `delete_user` → entity "user" with 3/4 CRUD ops
- Higher coverage = higher confidence
- Clusters represent data lifecycle management points

### Confidence Scoring

Final confidence for each finding:

```
confidence = base_type_weight
           * (1 + 0.1 * evidence_count)
           * (1 + pagerank_boost)
```

| Finding Type | Base Weight |
|---|---|
| RETRY | 0.8 |
| API_CALL | 0.6 |
| FAN_OUT | 0.4 |
| CRUD | 0.7 |

PageRank boost rewards functions that are structurally important in the call graph — a retry function that is called by many paths through the codebase gets a higher confidence than an isolated one.

---

## Multi-Language Support

agent-probe uses a `LanguageProfile` abstract interface to decouple parsing logic from language grammar. Adding a new language requires:

1. **Add a Tree-sitter grammar** via FetchContent in CMakeLists.txt
2. **Create a profile class** implementing `LanguageProfile` (map ~20 node type / field names)
3. **Register file extensions** in `main.cpp`

The parser automatically switches Tree-sitter grammars based on file extension, so mixed-language repos (e.g., Python backend + JavaScript frontend) are analyzed in a single scan.

Currently supported:
- **Python** (.py)
- **JavaScript / TypeScript** (.js, .jsx, .ts, .tsx, .mjs)

The profile interface:
```cpp
class LanguageProfile {
public:
    virtual std::string name() const = 0;
    virtual std::vector<std::string> file_extensions() const = 0;
    virtual std::string root_node_type() const = 0;
    virtual std::string function_def_type() const = 0;
    virtual std::string class_def_type() const = 0;
    virtual std::string call_expression_type() const = 0;
    virtual std::string import_statement_type() const = 0;
    virtual std::string arrow_function_type() const { return ""; }
    virtual std::string method_def_type() const { return ""; }
    // ... field names and API indicators
};
```

---

## Testing

73 tests across 12 test suites, covering every layer of the system:

```
$ ctest --output-on-failure

[==========] 73 tests from 12 test suites ran. (22 ms total)
[  PASSED  ] 73 tests.
```

| Suite | Tests | Coverage |
|---|---|---|
| JSParserFixture | 12 | Arrow functions, classes, imports, member expressions, fixture file |
| ApiCallAnalyzerTest | 5 | HTTP lib detection, decorator boost, non-API filtering |
| FanOutAnalyzerTest | 5 | Out-degree thresholds, centrality scoring, isolated nodes |
| RetryAnalyzerTest | 5 | Sleep detection, name matching, combined patterns |
| CrudAnalyzerTest | 5 | Entity extraction, partial/full CRUD coverage |
| ScorerTest | 6 | Type weights, evidence scaling, PageRank boost, sorting |
| GraphTest | 8 | Node/edge CRUD, name index, typed edges, incoming edges |
| BuilderTest | 5 | AST-to-graph conversion, edge types, class containment |
| AlgorithmTest | 10 | BFS, DFS, centrality on known graphs, PageRank convergence |
| ParserTest | 3 | Function extraction, method calls, multiple functions |
| FlaskFixtureTest | 6 | Full Flask app: imports, classes, decorators, calls |
| TreeSitterTest | 3 | Parser initialization, basic Python parsing |

---

## Project Stats

- **~2,670 lines** of C++ source code (26 files)
- **~1,380 lines** of C++ test code (5 test files)
- **~620 lines** of web frontend (HTML/JS/CSS)
- **73 tests**, all passing
- **29 commits** over 8 development phases
- **Zero runtime dependencies** — all libraries statically linked

---

## Problems Faced During Development

### 1. MSYS2 DLL Hell on Windows

**Problem**: The built `agent-probe.exe` ran fine from the MSYS2 terminal but crashed with exit code `0xC0000139` (STATUS_ENTRYPOINT_NOT_FOUND) when launched from Python's `subprocess.run()` or PowerShell.

**Root cause**: MinGW-w64 links against `libstdc++-6.dll`, `libgcc_s_seh-1.dll`, and `libwinpthread-1.dll` from `C:\msys64\ucrt64\bin`. The MSYS2 shell has this on PATH automatically, but other shells don't.

**Fix**: The FastAPI backend injects `C:\msys64\ucrt64\bin` into the subprocess PATH:
```python
def _build_env():
    env = os.environ.copy()
    msys_bin = r"C:\msys64\ucrt64\bin"
    if os.path.isdir(msys_bin):
        env["PATH"] = msys_bin + os.pathsep + env.get("PATH", "")
    return env
```

**Alternative**: Build with `-static` to avoid DLL dependencies entirely, or use MSVC instead of MinGW.

### 2. CMake 4.x Forward Compatibility

**Problem**: CMake 4.0+ changed the default behavior of `FetchContent_Declare` (policy CMP0169), causing `FetchContent_Populate()` to error with "should not be called for a dependency".

**Fix**: Pin `cmake_policy(SET CMP0169 OLD)` and `set(CMAKE_POLICY_VERSION_MINIMUM 3.5)` in CMakeLists.txt to preserve the populate-style FetchContent workflow. This ensures compatibility with both CMake 3.x and 4.x.

### 3. Tree-sitter Node Type Discovery

**Problem**: Tree-sitter's AST node types and field names are grammar-specific but not well-documented. The Python grammar uses `function_definition` while JavaScript uses `function_declaration`. Python has `import_from_statement` as a separate node; JavaScript uses `import_statement` for everything.

**Fix**: Used `ts_node_type()` and `ts_node_string()` to dump the AST for sample files, then built the `LanguageProfile` abstraction to map these per-language differences. The key insight was printing the full S-expression of parsed trees to discover the exact field names:
```cpp
// Debug: dump the full AST tree
TSNode root = ts_tree_root_node(tree);
char* s = ts_node_string(root);
printf("%s\n", s);
free(s);
```

### 4. JavaScript Arrow Function Names

**Problem**: Arrow functions like `const fetchData = async (url) => { ... }` don't have a `name` field in the AST. The `arrow_function` node only contains parameters and body — the name lives in the parent `variable_declarator`.

**Fix**: When encountering a `variable_declarator` node during traversal, check if its "value" child is an `arrow_function`. If so, extract the name from the declarator's "name" field and the params/body from the arrow function child:
```cpp
// Inside variable_declarator handling:
TSNode value_node = ts_node_child_by_field_name(node, "value", 5);
if (strcmp(ts_node_type(value_node), "arrow_function") == 0) {
    // name comes from the variable_declarator, not the arrow function
    TSNode name_node = ts_node_child_by_field_name(node, "name", 4);
    // params and body come from value_node (the arrow function)
}
```

### 5. Import Statement Ambiguity

**Problem**: Python and JavaScript both have `import_statement` as a node type, but the internal structure is completely different. Python has `dotted_name` and `aliased_import` children; JavaScript has `import_clause`, `named_imports`, and `import_specifier`. The first implementation tried to handle both with the same code path, producing empty imports for JS files.

**Fix**: Separated the import extraction by checking `profile.import_from_type()` — Python returns `"import_from_statement"` (non-empty), JavaScript returns `""`. The JS path extracts the module name from the `source` field (a string literal that needs quote stripping), and named imports from `named_imports` > `import_specifier` children.

### 6. Scanning 16,000 Files Instead of 60

**Problem**: Running agent-probe on a real project showed 16,189 files to scan. The tool was walking into `.venv` (thousands of installed packages), `node_modules`, `__pycache__`, `.git` internals, and every other non-source directory.

**Root cause**: The original `collect_files()` did a naive `recursive_directory_iterator` filtered only by file extension — no directory pruning at all.

**Fix**: Added a hardcoded skip set of 25+ common non-source directory names (`.venv`, `node_modules`, `build`, `dist`, `__pycache__`, `site-packages`, etc.) and an `--exclude` CLI flag for user-specified additions. Also skip all hidden directories (starting with `.`) by default. Uses `disable_recursion_pending()` to prune entire subtrees efficiently instead of checking every file:
```cpp
if (entry.is_directory()) {
    std::string dirname = entry.path().filename().string();
    if (skip.count(dirname) || dirname[0] == '.') {
        it.disable_recursion_pending();  // skip entire subtree
    }
}
```
Result: 16,189 files → 63 files for the same project.

### 7. GoogleTest Discovery with MinGW

**Problem**: `gtest_discover_tests()` sometimes fails on Windows/MinGW because the test executable can't find its DLLs during the CMake test discovery phase (which runs the binary to enumerate tests).

**Fix**: Run tests from the MSYS2 shell where the DLLs are on PATH, or set `PATH` in the CMake test environment:
```bash
# From MSYS2 UCRT64 terminal:
cd build && ctest --output-on-failure
```

---

## Debugging Guide

### Build Fails at FetchContent

```
CMake Error: FetchContent_Populate should not be called for dependency ...
```
You're using CMake 4.x. Add these lines near the top of CMakeLists.txt:
```cmake
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
cmake_policy(SET CMP0169 OLD)
```

### Binary Crashes on Windows (0xC0000139)

The executable can't find MinGW runtime DLLs. Either:
- Run from MSYS2 terminal
- Add `C:\msys64\ucrt64\bin` to your system PATH
- Rebuild with `-static` flag: `cmake -B build -DCMAKE_EXE_LINKER_FLAGS="-static"`

### Scanning Too Many Files

The tool scans thousands of files unexpectedly — likely walking into `node_modules`, `.venv`, or `build`. agent-probe skips these by default, but if you see unexpected counts:
```bash
# Exclude additional directories
agent-probe -p ./repo -e my_custom_build_dir -e generated
```

### No Findings Produced

Check that:
1. The path contains `.py`, `.js`, `.jsx`, `.ts`, `.tsx`, or `.mjs` files
2. The files contain actual function definitions (not just module-level code)
3. Your `--min-confidence` threshold isn't too high (try `-c 0.0`)

### GitHub Clone Fails

```
Error: git clone failed: ...
```
Ensure `git` is installed and on PATH. The tool runs `git clone --depth 1` via a shell command. For private repos, ensure your Git credentials (SSH key or credential helper) are configured. The web UI's GitHub scan has a 120-second timeout for large repos.

### Tests Fail with "fixture not found"

Some tests use fixture files relative to the working directory. Run tests from the project root:
```bash
cd /path/to/agent-probe
./build/agent-probe-tests
```

### Visualization Server Won't Start

```bash
# Ensure uv is installed
pip install uv

# Create venv and install deps
cd server
uv venv
uv pip install -r requirements.txt

# On Windows, ensure MSYS2 is on PATH for the subprocess call
set PATH=C:\msys64\ucrt64\bin;%PATH%
uv run uvicorn app:app --reload --port 8000
```

### Adding a New Language

1. Find the Tree-sitter grammar repo (e.g., `tree-sitter-go`)
2. Add `FetchContent_Declare(...)` block to CMakeLists.txt
3. Create `src/parser/go_profile.h` implementing `LanguageProfile`
4. Print the AST S-expression for a sample file to discover node types:
   ```cpp
   char* s = ts_node_string(ts_tree_root_node(tree));
   printf("%s\n", s);
   free(s);
   ```
5. Map each node type and field name in your profile
6. Register extensions in `main.cpp`'s `lang_map`
7. Link the grammar library in CMakeLists.txt

---

## Tech Stack

- **C++17** — core analysis engine
- **Tree-sitter** — incremental parsing framework (used by VS Code, Neovim, GitHub)
- **CMake** — build system with FetchContent for dependency management
- **GoogleTest** — unit testing
- **CLI11** — command-line argument parsing
- **nlohmann/json** — JSON serialization
- **FastAPI** — web backend (Python, optional)
- **D3.js** — force-directed graph visualization (optional)

---

## License

MIT
