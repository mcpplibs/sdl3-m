#!/usr/bin/env python3
"""The public surface of SDL 3.4.18's headers, from the pinned release.

Shared by tools/gen_exports.py (which writes src/sdl3.cppm, src/inline.cpp
and the generated part of src/macros.cppm) and tools/check_exports.py (which
audits the module against the headers, on every CI row). The headers come
from the upstream tarball, downloaded once into target/upstream and checked
against upstream/documents.json: the same bytes compat.sdl3 builds.

Two readers share one declaration parser:

- `surface()` reads the raw headers. It follows every #if branch, keeping
  the condition each declaration and macro is under (SDL's documentation-only
  SDL_WIKI_DOCUMENTATION_SECTION branches are dead, `__cplusplus` is true and
  SDL_MAIN_HANDLED is defined, as the module defines it for SDL_main.h).
- `declarations()` also reads a compiler's preprocessed output, where the
  conditions are already decided for one target; check_exports.py feeds it
  each CI row's own preprocessing of the headers.
"""
from __future__ import annotations

from dataclasses import dataclass, field
import hashlib
import json
from pathlib import Path
import re
import sys
import tarfile
import urllib.request

# The headers the module wraps: <SDL3/SDL.h> and its closure, <SDL3/SDL_vulkan.h>
# (SDL's Vulkan entry points; it declares the few Vulkan handles they take
# itself) and <SDL3/SDL_main.h> (included with SDL_MAIN_HANDLED: its
# functions, not its `main` redefinition). See NOT_WRAPPED for the rest.
ROOTS = ('SDL.h', 'SDL_vulkan.h', 'SDL_main.h', 'SDL_revision.h')

# The other public headers of include/SDL3, and why the module leaves them out.
NOT_WRAPPED = {
    'SDL_main_impl.h': "the definitions of the program's entry point that <SDL3/SDL_main.h> includes into the "
                       'translation unit defining main; textual by design',
    'SDL_egl.h': "Khronos' EGL API as SDL ships it (SDL's own EGL entry points are in SDL_video.h and exported); "
                 'include it, or the system <EGL/egl.h>, directly',
    'SDL_opengl.h': "Khronos' OpenGL API as SDL ships it (SDL_GL_* are in SDL_video.h and exported); include it directly",
    'SDL_opengl_glext.h': "Khronos' OpenGL extensions, included by SDL_opengl.h",
    'SDL_opengles.h': "Khronos' OpenGL ES 1 API; include it directly",
    'SDL_opengles2.h': "Khronos' OpenGL ES 2 API; include it directly",
    'SDL_opengles2_gl2.h': "Khronos' OpenGL ES 2 API, included by SDL_opengles2.h",
    'SDL_opengles2_gl2ext.h': "Khronos' OpenGL ES 2 extensions, included by SDL_opengles2.h",
    'SDL_opengles2_gl2platform.h': "Khronos' OpenGL ES 2 platform macros, included by SDL_opengles2.h",
    'SDL_opengles2_khrplatform.h': "Khronos' platform types, included by SDL_opengles2.h",
    'SDL_intrin.h': "which CPU intrinsics the compiler offers, as #if-tests (SDL_SSE2_INTRINSICS ...), and the "
                    "compiler's intrinsic headers; conditional compilation only, include it directly",
    'SDL_test.h': 'the SDL3_test library (test harness, fuzzer, ...), which compat.sdl3 does not build',
    'SDL_test_assert.h': 'the SDL3_test library', 'SDL_test_common.h': 'the SDL3_test library',
    'SDL_test_compare.h': 'the SDL3_test library', 'SDL_test_crc32.h': 'the SDL3_test library',
    'SDL_test_font.h': 'the SDL3_test library', 'SDL_test_fuzzer.h': 'the SDL3_test library',
    'SDL_test_harness.h': 'the SDL3_test library', 'SDL_test_log.h': 'the SDL3_test library',
    'SDL_test_md5.h': 'the SDL3_test library', 'SDL_test_memory.h': 'the SDL3_test library',
    'SDL_copying.h': 'the zlib license text, no declarations',
}

