plugins {
    id("java")
}

group = "LowYDripstoneCaveFinder"
version = "1.2.0"

repositories {
    mavenCentral()
}

dependencies {
}

tasks.test {
    useJUnitPlatform()
}