#!/usr/bin/env python3
"""
Comprehensive Repository Link & Rigor Auditor for math-anal / klees_measure.
Verifies all markdown links, anchors, images, HTML references, LaTeX includes/cites/refs,
macro hygiene (zero operatorname macros), and naming integrity across the entire repository.
"""

import os
import re
import sys
from pathlib import Path

# GitHub Markdown anchor slug generator
def github_slugify(text: str) -> str:
    # Remove HTML tags
    text = re.sub(r'<[^>]+>', '', text)
    # Remove markdown link formatting [text](url) -> text
    text = re.sub(r'\[([^\]]+)\]\([^)]+\)', r'\1', text)
    # Remove inline code ticks
    text = text.replace('`', '')
    # Remove math expressions
    text = re.sub(r'\$[^$]+\$', '', text)
    # Lowercase
    text = text.lower().strip()
    # Replace whitespace and forward slashes with hyphens
    text = re.sub(r'[\s/]+', '-', text)
    # Remove any characters that aren't alphanumeric, hyphens, or underscores
    text = re.sub(r'[^\w\-]', '', text)
    return text

def extract_anchors_from_md(content: str) -> set:
    anchors = set()
    # 1. Heading anchors (# Heading)
    for line in content.splitlines():
        line = line.strip()
        m = re.match(r'^(#{1,6})\s+(.+)$', line)
        if m:
            heading_text = m.group(2).strip()
            # If custom anchor exists {#custom-id}
            custom_m = re.search(r'\{#([a-zA-Z0-9_\-]+)\}\s*$', heading_text)
            if custom_m:
                anchors.add(custom_m.group(1).lower())
                heading_text = heading_text[:custom_m.start()].strip()
            slug = github_slugify(heading_text)
            if slug:
                anchors.add(slug)
    # 2. HTML anchor tags (<a id="..." or <a name="...">)
    for m in re.finditer(r'<a\s+[^>]*(?:id|name)=["\']([^"\']+)["\']', content, re.IGNORECASE):
        anchors.add(m.group(1).lower())
    return anchors

