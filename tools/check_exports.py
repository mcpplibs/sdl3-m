#!/usr/bin/env python3
"""Audit the module against SDL 3.4.18's headers, on this machine's target.

1. The generated sources are what tools/gen_exports.py writes from the
   pinned headers (src/sdl3.cppm, src/inline.cpp, src/macros.cppm,
   src/macros.cpp), and every name tools/policy.py lists exists.
2. The raw headers, every #if branch read: each declaration is exported or
   excluded with a reason, each macro is mapped, kept as a macro or excluded
   with a reason.
3. This target, as the compiler sees it: the headers are preprocessed with
   the module's own compile command (from compile_commands.json, so run
   `mcpp build` first) and so are the module sources. Every function, type,
   enumerator and struct the headers declare here must be exported here,
   every SDL_FORCE_INLINE function adapted, and every macro they leave
   defined mapped here (or listed). Run on every CI row: Linux, macOS and
   Windows declare different things (SDL_SetLinuxThreadPriority,
   SDL_GetDXGIOutputInfo, MSG ...).

The declaration parser is the generator's (tools/inventory.py), but here it
reads the preprocessor's output, with SDLCALL and SDL_FORCE_INLINE defined
as markers: a printf-format or thread-safety attribute in a declaration
cannot hide it.
"""
from __future__ import annotations

import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from inventory import NOT_WRAPPED, declarations, fetch, surface  # noqa: E402
import gen_exports  # noqa: E402
import policy  # noqa: E402

SDLCALL = '__mwrap_sdlcall'
FORCE_INLINE = '__mwrap_force_inline'


def statement_name(body: str) -> str | None:
    m = re.search(r'(\w+)\s*\(', body) or re.search(r'(\w+)\s*;\s*$', body)
    return m.group(1) if m else None


def compile_command(root: Path) -> list[str]:
    # The configuration databases mcpp keeps in target/<triple>/<fingerprint>/
    # (and the copy it publishes at the root when configured to): the newest
    # one is the last `mcpp build`'s.
    found = sorted([p for p in root.glob('target/*/*/compile_commands.json') if p.parts[-3] != 'conformance']
                   + list(root.glob('compile_commands.json')), key=lambda p: p.stat().st_mtime, reverse=True)
    norm = lambda p: os.path.normcase(str(Path(p).resolve()))  # noqa: E731
    module = norm(root / 'src/sdl3.cppm')
    entries = []
    for db in found:
        entries = [e for e in json.loads(db.read_text(encoding='utf-8'))
                   if norm(Path(e['directory'], e['file'])) == module]
        if entries:
            break
    if not entries:
        sys.exit('no compile command for src/sdl3.cppm (target/*/*/compile_commands.json): run `mcpp build` first')
    for entry in entries:
            args = entry.get('arguments') or shlex.split(entry['command'], posix=os.name != 'nt')
            break
    else:
        sys.exit('compile_commands.json has no command for src/sdl3.cppm')
    out, skip = [], False
    for a in args:
        if skip:
            skip = False
            continue
        if a in ('-c', '-o', '-x', '-MF', '-MT', '-MQ'):
            skip = a != '-c'
            continue
        if a == entry['file'] or a.endswith('sdl3.cppm') or a.startswith(('-fmodule', '-fprebuilt-module-path',
                                                                            '-fmodules', '-fdeps-', '--precompile',
                                                                            '-MD', '-MMD')):
            continue
        out.append(a)
    return out


def preprocess(cmd: list[str], source: str, work: Path, name: str, keep_defines: bool) -> str:
    work.mkdir(parents=True, exist_ok=True)
    path = work / name
    path.write_text(source, encoding='utf-8')
    run = subprocess.run(cmd + ['-x', 'c++', '-E'] + (['-dD'] if keep_defines else ['-P']) + [str(path)],
                         capture_output=True, text=True, encoding='utf-8', errors='replace')
    if run.returncode != 0:
        sys.exit(f'preprocessing {name} failed:\n{run.stderr[-4000:]}')
    return run.stdout


