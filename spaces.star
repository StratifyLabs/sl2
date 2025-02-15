"""

"""

load("//@star/packages/star/gh.star", "gh_add")
load("//@star/sdk/star/gh.star", "gh_add_publish_archive")
load(
    "//@star/sdk/star/checkout.star",
    "checkout_add_asset",
    "checkout_add_repo",
)
load(
    "//@star/sdk/star/info.star",
    "info_assert_member_semver",
    "info_get_path_to_member_with_rev",
    "info_is_platform_windows",
    "info_set_minimum_version",
)
load("//@star/sdk/star/cmake.star", "cmake_add_configure_build_install")
load("//@star/sdk/star/shell.star", "cp")
load("//@star/sdk/star/run.star", "run_add_exec")

info_set_minimum_version("0.12.6")

info_assert_member_semver(
    "https://github.com/work-spaces/sdk",
    semver = ">=0.1.2",
)

SL2_PATH = info_get_path_to_member_with_rev(
    "https://github.com/StratifyLabs/sl2",
    rev = None,
)

gh_add("gh1", "v2.64.0")

def _add_stratify_labs_library(
        name,
        revision):
    checkout_add_repo(
        "libs/{}".format(name),
        url = "https://github.com/StratifyLabs/{}".format(name),
        rev = revision,
        clone = "Blobless",
    )

_add_stratify_labs_library("CMakeSDK", "v2.1")
_add_stratify_labs_library("API", "v1.7")
_add_stratify_labs_library("StratifyOS", "v4.3")
_add_stratify_labs_library("JsonAPI", "v1.5")
_add_stratify_labs_library("InetAPI", "v1.5")
_add_stratify_labs_library("CryptoAPI", "v1.4")
_add_stratify_labs_library("SosAPI", "v1.4")
_add_stratify_labs_library("HalAPI", "v1.3")
_add_stratify_labs_library("CloudAPI", "v1.3")
_add_stratify_labs_library("SwdAPI", "v1.2")
_add_stratify_labs_library("ServiceAPI", "v1.2")
_add_stratify_labs_library("UsbAPI", "v1.3")

checkout_add_asset(
    "CMakeLists-Superproject",
    destination = "CMakeLists.txt",
    content = fs.read_file_to_string("{}/spaces/CMakeLists.txt".format(SL2_PATH)),
)

BUILD_INSTALL = info.get_absolute_path_to_workspace() + "/build/install"

CONFIGURE_ARGS = [
    "-GNinja",
    "-DCMSDK_LOCAL_PATH={}".format(BUILD_INSTALL),
]

cmake_add_configure_build_install(
    "cmakesdk_link",
    source_directory = "libs/CMakeSDK",
    configure_args = CONFIGURE_ARGS,
)

cmake_add_configure_build_install(
    "sl_link",
    source_directory = SL2_PATH,
    skip_install = True,
    configure_args = CONFIGURE_ARGS,
    deps = ["cmakesdk_link"],
)

SUFFIX = ".exe" if info_is_platform_windows() else ""
SL_BUILD_EXE = "build/sl{}".format(SUFFIX)

cp(
    "install",
    deps = ["sl_link"],
    source = "{}/app/build_release_link/sl_release_link{}".format(SL2_PATH, SUFFIX),
    destination = SL_BUILD_EXE,
    options = ["-f"],
)

run_add_exec(
    "test",
    command = SL_BUILD_EXE,
    args = [
        "--version",
        "--vanilla"
    ],
    deps = ["install"]
)

gh_add_publish_archive(
    name = "sl",
    input = "build/sl",
    version = "2.0.1",
    deploy_repo = "https://github.com/StratifyLabs/sl2",
    deps = ["test"],
)