# Conditions with a known value where the module includes the headers.
KNOWN = {
    'SDL_WIKI_DOCUMENTATION_SECTION': False,  # SDL's documentation generator only
    '__cplusplus': True,
    'SDL_MAIN_HANDLED': True,                 # the module includes SDL_main.h for its functions only
    'SDL_MAIN_NOIMPL': False,
    'SDL_PLATFORM_PRIVATE': False,            # console ports' private headers
    'SDL_PLATFORM_PRIVATE_MAIN': False,
}


def fetch(root: Path) -> Path:
    """The extracted upstream release (cached in target/upstream)."""
    pins = json.loads((root / 'upstream/documents.json').read_text(encoding='utf-8'))
    cache = root / 'target/upstream'
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / Path(pins['upstream']).name
    if not archive.exists() or hashlib.sha256(archive.read_bytes()).hexdigest() != pins['upstream_sha256']:
        print('+ fetch', pins['upstream'], file=sys.stderr, flush=True)
        with urllib.request.urlopen(pins['upstream'], timeout=300) as response:
            archive.write_bytes(response.read())
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if digest != pins['upstream_sha256']:
        sys.exit(f'upstream archive checksum {digest} != {pins["upstream_sha256"]}')
    with tarfile.open(archive) as tar:
        top = tar.getmembers()[0].name.split('/')[0]
        if not (cache / top).exists():
            tar.extractall(cache, filter='data')
    return cache / top


# ── text ────────────────────────────────────────────────────────────────────

def strip_comments(text: str) -> str:
    """Comments become spaces (newlines kept); string and character literals stay."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == '/' and text.startswith('/*', i):
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r'[^\n]', ' ', text[i:j]))
            i = j
        elif c == '/' and text.startswith('//', i):
            j = text.find('\n', i)
            j = n if j < 0 else j
            out.append(' ' * (j - i))
            i = j
        elif c in '"\'':
            j = i + 1
            while j < n and text[j] != c and text[j] != '\n':
                j += 2 if text[j] == '\\' else 1
            out.append(text[i:j + 1])
            i = j + 1
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def logical_lines(text: str) -> list[str]:
    """Physical lines with continuations joined; a joined line's followers become empty."""
    lines = text.split('\n')
    out, i = [], 0
    while i < len(lines):
        line, extra = lines[i], 0
        while line.endswith('\\') and i + extra + 1 < len(lines):
            extra += 1
            line = line[:-1] + ' ' + lines[i + extra]
        out.append(line)
        out.extend([''] * extra)
        i += extra + 1
    return out


# ── conditions ──────────────────────────────────────────────────────────────

@dataclass
class Branch:
    group: int
    index: int
    expr: str            # this branch's own test, as a #if expression ('1' for #else)
    known: bool | None   # its value where known


@dataclass
class Group:
    id: int
    branches: list = field(default_factory=list)
    has_else: bool = False


def known_value(expr: str) -> bool | None:
    e = ' '.join(expr.split())
    m = re.fullmatch(r'!?\s*defined\s*\(?\s*(\w+)\s*\)?', e)
    if m and m.group(1) in KNOWN:
        return KNOWN[m.group(1)] != e.startswith('!')
    if e in ('0', '1'):
        return e == '1'
    return None


def branch_condition(path: list[Branch], groups: dict[int, Group]) -> list[str]:
    """The #if-expressions (conjoined) that select this path, known values dropped."""
    terms = []
    for b in path:
        g = groups[b.group]
        for prior in g.branches[:b.index]:
            if prior.known is None:
                terms.append(f'!({prior.expr})')
        if b.known is None and b.expr != '1':
            terms.append(f'({b.expr})')
    return terms


# ── declarations ────────────────────────────────────────────────────────────