def target_headers(text: str, wrapped: set[str]):
    """The SDL headers' code and final macro set, from `-E -dD` output."""
    code, macros = [], {}
    current = None
    for line in text.splitlines():
        m = re.match(r'#\s*(?:line\s+)?\d+\s+"([^"]*)"', line)
        if m:
            path = m.group(1).replace('\\\\', '/').replace('\\', '/')
            base = path.rsplit('/', 1)[-1]
            current = base if '/SDL3/' in path and base in wrapped else None
            continue
        d = re.match(r'#\s*(define|undef)\s+(\w+)', line)
        if d:
            if d.group(1) == 'define' and current:
                macros[d.group(2)] = current
            elif d.group(1) == 'undef':
                macros.pop(d.group(2), None)
            continue
        if line.startswith('#'):
            continue
        if current:
            code.append(line)
    return '\n'.join(code), macros


def module_text(path: Path) -> str:
    """A module source without its module and import declarations, to preprocess."""
    lines = []
    for line in path.read_text(encoding='utf-8').splitlines():
        if re.match(r'\s*(export\s+)?(module|import)\b[^;]*;\s*$', line):
            continue
        lines.append(line)
    return '\n'.join(lines) + '\n'


def block(text: str, opener: str) -> str:
    """The body of `opener {` ... its closing brace (in preprocessed text)."""
    out = []
    for m in re.finditer(re.escape(opener) + r'\s*\{', text):
        i, depth = m.end(), 1
        while depth and i < len(text):
            depth += {'{': 1, '}': -1}.get(text[i], 0)
            i += 1
        out.append(text[m.end():i - 1])
    return '\n'.join(out)


def mapped_names(text: str) -> set[str]:
    names = set(re.findall(r'inline\s+constexpr\s+auto\s*&?\s*(\w+)\s*=', text))
    names |= set(re.findall(r'(?:^|[;}>]|\n)\s*(?:inline\s+|constexpr\s+)*[\w:\s\*&]*?\b(\w+)\s*\([^;{]*\)\s*(?:\{|;)',
                            text))
    return names


