load("@rules_proto_grpc//cpp:defs.bzl", "cpp_grpc_compile", "cpp_grpc_library")


DEPS = [
    "@libglog//:lib",
    "@gflags//:lib",
]

package_group(
    name = "subpackages",
    packages = [
        "//src/...",
    ],
)

proto_library(
  name = "level_proto",
  srcs = ["level.proto"],
)

cpp_grpc_library(
    name = "level_proto_cc",
    protos = [
        ":level_proto",
    ],
)

cc_library(
   name = "point",
   srcs = ["point.h"],
)

cc_library(
   name = "game",
   srcs = [
      "constants.h",
      "math.h",
      "game.h"
   ],
   deps = [":level_proto_cc", ":point"],
)

cc_test(
   name = "game_test",
   srcs = ["game_test.cc"],
   deps = [":game"],
   linkopts = ['-lgtest -lglog']
)

cc_binary(
    name = "bman_server",
    srcs = ["bman_server.cc"],
    defines = ["BAZEL_BUILD"],
    linkopts = ['-lglog'],
    deps = [
        ":level_proto_cc",
        ":game",
        "@gflags//:lib",
    ],
)


cc_library(
    name = "bman_client",
    srcs = ["bman_client.h", "bman_client.cc"],
    defines = ["BAZEL_BUILD"],
    deps = [
        ":level_proto_cc",
    ],
)


cc_binary(
    name = "bman",
    srcs = ["bman.cc"],
    defines = ["BAZEL_BUILD"],
    linkopts = ['-lSDL2  -lglog'],
    deps = [
        "@gflags//:lib",
        ":level_proto_cc",
        ":game",
        ":bman_client",
    ],
)
