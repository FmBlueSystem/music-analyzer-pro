{
  "targets": [
    {
      "target_name": "metadata_addon",
      "sources": [
        "src/addon_napi.cpp",
        "src/ai_algorithms_master.cpp",
        "src/ai_algorithms.cpp",
        "src/ai_algorithms_part2.cpp",
        "src/ai_algorithms_part3.cpp",
        "src/ai_algorithms_base.cpp",
        "src/ai_algorithms_hamms.cpp"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "src",
        "/usr/local/include/taglib",
        "/opt/homebrew/include/taglib",
        "/opt/homebrew/Cellar/taglib/2.1.1/include",
        "/opt/homebrew/include",
        "/usr/local/include"
      ],
      "libraries": [
        "-ltag",
        "-lfftw3",
        "-lfftw3f",
        "-L/usr/local/lib",
        "-L/opt/homebrew/lib",
        "-L/opt/homebrew/Cellar/taglib/2.1.1/lib"
      ],
      "cflags_cc": [
        "-std=c++17",
        "-fexceptions",
        "-frtti"
      ],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ],
      "conditions": [
        ["OS=='mac'", {
          "xcode_settings": {
            "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
            "GCC_ENABLE_CPP_RTTI": "YES",
            "CLANG_CXX_LIBRARY": "libc++",
            "MACOSX_DEPLOYMENT_TARGET": "10.15"
          }
        }]
      ]
    }
  ]
}