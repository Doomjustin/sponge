import sys
import re
import subprocess
from pathlib import Path


INCLUDE_RE = re.compile(r'^\s*#include\s*[<"]([^">]+)[">]')
SOURCE_STEM_RE = re.compile(r"^(?P<stem>.+?)(?:\.test)?\.(?:c|cc|cpp|cxx|m|mm)$")


def source_stem(filepath: str) -> str:
    m = SOURCE_STEM_RE.match(Path(filepath).name)
    return m.group("stem") if m else ""


def include_path(line: str) -> str:
    m = INCLUDE_RE.match(line)
    return m.group(1) if m else ""


def move_main_include_first(lines: list[str], filepath: str) -> bool:
    stem = source_stem(filepath)
    if not stem:
        return False

    include_indices = [i for i, line in enumerate(lines) if INCLUDE_RE.match(line)]
    if len(include_indices) < 2:
        return False

    expected = f"{stem}.h"
    main_idx = -1
    for i in include_indices:
        inc = include_path(lines[i])
        if inc and Path(inc).name == expected:
            main_idx = i
            break

    if main_idx == -1 or main_idx == include_indices[0]:
        return False

    main_line = lines.pop(main_idx)
    lines.insert(include_indices[0], main_line)

    # 在主头文件后插入空行（如果紧接着还有其他 include 且没有空行）
    main_pos = include_indices[0]
    if main_pos + 1 < len(lines):
        next_line = lines[main_pos + 1]
        # 只有在下一行是 include 且当前行不是空行时，才插入空行
        if INCLUDE_RE.match(next_line):
            lines.insert(main_pos + 1, "\n")

    return True


def cleanup_trailing_blank_lines_after_includes(lines: list[str]) -> None:
    """Remove excess blank lines after the last include statement."""
    include_indices = [i for i, line in enumerate(lines) if INCLUDE_RE.match(line)]
    if not include_indices:
        return

    last_include_idx = include_indices[-1]
    # Remove consecutive blank lines immediately after the last include
    # Keep only one blank line
    idx = last_include_idx + 1
    blank_count = 0
    while idx < len(lines) and lines[idx].strip() == "":
        blank_count += 1
        idx += 1

    # If there are more than 1 blank line, remove the excess
    if blank_count > 1:
        for _ in range(blank_count - 1):
            lines.pop(last_include_idx + 2)


def main(filepath):
    start_line = -1
    end_line = -1

    lines = []

    # 扫描文件，找出 #include 所在的行范围
    with open(filepath, "r", encoding="utf-8") as f:
        lines = f.readlines()
        for i, line in enumerate(lines):
            if re.match(r"^\s*#include", line):
                if start_line == -1:
                    start_line = i + 1  # clang-format 行号从 1 开始
                end_line = i + 1

    # 如果找到了头文件，就调用 clang-format 局部格式化
    if start_line != -1 and end_line != -1:
        cmd = ["clang-format", "-i", f"--lines={start_line}:{end_line}", filepath]
        try:
            subprocess.run(cmd, check=True)

            # clang-format 之后再强制把主头文件放到第一个 include。
            with open(filepath, "r", encoding="utf-8") as f:
                formatted_lines = f.readlines()
            if move_main_include_first(formatted_lines, filepath):
                cleanup_trailing_blank_lines_after_includes(formatted_lines)
                with open(filepath, "w", encoding="utf-8") as f:
                    f.writelines(formatted_lines)
        except subprocess.CalledProcessError as e:
            print(f"clang-format 执行失败: {e}")
        except FileNotFoundError:
            print("找不到 clang-format，请确保它已加入系统环境变量")


if __name__ == "__main__":
    if len(sys.argv) > 1:
        main(sys.argv[1])
