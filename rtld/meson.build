# Need to wrap libc objects in a static library first to be able to use --exclude-libs ALL
libc_static = static_library(
    'c',
    build_by_default: false,
    dependencies: hydrogen,
    objects: libc.extract_all_objects(recursive: true),
)

shared_library(
    'ld64',
    'main.c',
    'start.S',
    c_args: ['-fvisibility=hidden'],
    include_directories: inc,
    install: true,
    link_args: ['-Wl,-Bsymbolic,--exclude-libs=ALL,--entry=rtld_start'],
    link_with: libc_static,
    name_prefix: '',
    sources: crt0,
    soversion: 1,
)
