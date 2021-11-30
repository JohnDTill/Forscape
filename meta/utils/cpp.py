class Writer:
    """
    This is a class to generalize code generation for C++
    """
    def __init__(self, inner_namespace=None, includes=()):
        self.inner_namespace = inner_namespace
        self.str = "//CODEGEN FILE\n\n"
        self.class_list = []
        self.visibility = []
        self.indent_level = 0

    def _indent(self):
        self.str += "\t" * self.indent_level

    def increase_indent(self):
        self.indent_level += 1

    def decrease_indent(self):
        assert self.indent_level > 0
        self.indent_level -= 1

    def _case(self, stmt):
        if '\n' not in stmt:
            self.str += f" {stmt} break;\n"
        else:
            self.str += '{\n'
            self.increase_indent()
            for line in stmt.split('\n'):
                self._indent()
                self.str += line + '\n'
            self.indent()
            self.str += "break;\n"
            self.decrease_indent()
            self._indent()
            self.str += "}\n"

    def switch(self, var, cases, statements, default=None):
        assert len(cases) == len(statements)
        self._indent()
        self.str += f"switch({var}){{\n"
        self.increase_indent()
        for i in range(0, len(cases)):
            case = cases[i]
            stmt = statements[i]
            self._indent()
            self.str += f"case {case}:"
            self._case(stmt)
        if default:
            self._indent()
            self.str += "default:"
            self._case(stmt)

    def begin_class(self, name):
        self.class_list.append(name)
        self.visibility.append("private")
        self.str += f"class {name}{{\n"

    def _outer_namespace_begin(self):
        self.str += "namespace Hope {\n\n"

    def _outer_namespace_end(self):
        self.str += "} // namespace Hope\n\n"

    def _inner_namespace_begin(self):
        if self.inner_namespace:
            self.str += f"namespace {self.inner_namespace} {{\n\n"

    def _inner_namespace_end(self):
        assert len(self.class_list) == 0, "Closing C++ writer before closing all classes"
        if self.inner_namespace:
            self.str += f"}} // namespace {self.inner_namespace}\n\n"

    def _inclusion_list(self, includes):
        includes = sorted(includes)
        for include in includes:
            self.str += f"#include <{include}>\n"
        if len(includes) > 0:
            self.str += '\n'

    def write(self, out):
        self.str += out


class HeaderWriter(Writer):
    """
    This is a class to generalize code generation for C++
    """
    def __init__(self, name, inner_namespace=None, includes=()):
        super().__init__(inner_namespace, includes)
        self.name = name.lower()
        self.guard = (f"HOPE_{inner_namespace}_{name}_H" if inner_namespace else f"HOPE_{name}_H").upper()
        self.__head_boilerplate(includes)
        self.filename = (f"{self.inner_namespace}_{self.name}.h" if inner_namespace else f"{self.name}.h").lower()

    def finalize(self):
        self.__tail_boilerplate()
        with open(f"../src/generated/{self.filename}", "w", encoding="utf-8") as codegen_file:
            codegen_file.write(self.str)

    def __head_boilerplate(self, includes):
        self.__include_guard_start()
        self._inclusion_list(includes)
        self._outer_namespace_begin()
        self._inner_namespace_begin()
        # codegen

    def __tail_boilerplate(self):
        self._inner_namespace_end()
        self._outer_namespace_end()
        self.__include_guard_end()

    def __include_guard_start(self):
        self.str += f"#ifndef {self.guard}\n" \
                    f"#define {self.guard}\n\n"

    def __include_guard_end(self):
        self.str += f"#endif // {self.guard}\n"