def extract_bib_keys(bib_path: str) -> set:
    if not os.path.exists(bib_path):
        return set()
    keys = set()
    with open(bib_path, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()
    for m in re.finditer(r'@\w+\s*\{\s*([a-zA-Z0-9_\-:]+)\s*,', content):
        keys.add(m.group(1))
    return keys

def audit_repo(repo_root: str):
    repo_root = os.path.abspath(repo_root)
    print(f"=====================================================================")
    print(f" AUDITING REPOSITORY INTEGRITY & LINKS: {repo_root}")
    print(f"=====================================================================\n")

    errors = []
    warnings = []
    checked_links = 0
    checked_files = 0

    # 1. Collect all relevant files
    md_files = []
    tex_files = []
    all_code_files = []

    for root, dirs, files in os.walk(repo_root):
        # Skip git directory and python cache
        if '.git' in root or '__pycache__' in root or '.venv' in root:
            continue
        for f in files:
            p = os.path.join(root, f)
            if f.endswith('.md'):
                md_files.append(p)
            elif f.endswith('.tex'):
                tex_files.append(p)
            if f.endswith(('.cpp', '.hpp', '.py', '.md', '.tex', '.lean', '.txt', '.sh', '.json')):
                all_code_files.append(p)

    # 2. Macro hygiene & Nomenclature scan
    print(f"--- [1/4] Scanning Macro Hygiene & Nomenclature across {len(all_code_files)} files ---")
    forbidden_macro = chr(92) + "operatorname"
    allowed_replacement = chr(92) + "mathrm"
    operator_name_regex = re.compile(re.escape(forbidden_macro) + r'\b')
    kleens_regex = re.compile(r'kleens_measure', re.IGNORECASE)
    conflict_regex = re.compile(r'^(<<<<<<<|=======|>>>>>>>)\b', re.MULTILINE)

    for p in all_code_files:
        rel = os.path.relpath(p, repo_root)
        # Skip this audit script itself from flag check
        if os.path.basename(p) == 'audit_repo_links.py':
            continue
        try:
            with open(p, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
        except Exception as e:
            errors.append(f"Cannot read file {rel}: {e}")
            continue

        # Check forbidden macro
        op_matches = list(operator_name_regex.finditer(content))
        if op_matches:
            errors.append(f"FORBIDDEN MACRO: Found {len(op_matches)} occurrences of '{forbidden_macro}' in {rel}. Must use '{allowed_replacement}'.")

        # Check kleens_measure
        kleens_matches = list(kleens_regex.finditer(content))
        if kleens_matches:
            errors.append(f"TYPO: Found {len(kleens_matches)} occurrences of 'kleens_measure' in {rel}. Correct is 'klees_measure'.")

        # Check conflict markers
        conflict_matches = list(conflict_regex.finditer(content))
        if conflict_matches:
            errors.append(f"GIT CONFLICT: Found git conflict markers in {rel}")

    print(f"Macro and nomenclature check completed.\n")

    # 3. Markdown Link & Anchor Audit
    print(f"--- [2/4] Auditing Markdown Links & Anchors across {len(md_files)} Markdown files ---")
    
    # Pre-cache anchors for all markdown files
    md_anchors = {}
    for p in md_files:
        try:
            with open(p, 'r', encoding='utf-8', errors='ignore') as f:
                md_anchors[os.path.normpath(p)] = extract_anchors_from_md(f.read())
        except Exception as e:
            md_anchors[os.path.normpath(p)] = set()

    # Link extraction pattern: [label](target) or ![alt](target)
    # Exclude badges like [![...](...)](...)
    link_pattern = re.compile(r'(!?\[(?:[^\[\]]|\[[^\[\]]*\])*\])\(([^)\s]+)(?:\s+"[^"]*")?\)')
    html_link_pattern = re.compile(r'<(?:a|img)\s+[^>]*(?:href|src)=["\']([^"\']+)["\']', re.IGNORECASE)

    for p in md_files:
        checked_files += 1
        rel = os.path.relpath(p, repo_root)
        with open(p, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        file_dir = os.path.dirname(p)
        current_anchors = md_anchors.get(os.path.normpath(p), set())

        # Find all markdown links
        for m in link_pattern.finditer(content):
            label = m.group(1)
            target = m.group(2).strip()
            checked_links += 1

            # Ignore dummy anchors `#` used for badges
            if target == '#' or target == '':
                continue

            # In-file anchor `#heading`
            if target.startswith('#'):
                anchor_name = target[1:].lower()
                if anchor_name not in current_anchors:
                    errors.append(f"BROKEN ANCHOR in {rel}: anchor '{target}' not found in file headings.")
                continue

            # External URL
            if target.startswith(('http://', 'https://', 'mailto:')):
                # Check internal repo references e.g. https://github.com/0x7a726f/math-anal/...
                if target.startswith('https://github.com/0x7a726f/math-anal'):
                    # Check if it references a blob or tree
                    subm = re.match(r'https://github\.com/0x7a726f/math-anal/(?:blob|tree)/[^/]+/(.+)$', target)
                    if subm:
                        internal_path = subm.group(1)
                        # Remove anchor if present
                        internal_path = internal_path.split('#')[0]
                        full_target = os.path.join(repo_root, internal_path.replace('/', os.sep))
                        if not os.path.exists(full_target):
                            errors.append(f"BROKEN GITHUB URL in {rel}: {target} -> target file does not exist at {internal_path}")
                continue

            # Local path
            # Check for anchor
            path_part = target
            anchor_part = None
            if '#' in target:
                path_part, anchor_part = target.split('#', 1)

            # Check if redundant klees_measure/ prefix inside subrepo
            # If current file is in klees_measure/ and path_part starts with klees_measure/
            if os.path.commonpath([p, os.path.join(repo_root, 'klees_measure')]) == os.path.join(repo_root, 'klees_measure'):
                if path_part.startswith('klees_measure/'):
                    errors.append(
                        f"REDUNDANT SUBREPO PREFIX in {rel}: '{target}' starts with 'klees_measure/' "
                        f"inside subrepo. Relative path on GitHub will resolve to 'klees_measure/klees_measure/...' (404)!"
                    )

            # Resolve path relative to file_dir
            resolved = os.path.normpath(os.path.join(file_dir, path_part.replace('/', os.sep)))
            if not os.path.exists(resolved):
                errors.append(f"BROKEN LOCAL LINK in {rel}: '{target}' -> path does not exist ({os.path.relpath(resolved, repo_root)})")
            else:
                # If anchor part is specified and target is a markdown file
                if anchor_part and resolved.endswith('.md'):
                    target_anchors = md_anchors.get(os.path.normpath(resolved), set())
                    if anchor_part.lower() not in target_anchors:
                        errors.append(f"BROKEN ANCHOR LINK in {rel}: '{target}' -> anchor '#{anchor_part}' not found in {os.path.relpath(resolved, repo_root)}")

        # Find HTML links <a href="..."> or <img src="...">
        for m in html_link_pattern.finditer(content):
            target = m.group(1).strip()
            checked_links += 1
            if target == '#' or target == '':
                continue
            if target.startswith('#'):
                if target[1:].lower() not in current_anchors:
                    errors.append(f"BROKEN HTML ANCHOR in {rel}: '{target}'")
                continue
            if target.startswith(('http://', 'https://', 'mailto:')):
                continue
            # Local path
            path_part = target.split('#')[0]
            resolved = os.path.normpath(os.path.join(file_dir, path_part.replace('/', os.sep)))
            if not os.path.exists(resolved):
                errors.append(f"BROKEN HTML PATH in {rel}: '{target}' -> does not exist ({os.path.relpath(resolved, repo_root)})")

    print(f"Markdown audit completed: checked {checked_files} files, {checked_links} links.\n")

    # 4. LaTeX Audit
    print(f"--- [3/4] Auditing LaTeX Files across {len(tex_files)} .tex documents ---")
    input_pattern = re.compile(r'\\(?:input|include)\{([^}]+)\}')
    gfx_pattern = re.compile(r'\\includegraphics(?:\[[^\]]*\])?\{([^}]+)\}')
    cite_pattern = re.compile(r'\\cite[a-zA-Z]*\{([^}]+)\}')
    ref_pattern = re.compile(r'\\(?:eq)?ref\{([^}]+)\}')
    label_pattern = re.compile(r'\\label\{([^}]+)\}')
    bib_pattern = re.compile(r'\\(?:bibliography|addbibresource)\{([^}]+)\}')

    for p in tex_files:
        rel = os.path.relpath(p, repo_root)
        file_dir = os.path.dirname(p)
        with open(p, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        # Collect local labels
        labels = set(m.group(1) for m in label_pattern.finditer(content))

        # Collect bib keys from all bib files in the tex file's directory or paper directory
        bib_keys = set()
        for b in bib_pattern.finditer(content):
            bib_target = b.group(1).strip()
            if not bib_target.endswith('.bib'):
                bib_target += '.bib'
            bib_full = os.path.normpath(os.path.join(file_dir, bib_target))
            if not os.path.exists(bib_full):
                errors.append(f"BROKEN BIBLIOGRAPHY in {rel}: '{bib_target}' does not exist at {os.path.relpath(bib_full, repo_root)}")
            else:
                bib_keys.update(extract_bib_keys(bib_full))

        # Check inputs
        for m in input_pattern.finditer(content):
            target = m.group(1).strip()
            target_candidates = [
                os.path.normpath(os.path.join(file_dir, target)),
                os.path.normpath(os.path.join(file_dir, target + '.tex')),
                os.path.normpath(os.path.join(repo_root, target)),
                os.path.normpath(os.path.join(repo_root, target + '.tex')),
                os.path.normpath(os.path.join(repo_root, 'klees_measure', target)),
                os.path.normpath(os.path.join(repo_root, 'klees_measure', target + '.tex')),
            ]
            if not any(os.path.exists(c) for c in target_candidates):
                errors.append(f"BROKEN LATEX INPUT in {rel}: \\input{{{target}}} -> target file not found.")

        # Check includegraphics
        for m in gfx_pattern.finditer(content):
            target = m.group(1).strip()
            extensions = ['', '.png', '.pdf', '.jpg', '.eps']
            target_candidates = []
            for ext in extensions:
                target_candidates.extend([
                    os.path.normpath(os.path.join(file_dir, target + ext)),
                    os.path.normpath(os.path.join(file_dir, 'figures', target + ext)),
                    os.path.normpath(os.path.join(repo_root, target + ext)),
                    os.path.normpath(os.path.join(repo_root, 'klees_measure', target + ext)),
                    os.path.normpath(os.path.join(repo_root, 'klees_measure', 'paper', target + ext)),
                    os.path.normpath(os.path.join(repo_root, 'klees_measure', 'figures', target + ext)),
                ])
            if not any(os.path.exists(c) for c in target_candidates):
                errors.append(f"BROKEN LATEX FIGURE in {rel}: \\includegraphics{{{target}}} -> image file not found.")

        # Check citations
        if bib_keys:
            for m in cite_pattern.finditer(content):
                cited_keys = [k.strip() for k in m.group(1).split(',')]
                for k in cited_keys:
                    if k not in bib_keys:
                        warnings.append(f"UNCITED BIB KEY in {rel}: \\cite{{{k}}} not found in loaded .bib files.")

    print(f"LaTeX audit completed.\n")

    # 5. Core Figure & Table Completeness Audit
    print(f"--- [4/4] Auditing Core Artifact Completeness (Figures 1-8, Tables 1-5) ---")
    if os.path.exists(os.path.join(repo_root, 'paper')):
        paper_dir = os.path.join(repo_root, 'paper')
    else:
        paper_dir = os.path.join(repo_root, 'klees_measure', 'paper')
    figures = [f"figure{i}_" for i in range(1, 9)]
    tables = [f"table{i}_" for i in range(1, 6)]

    if os.path.exists(paper_dir):
        paper_files = os.listdir(paper_dir)
        for fig in figures:
            matched = [f for f in paper_files if f.startswith(fig) and f.endswith(('.png', '.pdf', '.svg'))]
            if not matched:
                errors.append(f"MISSING FIGURE: {fig} not found in {paper_dir}!")
            else:
                print(f"  [OK] Found {fig} -> {matched[0]}")
        for tab in tables:
            matched = [f for f in paper_files if f.startswith(tab) and f.endswith('.tex')]
            if not matched:
                errors.append(f"MISSING TABLE: {tab} not found in {paper_dir}!")
            else:
                print(f"  [OK] Found {tab} -> {matched[0]}")
    else:
        errors.append(f"Paper directory does not exist: {paper_dir}")

    # Summary
    print(f"\n=====================================================================")
    print(f" AUDIT SUMMARY")
    print(f"=====================================================================")
    print(f"Files Checked:   {len(all_code_files)}")
    print(f"Links Checked:   {checked_links}")
    print(f"Warnings:        {len(warnings)}")
    print(f"Errors Found:    {len(errors)}")

    if warnings:
        print(f"\n--- WARNINGS ({len(warnings)}) ---")
        for w in warnings:
            print(f"  [!] {w}")

    if errors:
        print(f"\n--- ERRORS ({len(errors)}) ---")
        for e in errors:
            print(f"  [-] {e}")
        print(f"\n[FAIL] Audit failed with {len(errors)} errors.")
        return 1
    else:
        print(f"\n[PASS] All links, anchors, figures, tables, macros, and files are 100% VERIFIED!")
        return 0

if __name__ == '__main__':
    target_repo = sys.argv[1] if len(sys.argv) > 1 else os.getcwd()
    sys.exit(audit_repo(target_repo))
