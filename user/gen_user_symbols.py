#!/usr/bin/env python3
"""Generate a GDB script that loads debug symbols for user-space ELF
programs embedded in the kernel image.

Usage:
    python3 gen_user_symbols.py [-o output.gdb] <kernel_elf> <user_elf_dir>
        [<program name> ...]

The generated script defines three GDB commands (written in GDB's Python):

    load_user_symbols <program> <pid_or_addr>
        Load the symbols of <program> (one of the programs listed on the
        command line; default: every .elf in <user_elf_dir>) for the process
        with that PID (a value below NPROC), whose memory starts at
        PROC_START + pid * PROC_SIZE, or at the given address.  The second
        argument can be any GDB expression.  Any user program symbols
        already loaded there are removed first, so it can simply be re-run
        to fix a mistake.
    unload_user_symbols <pid_or_addr>
        Remove the user program symbols loaded there.
    info_user_symbols
        List the user program symbols loaded, and where.

It then reads which user programs init() starts, and in what order, from
the kernel ELF: the global `boot_programs` array of program names if there
is one, and otherwise the global `executables` array of pointers to the
embedded ELF images (as in Project 3).  The array index gives the PID, and
the script loads each of them with load_user_symbols, which relocates
every ALLOC section.

The output can be sourced from .gdbinit to get proper debug symbols
(breakpoints, variable inspection, etc.) for user-space code.
"""

import argparse
import os
import re
import struct
import subprocess
import sys

# Defaults, overridden by parsing kernel/limits.h and kernel/memlayout.h.
PROC_START = 0xA0000
PROC_SIZE = 0x10000
NPROC = 16

# The macros we read from the kernel's headers
LAYOUT_MACROS = ("PROC_START", "PROC_SIZE", "NPROC")

# PROC_START and PROC_SIZE may be defined in either of these headers
LAYOUT_HEADERS = ("limits.h", "memlayout.h")

# A single C integer constant.  The body and the suffix are captured
# separately on purpose:  stripping a trailing suffix by character class would
# eat the `B` of `0xAB`.
_C_INT_RE = re.compile(
    r"""
    (?P<body>
        0[xX][0-9a-fA-F][0-9a-fA-F']*     # hex
      | 0[bB][01][01']*                   # binary (C23)
      | [0-9][0-9']*                      # decimal, or octal with a leading 0
    )
    (?P<suffix>
        [uU](?:ll|LL|l|L|wb|WB)?
      | (?:ll|LL|l|L|wb|WB)[uU]?
    )?
    """,
    re.VERBOSE,
)


def strip_c_comments(text):
    """Remove a trailing // comment and any /* */ comments from one line."""
    text = re.sub(r"/\*.*?\*/", " ", text)
    text = re.sub(r"/\*.*$", " ", text)  # an unterminated block comment
    text = re.sub(r"//.*$", "", text)
    return text.strip()


def parse_c_integer(text):
    """The value of a C integer constant, or None if it is not one.

    Handles the spellings a student might reasonably write for an address:
    hex, binary, octal or decimal, C23 digit separators (`0x400'000`), an
    integer suffix (`0x400000UL`), and redundant parentheses.

    Returns None for anything else -- an expression, a reference to another
    macro, a character constant -- so that callers treat the value as unknown
    rather than parsing half of it.  Reading `0x400'000` as `0x400` is worse
    than not reading it at all.
    """
    token = strip_c_comments(text)

    # Redundant parentheses, as in `#define PROC_START (0x400000)`.
    while token.startswith("(") and token.endswith(")"):
        token = token[1:-1].strip()

    match = _C_INT_RE.fullmatch(token)
    if not match:
        return None

    body = match.group("body").replace("'", "")
    try:
        # A leading zero means octal in C, which Python spells 0o.
        if re.fullmatch(r"0[0-7]+", body):
            return int(body, 8)
        return int(body, 0)
    except ValueError:
        return None


