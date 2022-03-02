import os
import pip
import shutil


def execfile(filepath, globals=None, locals=None):
    if globals is None:
        globals = {}
    globals.update({
        "__file__": filepath,
        "__name__": "__main__",
    })
    with open(filepath, 'rb') as file:
        exec(compile(file.read(), filepath, 'exec'), globals, locals)


def should_run():
    if not os.path.isdir("../src/generated"):
        return True
    included_extensions = ['csv', 'py']
    src = [fn for fn in os.listdir(".") if any(fn.endswith(ext) for ext in included_extensions)]
    latest_change = max([os.path.getmtime("./" + file) for file in src])
    latest_run = min([os.path.getmtime("../src/generated/" + file) for file in os.listdir("../src/generated")])
    print(f"Codegen: latest change @ {latest_change}, latest run @ {latest_run}")
    return latest_change > latest_run


def main():
    assert os.path.basename(os.getcwd()) == "meta", "Must run codegen from meta directory"
    if not should_run():
        return
    os.makedirs("../src/generated", exist_ok=True)
    execfile('ast_fields.py')
    execfile('construct_codes.py')
    execfile('tokens.py')
    execfile('parse_nodes.py')
    execfile('semantic_codes.py')
    execfile('typeset_keywords.py')
    execfile('typeset_shorthand.py')
    execfile('errors.py')
    execfile('interpreter_dispatch.py')
    execfile('unicode_scripts.py')
    # execfile('unicode_graphemes.py')
    
    shutil.copyfile("construct_codes.csv", "cache/construct_codes.csv")
    

if __name__ == "__main__":
    main()
