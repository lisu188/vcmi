# Python Scripting System

This page describes the internal working of the Python scripting backend (`pythonscript/`,
CMake option `ENABLE_PYTHON`, default OFF). It is the second backend behind the neutral
`scripting::Service` / `Pool` / `Context` / `Script` interface, coexisting with Lua via the
composite `ScriptingHandler` (`lib/scripting/`). For the Lua counterpart and the overall
scripting architecture see [Lua_Scripting_System.md](Lua_Scripting_System.md).

## Requirements

- **CPython >= 3.12** — the isolation model is one *own-GIL sub-interpreter per script*
  (PEP 684); per-interpreter GIL does not exist before 3.12.
- **pybind11 >= 3.0** — `pybind11/subinterpreter.h` (`py::subinterpreter`,
  `py::subinterpreter_scoped_activate`) first shipped in 3.0.
- Configure with `-DENABLE_PYTHON=ON`; the backend builds as the OBJECT library `vcmiPython`
  and is bundled into the `vcmi` facade like `vcmiLua`. Default builds (`OFF`) contain no
  Python code and no Python dependency.

## v1 scope

The complete engine core is implemented: host-interpreter lifecycle, per-script sub-interpreter
isolation, the sandbox, deterministic configuration, VFS script loading (`SCRIPTS/`, `.py`
extension dispatch through `Service::loadScript`), per-session pools, and the data-only
`Context::callGlobal` hook path used by general engine-event scripts (e.g.
`ON_PLAYER_TURN_START`). A working example lives at `scripts/examples/onTurn.py`.

Not yet implemented (planned increments, seams already in place):

