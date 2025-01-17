version_script = meson.current_source_dir() / 'rtld.lds'

shared_library(
    'ld64',
    'main.c',
    'object.c',
    'start.c',
    'start.S',
    c_args: ['-ffreestanding'], # If this is not passed, LTO may optimize a standard function into itself
    dependencies: hydrogen,
    include_directories: inc,
    install: true,
    link_args: ['-Wl,--entry=rtld_start', '-Wl,--version-script=' + version_script],
    link_depends: version_script,
    name_prefix: '',
    sources: crt0,
    objects: libc_static.extract_all_objects(recursive: true),
    soversion: 1,
)
