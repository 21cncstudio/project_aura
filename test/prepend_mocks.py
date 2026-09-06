Import("env")

import os

project_dir = env["PROJECT_DIR"]
mocks_path = os.path.join(project_dir, "test", "mocks")
prefer_real_headers = env["PIOENV"] == "native_test_sfa40_driver"
prefer_real_headers = prefer_real_headers or env["PIOENV"] == "native_test_sfa30_driver"
prefer_real_headers = prefer_real_headers or env["PIOENV"] == "native_test_dfr_optional_gas_driver"
prefer_real_headers = prefer_real_headers or env["PIOENV"] == "native_test_gp8403_driver"
prefer_real_headers = prefer_real_headers or env["PIOENV"] == "native_test_i2c_4_3_profile"
prefer_real_headers = prefer_real_headers or env["PIOENV"] == "native_test_i2c_7_profile"

# Most native tests rely on mock headers shadowing src/, but dedicated driver
# tests need the real driver headers while still seeing shared Arduino/I2C
# mocks.
if prefer_real_headers:
    env.AppendUnique(CPPPATH=[mocks_path])
else:
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
