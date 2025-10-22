plugins {
    id("com.android.library")
    id("org.jetbrains.kotlin.android")
    id("org.mozilla.rust-android-gradle.rust-android")
    id("maven-publish")
}

object Library {
    const val groupId = "com.github.smoldot"
    const val artifactId = "smoldot-android"
    const val version = "2.0.39-beta02"
}

android {
    namespace = Library.groupId
    compileSdk = 36
    ndkVersion = "27.2.12479018"
    defaultConfig {
        minSdk = 26
        version = Library.version

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        ndk {
            abiFilters += listOf("armeabi-v7a", "arm64-v8a")
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }
    kotlinOptions {
        jvmTarget = "1.8"
        freeCompilerArgs += "-Xcontext-receivers"
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}

kotlin {
    explicitApiWarning()
}

cargo {
    module = "../../rust"
    libname = "libsmoldot_ffi"
    targets = listOf("arm", "arm64")
    profile = "release"
    prebuiltToolchains = true
}

publishing {
    publications {
        register<MavenPublication>("release") {
            groupId = Library.groupId
            artifactId = Library.artifactId
            version = Library.version

            afterEvaluate {
                from(components["release"])
            }
        }
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.17.0")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.2")
    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.3.0")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.7.0")
}

val buildSmoldotFFI: TaskProvider<Task> = tasks.register("buildSmoldotFFI", Task::class.java) {
    dependsOn("cargoBuild")

    doLast {
        val targets = listOf(
            "aarch64-linux-android" to "arm64-v8a",
            "armv7-linux-androideabi" to "armeabi-v7a",
        )

        targets.forEach { (source, destination) ->
            copy {
                from("../../../target/$source/release/libsmoldot_light_mobile.a")
                into("./src/main/cpp/libs/$destination/")
                rename { "libsmoldot_ffi.a" }
            }
        }
    }
}
