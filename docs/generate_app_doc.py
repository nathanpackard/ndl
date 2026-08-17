#!/usr/bin/env python3
"""Turns an apps/ program's own top-comment into a Doxygen markdown
documentation page -- what it does, why it's useful, and how to run it --
plus concrete build/run instructions, WITHOUT ever executing it or
embedding its full source (a reader who wants the implementation can just
open the file at the path this page names; this page's own job is
explaining the app, not duplicating it).

This is deliberately a separate, much smaller script from
generate_tutorial.py, not a mode flag on it: that one turns a demo's
CAPTURED STDOUT (its step()/explain() walkthrough, saved PNGs, an embedded
static .ndlv viewer) into a page, which assumes the program can be run to
completion and its output faithfully captured. An apps/ program is a live
client/server process -- it's SUPPOSED to keep running until stopped, there
is no "completed output" to capture, and no faithful way to embed its
live, interactive result into a static page the way a demo's result is
embedded. So this script never runs the target at all; it only reads
source and prints instructions for a reader to run it themselves.

Usage: generate_app_doc.py <source_file> <page_label> <page_title>
                            <usage_line> <target_name> <generated_docs_dir>
"""
import re
import sys
from pathlib import Path


def extract_top_comment(source_lines):
    """The file's own leading `//` comment block (blank lines allowed
    between the `#include`s and the comment don't occur in this
    codebase's own style -- the comment is always the very first thing --
    so this simply takes every leading line that's blank or starts with
    `//`, stopping at the first line that's neither)."""
    prose_lines = []
    for line in source_lines:
        stripped = line.rstrip('\n')
        if stripped.strip() == '' or stripped.strip().startswith('//'):
            prose_lines.append(re.sub(r'^\s*//\s?', '', stripped))
        else:
            break
    # Collapse to paragraphs the same way generate_tutorial.py's own
    # preamble handling does: a run of non-blank lines reads as one
    # paragraph, not one line each.
    paragraphs = []
    current = []
    for line in prose_lines:
        if line.strip() == '':
            if current:
                paragraphs.append(' '.join(current))
                current = []
        else:
            current.append(line.strip())
    if current:
        paragraphs.append(' '.join(current))
    return '\n\n'.join(paragraphs)


def main():
    if len(sys.argv) != 7:
        print(__doc__, file=sys.stderr)
        sys.exit(1)
    source_file, page_label, page_title, usage_line, target_name, generated_docs_dir = sys.argv[1:7]

    source_path = Path(source_file)
    source_lines = source_path.read_text().splitlines(keepends=True)
    prose = extract_top_comment(source_lines)

    out = []
    out.append(f'{page_title} {{#{page_label}}}')
    out.append('=' * (len(page_title) + len(f' {{#{page_label}}}')))
    out.append('')
    out.append(prose)
    out.append('')
    out.append('Building and running')
    out.append('---------------------')
    out.append('')
    out.append(
        'This is an **app** (`apps/`), not a demo (`demo/`): a live client/server '
        'program that keeps running until stopped, rather than a run-to-completion '
        'pipeline whose output can be captured into this page the way a tutorial\'s '
        'can. Run it yourself:'
    )
    out.append('')
    out.append('```')
    out.append(f'cmake --build . --target {target_name}')
    out.append(usage_line)
    out.append('```')
    out.append('')
    out.append(f'Source: `{source_path.name}`')
    out.append('')

    Path(generated_docs_dir).mkdir(parents=True, exist_ok=True)
    out_path = Path(generated_docs_dir) / f'{page_label}.md'
    out_path.write_text('\n'.join(out) + '\n')
    print(f'wrote {out_path}')


if __name__ == '__main__':
    main()
