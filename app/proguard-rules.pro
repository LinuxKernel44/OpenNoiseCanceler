# Release builds currently ship with isMinifyEnabled = false (see app/build.gradle.kts
# for the reasoning), so this file has no effect yet. It is kept ready for when
# R8 shrinking/obfuscation is enabled after on-device validation.

# Keep JNI entry points: their signatures are matched by name from native code
# and must not be renamed or stripped.
-keepclasseswithmembers class com.opennoisecanceler.app.engine.NativeAudioEngine {
    native <methods>;
}
-keepclassmembers class com.opennoisecanceler.app.engine.NativeAudioEngine$* {
    *;
}
