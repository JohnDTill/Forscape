import shutil
import os

dirs = {'.', 'example', 'example/exampleWidgetQt', 'src', 'include', 'src/typeset_constructs', 'test'}

for d in dirs:
    for entry in os.scandir(d):
        if entry.is_dir() and len(entry.name) > 5 and entry.name[0:5] == 'build':
            shutil.rmtree(entry)
        elif entry.name == 'CMakeLists.txt.user':
            os.remove(entry)
        elif entry.name.endswith('.h') or entry.name.endswith('.cpp'):
            infile = open(entry, 'r', encoding='utf-8')
            firstLine = infile.readline().rstrip()
            infile.close()
            if firstLine == "//CODEGEN FILE":
                os.remove(entry)
