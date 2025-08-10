import os
import sys
import argparse
from typing import List


def array_to_cpparray(data: bytes):
    return ",".join(f"0x{int.from_bytes(data[i:i + 4], 'little'):08x}" for i in range(0, len(data), 4))


def generate_header(files_path: List[str]) -> str:
    result = ""
    result += "// Auto-generate file\n"
    result += "#pragma once\n\n"
    result += "namespace resource_gen{\n"

    for file_path in files_path:
        filename = os.path.basename(file_path)
        if not os.path.isfile(file_path):
            continue

        data: bytes
        try:
            with open(file_path, "rb") as file:
                data = file.read()
        except Exception as e:
            print(f"Error reading {file_path}: {e}")
            continue

        array_name = filename.replace('.', '_')
        cpp_array = array_to_cpparray(data)
        result += f"constexpr uint32_t {array_name}[] = {{{cpp_array}}};\n"

    result += "}\n"
    return result


def main():
    parser = argparse.ArgumentParser(description="Resource Generator Script")
    parser.add_argument("input_files", help="Path to the input files")
    parser.add_argument("--output_file", default="",
                        help="Path to the output file")

    args = parser.parse_args()
    input_files = args.input_files.split(",")
    input_files_abs = []

    # input_abs_path = os.path.abspath(args.input_dir)
    for input_file in input_files:
        input_abs_path = os.path.abspath(input_file)
        if not os.path.exists(input_abs_path):
            print(f"Input file {input_abs_path} does not exist.")
            continue
        input_files_abs.append(input_abs_path)

    output_abs_path = os.path.abspath(args.output_file)
    if args.output_file == "":
        output_abs_path = os.path.join(os.path.dirname(__file__), "resource.gen.h")
    if not os.path.exists(os.path.dirname(output_abs_path)):
        os.makedirs(os.path.dirname(output_abs_path))

    if os.path.exists(output_abs_path):
        os.remove(output_abs_path)

    header_content = generate_header(input_files_abs)
    with open(output_abs_path, "w") as header_file:
        header_file.write(header_content)
        print(f"Generated resource header file at {output_abs_path}")

    pass


if __name__ == "__main__":
    main()
