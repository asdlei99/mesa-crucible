#! /usr/bin/env python

import argparse
import cStringIO
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from textwrap import dedent

class Shader:
    def __init__(self, stage):
        self.stream = cStringIO.StringIO()
        self.stage = stage

        if self.stage == 'VERTEX':
            self.ext = 'vert'
        elif self.stage == 'TESS_CONTROL':
            self.ext = 'tesc'
        elif self.stage == 'TESS_EVALUATION':
            self.ext = 'tese'
        elif self.stage == 'GEOMETRY':
            self.ext = 'geom'
        elif self.stage == 'FRAGMENT':
            self.ext = 'frag'
        elif self.stage == 'COMPUTE':
            self.ext = 'comp'
        else:
            assert False

    def add_text(self, s):
        self.stream.write(s)

    def finish_text(self, line):
        self.line = line

    def glsl_source(self):
        return self.stream.getvalue()

    def compile(self):
        # We can assume if we got here that we have a temp directory and that
        # we're currently living in it.
        glsl_fname = 'shader{0}.{1}'.format(self.line, self.ext)
        spirv_fname = self.ext + '.spv'

        glsl_file = open(glsl_fname, 'w')
        glsl_file.write('#version 420 core\n')
        glsl_file.write(self.glsl_source())
        glsl_file.close()

        out = open('glslang.out', 'wb')
        err = subprocess.call([glslang, '-V', glsl_fname], stdout=out)
        if err != 0:
            out = open('glslang.out', 'r')
            sys.stderr.write(out.read())
            out.close()
            exit(1)

        def dwords(f):
            while True:
                dword_str = f.read(4)
                if not dword_str:
                    return
                assert len(dword_str) == 4
                yield struct.unpack('I', dword_str)[0]

        spirv_file = open(spirv_fname, 'rb')
        self.dwords = list(dwords(spirv_file))
        spirv_file.close()

        os.remove(glsl_fname)
        os.remove(spirv_fname)

    def _dump_glsl_code(self, f, var_name):
        # First dump the GLSL source as strings
        f.write('static const char {0}[] ='.format(var_name))
        f.write('\n__QO_SPIRV_' + self.stage)
        f.write('\n"#version 330\\n"')
        for line in self.glsl_source().splitlines():
            if not line.strip():
                continue
            f.write('\n"{0}\\n"'.format(line))
        f.write(';\n\n')

    def _dump_spirv_code(self, f, var_name):
        f.write('static const uint32_t {0}[] = {{'.format(var_name))
        line_start = 0
        while line_start < len(self.dwords):
            f.write('\n    ')
            for i in range(line_start, min(line_start + 6, len(self.dwords))):
                f.write(' 0x{:08x},'.format(self.dwords[i]))
            line_start += 6
        f.write('\n};\n')

    def dump_c_code(self, f, glsl_only = False):
        f.write('\n\n')
        var_prefix = '__qonos_shader{0}'.format(self.line)

        self._dump_glsl_code(f, var_prefix + '_glsl_src')

        if not glsl_only:
            self._dump_spirv_code(f, var_prefix + '_spir_v_src')

        f.write(dedent("""\
            static const QoShaderCreateInfo {0}_info = {{
                .glslSize = sizeof({0}_glsl_src),
                .pGlsl = {0}_glsl_src,
            """.format(var_prefix)))

        if not glsl_only:
            f.write(dedent("""\
                .spirvSize = sizeof({0}_spir_v_src),
                .pSpirv = {0}_spir_v_src,
            """.format(var_prefix)))

        f.write('};')

token_exp = re.compile(r'(qoShaderCreateInfoGLSL|qoCreateShaderGLSL|\(|\)|,)')

