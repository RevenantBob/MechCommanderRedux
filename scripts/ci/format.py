"""Formats the port's C++: clang-format with the repo's .clang-format (Allman braces, 4-space indent, 120 columns,
braces on every if/else/for/while body), then the passes clang-format can't do:

- braces around every case body of more than one statement (a trailing break is not counted, and goes inside);
- a blank line after a closing brace when more code follows at the same level (not before else, catch, another
  closing brace or break);
- a blank line before an if/for/while/do/switch/try that follows a statement (and before the comments above it).

Usage:
    python scripts/ci/format.py                      # format every file in place
    python scripts/ci/format.py --check              # report files that need formatting; exit 1 if any
    python scripts/ci/format.py object/ gui/updisp.cpp   # only these (relative to src/lib/MCCore, or the repo)

Covers src/lib/MCCore and src/apps. SDL (src/lib/SDL-release-*) and src/build are never touched. Uses the clang-format
that ships with Visual Studio, or the one on PATH, or the one named by $CLANG_FORMAT.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CORE = os.path.join(REPO, "src", "lib", "MCCore")
ROOTS = [CORE, os.path.join(REPO, "src", "apps")]
EXTENSIONS = (".cpp", ".h", ".hpp", ".inl")
VS_CLANG_FORMAT = r"C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Tools\Llvm\x64\bin\clang-format.exe"

# A line that closes a block: "}", "};", "} while (...);", optionally with a trailing comment.
CLOSE_BLOCK = re.compile(r"^\}(;|\s+while\s*\(.*\);)?\s*(//.*)?$")
# Lines that belong to the block just closed, so no blank line goes before them.
CLOSE_CONTINUES = re.compile(r"^(\}|\)|else\b|catch\b|break;|while\b|case\b|default\b|#)")
# A case label on its own line (clang-format puts it there).
CASE_LABEL = re.compile(r"^(case\b.*|default\s*):\s*(//.*)?$")
# A control statement that gets a blank line above it.
CONTROL = re.compile(r"^(if|for|while|do|switch|try)\b")
COMMENT = re.compile(r"^(//|/\*|\*)")


def find_clang_format():
    for candidate in (os.environ.get("CLANG_FORMAT"), VS_CLANG_FORMAT, shutil.which("clang-format")):
        if candidate and os.path.isfile(candidate):
            return candidate
    print("clang-format not found: install the VS 'C++ Clang tools' component or set CLANG_FORMAT", file=sys.stderr)
    sys.exit(2)


def walk(path):
    if os.path.isfile(path):
        return [path]
    files = []
    for root, dirs, names in os.walk(path):
        dirs[:] = [d for d in dirs if d not in ("build", ".vs") and not d.startswith("SDL-")]
        files += [os.path.join(root, n) for n in names if n.endswith(EXTENSIONS)]
    return files


def expand(items):
    if not items:
        return sorted(f for root in ROOTS for f in walk(root))
    files = []
    for item in items:
        candidates = [item] if os.path.isabs(item) else [os.path.join(CORE, item), os.path.join(REPO, item)]
        path = next((c for c in candidates if os.path.exists(c)), None)
        if path is None:
            print(f"no such file: {item}", file=sys.stderr)
            sys.exit(2)
        files += walk(path)
    return sorted(set(files))


def indent_of(line):
    return len(line) - len(line.lstrip(" "))


def brace_cases(text):
    """Wraps every case body of more than one statement (a trailing break not counted) in braces. Works on
    clang-format output, where a label sits on its own line and its body is indented one step deeper."""
    eol = "\r\n" if "\r\n" in text else "\n"
    lines = text.split(eol)
    out = []
    i = 0
    while i < len(lines):
        line = lines[i]
        out.append(line)
        i += 1
        if not CASE_LABEL.match(line.strip()) or line.rstrip().endswith("\\"):
            continue
        label_indent = indent_of(line)

        # The body runs to the next line at or left of the label (blank and preprocessor lines don't end it).
        end = i
        while end < len(lines):
            body_line = lines[end]
            if body_line.strip() and not body_line.lstrip().startswith("#") and indent_of(body_line) <= label_indent:
                break
            end += 1
        last = end - 1
        while last >= i and not lines[last].strip():
            last -= 1
        body = lines[i : last + 1]
        if not body:
            continue

        # Count the statements at the body's own level; a final break doesn't count.
        body_indent = min(indent_of(l) for l in body if l.strip() and not l.lstrip().startswith("#"))
        tops = [l.strip() for l in body if l.strip() and indent_of(l) == body_indent and not COMMENT.match(l.strip())]
        if tops and tops[-1] == "break;":
            tops.pop()
        if len(tops) <= 1:
            continue

        pad = " " * label_indent
        out.append(pad + "{")
        out.extend(body)
        out.append(pad + "}")
        i = last + 1
    return eol.join(out)


def space_blocks(text):
    """Adds the blank lines described at the top of this file. Leaves text inside macros and raw strings alone."""
    eol = "\r\n" if "\r\n" in text else "\n"
    lines = text.split(eol)
    blank_before = set()
    in_macro = False
    for i, line in enumerate(lines):
        stripped = line.strip()
        continued = in_macro
        in_macro = line.rstrip().endswith("\\")
        if continued or in_macro or 'R"' in line or not stripped:
            continue
        indent = indent_of(line)

        # Blank after a closing brace when the next line is more code at the same level.
        if CLOSE_BLOCK.match(stripped) and i + 1 < len(lines):
            following = lines[i + 1]
            if following.strip() and indent_of(following) == indent and not CLOSE_CONTINUES.match(following.strip()):
                blank_before.add(i + 1)

        # Blank before a control statement that follows a statement (above any comments that lead into it).
        if CONTROL.match(stripped):
            j = i - 1
            while j >= 0 and lines[j].strip() and COMMENT.match(lines[j].strip()) and indent_of(lines[j]) == indent:
                j -= 1
            if j >= 0 and lines[j].strip().endswith(";") and not lines[j].rstrip().endswith("\\"):
                blank_before.add(j + 1)

    if not blank_before:
        return text
    out = []
    for i, line in enumerate(lines):
        if i in blank_before:
            out.append("")
        out.append(line)
    return eol.join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="don't change files; list the ones that need formatting")
    ap.add_argument("paths", nargs="*", help="files or folders (default: all of src/lib/MCCore and src/apps)")
    args = ap.parse_args()

    clang_format = find_clang_format()
    files = expand(args.paths)
    style = "--style=file:" + os.path.join(REPO, ".clang-format")

    def run(path):
        with open(path, "rb") as f:
            original_bytes = f.read()
        original = original_bytes.decode("utf-8", "surrogateescape")

        def clang(data):
            result = subprocess.run([clang_format, style, "--assume-filename=" + path], input=data, capture_output=True)
            if result.returncode != 0:
                raise RuntimeError(result.stderr.decode("utf-8", "replace"))
            return result.stdout.decode("utf-8", "surrogateescape")

        try:
            # clang-format, then case braces (re-indented by a second clang-format), then blank lines.
            # Repeat until stable: re-formatting can move a comment onto a case label and expose another case.
            formatted = clang(original_bytes)
            for _ in range(4):
                braced = brace_cases(formatted)
                if braced == formatted:
                    break
                formatted = clang(braced.encode("utf-8", "surrogateescape"))
            formatted = space_blocks(formatted)
        except RuntimeError as err:
            return path, None, str(err)
        if formatted == original:
            return path, False, ""
        if not args.check:
            with open(path, "wb") as f:
                f.write(formatted.encode("utf-8", "surrogateescape"))
        return path, True, ""

    changed, failed = [], []
    with ThreadPoolExecutor(max_workers=os.cpu_count()) as pool:
        for path, was_changed, err in pool.map(run, files):
            if was_changed is None:
                failed.append(path)
                print(f"clang-format failed on {os.path.relpath(path, REPO)}: {err}", file=sys.stderr, end="")
            elif was_changed:
                changed.append(os.path.relpath(path, REPO))

    if args.check:
        for path in changed:
            print(f"needs formatting: {path}")
        print(f"{len(changed)} of {len(files)} files need formatting")
    else:
        print(f"formatted {len(changed)} of {len(files)} files" + (f", {len(failed)} failed" if failed else ""))
    return 1 if changed or failed else 0


if __name__ == "__main__":
    sys.exit(main())
