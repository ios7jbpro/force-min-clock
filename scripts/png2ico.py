import struct, sys

def png_to_ico(png_path, ico_path):
    with open(png_path, 'rb') as f:
        png_data = f.read()
    
    # ICO header: reserved(2) + type(2) + count(2)
    header = struct.pack('<HHH', 0, 1, 1)
    # ICO directory entry: width(1) + height(1) + colors(1) + reserved(1) + planes(2) + bpp(2) + size(4) + offset(4)
    entry = struct.pack('<BBBBHHII', 0, 0, 0, 0, 1, 32, len(png_data), 6 + 16)
    
    with open(ico_path, 'wb') as f:
        f.write(header + entry + png_data)
    print(f'Created {ico_path} ({len(header) + len(entry) + len(png_data)} bytes)')

png_to_ico(sys.argv[1], sys.argv[2])
