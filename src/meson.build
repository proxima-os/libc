libc = shared_library(
    'c',
    'auxv.c',
    'main.c',
    'string.c',
    c_args: ['-fno-builtin', '-fvisibility=hidden'],
    dependencies: hydrogen,
    include_directories: inc,
    install: true,
)
