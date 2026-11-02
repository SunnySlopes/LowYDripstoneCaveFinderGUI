rootProject.name = "LowYDripstoneCaveFinder"

// Native search core. LowYDripstoneCaveFinderBase owns the C++ engine, the
// JNI library, the standalone CLI and its nested cubiomes submodule; the main
// module only consumes its artifacts. See the contract comment in build.gradle.
include("lowydripstonecavefinderbase")
project(":lowydripstonecavefinderbase").projectDir = file("LowYDripstoneCaveFinderBase")
