#!/usr/bin/env python3
import os
import sys

EXTENSIONS = {".hpp", ".h", ".cpp", ".cc", ".c"}
DIRS = ["include", "src", "apps", "tests", "bench", "fuzz"]

def count_substantive_lines(filepath):
    substantive = 0
    in_block_comment = False
    with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            stripped = line.strip()
            if not stripped:
                continue
            if in_block_comment:
                if "*/" in stripped:
                    in_block_comment = False
                continue
            if stripped.startswith("/*"):
                if "*/" not in stripped:
                    in_block_comment = True
                continue
            if stripped.startswith("//"):
                continue
            substantive += 1
    return substantive

def main():
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    totals = {}
    grand_total = 0
    print(f"{'Directory':<15} {'Files':<10} {'Substantive LOC':<15}")
    print("-" * 42)
    for d in DIRS:
        dpath = os.path.join(repo_root, d)
        if not os.path.exists(dpath):
            continue
        count = 0
        loc = 0
        for root, _, files in os.walk(dpath):
            for file in files:
                if any(file.endswith(ext) for ext in EXTENSIONS):
                    count += 1
                    loc += count_substantive_lines(os.path.join(root, file))
        totals[d] = (count, loc)
        grand_total += loc
        print(f"{d:<15} {count:<10} {loc:<15}")
    print("-" * 42)
    print(f"{'Total':<15} {sum(c for c, _ in totals.values()):<10} {grand_total:<15}")

if __name__ == "__main__":
    main()