- the pybind11 game-API binding tree (`GAME` / `LIBRARY` parity with `luascript/api/`),
- the `"python"` spell-effect factory (`PythonModule::installScripting` is the seam,
  mirroring Lua's `registerFactory("lua", ...)`),
- `.pyi` stub / Markdown export from bindings (pybind11-stubgen), and a sandbox test suite.

## Architecture

```text
ScriptingHandler (composite scripting::Service, lib/scripting/)
  ├── LuaModule     (backend #1)
  └── PythonModule  (backend #2, ENABLE_PYTHON)
        ├── loadScript(".py") -> PythonScriptInstance (owned for the game lifetime)
        └── createPoolInstance()
              └── PythonScriptPool  (per game session, inside CompositePool)
                    └── PythonContext (one per script: own-GIL sub-interpreter)
```

- **PythonModule** (`scripting::Service`) — initializes the process-wide host interpreter on
  first use (`ensureHostInterpreter`), owns all general scripts loaded via `loadScript`, and
  re-registers them into every pool it creates.
- **PythonScriptInstance** (`scripting::Script`) — source text layers (base + patches) loaded
  from the VFS exactly like `LuaScriptInstance`; layers execute sequentially in one module
  namespace, so patch layers may override earlier definitions.
- **PythonScriptPool** (`scripting::Pool`) — per-session context cache;
  `getContext` returns nullptr for scripts it does not own (composite-pool contract).
- **PythonContext** (`scripting::Context`) — one isolated sub-interpreter per script.
  `callGlobal(name, params)` looks up a module-level function, calls it with the JSON
  parameters converted to a dict (`PythonJson`), and converts the plain-data result back.
  Missing functions and script exceptions are logged and yield a null result — a script can
  never abort engine processing.

## Script conventions

A Python script is a plain module. Engine hooks are module-level functions taking a single
dict of identifiers and returning plain data (or `None`):

```python
def onPlayerTurnStart(params):
    print("player", params["player"], "day", params["day"])
```

Only identifiers cross the engine boundary (same rule as Lua's `callGlobal` path). Scripts
must be pure and deterministic: no global mutable state, no assumptions about call count or
ordering — identical to the Lua "General rules".

## Host interpreter configuration (determinism)

`PythonModule::ensureHostInterpreter` initializes CPython once with:

- `PyConfig_InitIsolatedConfig` — environment variables, user site directories and
  command-line influence are ignored;
- `use_hash_seed = 1, hash_seed = 0` — deterministic `str`/`bytes` hashing across runs and
  machines (process-global; shared by all sub-interpreters; equivalent to `PYTHONHASHSEED=0`);
- `site_import = 0`, `install_signal_handlers = 0`;
- the main GIL is released after init so any engine thread can drive sub-interpreters;
- the interpreter is intentionally **never finalized** — tearing CPython down from static
  destructors is crash-prone, and the OS reclaims the process anyway.

Additional determinism rules enforced by the sandbox: `random`, `time`, `secrets` are not
importable (engine RNG will be exposed as a binding, like Lua removing `math.random`);
`id()` is disabled (address-derived); shipped builds must pin the exact CPython patch version,
since internal layout can shift `set` iteration order between versions.

## Sandbox

Installed in every sub-interpreter before any script layer runs
(`SANDBOX_BOOTSTRAP` in `PythonContext.cpp`):

1. **Isolation** — own-GIL sub-interpreter per script: separate builtins, `sys.modules` and
   import state; scripts cannot observe each other. Fork/exec/threads are disabled by the
   isolated interpreter config; legacy single-phase C extensions are rejected
   (`check_multi_interp_extensions`).
2. **Import allowlist** — a `sys.meta_path` guard raises `ImportError` for everything except
   `math` and the embedded `vcmi` module.
3. **Curated `__builtins__`** — scripts execute with a whitelist dict; `open`, `exec`, `eval`,
   `compile`, `input`, `breakpoint`, `globals`, `locals`, `vars`, `dir`, `getattr`, `setattr`,
   `delattr`, `memoryview`, `type`, `id`, `help`, `exit`, `quit` are replaced with raising
   stubs; `print` is redirected to the VCMI logger (`vcmi.logInfo`).
4. **Audit-hook tripwires** — `sys.addaudithook` aborts on `open`, `os.*`, `socket.*`,
   `subprocess.*`, `shutil.*`, `ctypes.*` events reached below the Python level.
5. **Module scrubbing** — dangerous preloaded modules are dropped from the interpreter's
   `sys.modules`.

### Threat model — read this before trusting it

This is **defense-in-depth for a cooperative mod ecosystem, not an adversarial security
boundary**. CPython is fully reflective: a determined attacker who finds any unguarded path
(the classic `().__class__.__bases__[0].__subclasses__()` family, C-extension state, a bug in
a bound object) can reach arbitrary code execution with the game's privileges. Sub-interpreters
share the OS process, memory and file descriptors; CPython's own documentation states that
audit hooks (PEP 578) and sub-interpreter isolation are *not* sandboxes. Treat `.py` mods with
exactly the same trust as binary mods. A hard boundary would require OS-level isolation
(separate process + seccomp/AppContainer, or WASM) — explicitly future work.

## The embedded `vcmi` module

`pythonscript/api/VcmiModule.cpp`, declared with
`PYBIND11_EMBEDDED_MODULE(vcmi, m, py::multiple_interpreters::per_interpreter_gil())` — the
tag is mandatory for own-GIL sub-interpreters. Each sub-interpreter imports its own copy, so
the module must not rely on shared mutable state.

v1 surface: `logInfo(msg)`, `logWarn(msg)`, `logError(msg)`. The game API (creatures, heroes,
battle, callbacks) plugs in here next, mapping the `ApiTags.h` lifetime models onto pybind11
return-value policies (`reference` / `reference_internal`, `shared_ptr` holders, `copy`) —
see the Lua registrar docs for the exposure rules that carry over.

## Implementation sharp edges (for maintainers)

- `py::error_already_set` must be caught **inside** a `subinterpreter_scoped_activate` scope —
  it holds references into that interpreter and escaping the scope is a documented crash.
- `PythonContext::scriptGlobals` is interpreter-owned: it is released while the interpreter is
  active in the destructor, before the sub-interpreter is destroyed.
- On Python 3.12 a sub-interpreter must be destroyed on the thread that created it (relaxed in
  3.13); pools are created and destroyed on the server/game thread.
- Packaging must ship a pinned CPython >= 3.12 with a curated stdlib subset; the runtime
  allowlist and the shipped file set must agree. Conan: `pybind11/3.0.x` behind a
  `with_python` option in the `vcmi-dependencies` recipe (see Conan.md).
