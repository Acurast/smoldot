plugins {
    alias(libs.plugins.android.library)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.rust.android)
    id("maven-publish")
}

object Library {
    const val groupId = "com.github.smoldot"
    const val artifactId = "smoldot-android"
    const val version = "3.2.0-beta01"
}

android {
    namespace = Library.groupId
    compileSdk = 37
    ndkVersion = "29.0.14206865"
    defaultConfig {
        minSdk = 26
        version = Library.version

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        consumerProguardFiles("consumer-rules.pro")

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
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlin {
        jvmToolchain(17)
        compilerOptions {
            freeCompilerArgs.addAll(
                "-Xcontext-parameters",
                "-Xexplicit-backing-fields",
            )
        }
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    sourceSets {
        getByName("androidTest") {
            assets.srcDir("../../../demo-chain-specs")
        }
    }
    publishing {
        singleVariant("release") {}
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
        register<MavenPublication>("maven") {
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
    implementation(libs.androidx.core.ktx)
    implementation(libs.kotlinx.coroutines.android)

    // Test

    testImplementation(libs.junit)
    testImplementation(libs.kotlinx.coroutines.test)

    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(libs.kotlinx.coroutines.test)
}

val buildSmoldotFFI: TaskProvider<Task> = tasks.register("ffiBuild", Task::class.java) {
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