def find_proc_layout(kernel_dir):
    """Look for PROC_START, PROC_SIZE and NPROC in the kernel's headers.

    Returns (values, conflicts):
      - values:    {macro: int} for each macro found as a plain literal
      - conflicts: {macro: [(path, value), ...]} for any macro defined more
                   than once with *different* values, in which case the macro
                   is left out of `values` -- we cannot tell which definition
                   the build used.

    A macro defined as an expression rather than a literal (`kernel_end +
    0x1000`, say) does not match and is simply absent from `values`.  Callers
    must cope with not finding one.
    """
    seen = {}  # macro -> [(path, value)]
    for header in LAYOUT_HEADERS:
        path = os.path.join(kernel_dir, header)
        try:
            with open(path) as f:
                for line in f:
                    for macro in LAYOUT_MACROS:
                        # Take the rest of the line and hand it to the literal
                        # parser, rather than matching a number shape here:
                        # that way an unparseable value is reported as unknown
                        # instead of being silently truncated.
                        m = re.match(
                            r"\s*#\s*define\s+" + macro + r"\s+(.*)$", line
                        )
                        if not m:
                            continue
                        value = parse_c_integer(m.group(1))
                        if value is not None:
                            seen.setdefault(macro, []).append((path, value))
        except OSError:
            continue

    values = {}
    conflicts = {}
    for macro, found in seen.items():
        distinct = {value for _path, value in found}
        if len(distinct) == 1:
            values[macro] = found[0][1]
        else:
            conflicts[macro] = found
    return values, conflicts


def parse_limits(kernel_dir):
    """Set the PROC_START/PROC_SIZE/NPROC globals from the kernel's headers."""
    global PROC_START, PROC_SIZE, NPROC
    values, _conflicts = find_proc_layout(kernel_dir)
    PROC_START = values.get("PROC_START", PROC_START)
    PROC_SIZE = values.get("PROC_SIZE", PROC_SIZE)
    NPROC = values.get("NPROC", NPROC)


# The arrays that can tell us what init() starts, in order of preference:
# program names, or pointers to the embedded ELF images.
BOOT_ARRAYS = ("boot_programs", "executables")


def get_nm_symbols(kernel_elf):
    """Run nm -S on the kernel ELF.  Return:
      - blob_syms: {address: program_name} for _binary_user_*_elf_start symbols
      - arrays: {name: (address, size in bytes)} for those of BOOT_ARRAYS
        that the kernel defines
    """
    result = subprocess.run(
        [f"{TOOLPREFIX}nm", "-S", kernel_elf], capture_output=True, text=True, check=True
    )
    blob_syms = {}
    arrays = {}

    for line in result.stdout.splitlines():
        parts = line.split()
        if len(parts) < 3:
            continue

        # nm -S output: "addr size type name" (4 cols) or "addr type name" (3 cols)
        if len(parts) >= 4 and len(parts[1]) > 1 and all(c in "0123456789abcdef" for c in parts[1]):
            addr, size, typ, name = int(parts[0], 16), int(parts[1], 16), parts[2], parts[3]
        else:
            addr, typ, name = int(parts[0], 16), parts[1], parts[2]
            size = None

        if re.fullmatch(r"_binary_user_[A-Za-z0-9_]+_elf_start", name):
            # Extract program name: _binary_user_<name>_elf_start
            prog = name.removeprefix("_binary_user_").removesuffix("_elf_start")
            blob_syms[addr] = prog

        if name in BOOT_ARRAYS and size is not None:
            arrays[name] = (addr, size)

    return blob_syms, arrays


def get_load_segments(kernel_elf):
    """Return a list of (vaddr, file_offset, filesz) for LOAD segments."""
    result = subprocess.run(
        [f"{TOOLPREFIX}readelf", "-lW", kernel_elf], capture_output=True, text=True, check=True
    )
    segments = []
    for line in result.stdout.splitlines():
        # Match LOAD lines:  LOAD  0x010000 0x0000000000080000 ... 0x01af00 ...
        m = re.match(
            r"\s*LOAD\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)",
            line,
        )
        if m:
            file_offset = int(m.group(1), 16)
            vaddr = int(m.group(2), 16)
            # paddr = m.group(3)
            filesz = int(m.group(4), 16)
            segments.append((vaddr, file_offset, filesz))
    return segments


