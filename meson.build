project(
    'libc',
    'c',
    default_options: ['c_std=gnu11', 'warning_level=2'],
    license: 'MIT',
    license_files: 'LICENSE',
    meson_version: '>=1.1.0',
    version: '0.1.0',
)

compiler = meson.get_compiler('c')
hydrogen = dependency('hydrogen')

inc = [include_directories('include')]

add_project_link_arguments('-nostdlib', language: 'c')

subdir('crt')
subdir('libc')
subdir('rtld')
subdir('include')

libc = shared_library(
    'c',
    dependencies: hydrogen,
    install: true,
    objects: libc_static.extract_all_objects(recursive: true),
    soversion: 1,
)
