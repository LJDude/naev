#!/usr/bin/env python3

def gammaToLinear(x):
    if x <= 0.04045:
        return x / 12.92
    return pow((x + 0.055) / 1.055, 2.4)

class Colour:
    def __init__(self, name, r, g, b, a=1.0):
        self.name = name
        self.r = gammaToLinear( r )
        self.g = gammaToLinear( g )
        self.b = gammaToLinear( b )
        self.a = a

    def write_header(self, f):
        f.write(f"extern const glColour c{self.name};\n")

    def write_source(self, f):
        f.write(f"const glColour c{self.name} = {{ .r={self.r}, .g={self.g}, .b={self.b}, .a={self.a} }};\n")


COLOURS = [
]

def write_header(f):
    f.write(f"/* FILE GENERATED BY {__file__} */")

def generate_h_file(f):
    write_header(f)

    f.write("""
#ifndef COLOURS_GEN_C_H
#define COLOURS_GEN_C_H
""")
    for col in COLOURS:
        col.write_header( f )
    f.write("""

const glColour* col_fromName( const char* name );

#endif /* SHADERS_GEN_C_H */""")

def generate_c_file(f):
    write_header(f)

    f.write("""
#include <string.h>
#include "colour.h"
#include "log.h"

""")
    for col in COLOURS:
        col.write_source( f )
    f.write("""

const glColour* col_fromName( const char* name )
{
""")
    for col in COLOURS:
        f.write(f"   if (strcasecmp(name,{col.name})==0) return &c{col.name};\n")
    f.write("""
   WARN(_("Colour '%s' not found!"),name);
   return NULL;
}""")

with open("colours.gen.h", "w") as colours_ggen_h:
    generate_h_file(colours_ggen_h)

with open("colours.gen.c", "w") as colours_ggen_c:
    generate_c_file(colours_ggen_c)
