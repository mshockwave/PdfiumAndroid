# PdfiumAndroid - SDK 36 / Android 16 Compatible

This repository is a fork of [meganz/PdfiumAndroid](https://github.com/meganz/PdfiumAndroid), which itself is based on [barteksc/PdfiumAndroid](https://github.com/barteksc/PdfiumAndroid).

## What's New in This Fork

This fork is updated to **SDK 35 / NDK 28** and delivers a working AAR compatible with **Android 15 (16KB page size support)**.

**Version 2.0.2** - Significant updates to the build layer and compatibility improvements warrant a major version bump from the original 1.9.0.

### Key Updates:
- **Android SDK 36** and **NDK 28** compatibility
- **Android Gradle Plugin 8.13.0** support
- **Java 17** requirement (configured via `gradle.properties`)
- **BuildConfig generation** fixed for library modules
- **Native library packaging** corrected in AAR
- **16KB page size support** for Android 15
- **Windows Git Bash** compatible build script

## Requirements

- **Java 17+** (required for AGP 8.13.0+)
- **Android NDK 28.2.13676358**
- **Git Bash** (for Windows users)
- **Ninja build tool** - https://github.com/ninja-build/ninja/releases

## Building the Library

### 1. Build Native Libraries

Remark: you might need to update the `NDK_ROOT` environment variable in the `build.sh` script to point to your NDK (preferrably v28+)

```bash
# Run the build script to compile native libraries
./build.sh

# Optional: Build specific components
./build.sh --build-png --build-freetype

```

### 2. Build AAR

Remark: you might need to update the 'javaHome' environment variable in the `gradle.properties` file to point to your Java 17 installation

```bash
# Build debug AAR
./gradlew assembleDebug

# Build release AAR  
./gradlew assembleRelease

# Output: build/outputs/aar/PdfiumAndroid-2.0.0-release.aar
```

## Using in Your Project

### Method 1: Local AAR
1. Copy `PdfiumAndroid-2.0.2-release.aar` to your app's `libs/` folder
2. Add to your app's `build.gradle`:
```groovy
dependencies {
    implementation files('libs/PdfiumAndroid-2.0.2-release.aar')
    implementation 'androidx.core:core:1.16.0'
}
```

### Method 2: JitPack (if published)
```groovy
repositories {
    maven { url 'https://jitpack.io' }
}

dependencies {
    implementation 'com.github.aiuspaktyn:PdfiumAndroid:2.0.2'
}
```

## Usage Example

```java
import com.shockwave.pdfium.PdfiumCore;
import com.shockwave.pdfium.PdfDocument;

// Initialize
PdfiumCore pdfiumCore = new PdfiumCore(context);

// Open PDF
PdfDocument pdfDocument = pdfiumCore.newDocument(parcelFileDescriptor);

// Use the library...
```

## Changes Made

### Build System Updates
- Updated `build.gradle` for AGP 8.13.0 compatibility
- Added `buildFeatures { buildConfig = true }`
- Added `buildConfigField` for `VERSION_NAME`
- Fixed `jniLibs.srcDir` path for native libraries
- Configured Java 17 in `gradle.properties`

### Native Library Fixes
- Updated `build.sh` to use ninja (for windows)
- Updated `build.sh` to copy `libjniPdfium.so` to correct location
- Fixed AAR packaging to include all required `.so` files
- Ensured compatibility with NDK 28

### Android 15 Compatibility
- Added 16KB page size support via `ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON`
- Updated manifest and build configuration

## Original Credits

- Original work: [barteksc/PdfiumAndroid](https://github.com/barteksc/PdfiumAndroid)
- 16KB page size foundation: [meganz/PdfiumAndroid](https://github.com/meganz/PdfiumAndroid)
- Upgrade to [PDFium 133.0.6927.0](https://github.com/bblanchon/pdfium-binaries/releases/tag/chromium%2F6927)
- Add a `CMakeLists.txt` for building PdfiumAndroid `.so` file.
- Update [libpng v1.6.44](https://github.com/pnggroup/libpng/releases/tag/v1.6.44) and [libfreetype2 v2.10.0](https://download.savannah.gnu.org/releases/freetype/) binaries for building PdfiumAndroid library. 

## License

Same as original project - check the LICENSE file.
