
# -*- python -*-
APPNAME = 'rem'
VERSION = '1.0.0'

def options(opt):
    opt.load('compiler_cc perl ruby')

def configure(conf):
    conf.load('compiler_cc perl ruby')
    conf.check_python_version((2,4,2))
    conf.check_ruby_version((1,8,7))
    conf.env.append_unique('CFLAGS', ['-O2'])
    conf.env.INCLUDES += '.'
    conf.env.LIB += ['pthread']

def build(bld):
    bld.shlib(source='lib/log.c', target='remlog', defines=['_LARGEFILE64_SOURCE', '_GNU_SOURCE'], lib = ['dl'], install_path='${PREFIX}/lib/')
    bld(rule='perl -ple "if(/use\s+constant\s+PRELOAD_LIBRARY_DIR/){s|=>.*|=> \\"${PREFIX}/lib/\\";|}" ${SRC} > ${TGT}', source='script/rem', target='rem');
    bld.install_files('${PREFIX}/bin', 'rem', chmod=0755)
    executables = ['rem', 'edag']
    bld.install_files('${PREFIX}/bin', ['script/' + x for x in executables], chmod=0755)
