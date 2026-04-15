project('Meshtastic temperature bridge', 'cpp', version: '0.1.0', default_options: ['cpp_std=c++20', 'c_std=c17'])

all_sources = [
    'src/http/http_server.cpp',
    'src/http/http_server_sec.cpp'
]

ssl = dependency('openssl')

executable('meshbridge', sources: ['src/main.cpp', all_sources], dependencies: [ssl])