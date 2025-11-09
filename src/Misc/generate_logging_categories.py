import os
import re
import sys

def scan_src(src_dir):
    categories = set()
    for root, dirs, files in os.walk(src_dir):
        for file in files:
            if file.endswith('.cpp'):
                try:
                    with open(os.path.join(root, file), 'r', encoding='utf-8') as f:
                        for line in f:
                            match = re.search(r'Q_LOGGING_CATEGORY\([^,]+,\s*"([^"]+)"\)', line)
                            if match:
                                categories.add(match.group(1))
                except UnicodeDecodeError:
                    pass  # Skip files with encoding issues
    return sorted(categories)

if __name__ == '__main__':
    # Script is in src/Misc/, src/ is ../
    src_dir = '../'
    cats = scan_src(src_dir)
    print('#pragma once')
    print('static const char* logging_categories[] = {')
    for cat in cats:
        print(f'    "{cat}",')
    print('};')
    print(f'static const int logging_categories_count = {len(cats)};')