class Parser:
    def __init__(self, f):
        self.infile = f
        self.paren_depth = 0
        self.shader = None
        self.line_number = 1
        self.shaders = []

        def tokenize(f):
            leftover = ''
            for line in f:
                pos = 0
                while True:
                    m = token_exp.search(line, pos)
                    if m:
                        if m.start() > pos:
                            leftover += line[pos:m.start()]
                        pos = m.end()

                        if leftover:
                            yield leftover
                            leftover = ''

                        yield m.group(0)

                    else:
                        leftover += line[pos:]
                        break

                self.line_number += 1

            if leftover:
                yield leftover

        self.token_iter = tokenize(self.infile)

    def handle_shader_src(self):
        paren_depth = 1
        for t in self.token_iter:
            if t == '(':
                paren_depth += 1
            elif t == ')':
                paren_depth -= 1
                if paren_depth == 0:
                    return

            self.current_shader.add_text(t)

    def handle_macro(self, macro):
        t = self.token_iter.next()
        assert t == '('

        if macro == 'qoCreateShaderGLSL':
            # Throw away the device parameter
            t = self.token_iter.next()
            t = self.token_iter.next()
            assert t == ','

        stage = self.token_iter.next().strip()

        t = self.token_iter.next()
        assert t == ','

        self.current_shader = Shader(stage)
        self.handle_shader_src()
        self.current_shader.finish_text(self.line_number)

        self.shaders.append(self.current_shader)
        self.current_shader = None

    def run(self):
        for t in self.token_iter:
            if t in ('qoShaderCreateInfoGLSL', 'qoCreateShaderGLSL'):
                self.handle_macro(t)

def open_file(name, mode):
    if name == '-':
        if mode == 'w':
            return sys.stdout
        elif mode == 'r':
            return sys.stdin
        else:
            assert False
    else:
        return open(name, mode)

def parse_args():
    description = dedent("""\
        This program scrapes a C file for any instance of the
        qoShaderCreateInfoGLSL and qoCreateShaderGLSL macaros, grabs the
        GLSL source code, compiles it to SPIR-V.  The resulting SPIR-V code
        is written to another C file as an array of 32-bit words.

        If '-' is passed as the input file or output file, stdin or stdout
        will be used instead of a file on disc.""")

    p = argparse.ArgumentParser(
            description=description,
            formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('-o', '--outfile', default='-',
                        help='Output to the given file (default: stdout).')
    p.add_argument('--with-glslang', metavar='PATH',
                        default='glslangValidator',
                        dest='glslang',
                        help='Full path to the glslangValidator program.')
    p.add_argument('--glsl-only', action='store_true')
    p.add_argument('infile', metavar='INFILE')

    return p.parse_args()


args = parse_args()
infname = args.infile
outfname = args.outfile
glslang = args.glslang
glsl_only = args.glsl_only

with open_file(infname, 'r') as infile:
    parser = Parser(infile)
    parser.run()

if not glsl_only:
    # glslangValidator has an absolutely *insane* interface.  We pretty much
    # have to run in a temporary directory.  Sad day.
    current_dir = os.getcwd()
    tmpdir = tempfile.mkdtemp('glsl_scraper')

    try:
        os.chdir(tmpdir)

        for shader in parser.shaders:
            shader.compile()

        os.chdir(current_dir)
    finally:
        shutil.rmtree(tmpdir)

with open_file(outfname, 'w') as outfile:
    outfile.write(dedent("""\
        /* ==========================  DO NOT EDIT!  ==========================
         *             This file is autogenerated by glsl_scraper.py.
         */

        #include <stdint.h>

        #define __QO_SPIRV_MAGIC "\\x03\\x02\\x23\\x07\\0\\0\\0\\0"

        #define __QO_SPIRV_VERTEX              __QO_SPIRV_MAGIC "\\0\\0\\0\\0"
        #define __QO_SPIRV_TESS_CONTROL      __QO_SPIRV_MAGIC "\\1\\0\\0\\0"
        #define __QO_SPIRV_TESS_EVALUATION  __QO_SPIRV_MAGIC "\\2\\0\\0\\0"
        #define __QO_SPIRV_GEOMETRY            __QO_SPIRV_MAGIC "\\3\\0\\0\\0"
        #define __QO_SPIRV_FRAGMENT            __QO_SPIRV_MAGIC "\\4\\0\\0\\0"
        #define __QO_SPIRV_COMPUTE             __QO_SPIRV_MAGIC "\\5\\0\\0\\0"

        #define __QO_SHADER_INFO_VAR2(_line) __qonos_shader ## _line ## _info
        #define __QO_SHADER_INFO_VAR(_line) __QO_SHADER_INFO_VAR2(_line)

        #define qoShaderCreateInfoGLSL(stage, ...)  \\
            __QO_SHADER_INFO_VAR(__LINE__)

        #define qoCreateShaderGLSL(dev, stage, ...) \\
            __qoCreateShader((dev), &__QO_SHADER_INFO_VAR(__LINE__))
        """))

    for shader in parser.shaders:
        shader.dump_c_code(outfile, glsl_only)
