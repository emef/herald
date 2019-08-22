from conans import ConanFile, CMake


class PrpcConan(ConanFile):
    name = "herald"
    version = "0.1"
    license = "public"
    author = "Matt Forbes matt.r.forbes@gmail.com"
    url = "https://github.com/emef/herald"
    description = "Lightweight, minimal, inter-process pubsub library"
    topics = ("pubsub", "ipc")
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False]}
    default_options = "shared=False"
    generators = "cmake"
    exports_sources = "herald/*"

    def build(self):
        cmake = CMake(self)
        cmake.configure(source_folder="herald")
        cmake.build()

    def package(self):
        self.copy("*.h", dst="include", src="herald/include")
        self.copy("*.lib", dst="lib", keep_path=False)
        self.copy("*.dll", dst="bin", keep_path=False)
        self.copy("*.dylib*", dst="lib", keep_path=False)
        self.copy("*.so", dst="lib", keep_path=False)
        self.copy("*.a", dst="lib", keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["herald", "pthread", "rt"]