def vaddr_to_file_offset(vaddr, segments):
    """Convert a virtual address to a file offset using LOAD segment mappings."""
    for seg_vaddr, seg_offset, seg_filesz in segments:
        if seg_vaddr <= vaddr < seg_vaddr + seg_filesz:
            return seg_offset + (vaddr - seg_vaddr)
    return None


def read_pointer_array(kernel_elf, what, addr, size, segments):
    """Read the array of pointers called `what`, at `addr`, from the kernel
    ELF file.  Returns a list of pointer values."""
    file_offset = vaddr_to_file_offset(addr, segments)
    if file_offset is None:
        print(
            f"error: could not map {what} address 0x{addr:x} to file offset",
            file=sys.stderr,
        )
        sys.exit(1)

    num_entries = size // 8
    with open(kernel_elf, "rb") as f:
        f.seek(file_offset)
        data = f.read(num_entries * 8)

    return list(struct.unpack(f"<{num_entries}Q", data))


def read_c_string(kernel_elf, addr, segments, max_len=64):
    """Read the NUL-terminated string at `addr` from the kernel ELF file, or
    return None if it is not in the file (or not terminated in time)."""
    file_offset = vaddr_to_file_offset(addr, segments)
    if file_offset is None:
        return None
    with open(kernel_elf, "rb") as f:
        f.seek(file_offset)
        data = f.read(max_len)
    end = data.find(b"\0")
    if end < 0:
        return None
    return data[:end].decode("ascii", errors="replace")


def elf_name_to_symbol(name):
    """Convert an ELF filename (e.g. 'loader-test') to the objcopy-mangled
    program name.  objcopy replaces any non-alphanumeric character with '_'."""
    return re.sub(r"[^A-Za-z0-9]", "_", name)


def get_alloc_sections(elf_path):
    """Return a list of (name, vaddr) for every ALLOC section with
    nonzero size in the given ELF file."""
    result = subprocess.run(
        [f"{TOOLPREFIX}readelf", "-SW", elf_path], capture_output=True, text=True, check=True
    )
    sections = []
    for line in result.stdout.splitlines():
        # Section header lines look like:
        #   [ 1] .text  PROGBITS  0000000000000000  000158  0010f4  ...  AX  ...
        m = re.match(
            r"\s*\[\s*\d+\]\s+(\S+)\s+\S+\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s+([0-9a-fA-F]+)",
            line,
        )
        if not m:
            continue
        name = m.group(1)
        vaddr = int(m.group(2), 16)
        size = int(m.group(3), 16)

        if size == 0:
            continue
        # Check for the A(lloc) flag in the flags column.
        flag_part = line[m.end():]
        flag_match = re.search(r"\b([WAXMSILTCGE]+)\b", flag_part)
        if not flag_match or "A" not in flag_match.group(1):
            continue

        sections.append((name, vaddr))
    return sections


def find_user_elf(user_elf_dir, mangled_name):
    """Find the .elf file in user_elf_dir whose mangled name matches.
    Returns the path, or None."""
    for f in os.listdir(user_elf_dir):
        if not f.endswith(".elf"):
            continue
        prog_name = f.removesuffix(".elf")
        if elf_name_to_symbol(prog_name) == mangled_name:
            return os.path.join(user_elf_dir, f)
    return None


def program_sections(elf_path):
    """[(section, offset), ...] for the ALLOC sections of a user ELF, .text
    first (add-symbol-file takes the .text address as its positional
    argument), or None, with a warning, if it cannot be loaded."""
    sections = get_alloc_sections(elf_path)
    if not sections:
        print(f"warning: no ALLOC sections found in {elf_path}", file=sys.stderr)
        return None
    text = [s for s in sections if s[0] == ".text"]
    if not text:
        print(f"warning: no .text section found in {elf_path}", file=sys.stderr)
        return None
    return text + [s for s in sections if s[0] != ".text"]


