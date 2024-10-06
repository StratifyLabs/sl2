"""
Spaces starlark file to checkout dependencies and build SL
"""


# This is used for publishing sl
checkout.add_repo(
    rule = {"name": "tools/sysroot-gh"},
    repo = {
        "url": "https://github.com/work-spaces/sysroot-gh",
        "rev": "v2",
        "checkout": "Revision",
    },
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

