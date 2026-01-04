import re


def convert_header_to_utf8(input_filename, output_filename):
    pattern = re.compile(r"#define\s+(\w+)\s+0x([0-9a-fA-F]+)")

    # Range tracking
    min_cp = float('inf')
    max_cp = float('-inf')

    try:
        with open(input_filename, 'r') as infile:
            lines = infile.readlines()

        output_lines = ["#pragma once\n\n"]

        for line in lines:
            match = pattern.search(line)
            if match:
                name, hex_val = match.groups()
                code_point = int(hex_val, 16)

                # Update range
                if code_point < min_cp: min_cp = code_point
                if code_point > max_cp: max_cp = code_point

                # Convert to UTF-8
                utf8_bytes = chr(code_point).encode('utf-8')
                escaped_str = "".join(f"\\x{b:02x}" for b in utf8_bytes)

                output_lines.append(f'#define {name} "{escaped_str}"\t// U+{hex_val.lower()}\n')
            else:
                # Keep existing formatting/comments
                if line.strip() != "#pragma once":  # Avoid double pragmas
                    output_lines.append(line)

        # Add the range defines at the bottom if we found any codepoints
        if min_cp != float('inf'):
            output_lines.append("\n// Range Information\n")
            output_lines.append(f"#define MATSYM_MIN_CODEPOINT 0x{min_cp:04x}\n")
            output_lines.append(f"#define MATSYM_MAX_CODEPOINT 0x{max_cp:04x}\n")

        with open(output_filename, 'w') as outfile:
            outfile.writelines(output_lines)

        print(f"Success! Range: 0x{min_cp:04x} - 0x{max_cp:04x}")

    except FileNotFoundError:
        print(f"Error: The file '{input_filename}' was not found.")


if __name__ == "__main__":
    convert_header_to_utf8("matsym_codepoints.h", "matsym_codepoints_utf8.h")