@dataclass
class Decl:
    kind: str     # function | type | tag | enumerator | inline
    name: str
    pos: int      # offset of the name in the text it was read from
    detail: dict = field(default_factory=dict)


IDENT = r'[A-Za-z_]\w*'


def statements(code: str):
    """Top-level statements of C declarations: (text, start offset). `extern "C" {`
    and its brace are transparent; a function body ends its definition."""
    out, start, i, n = [], 0, 0, len(code)
    depth = paren = 0
    transparent = []  # brace depths opened by extern "C" {
    func_body = False
    while i < n:
        c = code[i]
        if c in '"\'':
            j = i + 1
            while j < n and code[j] != c:
                j += 2 if code[j] == '\\' else 1
            i = j + 1
            continue
        if c == '(':
            paren += 1
        elif c == ')':
            paren -= 1
        elif c == '{' and paren == 0:
            head = code[start:i]
            if depth == 0 and re.fullmatch(r'\s*extern\s*"C\+*"\s*', head):
                transparent.append(depth)
                start = i + 1
                i += 1
                continue
            if depth == 0 and re.search(r'\)\s*$', head) and not re.match(r'\s*(typedef|struct|union|enum)\b', head) \
                    and '=' not in head:
                func_body = True
            depth += 1
        elif c == '}' and paren == 0:
            if depth == 0 and transparent:
                transparent.pop()
                start = i + 1
                i += 1
                continue
            depth -= 1
            if depth == 0 and func_body:
                out.append((code[start:i + 1], start))
                start, func_body = i + 1, False
        elif c == ';' and depth == 0 and paren == 0:
            out.append((code[start:i + 1], start))
            start = i + 1
        i += 1
    rest = code[start:]
    if rest.strip():
        out.append((rest, start))
    return out


def _skip_ws(text, i):
    while i < len(text) and text[i].isspace():
        i += 1
    return i


def declarations(code: str, sdlcall: str = 'SDLCALL', force_inline: str = 'SDL_FORCE_INLINE',
                 known_statement=None) -> tuple[list[Decl], list[tuple[str, int]]]:
    """Declarations in C header code (directives removed). Returns the
    declarations and the statements nothing was recognised in."""
    decls, unknown = [], []
    for text, base in statements(code):
        body = text.strip()
        if not body or body == ';':
            continue
        lead = len(text) - len(text.lstrip())
        off = base + lead
        if re.match(r'(extern\s+|static\s+|__extension__\s+)*(' + re.escape(force_inline) + r')\b', body) or \
                re.match(r'static\s+(__inline__|__inline|inline)\b', body) or \
                (body.endswith('}') and re.match(r'(\w+\s+)*__mwrap_force_inline\b', body)):
            m = re.search(r'(' + IDENT + r')\s*\(([^()]*)\)\s*\{', body)
            if m:
                head = body[:m.start(1)]
                head = re.sub(r'\b' + re.escape(force_inline) + r'\b|\bstatic\b|\b__inline__\b|\b__inline\b|\binline\b'
                              r'|__attribute__\s*\(\(.*?\)\)|\b__forceinline\b', ' ', head)
                decls.append(Decl('inline', m.group(1), off + m.start(1),
                                  {'returns': ' '.join(head.split()), 'params': ' '.join(m.group(2).split())}))
                continue
        if re.match(r'extern\b', body) and not re.match(r'extern\s*"C', body):
            m = re.search(r'\b' + re.escape(sdlcall) + r'\s+(' + IDENT + r')\s*\(', body)
            if m:
                decls.append(Decl('function', m.group(1), off + m.start(1)))
                continue
            m = re.match(r'extern\s+__inline\s+\w+\s+(\w+)\s*\(', body)  # Watcom's SDL_Swap64
            if m:
                decls.append(Decl('inline', m.group(1), off + m.start(1), {'watcom': True}))
                continue
            unknown.append((body, off))
            continue
        if body.startswith('typedef'):
            m = re.match(r'typedef\s+(struct|union|enum)\s*(' + IDENT + r')?\s*\{', body)
            if m:
                close = _matching(body, body.index('{', m.start()))
                tail = body[close + 1:]
                n = re.match(r'\s*(' + IDENT + r')\s*(\[[^\]]*\])?\s*;', tail)
                if not n:
                    unknown.append((body, off))
                    continue
                decls.append(Decl('type', n.group(1), off + close + 1 + n.start(1)))
                if m.group(2) and m.group(2) != n.group(1):
                    decls.append(Decl('tag', m.group(2), off + m.start(2)))
                if m.group(1) == 'enum':
                    inner_start = body.index('{', m.start()) + 1
                    decls += _enumerators(body[inner_start:close], off + inner_start)
                continue
            m = re.search(r'\(\s*(?:' + re.escape(sdlcall) + r'\s*)?\*\s*(' + IDENT + r')\s*\)\s*\(', body)
            if m:
                decls.append(Decl('type', m.group(1), off + m.start(1)))
                continue
            m = re.search(r'(' + IDENT + r')\s*(\[[^\]]*\])*\s*;\s*$', body)
            if m:
                decls.append(Decl('type', m.group(1), off + m.start(1)))
                t = re.match(r'typedef\s+(?:struct|union|enum)\s+(' + IDENT + r')\b', body)
                if t and t.group(1) != m.group(1):
                    decls.append(Decl('tag', t.group(1), off + t.start(1)))
                continue
            unknown.append((body, off))
            continue
        m = re.match(r'(struct|union|enum)\s+(' + IDENT + r')\s*(\{|;)', body)
        if m:
            decls.append(Decl('tag', m.group(2), off + m.start(2)))
            if m.group(1) == 'enum' and m.group(3) == '{':
                inner = body.index('{') + 1
                decls += _enumerators(body[inner:_matching(body, inner - 1)], off + inner)
            continue
        if known_statement and known_statement(body):
            continue
        unknown.append((body, off))
    return decls, unknown


