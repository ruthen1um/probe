from conan import ConanFile
from conan.tools.cmake import cmake_layout


class ExampleRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.tool_requires("cmake/4.3.2")

        self.requires("libmagic/5.45")
        self.requires("nlohmann_json/3.12.0")

    def layout(self):
        cmake_layout(self)
