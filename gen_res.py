#!/usr/bin/python

import os
import sys

key = sys.argv[1]
res = sys.argv[2]
output = sys.argv[3]


def main():
    with open(res, "rb") as f:
        bs = f.read()

    os.makedirs(os.path.dirname(output), exist_ok=True)
    with open(output, "w") as f:
        print("#include <util/resource.h>", file=f)
        print("static const char data[] = {", file=f)
        print(*map(hex, bs), sep=", ", file=f)
        print("};", file=f)
        print(file=f)
        print(
            "static TResourceRegisterer registerer(\"{}\", "
            "reinterpret_cast<const char*>(&data), {});".format(key, len(bs)), file=f)


if __name__ == '__main__':
    main()