def _matching(text: str, open_at: int) -> int:
    depth = 0
    for i in range(open_at, len(text)):
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
            if depth == 0:
                return i
    raise ValueError('unbalanced braces')


def _enumerators(body: str, base: int) -> list[Decl]:
    out, depth, start = [], 0, 0
    items = []
    for i, c in enumerate(body + ','):
        if c in '([':
            depth += 1
        elif c in ')]':
            depth -= 1
        elif c in ',\x0b' and depth == 0:
            items.append((body[start:i], start))
            start = i + 1
    for item, at in items:
        m = re.match(r'\s*(' + IDENT + r')', item)
        if m:
            out.append(Decl('enumerator', m.group(1), base + at + m.start(1)))
    return out


# ── the raw headers ─────────────────────────────────────────────────────────

@dataclass
class Macro:
    name: str
    params: list[str] | None   # None: object-like
    body: str
    header: str
    line: int
    condition: list[str]       # conjoined #if-expressions
    path: tuple                # the branch path, for coverage


@dataclass
class Entry:
    """A declared name: every place it is declared, under its conditions."""
    kind: str
    name: str
    header: str
    places: list = field(default_factory=list)   # (path, condition terms)
    detail: dict = field(default_factory=dict)


@dataclass
class Header:
    name: str
    includes: list[str]
    macros: list[Macro]
    undefs: list[tuple[str, tuple]]
    decls: list[tuple[Decl, tuple, list[str]]]
    unknown: list[tuple[str, int]]


_gid = [0]


