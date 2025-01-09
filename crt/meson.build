crt0 = custom_target(
    'crt0',
    build_by_default: true,
    command: compiler.cmd_array() + ['@INPUT@', '-o', '@OUTPUT@', '-c'],
    input: 'crt0.S',
    install: true,
    install_dir: get_option('libdir'),
    install_tag: 'devel',
    output: 'crt0.o',
)
