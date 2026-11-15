plugins {
    id("com.android.application")
}

android {
    namespace = "org.taviq.app"
    compileSdk = 36
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "org.taviq.app"
        minSdk = 23
        targetSdk = 36
        versionCode = 1
        versionName = "0.4.0"
        testInstrumentationRunner = "android.test.InstrumentationTestRunner"
        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                arguments += listOf("-DANDROID_PLATFORM=android-23", "-DANDROID_STL=c++_shared")
                cppFlags += listOf("-std=c++20")
                targets += listOf("taviq_android")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../../../CMakeLists.txt")
            version = "3.31.6"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildTypes {
        debug {
            applicationIdSuffix = ".debug"
            isDebuggable = true
        }
        release {
            isMinifyEnabled = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
}
