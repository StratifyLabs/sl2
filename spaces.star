"""

"""

load("spaces-starlark-sdk/github.com/packages/cli/cli/v2.60.1.star", gh_platforms = "platforms")
load("spaces-starlark-sdk/star/gh.star", "add_publish_archive")

checkout.add_platform_archive(
    rule = {"name": "gh1"},
    platforms = gh_platforms
)

def add_stratify_labs_library(
    name,
    revision):
    checkout.add_repo(
    rule = { "name": "libs/{}".format(name)},
    repo = {
        "url": "https://github.com/StratifyLabs/{}".format(name),
        "rev": revision,
        "checkout": "Revision"
    }
)

add_stratify_labs_library("CMakeSDK", "v2.1")
add_stratify_labs_library("API", "v1.7")
add_stratify_labs_library("StratifyOS", "v4.3")
add_stratify_labs_library("JsonAPI", "v1.5")
add_stratify_labs_library("InetAPI", "v1.5")
add_stratify_labs_library("CryptoAPI", "v1.4")
add_stratify_labs_library("SosAPI", "v1.4")
add_stratify_labs_library("HalAPI", "v1.3")
add_stratify_labs_library("CloudAPI", "v1.3")
add_stratify_labs_library("SwdAPI", "v1.2")
add_stratify_labs_library("ServiceAPI", "v1.2")
add_stratify_labs_library("UsbAPI", "v1.3")

sl2_path = info.get_path_to_checkout()

checkout.add_asset(
    rule = {"name": "CMakeLists-Superproject"},
    asset = {
        "destination": "CMakeLists.txt",
        "content": fs.read_file_to_string("{}/spaces/CMakeLists.txt".format(sl2_path))
    }
)

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