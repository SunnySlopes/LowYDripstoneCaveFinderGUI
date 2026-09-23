plugins {
    id("java")
}

group = "LowYDripstoneCaveFinder"
version = "1.2.1"

repositories {
    mavenCentral()
}

dependencies {
}

tasks.test {
    useJUnitPlatform()
}