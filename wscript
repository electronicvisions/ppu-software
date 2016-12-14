def depends(dep):
    dep('libnux')

def options(opt):
    pass

def configure(conf):
    pass

def build(bld):
    bld.program(
        target = 'test_synram',
        source = ['src/test_synram.c'],
        use = ['nux', 'nux_inc', 'nux_runtime'],
    )

    bld.program(
        target = 'ppu_sweep',
        source = ['src/ppu_sweep.c'],
        use = ['nux', 'nux_inc', 'nux_runtime'],
    )

    bld.program(
        target = 'rstdp',
        source = ['src/rstdp.c'],
        use = ['nux', 'nux_inc', 'nux_runtime'],
    )

    bld.program(
        target = 'get_corr',
        source = ['src/get_corr.c'],
        use = ['nux', 'nux_inc', 'nux_runtime'],
    )

    bld.program(
        target = 'weight_incr',
        source = ['src/weight_incr.c'],
        use = ['nux', 'nux_inc', 'nux_runtime'],
    )
