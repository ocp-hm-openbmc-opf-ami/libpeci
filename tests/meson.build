gtest_dep = dependency('gtest', main: false, required: false)
gmock_dep = dependency('gmock', required: false)

if not gtest_dep.found() or not gmock_dep.found()
    cmake = import('cmake')
    opt_var = cmake.subproject_options()
    opt_var.add_cmake_defines({'CMAKE_CXX_FLAGS': '-Wno-pedantic'})
    opt_var.set_override_option('warning_level', '3')
    opt_var.set_override_option('werror', 'true')

    gtest_proj = cmake.subproject('gtest', options: opt_var, required: true)
    if gtest_proj.found()
        gtest_dep = declare_dependency(
            dependencies: [dependency('threads'), gtest_proj.dependency('gtest')]
        )
        gmock_dep = gtest_proj.dependency('gmock')
    else
        assert(
            not get_option('tests').enabled(),
            'Googletest is required if tests are enabled',
        )
    endif
endif

# Linker --wrap flags to intercept system calls used by peci.c
wrap_link_args = [
    '-Wl,--wrap=ioctl',
    '-Wl,--wrap=open',
    '-Wl,--wrap=open64',
    '-Wl,--wrap=openat',
    '-Wl,--wrap=openat64',
    '-Wl,--wrap=close',
    '-Wl,--wrap=read',
    '-Wl,--wrap=fstatat',
    '-Wl,--wrap=fstatat64',
    '-Wl,--wrap=readlink',
    '-Wl,--wrap=scandir',
    '-Wl,--wrap=scandir64',
    '-Wl,--wrap=nanosleep',
    '-Wl,--wrap=getenv',
    '-Wl,--wrap=syslog',
    '-Wl,--allow-multiple-definition',
]

common_deps = [
    gtest_dep,
    gmock_dep,
]

# ---------------------------------------------------------------------------
# unit_test_peci — tests for peci.c public API
# ---------------------------------------------------------------------------
unit_test_peci = executable(
    'unit_test_peci',
    [
        'test_main.cpp',
        'test_peci.cpp',
        'mock_syscalls.c',
        '../peci.c',
    ],
    include_directories: include_directories('..'),
    dependencies: common_deps,
    link_args: wrap_link_args,
    c_args: ['-Wno-conversion', '-Wno-sign-conversion'],
    cpp_args: ['-Wno-conversion', '-Wno-sign-conversion'],
)
test(
    'unit_test_peci',
    unit_test_peci,
    timeout: 120,
)

# ---------------------------------------------------------------------------
# unit_test_peci_cmds — tests for peci_cmds.c CLI tool
# ---------------------------------------------------------------------------
unit_test_peci_cmds = executable(
    'unit_test_peci_cmds',
    [
        'test_main.cpp',
        'test_peci_cmds.cpp',
        'mock_syscalls.c',
        '../peci.c',
        '../peci_cmds.c',
    ],
    include_directories: include_directories('..'),
    dependencies: common_deps,
    link_args: wrap_link_args,
    c_args: ['-Wno-conversion', '-Wno-sign-conversion', '-Dmain=peci_cmds_main'],
    cpp_args: ['-Wno-conversion', '-Wno-sign-conversion'],
)
test(
    'unit_test_peci_cmds',
    unit_test_peci_cmds,
    timeout: 120,
)

# ---------------------------------------------------------------------------
# unit_test_dbus_raw_peci — tests for dbus_raw_peci.cpp Send handler logic
# ---------------------------------------------------------------------------
unit_test_dbus_raw_peci = executable(
    'unit_test_dbus_raw_peci',
    [
        'test_main.cpp',
        'test_dbus_raw_peci.cpp',
        'mock_syscalls.c',
        '../peci.c',
    ],
    include_directories: include_directories('..'),
    dependencies: common_deps,
    link_args: wrap_link_args,
    c_args: ['-Wno-conversion', '-Wno-sign-conversion'],
    cpp_args: ['-Wno-conversion', '-Wno-sign-conversion'],
)
test(
    'unit_test_dbus_raw_peci',
    unit_test_dbus_raw_peci,
    timeout: 120,
)
