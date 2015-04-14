
def configure(conf):
    conf.find_program('powerpc-linux-eabi-as', var='PPC_AS', mandatory=True)
    conf.find_program('powerpc-linux-eabi-ld', var='PPC_LD', mandatory=True)
    conf.find_program('powerpc-linux-eabi-objcopy', var='PPC_OBJCOPY', mandatory=True)
    conf.find_program('powerpc-linux-eabi-gcc', var='PPC_CC', mandatory=True)

    conf.env.PPC_CSHELL_LINKER_SCRIPT = conf.path.make_node('cshell_linker.x').abspath()
    conf.env.PPC_CFLAGS = [
        '-Wall',
        '-O2',
        '-msdata=none',
        '-mstrict-align',
        '-msoft-float',
        #'-fno-exceptions',
        '-mno-relocatable',
        '-ggdb',
        #'-I' + conf.path.find_dir('source/software/s2pplib/include').abspath(),
        '-D%s' %(conf.env.SYSTEM),
    ]
    conf.env.PPC_LDFLAGS = [
        '-static',
        '-nostdlib',
        #'-L/superfast/home/sfriedma/s2pp-cross-comp/install/lib/gcc/powerpc-linux-eabi/4.8.2/',
        '-lgcc',
    ]


def build(bld):
    s2pplib_source = """
        s2pplib/fxv.c
        s2pplib/exp.c
    """

    bld(
        name = 'create_synram',
        source = 'synram.c' + s2pplib_source,
        includes = [
            's2pplib/include'
        ],
        target = bld.path.find_or_declare('synram.raw'),
        cshell = 'cshell.s',
        features = 'ppu_program',
        is_copy = True
    )



from waflib import Task, TaskGen
from waflib.Tools import c_preproc

class ppu_assemble(Task.Task):
    run_str = '${PPC_AS} -mpower7 ${SRC} -o ${TGT}'
    color = 'BLUE'
    ext_out = ['.o']
    before = 'ppu_link ppu_link_cshell'

class ppu_assemble_reference(Task.Task):
    run_str = '${PPC_AS} -mpower7 --defsym RESULT=1 ${SRC} -o ${TGT}'
    color = 'BLUE'
    ext_out = ['.o']
    before = 'ppu_link ppu_link_cshell'

class ppu_compile_c(Task.Task):
    #run_str = '${PPC_CC} --save-temps -c ${PPC_CFLAGS} ${PPC_CC_INCFLAGS} ${SRC} -o ${TGT}'
    run_str = '${PPC_CC} -c ${PPC_CFLAGS} ${PPC_CC_INCFLAGS} ${SRC} -o ${TGT}'
    #shell = True
    color = 'BLUE'
    ext_out = ['.o']
    before = 'ppu_link ppu_link_cshell'
    scan = c_preproc.scan

class ppu_link(Task.Task):
    run_str = '${PPC_LD} -static -nostdlib -T ${PPC_LINKER_SCRIPT} ${SRC} -o ${TGT}'
    color = 'BLUE'
    ext_out = ['.elf']

class ppu_link_cshell(Task.Task):
    run_str = '${PPC_CC} ${SRC} -T ${PPC_CSHELL_LINKER_SCRIPT} ${PPC_LDFLAGS} -o ${TGT}'
    color = 'BLUE'
    ext_out = ['.elf']

class ppu_extract_bits(Task.Task):
    run_str = '${PPC_OBJCOPY} -O binary ${SRC} ${TGT}'
    color = 'BLUE'
    ext_out = ['.raw']
    after = 'ppu_link ppu_link_cshell'

class copy(Task.Task):
    run_str = 'cp ${SRC} ${TGT}'


@TaskGen.feature('ppu_program')
@TaskGen.before_method('process_source')
def ppu_cshell(self):
    # mappings for c and s files
    def compile_c(self, node):
        # include dependencies
        obj_node = node.change_ext('.o')
        task = self.create_task('ppu_compile_c', node, obj_node)
        task.env['PPC_CC_INCFLAGS'] = [
            '-I%s' % i.abspath()
            for i in self.includes_nodes
        ]

    def assemble(self, node):
        obj_node = node.change_ext('.o')
        self.create_task('ppu_assemble', node, obj_node)

    self.mappings['.c'] = compile_c
    self.mappings['.s'] = assemble

    # add include directories
    self.includes_nodes = [
        self.bld.path.find_dir(i)
        for i in self.to_list(getattr(self, 'includes', []))
    ]

    # assemble the C-Shell wrapper
    cshell_src_node = self.to_nodes(self.to_list(getattr(self, 'cshell', [])))[0]
    assemble(self, cshell_src_node)

    # link all object files
    src_nodes = self.to_nodes(self.to_list(getattr(self, 'source', [])))
    obj_nodes = [ i.change_ext('.o') for i in src_nodes ]
    obj_nodes.append(cshell_src_node.change_ext('.o'))
    #target_node = self.bld.path.find_or_declare(getattr(self, 'target', None))
    target_node = getattr(self, 'target', None)
    #print "path: ", target_node.abspath()
    elf_node = target_node.change_ext('.elf')
    self.create_task('ppu_link_cshell', obj_nodes, elf_node)

    # produce raw file for programming
    self.create_task('ppu_extract_bits', elf_node, target_node)


@TaskGen.feature('ppu_testimage')
@TaskGen.before_method('process_source')
def ppu_testimage(self):
    def assemble(self, node):
        obj_node = node.change_ext('.o')
        ref_node = node.change_ext('_exp.o')
        self.create_task('ppu_assemble', node, obj_node)
        self.create_task('ppu_assemble_reference', node, ref_node)
        self.source.extend([obj_node, ref_node])

    def link(self, node):
        elf_node = node.change_ext('.elf')
        self.create_task('ppu_link', node, elf_node)
        self.source.append(elf_node)

    def extract_bits(self, node):
        raw_node = node.change_ext('.raw')
        self.create_task('ppu_extract_bits', node, raw_node)

    def copy(self, node):
        self.create_task('copy', node.get_src(), node.get_bld())

    self.mappings['.raw'] = copy
    self.mappings['.s'] = assemble
    self.mappings['.o'] = link
    self.mappings['_exp.o'] = link
    self.mappings['.elf'] = extract_bits
    self.mappings['_exp.elf'] = extract_bits

    self.source = self.to_nodes(self.to_list(getattr(self, 'source', [])))



# vim: set et fenc= ff=unix sts=0 sw=4 ts=4 : 
