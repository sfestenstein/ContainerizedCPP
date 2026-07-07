#!/usr/bin/env python3
"""
IDL-to-JSON helper generator for DDS message types.

Parses OMG IDL files and generates C++ toJson() / fromJson() functions that
convert between DDS-generated C++ types and crow::json.  The generated header
is DDS-vendor-neutral at the IDL level; only the accessor style differs
between vendors and is controlled by --accessor-style.

Supports:
   - Primitive types (boolean, short, long, long long, float, double, string)
   - Bounded strings (string<N>)
   - Enumerations (cast to/from int)
   - Nested struct types (recursive toJson/fromJson calls)
   - Sequences (sequence<T>) mapped to JSON arrays
   - Cross-file struct references (all IDL files parsed before code generation)

Usage (called automatically by CMake):
   python3 generate_dds_json_helpers.py       \\
      --idl-dir   src/libs/CycloneDDS/idl     \\
      --output    DdsJsonHelpers.h             \\
      --accessor-style method
"""

import argparse
import re
import sys
from pathlib import Path

# ── IDL type → { crow read accessor } ──────────────────────────────────────
IDL_TYPE_MAP = {
    "boolean":            "b",
    "short":              "i",
    "unsigned short":     "i",
    "long":               "i",
    "unsigned long":      "i",
    "long long":          "i",
    "unsigned long long": "i",
    "float":              "d",
    "double":             "d",
    "string":             "s",
    "octet":              "i",
}

# Types that need an explicit narrowing cast in fromJson setters
CAST_TYPES = {
    "long":           "int32_t",
    "unsigned long":  "uint32_t",
    "short":          "int16_t",
    "unsigned short": "uint16_t",
}

# ── Regex patterns for IDL parsing ──────────────────────────────────────────
RE_MODULE   = re.compile(r"module\s+(\w+)")
RE_ENUM     = re.compile(r"enum\s+(\w+)\s*\{([^}]+)\}")
RE_STRUCT   = re.compile(
    r"(?:@topic\s+)?struct\s+(\w+)\s*\{([^}]+)\}", re.DOTALL
)
RE_FIELD    = re.compile(
    r"(?:@\w+(?:\s*\([^)]*\))?\s+)*"              # annotations
    r"([\w\s<>,]+?)\s+(\w+)\s*;"                   # type  name ;
)
RE_STRING_BOUND = re.compile(r"string<\d+>")
RE_SEQUENCE     = re.compile(r"sequence<(.+?)(?:,\s*\d+)?>")


def strip_comments(text: str) -> str:
    """Remove block and line comments from IDL text."""
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//[^\n]*", "", text)
    return text


def parse_idl(path: Path):
    """Return (module, enums, structs) from a single IDL file.

    structs is a list of (struct_name, [(idl_type, field_name), ...]).
    """
    text = strip_comments(path.read_text())

    module = ""
    m = RE_MODULE.search(text)
    if m:
        module = m.group(1)

    enums: set[str] = set()
    for em in RE_ENUM.finditer(text):
        enums.add(em.group(1))

    structs: list[tuple[str, list[tuple[str, str]]]] = []
    for sm in RE_STRUCT.finditer(text):
        struct_name = sm.group(1)
        body = sm.group(2)
        fields: list[tuple[str, str]] = []
        for fm in RE_FIELD.finditer(body):
            raw_type = fm.group(1).strip()
            field_name = fm.group(2)
            # Normalise bounded strings → "string"
            idl_type = RE_STRING_BOUND.sub("string", raw_type)
            fields.append((idl_type, field_name))
        structs.append((struct_name, fields))

    return module, enums, structs


# ── Type classification helpers ─────────────────────────────────────────────

def is_sequence(idl_type: str) -> bool:
    return bool(RE_SEQUENCE.match(idl_type))


def sequence_element_type(idl_type: str) -> str:
    m = RE_SEQUENCE.match(idl_type)
    return m.group(1).strip() if m else idl_type


def classify_type(idl_type: str, enums: set[str], structs: set[str]):
    """Return one of: 'primitive', 'enum', 'struct', 'sequence'."""
    if is_sequence(idl_type):
        return "sequence"
    if idl_type in enums:
        return "enum"
    if idl_type in structs:
        return "struct"
    if idl_type in IDL_TYPE_MAP:
        return "primitive"
    # Might be a qualified name from another module — treat as struct
    base = idl_type.split("::")[-1]
    if base in structs:
        return "struct"
    return "unknown"


def qualified_cpp(name: str, module: str) -> str:
    """Return the fully-qualified C++ name for a user-defined type."""
    if "::" in name:
        return f"::{name}"
    return f"::{module}::{name}" if module else name


# ── toJson code generation ──────────────────────────────────────────────────