def read_header(path: Path, groups: dict[int, Group]) -> Header:
    raw = path.read_text(encoding='utf-8')
    lines = logical_lines(strip_comments(raw))
    stack: list[tuple[Group, Branch]] = []
    code_lines, line_path = [], []
    macros, undefs, includes = [], [], []
    guard = None

    def live():
        for g, b in stack:
            if b.known is False:
                return False
            if any(p.known is True for p in g.branches[:b.index]):
                return False
        return True

    for no, line in enumerate(lines, 1):
        d = re.match(r'\s*#\s*(\w+)\s*(.*)$', line)
        path_now = tuple(b for _, b in stack)
        if not d:
            code_lines.append(line if live() else '')
            line_path.append(path_now)
            continue
        code_lines.append('\x0b')  # whitespace that still separates enumerators
        line_path.append(path_now)
        kw, rest = d.group(1), d.group(2).strip()
        if kw in ('if', 'ifdef', 'ifndef'):
            expr = rest if kw == 'if' else (f'defined({rest})' if kw == 'ifdef' else f'!defined({rest})')
            if guard is None and kw == 'ifndef' and not stack and re.fullmatch(r'SDL\w*_h_?', rest):
                guard = rest
                expr = '1'
            _gid[0] += 1
            g = Group(_gid[0])
            groups[g.id] = g
            b = Branch(g.id, 0, expr, known_value(expr) if expr != '1' else True)
            g.branches.append(b)
            stack.append((g, b))
        elif kw == 'elif':
            g, _ = stack.pop()
            b = Branch(g.id, len(g.branches), rest, known_value(rest))
            g.branches.append(b)
            stack.append((g, b))
        elif kw == 'else':
            g, _ = stack.pop()
            b = Branch(g.id, len(g.branches), '1', True)
            g.branches.append(b)
            g.has_else = True
            stack.append((g, b))
        elif kw == 'endif':
            stack.pop()
        elif not live():
            continue
        elif kw == 'define':
            m = re.match(r'(' + IDENT + r')(\(([^)]*)\))?\s*(.*)$', rest)
            name = m.group(1)
            params = None if m.group(2) is None else [p.strip() for p in m.group(3).split(',') if p.strip()]
            path_t = tuple(b for _, b in stack)
            macros.append(Macro(name, params, m.group(4).strip(), path.name, no,
                                branch_condition(list(path_t), groups), path_t))
        elif kw == 'undef':
            undefs.append((rest.split()[0], tuple(b for _, b in stack)))
        elif kw == 'include':
            m = re.match(r'<SDL3/(\w+\.h)>', rest)
            if m:
                includes.append(m.group(1))
    code = '\n'.join(code_lines)
    # SDL_vulkan.h declares its Vulkan handles through a macro it #undefs again.
    code = re.sub(r'\bVK_DEFINE_(?:NON_DISPATCHABLE_)?HANDLE\((\w+)\)',
                  lambda m: f'typedef struct {m.group(1)}_T *{m.group(1)};', code)
    starts = [0]
    for line in code_lines:
        starts.append(starts[-1] + len(line) + 1)

    def line_of(pos):
        lo, hi = 0, len(starts) - 1
        while lo < hi:
            mid = (lo + hi + 1) // 2
            if starts[mid] <= pos:
                lo = mid
            else:
                hi = mid - 1
        return lo

    found, unknown = declarations(code, known_statement=_raw_known_statement)
    decls = []
    for d in found:
        p = line_path[line_of(d.pos)]
        decls.append((d, p, branch_condition(list(p), groups)))
    return Header(path.name, includes, macros, undefs, decls, unknown)


def _raw_known_statement(body: str) -> bool:
    # SDL_COMPILE_TIME_ASSERT(name, cond); — checks, not declarations.
    return bool(re.match(r'SDL_COMPILE_TIME_ASSERT\s*\(', body))


