"""Small adapter for sharing Aura's existing build policies with native IDF.

Only the SCons operations used by the existing policy/generator scripts are
implemented. Unsupported operations fail rather than silently skipping a check.
"""

import configparser
from pathlib import Path
import runpy
import shutil
import sys

PROJECT = Path(__file__).resolve().parents[1]
PROFILES = {"4_3": "project_aura", "7_dual_i2c": "project_aura_7"}


class BuildEnv(dict):
    def __init__(self, profile, build_dir, size_tool="xtensa-esp-elf-size"):
        if profile not in PROFILES:
            raise ValueError(f"Unsupported hardware profile: {profile}")
        config = configparser.ConfigParser(interpolation=None, inline_comment_prefixes=(";",))
        config.read(PROJECT / "platformio.ini", encoding="utf-8")
        options = dict(config["env:project_aura"])
        options.update(config[f"env:{PROFILES[profile]}"])
        super().__init__(PROJECT_DIR=str(PROJECT), BUILD_DIR=str(Path(build_dir).resolve()),
                         PIOENV=PROFILES[profile], PROGNAME="aura_aq",
                         SIZETOOL=size_tool, CPPDEFINES=[], CPPPATH=[])
        self.options = options
        self.actions = []

    def GetProjectOption(self, name, default=""):
        return self.options.get(name, default)

    def subst(self, value):
        for key, replacement in self.items():
            if isinstance(replacement, str):
                value = value.replace("${" + key + "}", replacement).replace("$" + key, replacement)
        return value

    def Append(self, **kwargs):
        for key, values in kwargs.items():
            self.setdefault(key, []).extend(values)

    def AppendUnique(self, **kwargs):
        for key, values in kwargs.items():
            target = self.setdefault(key, [])
            target.extend(value for value in values if value not in target)

    def AddPostAction(self, target, action):
        self.actions.append((self.subst(target), action))

    def WhereIs(self, name):
        return shutil.which(name)

    def run(self, script):
        sys.path.insert(0, str(PROJECT / "scripts"))
        return runpy.run_path(str(PROJECT / "scripts" / script),
                              init_globals={"env": self, "Import": lambda name: None})
