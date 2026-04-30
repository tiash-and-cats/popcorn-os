import lief
import struct
import sys

def make_minipop(infile, outfile):
    # Parse the executable with LIEF
    binary = lief.parse(infile)
    if not binary:
        raise RuntimeError(f"Could not parse {infile}")

    # Get entry point (absolute virtual address)
    entry = binary.get_function_address("pop_main")

    # Flatten raw content: concatenate all loadable segments
    raw_code = b""
    for section in binary.sections:
        if section.name in {".code", ".data"}:
            raw_code += bytes(section.content)

    # Build header: [3B magic][4B entry]
    print(entry)
    header = b"POP" + struct.pack("<I", entry)

    with open(outfile, "wb") as f:
        f.write(header)
        f.write(raw_code)

    print(f"Wrote MINIPOP binary {outfile} with entry 0x{entry:x}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python minipop.py input.exe output.pop")
        sys.exit(1)
    make_minipop(sys.argv[1], sys.argv[2])