def collect_programs(user_elf_dir, names):
    """{name: (elf_path, sections)} for the programs load_user_symbols can
    load: the given names (from the Makefile), or, with none, every .elf in
    user_elf_dir."""
    if not names:
        names = sorted(
            f.removesuffix(".elf") for f in os.listdir(user_elf_dir) if f.endswith(".elf")
        )
    programs = {}
    for name in names:
        elf_path = os.path.join(user_elf_dir, name + ".elf")
        if not os.path.isfile(elf_path):
            print(
                f"warning: no .elf file found for program '{name}' in {user_elf_dir}",
                file=sys.stderr,
            )
            continue
        sections = program_sections(elf_path)
        if sections is not None:
            programs[name] = (elf_path, sections)
    return programs


def boot_entries(kernel_elf):
    """(array, [(pid, mangled_name), ...]) for the programs that init()
    starts, in order, from whichever of BOOT_ARRAYS the kernel defines; or
    (None, []) if it defines neither."""
    blob_syms, arrays = get_nm_symbols(kernel_elf)
    array = next((a for a in BOOT_ARRAYS if a in arrays), None)
    if array is None:
        return None, []
    if not blob_syms:
        print(
            f"warning: no _binary_user_*_elf_start symbols found in {kernel_elf}",
            file=sys.stderr,
        )
        return array, []

    segments = get_load_segments(kernel_elf)
    addr, size = arrays[array]
    pointers = read_pointer_array(kernel_elf, array, addr, size, segments)

    entries = []
    for pid, ptr in enumerate(pointers):
        if ptr == 0:  # skip nullptr terminator
            continue

        if array == "boot_programs":
            # A program name, as init() passes it to find_program().
            name = read_c_string(kernel_elf, ptr, segments)
            if name is None or elf_name_to_symbol(name) not in blob_syms.values():
                print(
                    f"warning: boot_programs[{pid}] = 0x{ptr:x} does not name "
                    f"an embedded program",
                    file=sys.stderr,
                )
                continue
            entries.append((pid, elf_name_to_symbol(name)))
        else:
            # A pointer to the embedded ELF image itself.
            if ptr not in blob_syms:
                print(
                    f"warning: executables[{pid}] = 0x{ptr:x} does not match "
                    f"any _binary_user_*_elf_start symbol",
                    file=sys.stderr,
                )
                continue
            entries.append((pid, blob_syms[ptr]))
    return array, entries


def boot_programs(kernel_elf, user_elf_dir, programs):
    """(array, [(pid, name), ...]) for the programs that init() starts, in
    order (see boot_entries()), adding any that are missing to
    `programs`."""
    array, entries = boot_entries(kernel_elf)

    if array is None:
        print(
            "warning: neither 'boot_programs' nor 'executables' found in the "
            "kernel ELF, so no user program symbols are loaded at startup.\n"
            "Use load_user_symbols in GDB to load them.",
            file=sys.stderr,
        )
        return None, []

    by_mangled = {elf_name_to_symbol(name): name for name in programs}
    boot = []
    for pid, mangled_name in entries:
        name = by_mangled.get(mangled_name)
        if name is None:
            # Not in the program list: find its ELF the old way
            elf_path = find_user_elf(user_elf_dir, mangled_name)
            if elf_path is None:
                print(
                    f"warning: no .elf file found for program '{mangled_name}' "
                    f"in {user_elf_dir}",
                    file=sys.stderr,
                )
                continue
            sections = program_sections(elf_path)
            if sections is None:
                continue
            name = os.path.basename(elf_path).removesuffix(".elf")
            programs[name] = (elf_path, sections)
            by_mangled[mangled_name] = name

        boot.append((pid, name))
    return array, boot


