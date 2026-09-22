import java.util.Properties

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

// --- Release signing -------------------------------------------------------
// Signing credentials live only in a local, git-ignored keystore.properties
// file (see keystore.properties.example). They are never committed and never
// printed to build logs.
val keystorePropertiesFile = rootProject.file("keystore.properties")
val keystoreProperties = Properties()
val hasReleaseKeystore = keystorePropertiesFile.exists()
if (hasReleaseKeystore) {
    keystoreProperties.load(keystorePropertiesFile.inputStream())
}

android {
    namespace = "com.opennoisecanceler.app"

    // compileSdk is intentionally higher than minSdk/targetSdk: modern
    // AndroidX/Compose artifacts declare a minCompileSdk that Gradle enforces
    // as a hard error, so compileSdk must satisfy the toolchain even though
    // this app only ever *targets* and *runs against* Android 12 (API 31)
    // behavior on the Galaxy S10e. compileSdk has no effect on runtime
    // behavior; targetSdk does, and that stays pinned to 31.
    compileSdk = 34

    // NDK version pinned explicitly so native builds are reproducible across
    // machines instead of silently picking up whichever NDK AGP defaults to.
    ndkVersion = "27.0.12077973"

    defaultConfig {
        applicationId = "com.opennoisecanceler.app"
        // Pinned to Android 12 (API 31) to match the Galaxy S10e exactly and
        // to avoid API 33+ notification-permission and API 34+
        // foreground-service-type enforcement changes that do not apply to
        // the target device.
        minSdk = 31
        targetSdk = 31
        versionCode = 1
        versionName = "0.1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        // Only build for arm64-v8a: the Snapdragon 855 in the target device
        // is a 64-bit ARM SoC, and narrowing the ABI set keeps native build
        // times down and avoids shipping/testing code paths that can never
        // run on the target hardware.
        ndk {
            abiFilters += "arm64-v8a"
        }

        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++20", "-Wall", "-Wextra")
                arguments += listOf("-DANDROID_STL=c++_shared")
            }
        }
    }

    signingConfigs {
        if (hasReleaseKeystore) {
            create("release") {
                storeFile = file(keystoreProperties.getProperty("storeFile"))
                storePassword = keystoreProperties.getProperty("storePassword")
                keyAlias = keystoreProperties.getProperty("keyAlias")
                keyPassword = keystoreProperties.getProperty("keyPassword")
            }
        }
    }

    buildTypes {
        debug {
            isDebuggable = true
            applicationIdSuffix = ".debug"
        }
        release {
            isMinifyEnabled = false
            isShrinkResources = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            if (hasReleaseKeystore) {
                signingConfig = signingConfigs.getByName("release")
            }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildFeatures {
        compose = true
        prefab = true
        buildConfig = true
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    packaging {
        resources {
            excludes += "/META-INF/{AL2.0,LGPL2.1}"
        }
    }

    testOptions {
        unitTests {
            isIncludeAndroidResources = false
            isReturnDefaultValues = true
        }
    }

    lint {
        // ExpiredTargetSdkVersion is a Google Play *store policy* check, not
        // a correctness or security issue: this app is never published to
        // Play (sideloaded via GitHub Releases), and targetSdk is pinned to
        // 31 deliberately to match Android 12 behavior on the single
        // supported device (see the comment on targetSdk above).
        disable += "ExpiredTargetSdkVersion"
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.activity:activity-compose:1.9.2")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.8.6")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.8.6")
    implementation("androidx.lifecycle:lifecycle-service:2.8.6")
    implementation("androidx.lifecycle:lifecycle-viewmodel-ktx:2.8.6")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.8.6")

    val composeBom = platform("androidx.compose:compose-bom:2024.09.03")
    implementation(composeBom)
    androidTestImplementation(composeBom)
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-graphics")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.material:material-icons-core")
    debugImplementation("androidx.compose.ui:ui-tooling")

    implementation("androidx.datastore:datastore-preferences:1.1.1")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.8.1")

    // Prebuilt Oboe AAR (headers + per-ABI .so via prefab). No need to vendor
    // or build Oboe from source.
    implementation("com.google.oboe:oboe:1.9.3")

    testImplementation("junit:junit:4.13.2")
    testImplementation("org.jetbrains.kotlinx:kotlinx-coroutines-test:1.8.1")
    androidTestImplementation("androidx.test.ext:junit:1.2.1")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.6.1")
}
