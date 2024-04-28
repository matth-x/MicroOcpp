Import("env", "ARDUINOJSON_DIR")

import os, pathlib

def getAllDirs(root_dir):
    dir_list = []
    for root, subfolders, files in os.walk(root_dir.abspath):
        dir_list.append(Dir(root))
    return dir_list

SOURCE_DIR = Dir(".").srcnode()

source_dirs = getAllDirs(SOURCE_DIR.Dir("src"))
source_dirs += getAllDirs(ARDUINOJSON_DIR.Dir("src"))

source_files = []

for folder in source_dirs:
    source_files += folder.glob("*.cpp")
    env["CPPPATH"].append(folder)

compiled_objects = []
for source_file in source_files:
    obj = env.Object(
        target = pathlib.Path(source_file.path).stem
        + ".o",
        source=source_file,
    )
    compiled_objects.append(obj)

libmicroocpp = env.StaticLibrary(
    target='libmicroocpp',
    source=sorted(compiled_objects)
)

exports = {
    'library': libmicroocpp,
    'CPPPATH': source_dirs.copy()
}

Return("exports")