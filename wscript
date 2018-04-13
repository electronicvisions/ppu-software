def depends(dep):
    dep('libnux')

def options(opt):
    opt.load('nux_compiler')

def configure(conf):
    env = conf.env
    conf.setenv('nux')
    conf.load('nux_compiler')
    conf.load('objcopy')
    conf.setenv('', env=env)

def build(bld):
    bld.program(
        features = 'c objcopy',
        objcopy_bfdname = 'binary',
        target = 'test_synram.bin',
        source = ['src/test_synram.c'],
        use = ['nux', 'nux_runtime'],
        env = bld.all_envs['nux'],
    )
    bld.program(
        features = 'c objcopy',
        objcopy_bfdname = 'binary',
        target = 'ppu_sweep',
        source = ['src/ppu_sweep.c'],
        use = ['nux', 'nux_runtime'],
        env = bld.all_envs['nux'],
    )

    bld.program(
        features = 'c objcopy',
        objcopy_bfdname = 'binary',
        target = 'rstdp',
        source = ['src/rstdp.c'],
        use = ['nux', 'nux_runtime'],
        env = bld.all_envs['nux'],
    )

    bld.program(
        features = 'c objcopy',
        objcopy_bfdname = 'binary',
        target = 'get_corr',
        source = ['src/get_corr.c'],
        use = ['nux', 'nux_runtime'],
        env = bld.all_envs['nux'],
    )

    bld.program(
        features = 'c objcopy',
        objcopy_bfdname = 'binary',
        target = 'weight_incr',
        source = ['src/weight_incr.c'],
        use = ['nux', 'nux_runtime'],
        env = bld.all_envs['nux'],
    )
