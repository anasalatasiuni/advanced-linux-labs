#!/usr/bin/env python3
"""
Backward LDD (bldd)

Scan a directory for ELF executables and report which executables
use specified shared libraries. Supports x86, x86_64, armv7, aarch64.
Generates text or PDF reports.
"""

import argparse
import textwrap
import os
import sys
from collections import defaultdict
import datetime

# Third-party imports
try:
    from elftools.elf.elffile import ELFFile
except ImportError:
    print(
        "Error: pyelftools is required. "
        "Install with 'pip install pyelftools'."
    )
    sys.exit(1)

PDF_SUPPORTED = True
try:
    from reportlab.lib.styles import getSampleStyleSheet
    from reportlab.platypus import Paragraph, SimpleDocTemplate, Spacer
except ImportError:
    PDF_SUPPORTED = False

# ELF architecture mapping
ARCH_MAP = {
    'x86':    'EM_386',
    'x86_64': 'EM_X86_64',
    'armv7':  'EM_ARM',
    'aarch64': 'EM_AARCH64',
}


def get_file_arch(path):
    """Return the ELF e_machine value for an ELF file."""
    with open(path, 'rb') as f:
        elf = ELFFile(f)
        return elf.header['e_machine']


def get_needed_libs(path):
    """Return a list of DT_NEEDED entries from the ELF .dynamic section."""
    libs = []
    with open(path, 'rb') as f:
        elf = ELFFile(f)
        section = elf.get_section_by_name('.dynamic')

        if section is None:
            return libs

        for tag in section.iter_tags():
            if tag.entry.d_tag == 'DT_NEEDED':
                libs.append(tag.needed)

    return libs


def is_elf_executable(path):
    """Return True if path is an ELF executable or PIE."""
    try:
        with open(path, 'rb') as f:
            # Check for ELF magic number
            magic = f.read(4)
            if magic != b'\x7fELF':
                print(f"Not an ELF file: {path}")
                return False

            f.seek(0)
            elf = ELFFile(f)
            etype = elf.header['e_type']
            # ET_EXEC=2 or ET_DYN=3 for PIE
            is_exec = etype in ('ET_EXEC', 'ET_DYN')
            if not is_exec:
                print(f"Not an executable: {path} (type: {etype})")
            return is_exec

    except Exception as e:
        print(f"Error checking ELF file {path}: {e}")
        return False


def scan_directory(root, libraries, arch_filter):
    """Scan root for ELF executables that depend on libraries."""
    usage = defaultdict(list)
    all_libs = defaultdict(list)
    print(f"Scanning directory: {root}")
    print(f"Looking for libraries: {libraries}")
    print(f"Architecture filter: {arch_filter}")

    for dirpath, _, files in os.walk(root):
        for name in files:
            path = os.path.join(dirpath, name)
            print(f"\nChecking file: {path}")

            if not is_elf_executable(path):
                print(f"Not an ELF executable: {path}")
                continue

            try:
                arch = get_file_arch(path)
                print(f"File architecture: {arch}")
            except Exception as e:
                print(f"Error getting architecture: {e}")
                continue

            if arch_filter != 'all' and arch != ARCH_MAP[arch_filter]:
                print(
                    f"Architecture mismatch: {arch} != {ARCH_MAP[arch_filter]}"
                    )
                continue

            needed = get_needed_libs(path)
            print(f"Found dependencies: {needed}")

            # If no specific libraries were provided, collect all libraries
            if not libraries:
                for lib in needed:
                    all_libs[lib].append(path)
            else:
                for lib in libraries:
                    if lib in needed:
                        print(f"Found match: {lib} in {path}")
                        usage[lib].append(path)

    return all_libs if not libraries else usage


def write_txt_report(output, data):
    """Write a text report sorted by usage count."""
    with open(output, 'w', encoding='utf-8') as f:
        f.write('bldd Report\n')
        f.write('===========\n\n')
        timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        f.write(f'Generated on: {timestamp}\n\n')

        # Summary section
        total_libs = len(data)
        total_executables = sum(len(paths) for _, paths in data)
        f.write('Summary:\n')
        f.write(f'Total libraries found: {total_libs}\n')
        f.write(f'Total executables analyzed: {total_executables}\n\n')

        f.write('Detailed Report:\n')
        f.write('--------------\n\n')

        for lib, paths in data:
            f.write(f'Library: {lib}\n')
            f.write(f'Total usages: {len(paths)}\n')
            f.write('Executables:\n')
            for p in paths:
                try:
                    arch = get_file_arch(p)
                    arch_name = next(
                        (k for k, v in ARCH_MAP.items() if v == arch),
                        arch
                    )
                    f.write(f'  - {p} ({arch_name})\n')
                except Exception:
                    f.write(f'  - {p} (architecture unknown)\n')
            f.write('\n')

    print(f'Text report saved to {output}')


