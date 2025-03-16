{
  "targets": [
    {
      "target_name": "shared_memory",
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "sources": [ "src/shared_memory.cpp" ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "/opt/homebrew/include"
      ],
      'defines': [ 'NAPI_DISABLE_CPP_EXCEPTIONS' ],
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
            'CLANG_CXX_LIBRARY': 'libc++',
            'MACOSX_DEPLOYMENT_TARGET': '10.15',
            'OTHER_CPLUSPLUSFLAGS': [
              '-std=c++17',
              '-stdlib=libc++'
            ]
          },
          'include_dirs': [
            '/opt/homebrew/include'
          ],
          'library_dirs': [
            '/opt/homebrew/lib'
          ],
          'link_settings': {
            'libraries': [
              '-lboost_system',
              '-lboost_thread',
              '-lboost_date_time'
            ]
          }
        }],
        ['OS=="linux"', {
          'cflags': [
            '-std=c++17'
          ],
          'link_settings': {
            'libraries': [
              '-lboost_system',
              '-lboost_thread',
              '-lboost_date_time',
              '-lrt',
              '-lpthread'
            ]
          }
        }],
        ['OS=="win"', {
          'msvs_settings': {
            'VCCLCompilerTool': {
              'ExceptionHandling': 1,
              'AdditionalOptions': ['/std:c++17']
            }
          },
          'link_settings': {
            'libraries': [
              'libboost_system-mt.lib',
              'libboost_thread-mt.lib',
              'libboost_date_time-mt.lib'
            ]
          }
        }]
      ]
    }
  ]
} 