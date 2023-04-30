load("@pybind11_bazel//:build_defs.bzl", "pybind_extension")

pybind_extension(
    name = "game_wrapper",
    srcs = ["game_wrapper.cc"],
    copts = [
          "-I/usr/include/python3.7m",
          "-I/usr/include/OIS",
          "-I/usr/include/OGRE",
    ],
    deps = [
        "//:agent",
        "//:game",
        "//:game_renderer",
    ],
    linkopts = ['-lSDL2 -lSDL2_ttf' ],
)