def write_pdf_report(output, data):
    """Write a PDF report using ReportLab."""
    if not PDF_SUPPORTED:
        print(
            "Error: reportlab is required for PDF output. "
            "Install with 'pip install reportlab'."
        )
        sys.exit(1)

    doc = SimpleDocTemplate(output)
    styles = getSampleStyleSheet()
    elems = []

    # Title and timestamp
    elems.append(Paragraph('bldd Report', styles['Title']))
    timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    elems.append(Paragraph(
        f'Generated on: {timestamp}',
        styles['Normal']
    ))
    elems.append(Spacer(1, 12))

    # Summary section
    total_libs = len(data)
    total_executables = sum(len(paths) for _, paths in data)
    elems.append(Paragraph('Summary', styles['Heading1']))
    elems.append(Paragraph(
        f'Total libraries found: {total_libs}',
        styles['Normal']
    ))
    elems.append(Paragraph(
        f'Total executables analyzed: {total_executables}',
        styles['Normal']
    ))
    elems.append(Spacer(1, 12))

    # Detailed report
    elems.append(Paragraph('Detailed Report', styles['Heading1']))
    elems.append(Spacer(1, 12))

    for lib, paths in data:
        elems.append(Paragraph(
            f'{lib} - {len(paths)} usages',
            styles['Heading2']
        ))
        for p in paths:
            try:
                arch = get_file_arch(p)
                arch_name = next(
                    (k for k, v in ARCH_MAP.items() if v == arch),
                    arch
                )
                elems.append(Paragraph(
                    f'{p} ({arch_name})',
                    styles['Normal']
                ))
            except Exception:
                elems.append(Paragraph(
                    f'{p} (architecture unknown)',
                    styles['Normal']
                ))
        elems.append(Spacer(1, 12))

    doc.build(elems)
    print(f'PDF report saved to {output}')


def main():
    parser = argparse.ArgumentParser(
        description='bldd: backward ldd – find executables depending on shared libraries',
        formatter_class=argparse.RawTextHelpFormatter,
        epilog=textwrap.dedent('''\
          Examples:
            # Scan /usr/bin for libc.so.6 and libssl.so.1.1, TXT report
            bldd.py -d /usr/bin -l libc.so.6 libssl.so.1.1

            # Scan current dir for all libs, PDF report
            bldd.py -d . -f pdf -o my_report.pdf

            # Filter to x86_64 executables only
            bldd.py -d ./build -l libcrypto.so.1.1 -a x86_64
        ''')
    )

    parser.add_argument(
        '-d', '--dir',
        required=True,
        metavar='DIR',
        help='Root directory to scan for ELF executables'
    )
    parser.add_argument(
        '-l', '--libs',
        nargs='*',
        metavar='LIB',
        help=(
            'Library names to search for, e.g., libssl.so.1.1. '
            'If not specified, all libraries will be reported.'
        )
    )
    parser.add_argument(
        '-a', '--arch',
        choices=list(ARCH_MAP) + ['all'],
        default='all',
        metavar='ARCH',
        help='Architecture filter (default: all)'
    )
    parser.add_argument(
        '-o', '--output',
        default='bldd_report.txt',
        metavar='FILE',
        help='Path to save the report (default: bldd_report.txt)'
    )
    parser.add_argument(
        '-f', '--format',
        choices=['txt', 'pdf'],
        default='txt',
        metavar='FMT',
        help='Report format (txt or pdf, default: txt)'
    )

    args = parser.parse_args()

    usage = scan_directory(args.dir, args.libs, args.arch)
    sorted_usage = sorted(
        usage.items(),
        key=lambda item: len(item[1]),
        reverse=True
    )

    if args.format == 'pdf':
        write_pdf_report(args.output, sorted_usage)
    else:
        write_txt_report(args.output, sorted_usage)


if __name__ == '__main__':
    main()