def generate_gdb_commands(kernel_elf, user_elf_dir, names=()):
    """Generate the GDB script: the user programs' sections, the
    load_user_symbols family of commands, and a load_user_symbols command
    for each program that init() starts."""
    # Try to read PROC_START/PROC_SIZE/NPROC from limits.h or memlayout.h
    # (whichever defines them) relative to kernel ELF
    kernel_dir = os.path.dirname(kernel_elf)
    parse_limits(kernel_dir)

    programs = collect_programs(user_elf_dir, names)
    array, boot = boot_programs(kernel_elf, user_elf_dir, programs)

    lines = ["# Generated by gen_user_symbols.py — do not edit"]
    lines.append(f"# Kernel: {kernel_elf}")
    lines.append(f"# PROC_START=0x{PROC_START:x}  PROC_SIZE=0x{PROC_SIZE:x}  NPROC={NPROC}")
    lines.append("#")
    lines.append("# Commands (`help <command>` in GDB for details):")
    lines.append("#   load_user_symbols <program> <pid_or_addr>")
    lines.append("#   unload_user_symbols <pid_or_addr>")
    lines.append("#   info_user_symbols")
    lines.append("")
    lines.append("python")
    lines.append(f"PROC_START = 0x{PROC_START:x}")
    lines.append(f"PROC_SIZE = 0x{PROC_SIZE:x}")
    lines.append(f"NPROC = {NPROC}")
    lines.append("# program -> (ELF, [(section, offset in the ELF), ...]), .text first")
    lines.append("USER_PROGRAMS = {")
    for name in sorted(programs):
        elf_path, sections = programs[name]
        secs = ", ".join(f'("{sec}", 0x{off:x})' for sec, off in sections)
        lines.append(f'    "{name}": ("{elf_path}", [{secs}]),')
    lines.append("}")
    lines.append(GDB_COMMANDS_PY.strip("\n"))
    lines.append("end")
    lines.append("")

    for pid, name in boot:
        lines.append(f"# {array}[{pid}]: {name}")
        lines.append(f"load_user_symbols {name} {pid}")
    if not boot:
        lines.append("# (no programs that init() starts to load at startup)")
    lines.append("")

    return lines