def gen_to_json_rhs(idl_type: str, field_name: str, enums: set[str],
                    all_structs: set[str], module: str,
                    accessor_style: str) -> str:
    """Return the C++ expression for the RHS of a toJson assignment."""
    getter = (f"m.{field_name}()" if accessor_style == "method"
              else f"m.{field_name}")
    kind = classify_type(idl_type, enums, all_structs)

    if kind == "enum":
        return f"static_cast<int>({getter})"
    if kind == "struct":
        return f"toJson({getter})"
    if kind == "sequence":
        elem = sequence_element_type(idl_type)
        elem_kind = classify_type(elem, enums, all_structs)
        # Return a marker; caller will emit a loop instead
        return None  # handled specially by the caller
    return getter


def gen_to_json_sequence(idl_type: str, field_name: str, enums: set[str],
                         all_structs: set[str], module: str,
                         accessor_style: str) -> list[str]:
    """Return lines for serialising a sequence field to a JSON list."""
    getter = (f"m.{field_name}()" if accessor_style == "method"
              else f"m.{field_name}")
    elem = sequence_element_type(idl_type)
    elem_kind = classify_type(elem, enums, all_structs)

    lines = [f'   j["{field_name}"] = crow::json::wvalue::list();']
    lines.append(f"   for (size_t _i = 0; _i < {getter}.size(); ++_i)")
    lines.append("   {")
    if elem_kind == "struct":
        lines.append(
            f'      j["{field_name}"][_i] = toJson({getter}[_i]);')
    elif elem_kind == "enum":
        lines.append(
            f'      j["{field_name}"][_i] = static_cast<int>('
            f'{getter}[_i]);')
    else:
        lines.append(
            f'      j["{field_name}"][_i] = {getter}[_i];')
    lines.append("   }")
    return lines


# ── fromJson code generation ────────────────────────────────────────────────

def gen_from_json_expr(idl_type: str, field_name: str, enums: set[str],
                       all_structs: set[str], module: str) -> str:
    """Return the C++ expression for reading a field from JSON."""
    kind = classify_type(idl_type, enums, all_structs)

    if kind == "enum":
        qualified = qualified_cpp(idl_type, module)
        return f'static_cast<{qualified}>(d["{field_name}"].i())'
    if kind == "struct":
        qualified = qualified_cpp(idl_type, module)
        return (f'fromJson(d["{field_name}"],'
                f' static_cast<{qualified} const *>(nullptr))')
    if kind == "sequence":
        return None  # handled specially
    accessor = IDL_TYPE_MAP.get(idl_type, "s")
    expr = f'd["{field_name}"].{accessor}()'
    cast = CAST_TYPES.get(idl_type)
    if cast:
        return f"static_cast<{cast}>({expr})"
    return expr


def gen_from_json_sequence(idl_type: str, field_name: str, enums: set[str],
                           all_structs: set[str], module: str,
                           accessor_style: str) -> list[str]:
    """Return lines for deserialising a JSON array into a sequence field."""
    elem = sequence_element_type(idl_type)
    elem_kind = classify_type(elem, enums, all_structs)

    lines = []
    lines.append(f'   for (const auto &_elem : d["{field_name}"])')
    lines.append("   {")
    if elem_kind == "struct":
        qualified = qualified_cpp(elem, module)
        lines.append(
            f"      {_setter_push(field_name, accessor_style)}"
            f"fromJson(_elem,"
            f" static_cast<{qualified} const *>(nullptr)));")
    elif elem_kind == "enum":
        qualified = qualified_cpp(elem, module)
        lines.append(
            f"      {_setter_push(field_name, accessor_style)}"
            f"static_cast<{qualified}>(_elem.i()));")
    else:
        accessor = IDL_TYPE_MAP.get(elem, "s")
        lines.append(
            f"      {_setter_push(field_name, accessor_style)}"
            f"_elem.{accessor}());")
    lines.append("   }")
    return lines


def _setter_push(field_name: str, accessor_style: str) -> str:
    if accessor_style == "method":
        return f"m.{field_name}().push_back("
    return f"m.{field_name}.push_back("


# ── Topological sort for struct dependencies ────────────────────────────────

def topo_sort_structs(all_data, all_structs: set[str]):
    """Yield (module, struct_name, fields) in dependency order.

    Structs that are used as fields in other structs come first.
    """
    # Build name → (module, fields) map and adjacency
    struct_info: dict[str, tuple[str, list]] = {}
    deps: dict[str, set[str]] = {}

    for module, enums, structs in all_data:
        for sname, fields in structs:
            struct_info[sname] = (module, fields)
            field_deps: set[str] = set()
            for idl_type, _ in fields:
                base = sequence_element_type(idl_type).split("::")[-1]
                if base in all_structs and base != sname:
                    field_deps.add(base)
            deps[sname] = field_deps

    visited: set[str] = set()
    order: list[str] = []

    def visit(name: str):
        if name in visited:
            return
        visited.add(name)
        for dep in deps.get(name, set()):
            visit(dep)
        order.append(name)

    for name in struct_info:
        visit(name)

    for name in order:
        module, fields = struct_info[name]
        yield module, name, fields


