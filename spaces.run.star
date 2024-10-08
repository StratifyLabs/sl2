"""

"""

load("tools/sysroot-gh/publish.star", "add_publish_archive")

sysroot = info.get_absolute_path_to_workspace() + "/sysroot"

run.add_exec(
    rule = {"name": "configure_cmsdk"},
    exec = {
        "command": "cmake",
        "args": [
            "-GNinja",
            "-Slibs/CMakeSDK",
            "-Bbuild/CMakeSDK/cmake_link",
            "-DCMSDK_LOCAL_PATH={}".format(sysroot),
        ]
    },
)

run.add_exec(
    rule = {"name": "install_cmsdk", "deps": ["configure_cmsdk"]},
    exec = {
        "command": "ninja",
        "args": [
            "-Cbuild/CMakeSDK/cmake_link",
            "install",
        ]
    },
)



run.add_exec(
    rule = {"name": "configure"},
    exec = {
        "command": "cmake",
        "args": [
            "-GNinja",
            "-Bbuild/cmake_link",
        ],
    },
)

run.add_exec(
    rule = {"name": "build", "deps": ["configure", "install_cmsdk"]},
    exec = {
        "command": "ninja",
        "args": [
            "-Cbuild/cmake_link",
        ],
    },
)

suffix = ".exe" if info.is_platform_windows() else ""

run.add_exec(
    rule = {"name": "install", "deps": ["build"]},
    exec = {
        "command": "cp",
        "args": [
            "sl2/app/build_release_link/sl_release_link{}".format(suffix),
            "build/sl{}".format(suffix)
        ],
    },
)


add_publish_archive(
    name = "sl",
    input = "build/sl",
    version = "2.0.1",
    deploy_repo = "https://github.com/StratifyLabs/sl2",
    deps = ["install"]
)