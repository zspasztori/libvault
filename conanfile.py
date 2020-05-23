from conans import ConanFile, CMake, tools


class LibvaultConan(ConanFile):
    name = "libvault"
    version = "0.10.0"
    license = "MIT"
    author = "Aaron Bedra aaron@aaronbedra.com"
    url = "https://github.com/abedra/libvault"
    description = "A C++ Library for HashiCorp Vault"
    topics = ("hashicorp", "vault", "security")
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False]}
    default_options = {"shared": True}
    generators = "cmake"

    def source(self):
        self.run("git clone https://github.com/abedra/libvault.git")

    def build(self):
        cmake = CMake(self)
        cmake.configure(source_folder="libvault")
        cmake.build()

    def package(self):
        self.copy("include/VaultClient.h", dst="include", src="libvault", keep_path=False)
        self.copy("*libvault.lib", dst="lib", keep_path=False)
        self.copy("*.dll", dst="bin", keep_path=False)
        self.copy("*.so", dst="lib", keep_path=False)
        self.copy("*.dylib", dst="lib", keep_path=False)
        self.copy("*.a", dst="lib", keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["vault"]