def covered(paths: list[tuple], groups: dict[int, Group]) -> bool:
    """Whether a name declared on these branch paths is declared whatever the
    unknown conditions are (every branch of a group with an #else, recursively)."""
    def cov(prefix_len: int, subset: list[tuple]) -> bool:
        if any(len(p) == prefix_len for p in subset):
            return True
        by_group: dict[int, dict[int, list[tuple]]] = {}
        for p in subset:
            b = p[prefix_len]
            by_group.setdefault(b.group, {}).setdefault(b.index, []).append(p)
        for gid, branches in by_group.items():
            g = groups[gid]
            # A branch known to be taken covers the group alone.
            for idx, ps in branches.items():
                b = g.branches[idx]
                if b.known is True and all(x.known is not True for x in g.branches[:idx]) and cov(prefix_len + 1, ps):
                    return True
            live = [b for b in g.branches if b.known is not False]
            taken = next((i for i, b in enumerate(g.branches) if b.known is True), None)
            needed = [b.index for b in live if taken is None or b.index <= taken]
            if (g.has_else or taken is not None) and all(i in branches and cov(prefix_len + 1, branches[i])
                                                         for i in needed):
                return True
        return False
    return cov(0, paths)


@dataclass
class Surface:
    root: Path
    headers: dict[str, Header]
    order: list[str]
    groups: dict[int, Group]
    entries: dict[str, Entry]          # declared names
    macros: dict[str, list[Macro]]     # every live #define, by name
    transient: set[str]                # macros a header #undefs again

    def condition(self, places) -> str | None:
        """None when unconditional, else a #if-expression."""
        if covered([p for p, _ in places], self.groups):
            return None
        alternatives = []
        for _, terms in places:
            alt = ' && '.join(terms) if terms else '1'
            if alt not in alternatives:
                alternatives.append(alt)
        return alternatives[0] if len(alternatives) == 1 else ' || '.join(f'({a})' for a in alternatives)


def closure(include: Path) -> list[str]:
    order, seen = [], set()

    def visit(name):
        if name in seen or name in ('SDL_main_impl.h',):
            return
        seen.add(name)
        text = strip_comments((include / name).read_text(encoding='utf-8'))
        for inc in re.findall(r'^\s*#\s*include\s*<SDL3/(\w+\.h)>', text, flags=re.M):
            visit(inc)
        order.append(name)
    for r in ROOTS:
        visit(r)
    return order


def surface(upstream: Path) -> Surface:
    include = upstream / 'include/SDL3'
    order = closure(include)
    groups: dict[int, Group] = {}
    headers = {name: read_header(include / name, groups) for name in order}
    entries: dict[str, Entry] = {}
    macros: dict[str, list[Macro]] = {}
    transient = set()
    for name in order:
        h = headers[name]
        for d, path, terms in h.decls:
            e = entries.get(d.name)
            if e is None:
                e = entries[d.name] = Entry(d.kind, d.name, name, detail=d.detail)
            elif e.kind != d.kind and {e.kind, d.kind} != {'type', 'tag'}:
                e.detail.setdefault('also', set()).add(d.kind)
            e.places.append((path, terms))
        for m in h.macros:
            macros.setdefault(m.name, []).append(m)
        for u, _ in h.undefs:
            if any(m.header == name for m in macros.get(u, [])):
                transient.add(u)
    return Surface(upstream, headers, order, groups, entries, macros, transient)


def wrapped_header_files(upstream: Path) -> list[str]:
    return closure(upstream / 'include/SDL3')


if __name__ == '__main__':
    root = Path(__file__).resolve().parents[1]
    s = surface(fetch(root))
    kinds = {}
    for e in s.entries.values():
        kinds[e.kind] = kinds.get(e.kind, 0) + 1
    print(len(s.order), 'headers:', ' '.join(s.order))

    print(kinds, len(s.macros), 'macros,', len(s.transient), 'transient')
    for h in s.headers.values():
        for body, _ in h.unknown:
            print('UNKNOWN', h.name, ' '.join(body.split())[:160])
    for e in s.entries.values():
        c = s.condition(e.places)
        if c:
            print('COND', e.kind, e.name, '::', c[:200])
        if 'also' in e.detail:
            print('MIXED', e.kind, e.name, e.detail['also'])
    present = {p.name for p in (s.root / 'include/SDL3').glob('*.h')}
    print('unaccounted headers:', sorted(present - set(s.order) - NOT_WRAPPED.keys()))
