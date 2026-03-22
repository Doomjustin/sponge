import sys
import re
import subprocess


def main(filepath):
    start_line = -1
    end_line = -1

    # 扫描文件，找出 #include 所在的行范围
    with open(filepath, "r", encoding="utf-8") as f:
        for i, line in enumerate(f):
            if re.match(r"^\s*#include", line):
                if start_line == -1:
                    start_line = i + 1  # clang-format 行号从 1 开始
                end_line = i + 1

    # 如果找到了头文件，就调用 clang-format 局部格式化
    if start_line != -1 and end_line != -1:
        cmd = ["clang-format", "-i", f"--lines={start_line}:{end_line}", filepath]
        try:
            subprocess.run(cmd, check=True)
        except subprocess.CalledProcessError as e:
            print(f"clang-format 执行失败: {e}")
        except FileNotFoundError:
            print("找不到 clang-format，请确保它已加入系统环境变量")


if __name__ == "__main__":
    if len(sys.argv) > 1:
        main(sys.argv[1])