# The GDB side of the commands, run by GDB's Python inside the generated
# `python ... end` block, after the constants and USER_PROGRAMS above it.
# (GDB ends the block at a line that is just `end`, so none of these lines
# may be.)
GDB_COMMANDS_PY = r"""
import os

# base address -> program name, for every load made by these commands. Kept
# if procs.gdb is sourced again, since GDB keeps the symbols too.
_user_loads = globals().get("_user_loads", {})

# GDB 13+ can tell us which symbol file covers an address
_HAVE_OBJFILE_FOR_ADDRESS = hasattr(gdb.Progspace, "objfile_for_address")


def _same_file(a, b):
    return os.path.realpath(a) == os.path.realpath(b)


def _objfile_at(addr):
    return gdb.current_progspace().objfile_for_address(addr)


def _is_kernel(objfile):
    main = gdb.current_progspace().filename
    return main is not None and _same_file(objfile.filename, main)


def _loaded_here(base):
    # Is the program we recorded at base still loaded?
    elf = USER_PROGRAMS[_user_loads[base]][0]
    if _HAVE_OBJFILE_FOR_ADDRESS:
        objfile = _objfile_at(base)
        return objfile is not None and _same_file(objfile.filename, elf)
    return any(_same_file(o.filename, elf) for o in gdb.objfiles())


def _to_number(expr):
    try:
        value = gdb.parse_and_eval(expr)
    except gdb.error as e:
        raise gdb.GdbError(f"cannot evaluate '{expr}': {e}")
    try:
        return int(value)
    except (gdb.error, TypeError, ValueError):
        return int(value.cast(gdb.lookup_type("unsigned long")))


def _resolve(expr):
    # (base address, description) for a PID or an address, with warnings.
    # Addresses are used as given, never rounded: if we parsed PROC_START or
    # PROC_SIZE wrongly, the user knows better than we do.
    n = _to_number(expr)
    if n < 0:
        raise gdb.GdbError(f"'{expr}' is negative ({n}): not a PID or an address")
    if n < NPROC:
        return PROC_START + n * PROC_SIZE, f"pid {n}"
    end = PROC_START + NPROC * PROC_SIZE
    if not PROC_START <= n < end:
        print(f"warning: 0x{n:x} is outside the process slots "
              f"(0x{PROC_START:x}-0x{end:x}); using it anyway")
    elif (n - PROC_START) % PROC_SIZE != 0:
        slot = (n - PROC_START) // PROC_SIZE
        print(f"warning: 0x{n:x} is 0x{(n - PROC_START) % PROC_SIZE:x} into "
              f"slot {slot}, which starts at 0x{PROC_START + slot * PROC_SIZE:x} "
              f"(did you mean pid {slot}?); using 0x{n:x} anyway")
    return n, f"0x{n:x}"


def _slot_of(addr):
    # The process slot an address is in, or the address itself outside them
    if PROC_START <= addr < PROC_START + NPROC * PROC_SIZE:
        return ("slot", (addr - PROC_START) // PROC_SIZE)
    return ("address", addr)


def _forget(filename, addr):
    # Drop our record of the load of filename that covers addr (the nearest
    # one at or below it), and return its program name
    bases = [b for b, n in _user_loads.items()
             if b <= addr and _same_file(USER_PROGRAMS[n][0], filename)]
    if not bases:
        return os.path.basename(filename).removesuffix(".elf")
    return _user_loads.pop(max(bases))


def _remove_exact(addr):
    # Remove the user program symbols loaded at addr (as many copies as
    # there are), never the kernel's. Returns the names removed.
    removed = []
    if _HAVE_OBJFILE_FOR_ADDRESS:
        for _ in range(16):  # each remove-symbol-file removes one copy
            objfile = _objfile_at(addr)
            if objfile is None or _is_kernel(objfile):
                break
            filename = objfile.filename  # (invalid once removed)
            gdb.execute(f"remove-symbol-file -a 0x{addr:x}", to_string=True)
            removed.append(_forget(filename, addr))
    elif addr in _user_loads:
        # Older GDB: trust our own record of what we loaded
        try:
            gdb.execute(f"remove-symbol-file -a 0x{addr:x}", to_string=True)
            removed.append(_user_loads[addr])
        except gdb.error:
            pass
    _user_loads.pop(addr, None)
    return removed


def _remove_at(base):
    # Remove the user program symbols loaded at base, and any we loaded
    # elsewhere in the same process slot (e.g. at an address that was not
    # the start of the slot). Returns the names removed.
    removed = []
    for other in sorted(_user_loads):
        if other != base and _slot_of(other) == _slot_of(base):
            removed += _remove_exact(other)
    return removed + _remove_exact(base)


def _check_not_kernel(base):
    if _HAVE_OBJFILE_FOR_ADDRESS:
        objfile = _objfile_at(base)
        if objfile is not None and _is_kernel(objfile):
            raise gdb.GdbError(f"0x{base:x} is inside the kernel, not a process")


def _without_confirm(command):
    confirm = gdb.parameter("confirm")
    gdb.execute("set confirm off")
    try:
        return gdb.execute(command, to_string=True)
    finally:
        if confirm:
            gdb.execute("set confirm on")


class LoadUserSymbols(gdb.Command):
    '''Load a user program's symbols for one process.

Usage: load_user_symbols PROGRAM PID_OR_ADDR

PROGRAM is the name of an embedded user program (press Tab to list them).
PID_OR_ADDR is any expression, e.g. 3, 0x380000, or current_process->pid.
A value below NPROC is a PID, whose process runs at
PROC_START + PID * PROC_SIZE; anything else is the address where the
program starts. User program symbols already loaded there are replaced,
so you can simply run the command again to fix a mistake.
See also: info_user_symbols, unload_user_symbols.'''

    def __init__(self):
        super().__init__("load_user_symbols", gdb.COMMAND_FILES)

    def invoke(self, arg, from_tty):
        parts = arg.split(None, 1)
        if len(parts) != 2:
            raise gdb.GdbError("usage: load_user_symbols PROGRAM PID_OR_ADDR")
        name, expr = parts
        if name not in USER_PROGRAMS:
            raise gdb.GdbError(f"no user program '{name}'; the programs are: "
                               + " ".join(sorted(USER_PROGRAMS)))
        elf, sections = USER_PROGRAMS[name]
        if not os.path.isfile(elf):
            raise gdb.GdbError(f"{elf} does not exist (run make?)")
        base, where = _resolve(expr)
        _check_not_kernel(base)
        for old in _remove_at(base):
            print(f"replacing the symbols of {old} at 0x{base:x}")
        _text, text_offset = sections[0]
        command = f"add-symbol-file {elf} 0x{base + text_offset:x}"
        for section, offset in sections[1:]:
            command += f" -s {section} 0x{base + offset:x}"
        _without_confirm(command)
        _user_loads[base] = name
        print(f"loaded the symbols of {name} ({where}) at 0x{base:x}")

    def complete(self, text, word):
        if " " not in text.lstrip():
            return [n for n in sorted(USER_PROGRAMS) if n.startswith(word or "")]
        return gdb.COMPLETE_EXPRESSION


class UnloadUserSymbols(gdb.Command):
    '''Remove the user program symbols loaded for one process.

Usage: unload_user_symbols PID_OR_ADDR

PID_OR_ADDR works as for load_user_symbols.'''

    def __init__(self):
        super().__init__("unload_user_symbols", gdb.COMMAND_FILES)

    def invoke(self, arg, from_tty):
        if not arg.strip():
            raise gdb.GdbError("usage: unload_user_symbols PID_OR_ADDR")
        base, _where = _resolve(arg.strip())
        _check_not_kernel(base)
        removed = _remove_at(base)
        if not removed:
            print(f"no user program symbols loaded at 0x{base:x}")
        for old in removed:
            print(f"removed the symbols of {old} at 0x{base:x}")

    def complete(self, text, word):
        return gdb.COMPLETE_EXPRESSION


class InfoUserSymbols(gdb.Command):
    '''List the user program symbols loaded by load_user_symbols, and where.

Usage: info_user_symbols'''

    def __init__(self):
        super().__init__("info_user_symbols", gdb.COMMAND_STATUS)

    def invoke(self, arg, from_tty):
        for base in sorted(_user_loads):
            if not _loaded_here(base):
                del _user_loads[base]  # removed by hand (remove-symbol-file)
        if not _user_loads:
            print("no user program symbols loaded")
            return
        print(f"{'PID':<5}{'address':<12}{'program':<16}ELF")
        for base in sorted(_user_loads):
            name = _user_loads[base]
            offset = base - PROC_START
            if 0 <= offset < NPROC * PROC_SIZE and offset % PROC_SIZE == 0:
                pid = str(offset // PROC_SIZE)
            else:
                pid = "-"
            print(f"{pid:<5}{'0x%x' % base:<12}{name:<16}{USER_PROGRAMS[name][0]}")


LoadUserSymbols()
UnloadUserSymbols()
InfoUserSymbols()
"""