def main():
    root = Path(__file__).resolve().parents[1]
    problems = []

    # 1. generated sources
    generated = subprocess.run([sys.executable, str(root / 'tools/gen_exports.py'), '--check'],
                               capture_output=True, text=True)
    if generated.returncode != 0:
        problems.append(generated.stderr.strip() or generated.stdout.strip())

    # 2. the raw headers
    upstream = fetch(root)
    s = surface(upstream)
    present = {p.name for p in (upstream / 'include/SDL3').glob('*.h')}
    if present - set(s.order) - NOT_WRAPPED.keys():
        problems.append('public headers neither wrapped nor listed: ' +
                        ', '.join(sorted(present - set(s.order) - NOT_WRAPPED.keys())))
    if NOT_WRAPPED.keys() - present:
        problems.append('NOT_WRAPPED lists absent headers: ' + ', '.join(sorted(NOT_WRAPPED.keys() - present)))
    for h in s.headers.values():
        for body, _ in h.unknown:
            if statement_name(body) not in policy.NOT_SDL_STATEMENTS:
                problems.append(f'{h.name}: a statement the inventory does not understand: {" ".join(body.split())[:120]}')
    exported = {e.name for e, _ in gen_exports.exported_entries(s)}
    inline = {n for n, e in s.entries.items() if e.kind == 'inline'}
    declared = set(s.entries)
    missing = declared - exported - inline - policy.EXCLUDED_DECLS.keys()
    if missing:
        problems.append('declarations neither exported nor excluded: ' + ', '.join(sorted(missing)))
    missing = inline - policy.ADAPTED.keys() - policy.EXCLUDED_DECLS.keys()
    if missing:
        problems.append('SDL_FORCE_INLINE functions neither adapted nor excluded: ' + ', '.join(sorted(missing)))
    stale = (policy.EXCLUDED_DECLS.keys() | policy.ADAPTED.keys()) - declared
    if stale:
        problems.append('tools/policy.py lists undeclared names: ' + ', '.join(sorted(stale)))
    table = gen_exports.macro_table(s)
    kinds = {}
    for n, h, k, r in table:
        kinds[k] = kinds.get(k, 0) + 1
        if k == 'error':
            problems.append(f'macro {n} ({h}): {r}')
    listed = (policy.FUNCTION_MACROS.keys() | policy.SIDE_HEADER.keys() | policy.EXCLUDED_MACROS.keys())
    stale = listed - s.macros.keys()
    if stale:
        problems.append('tools/policy.py lists undefined macros: ' + ', '.join(sorted(stale)))
    both = [n for n in policy.FUNCTION_MACROS if n in policy.SIDE_HEADER or n in policy.EXCLUDED_MACROS]
    if both:
        problems.append('mapped yet listed as unmapped: ' + ', '.join(both))

    # 3. this target
    cmd = compile_command(root)
    work = root / 'target/audit'
    probe = f'#define SDLCALL {SDLCALL}\n#define SDL_FORCE_INLINE {FORCE_INLINE}\n' + gen_exports.INCLUDES
    code, target_macros = target_headers(preprocess(cmd, probe, work, 'headers.cpp', True), set(s.order))
    found, unknown = declarations(code, SDLCALL, FORCE_INLINE,
                                  known_statement=lambda b: b.startswith(('static_assert', '_Static_assert')))
    for body, _ in unknown:
        if statement_name(body) not in policy.NOT_SDL_STATEMENTS:
            problems.append(f'[target] a statement the audit does not understand: {" ".join(body.split())[:120]}')
    here = {d.name for d in found if d.kind != 'inline'}
    here_inline = {d.name for d in found if d.kind == 'inline'}
    module = preprocess(cmd, module_text(root / 'src/sdl3.cppm'), work, 'sdl3.cpp', False)
    exported_here = set(re.findall(r'export\s+using\s+::(\w+)\s*;', module))
    adapted_here = mapped_names(block(module, 'export namespace mcpplibs::mwrap::sdl3'))
    macros_module = preprocess(cmd, module_text(root / 'src/macros.cppm'), work, 'macros.cpp', False)
    mapped_here = mapped_names(block(macros_module, 'export namespace mcpplibs::mwrap::sdl3::macros'))

    missing = here - exported_here - policy.EXCLUDED_DECLS.keys()
    if missing:
        problems.append('[target] declared here, not exported: ' + ', '.join(sorted(missing)))
    extra = exported_here - here
    if extra:
        problems.append('[target] exported, not declared here: ' + ', '.join(sorted(extra)))
    missing = here_inline - adapted_here - policy.EXCLUDED_DECLS.keys()
    if missing:
        problems.append('[target] SDL_FORCE_INLINE functions here, not adapted: ' + ', '.join(sorted(missing)))
    if adapted_here != policy.ADAPTED.keys():
        problems.append('[target] the adaptations are not ADAPTED: ' +
                        ', '.join(sorted(adapted_here ^ policy.ADAPTED.keys())))
    classes = {n: k for n, _, k, _ in table}
    target_macros.pop('SDLCALL', None)
    target_macros.pop('SDL_FORCE_INLINE', None)
    unaccounted, unmapped = [], []
    for n, header in sorted(target_macros.items()):
        k = classes.get(n)
        if k is None:
            unaccounted.append(f'{n} ({header})')
        elif k in ('constant', 'string', 'function') and n not in mapped_here:
            unmapped.append(n)
    if unaccounted:
        problems.append('[target] macros the raw headers did not show: ' + ', '.join(unaccounted))
    if unmapped:
        problems.append('[target] macros defined here, not mapped here: ' + ', '.join(unmapped))
    stale = {n for n in mapped_here if n not in target_macros}
    if stale:
        problems.append('[target] mapped, not defined here: ' + ', '.join(sorted(stale)))

    if problems:
        sys.exit('\n'.join(problems))
    counted = {}
    for n in here:
        k = s.entries[n].kind if n in s.entries else '?'
        counted[k] = counted.get(k, 0) + 1
    print(f'raw headers: {len(exported)} declarations exported, {len(policy.ADAPTED)} inline functions adapted, '
          f'{len(policy.EXCLUDED_DECLS)} excluded; {len(table)} macros: '
          + ', '.join(f'{v} {k}' for k, v in sorted(kinds.items())))
    print(f'this target: {len(here)} declarations ({", ".join(f"{v} {k}" for k, v in sorted(counted.items()))}) '
          f'all exported; {len(here_inline)} inline functions adapted or excluded; {len(target_macros)} macros: '
          f'{len([n for n in target_macros if n in mapped_here])} mapped, the rest kept as macros or excluded')


if __name__ == '__main__':
    main()
