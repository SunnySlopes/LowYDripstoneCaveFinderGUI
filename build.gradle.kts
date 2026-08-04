plugins {
    id("java")
}

group = "LowYDripstoneCaveFinder"
version = "1.1.0"

repositories {
    mavenCentral()
}

dependencies {
}

tasks.test {
    useJUnitPlatform()
}