def find_show_limits(kernel_elf):
    global PROC_START, PROC_SIZE

    kernel_dir = os.path.dirname(kernel_elf)
    parse_limits(kernel_dir)

    print(f"# Kernel: {kernel_elf}")
    print(f"PROC_START=0x{PROC_START:x}")
    print(f"PROC_SIZE=0x{PROC_SIZE:x}")
    print(f"NPROC={NPROC}")


def main():
    parser = argparse.ArgumentParser(
        description="Generate GDB script to load user-space debug symbols."
    )
    parser.add_argument("kernel_elf", help="Path to the kernel ELF file")
    parser.add_argument("user_elf_dir", help="Directory containing user .elf files")
    parser.add_argument(
        "programs", nargs="*",
        help="Names of the user programs (default: every .elf in user_elf_dir)",
    )
    parser.add_argument("--detect-only", help="Print detected values for PROC_START AND PROC_SIZE and exit", action="store_true")
    parser.add_argument("-t", "--toolprefix", default=None, help="Toolchain prefix (e.g., aarch64-elf-)")
    parser.add_argument(
        "-o", "--output", default=None, help="Output file (default: stdout)"
    )
    args = parser.parse_args()

    if args.detect_only:
        find_show_limits(args.kernel_elf)
        return

    if args.toolprefix:
        global TOOLPREFIX
        TOOLPREFIX = args.toolprefix

    lines = generate_gdb_commands(args.kernel_elf, args.user_elf_dir, args.programs)
    output = "\n".join(lines) + "\n" if lines else ""

    if args.output:
        with open(args.output, "w") as f:
            f.write(output)
    else:
        sys.stdout.write(output)

TOOLPREFIX = ""  # Global variable to hold the toolchain prefix

if __name__ == "__main__":
    main()
