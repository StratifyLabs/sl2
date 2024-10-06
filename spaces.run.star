"""

"""

load("tools/sysroot-gh/publish.star", "add_publish_archive")

run.add_exec(
    rule = {"name": "configure"},
    exec = {
        "command": "cmake",
        "args": [
            "-GNinja",
            "-Bbuild",
        ],
    },
)

run.add_exec(
    rule = {"name": "build"},
    exec = {
        "command": "ninja",
        "args": [
            "-Cbuild",
        ],
    },
)

add_publish_archive(
    name = "spaces",
    input = "build/install",
    version = version,
    deploy_repo = "https://github.com/work-spaces/tools-sl2",
    deps = ["install"]
)