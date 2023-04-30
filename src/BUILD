load("@rules_proto_grpc//cpp:defs.bzl", "cpp_grpc_compile", "cpp_grpc_library")


DEPS = [
    "@libglog//:lib",
    "@gflags//:lib",
]

package_group(
    name = "subpackages",
    packages = [
        "//...",
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
      "game.h",
      "timer.h",
      "types.h",
      "grid_map.h",
      "grid_map.cc"
   ],
   visibility = [":subpackages"],
   deps = [
       ":level_proto_cc",
       ":point",
       "@com_github_glog_glog//:glog",
       ],
)

cc_test(
   name = "game_test",
   srcs = ["game_test.cc"],
   deps = [":game"],
   linkopts = ['-lgtest -lglog']
)

cc_library(
   name = "agent",
   srcs = [
        "agent.h",
        "simple_agent.h",
        "simple_agent.cc",
   ],
   visibility = [":subpackages"],   
   defines = ["BAZEL_BUILD"],
   deps = [
      ":level_proto_cc",
      "@com_github_glog_glog//:glog",
       ":game",
   ],
)

cc_binary(
    name = "bman_server",
    srcs = ["bman_server.cc"],
    defines = ["BAZEL_BUILD"],
    deps = [
        ":level_proto_cc",
        ":game",
        "@com_github_glog_glog//:glog",
        "@com_github_gflags_gflags//:gflags",
    ],
)


cc_library(
    name = "bman_client",
    srcs = ["bman_client.h", "bman_client.cc"],
    defines = ["BAZEL_BUILD"],
    deps = [
        ":level_proto_cc",
        "@com_github_glog_glog//:glog",
    ],
)

cc_library(
    name = "game_renderer",
    srcs = [
       "game_renderer.h",
       "game_renderer.cc"
    ],
    visibility = [":subpackages"],
    defines = ["BAZEL_BUILD"],
    linkopts = ['-lSDL2 -lSDL2_ttf' ],
    deps = [
        ":level_proto_cc",
        ":game",
    ],
)

cc_binary(
    name = "bman",
    srcs = ["bman.cc"],
    defines = ["BAZEL_BUILD"],
    linkopts = ['-lSDL2 -lSDL2_ttf' ],
    deps = [
        "@com_github_gflags_gflags//:gflags",
        "@com_github_glog_glog//:glog",
        ":agent",
        ":level_proto_cc",
        ":game",
        ":bman_client",
        ":game_renderer",
    ],
)
