Import("env")

import os

project_dir = env["PROJECT_DIR"]
mocks_path = os.path.join(project_dir, "test", "mocks")

# Put mocks first so they win over real headers in src/.
env.PrependUnique(CPPFLAGS=[f"-I{mocks_path}"])
env.PrependUnique(CPPPATH=[mocks_path])

# Ensure mock sources are compiled for unit tests.
env.AppendUnique(
    PIOTEST_SRC_FILTER=[
        "+<mocks/*.c>",
        "+<mocks/*.cc>",
        "+<mocks/*.cpp>",
        "+<mocks/*.cxx>",
    ]
)
