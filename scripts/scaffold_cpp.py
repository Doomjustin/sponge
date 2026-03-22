#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path


def fail(msg: str, code: int = 1) -> None:
    print(msg)
    raise SystemExit(code)


def normalize_submodule(raw: str) -> str:
    sub = raw.strip().strip("/")
    if not sub:
        return ""

    if not re.match(r"^[A-Za-z][A-Za-z0-9_]*(/[A-Za-z][A-Za-z0-9_]*)*$", sub):
        fail(
            "ERROR: submodule must be slash-separated identifiers, "
            "each starting with a letter and containing only letters, digits, or underscores."
        )
    return sub


def derive_namespace(submodule: str, user_ns: str) -> str:
    if user_ns:
        return user_ns

    if not submodule:
        return "spg"

    return "spg::" + "::".join(submodule.split("/"))


def include_guard(include_rel: str, name: str) -> str:
    rel_part = include_rel.replace("/", "_").upper()
    return f"{rel_part}_{name.upper()}_H"


def main() -> int:
    args = sys.argv[1:]
    if not args:
        fail("ERROR: file name cannot be empty.")

    name = args[0]
    submodule_raw = ""
    workspace_raw = ""
    user_ns = ""

    # Supports both forms:
    #   1) name workspace
    #   2) name submodule workspace [namespace]
    if len(args) == 2:
        workspace_raw = args[1]
    elif len(args) >= 3:
        submodule_raw = args[1]
        workspace_raw = args[2]
        if len(args) >= 4:
            user_ns = args[3]

    if not name:
        fail("ERROR: file name cannot be empty.")

    if not re.match(r"^[A-Za-z][A-Za-z0-9_]*$", name):
        fail(
            "ERROR: file name must start with a letter and contain only letters, digits, or underscores."
        )

    if not workspace_raw:
        fail("ERROR: workspace path is required.")

    workspace = Path(workspace_raw).resolve()
    submodule = normalize_submodule(submodule_raw)

    if submodule:
        include_rel = f"sponge/{submodule}"
        src_rel = submodule
    else:
        include_rel = "sponge"
        src_rel = "common"

    include_dir = workspace / "include" / include_rel
    src_dir = workspace / "src" / src_rel
    include_dir.mkdir(parents=True, exist_ok=True)
    src_dir.mkdir(parents=True, exist_ok=True)

    ns = derive_namespace(submodule, user_ns)
    guard = include_guard(include_rel, name)

    h = include_dir / f"{name}.h"
    cpp = src_dir / f"{name}.cpp"
    test = src_dir / f"{name}.test.cpp"
    include_stmt = f"{include_rel}/{name}.h"

    for f in (h, cpp, test):
        if f.exists():
            fail(
                f"ERROR: target file already exists:\n  - {f}\nNo files were overwritten.",
                2,
            )

    h.write_text(
        "\n".join(
            [
                f"#ifndef {guard}",
                f"#define {guard}",
                "",
                f"namespace {ns} {{",
                "",
                f"}} // namespace {ns}",
                "",
                f"#endif // {guard}",
                "",
            ]
        ),
        encoding="utf-8",
    )

    cpp.write_text(
        "\n".join(
            [
                f'#include "{include_stmt}"',
                "",
                f"namespace {ns} {{",
                "",
                f"}} // namespace {ns}",
                "",
            ]
        ),
        encoding="utf-8",
    )

    test.write_text(
        "\n".join(
            [
                "#include <catch2/catch_test_macros.hpp>",
                "",
                f'#include "{include_stmt}"',
                "",
                f'TEST_CASE("{name} smoke", "[{name}]") {{',
                "    SUCCEED();",
                "}",
                "",
            ]
        ),
        encoding="utf-8",
    )

    print("Scaffold created successfully:")
    print(f"  name      : {name}")
    print(f"  submodule : {submodule if submodule else '(default)'}")
    print(f"  namespace : {ns}")
    print(f"  include   : {include_dir}")
    print(f"  src       : {src_dir}")
    print("  files:")
    print(f"    - {h}")
    print(f"    - {cpp}")
    print(f"    - {test}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
