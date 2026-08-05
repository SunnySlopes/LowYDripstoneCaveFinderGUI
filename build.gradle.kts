plugins {
    id("java")
}

group = "LowYDripstoneCaveFinder"
version = "1.1.2"

repositories {
    mavenCentral()
}

dependencies {
}

tasks.test {
    useJUnitPlatform()
}