# ── Main code generation ────────────────────────────────────────────────────

def generate_header(idl_dir: Path, accessor_style: str) -> str:
    """Generate the full C++ header string."""
    idl_files = sorted(idl_dir.glob("*.idl"))
    if not idl_files:
        print(f"WARNING: no .idl files found in {idl_dir}", file=sys.stderr)

    # First pass: collect all types from every IDL file
    all_data: list[tuple[str, set[str], list]] = []
    all_enums: set[str] = set()
    all_structs: set[str] = set()
    includes: list[str] = []

    for idl_file in idl_files:
        module, enums, structs = parse_idl(idl_file)
        all_enums |= enums
        for sname, _ in structs:
            all_structs.add(sname)
        all_data.append((module, enums, structs))
        includes.append(f'#include "{idl_file.stem}.hpp"')

    lines: list[str] = []

    # Header guard & includes
    lines.append("/***********************************************")
    lines.append(" * AUTO-GENERATED — do not edit by hand.")
    lines.append(" * Generated by generate_dds_json_helpers.py")
    lines.append(" ***********************************************/")
    lines.append("#ifndef DDS_JSON_HELPERS_H_")
    lines.append("#define DDS_JSON_HELPERS_H_")
    lines.append("")
    for inc in sorted(set(includes)):
        lines.append(inc)
    lines.append("")
    lines.append("#include <crow/json.h>")
    lines.append("")
    lines.append("#include <cstddef>")
    lines.append("")
    lines.append("namespace DdsJsonHelpers")
    lines.append("{")
    lines.append("")

    # Emit structs in dependency order so that toJson/fromJson for
    # "leaf" types appear before the structs that reference them.
    for module, struct_name, fields in topo_sort_structs(all_data, all_structs):
        ns = f"{module}::" if module else ""
        qualified = f"::{ns}{struct_name}"

        # ── toJson ──────────────────────────────────────────────────
        lines.append(
            f"inline crow::json::wvalue toJson(const {qualified} &m)")
        lines.append("{")
        lines.append("   crow::json::wvalue j;")
        for idl_type, fname in fields:
            if is_sequence(idl_type):
                lines.extend(gen_to_json_sequence(
                    idl_type, fname, all_enums, all_structs, module,
                    accessor_style))
            else:
                rhs = gen_to_json_rhs(idl_type, fname, all_enums,
                                      all_structs, module, accessor_style)
                lines.append(f'   j["{fname}"] = {rhs};')
        lines.append("   return j;")
        lines.append("}")
        lines.append("")

        # ── fromJson ────────────────────────────────────────────────
        lines.append(
            f"inline {qualified} fromJson(const crow::json::rvalue &d,")
        lines.append(
            f"   {qualified} const * /*tag*/ = nullptr)")
        lines.append("{")
        lines.append(f"   {qualified} m;")
        for idl_type, fname in fields:
            if is_sequence(idl_type):
                lines.extend(gen_from_json_sequence(
                    idl_type, fname, all_enums, all_structs, module,
                    accessor_style))
            else:
                expr = gen_from_json_expr(idl_type, fname, all_enums,
                                          all_structs, module)
                if accessor_style == "method":
                    lines.append(f"   m.{fname}({expr});")
                else:
                    lines.append(f"   m.{fname} = {expr};")
        lines.append("   return m;")
        lines.append("}")
        lines.append("")

    lines.append("} // namespace DdsJsonHelpers")
    lines.append("")
    lines.append("#endif // DDS_JSON_HELPERS_H_")
    lines.append("")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Generate C++ JSON helpers from OMG IDL files."
    )
    parser.add_argument("--idl-dir", required=True, type=Path,
                        help="Directory containing .idl files")
    parser.add_argument("--output", required=True, type=Path,
                        help="Output header file path")
    parser.add_argument("--accessor-style", default="method",
                        choices=["method", "field"],
                        help="C++ accessor style: 'method' (CycloneDDS/FastDDS)"
                             " or 'field' (OpenDDS)")
    args = parser.parse_args()

    header = generate_header(args.idl_dir, args.accessor_style)

    # Only write if content changed (avoid unnecessary rebuilds)
    if args.output.exists() and args.output.read_text() == header:
        return

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(header)
    print(f"Generated {args.output}")


if __name__ == "__main__":
    main()
