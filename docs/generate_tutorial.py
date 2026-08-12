#!/usr/bin/env python3
"""Turns a demo program's captured stdout into a Doxygen markdown tutorial
page: the exact same step()/explain() walkthrough a reader would see running
the demo themselves, but with the saved PNGs embedded inline and formatted
as a real documentation page instead of terminal text.

Usage: generate_tutorial.py <captured_output.txt> <page_label> <page_title>
                             <demo_output_image_dir> <generated_docs_dir>

Deliberately a small, format-specific parser (not a general terminal-output
parser) -- it only needs to understand the one structured convention every
demo's step()/showArray()/showText()/saveForInspection() helpers already
produce, not arbitrary text.
"""
import re
import shutil
import sys
from pathlib import Path

CODE_RE = re.compile(r'^(?:\[(\d+)\]\s*)?code:\s*(.*)$')
EXPLAIN_RE = re.compile(r'^\s{4}explain:\s*(.*)$')
SECTION_RE = re.compile(r'^===\s*(.*?)\s*===$')
SAVING_RE = re.compile(r'^saving output file:\s*(\S+)$')
LOADING_RE = re.compile(r'^loading file:\s*(\S+)$')


def indent_of(line):
    return len(line) - len(line.lstrip(' '))


def dedent(data_lines):
    non_blank = [l for l in data_lines if l.strip() != '']
    if not non_blank:
        return data_lines
    common = min(indent_of(l) for l in non_blank)
    return [l[common:] if len(l) >= common else l for l in data_lines]


def render_markdown(lines, page_label, image_dir_rel):
    out = []
    data_buf = []
    seen_structure = [False]  # mutable cell, closed over by flush_data

    def flush_data(final_flush=False):
        if not data_buf:
            return
        while data_buf and data_buf[-1].strip() == '':
            data_buf.pop()
        while data_buf and data_buf[0].strip() == '':
            data_buf.pop(0)
        if not data_buf:
            return
        # Preamble text (before the first "=== ... ===" or "code:" line) and
        # the closing "how to read the results" summary (the very last
        # block in the file, by convention in how these demos are written)
        # are both hand-wrapped narrative prose, not structured data --
        # they read far better as an actual paragraph than a monospace box.
        if not seen_structure[0] or final_flush:
            out.append(' '.join(l.strip() for l in data_buf))
            out.append('')
        else:
            out.append('```text')
            out.extend(dedent(data_buf))
            out.append('```')
            out.append('')
        data_buf.clear()

    i = 0
    n = len(lines)
    in_explain = False
    while i < n:
        line = lines[i].rstrip('\n')

        m = SECTION_RE.match(line)
        if m:
            flush_data()
            seen_structure[0] = True
            in_explain = False
            out.append(f'## {m.group(1)}')
            out.append('')
            i += 1
            continue

        m = CODE_RE.match(line)
        if m:
            flush_data()
            seen_structure[0] = True
            in_explain = False
            step_num, code = m.group(1), m.group(2)
            if step_num:
                out.append(f'### Step {step_num}')
            out.append('```cpp')
            out.append(code)
            i += 1
            # A multi-line code block (the demo's second step() overload,
            # for a call worth showing as more than one line): continuation
            # lines are printed indented 13 spaces, matching "explain:"'s
            # own continuation width, so anything indented past 4 spaces
            # here is still code, not a new section/explain block ending it.
            while i < n:
                cont = lines[i].rstrip('\n')
                if cont.strip() == '' or indent_of(cont) <= 4:
                    break
                out.append(cont[13:] if cont[:13] == ' ' * 13 else cont.strip())
                i += 1
            out.append('```')
            out.append('')
            continue

        m = EXPLAIN_RE.match(line)
        if m:
            flush_data()
            in_explain = True
            out.append(m.group(1).strip())
            i += 1
            continue

        if in_explain:
            # Continuation lines are indented well past the "explain: "
            # label (>4 spaces); anything indented exactly 4 spaces (a new
            # showText()/showArray() label) or blank ends the explain
            # paragraph.
            if line.strip() != '' and indent_of(line) > 4:
                out[-1] = out[-1] + ' ' + line.strip()
                i += 1
                continue
            in_explain = False
            out.append('')
            # fall through to reprocess this line as ordinary content

        m = SAVING_RE.match(line)
        if m:
            flush_data()
            src = Path(m.group(1))
            # Prefixed with page_label, not just the original basename:
            # Doxygen's IMAGE_PATH flattens every embedded image into one
            # shared output directory by basename alone, so two demos both
            # saving e.g. "01_original.png" would silently overwrite one
            # another there even though they're copied into separate
            # per-page source subdirectories below.
            dst_name = f'{page_label}_{src.name}'
            out.append(f'![{dst_name}]({image_dir_rel}/{dst_name})')
            out.append('')
            i += 1
            continue

        if LOADING_RE.match(line):
            # Plumbing, not pedagogically useful -- drop.
            i += 1
            continue

        if line.strip() == '':
            flush_data()
            i += 1
            continue

        data_buf.append(line)
        i += 1

    flush_data(final_flush=True)
    return '\n'.join(out)


def main():
    if len(sys.argv) != 6:
        sys.exit(f"usage: {sys.argv[0]} <captured_output.txt> <page_label> <page_title> <demo_output_image_dir> <generated_docs_dir>")
    captured_path, page_label, page_title, image_src_dir, generated_dir = sys.argv[1:]

    generated_dir = Path(generated_dir)
    image_dst_dir = generated_dir / 'images' / page_label
    image_dst_dir.mkdir(parents=True, exist_ok=True)

    image_src_dir = Path(image_src_dir)
    if image_src_dir.is_dir():
        for png in sorted(image_src_dir.glob('*.png')):
            # Prefixed name, matching the SAVING_RE handling in
            # render_markdown() above -- see the comment there for why.
            shutil.copy2(png, image_dst_dir / f'{page_label}_{png.name}')

    with open(captured_path) as f:
        lines = f.readlines()

    body = render_markdown(lines, page_label, f'images/{page_label}')

    generated_dir.mkdir(parents=True, exist_ok=True)
    out_path = generated_dir / f'{page_label}.md'
    with open(out_path, 'w') as f:
        f.write(f'# {page_title} {{#{page_label}}}\n\n')
        f.write(body)
        f.write('\n')
    print(f'wrote {out_path}')


if __name__ == '__main__':
    main()
