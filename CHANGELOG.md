## 2.0.2 (2025-09-27)
* Android 16 (16KB page size support)

## 1.9.2 (2025-03-15)
* Upgrade libfreetype to 2.13.3
  * It is a security update, see [here](https://nvd.nist.gov/vuln/detail/CVE-2025-27363) for more details.

## 1.9.1 (2025-01-06)
* Upgrade to latest [PDFium 133.0.6927.0](https://github.com/bblanchon/pdfium-binaries/releases/tag/chromium%2F6927)
    * Upgrade `include` folder
    * Upgrade `libmodpdfium.so` for `arm32`, `arm64`, `x86` and `x86_64`(`mips` binary not included)
    * Use new Pdfium API in `mainJNILib.cpp`
* Add `CMakeLists.txt` for building the PdfiumAndroid library
    * Example cmake command
    ```bash
    export ABI=arm64-v8a && \
        export NDK_ROOT=PATH/TO/NDK && \
        cmake -B builddir/${ANDROID_ABI}/ \
        -S . \
        -DCMAKE_BUILD_TYPE=Release \
        -DANDROID_NDK=${NDK_ROOT} \
        -DCMAKE_ANDROID_NDK=${NDK_ROOT} \
        -DCMAKE_SYSTEM_NAME=Android \
        -DCMAKE_ANDROID_ARCH_ABI=${ANDROID_ABI} \
        -DANDROID_ABI=${ANDROID_ABI} \
        -DANDROID_PLATFORM=android-26 \
        -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON
    ```
* Add [libpng v1.6.44](https://github.com/pnggroup/libpng/releases/tag/v1.6.44) and [libfreetype2 v2.10.0](https://download.savannah.gnu.org/releases/freetype/) binaries for building PdfiumAndroid library. 
    * Add `build.sh` script for building `libpng` and `libfreetype2`

## 1.9.0 (2018-06-29)
* Updated Pdfium library to 7.1.2_r36
* Changed `gnustl_static` to `c++_shared`
* Update Gradle plugins
* Update compile SDK and support library to 26
* Change minimum SDK to 14
* Add support for mips64

## 1.8.2 (2017-12-15)
* Merge pull request by [mcsong](https://github.com/mcsong) fixing potential NPE when getting links

## 1.8.1 (2017-11-15)
* Handle `PdfiumCore#getPageSize()` errors and return `Size(0, 0)`

## 1.8.0 (2017-11-11)
* Add method for reading links from given page
* Add method for mapping page coordinates to screen coordinates
* Add `PdfiumCore#getPageSize(...)` method, which does not require page to be opened
* Add `Size` and `SizeF` utility classes
* Add javadoc comments to `PdfiumCore`

## 1.7.1 (2017-10-28)
* Merge pull request by [Phaestion](https://github.com/Phaestion) which prevents `UnsatisfiedLinkError`

## 1.7.0 (2017-06-21)
* Add rendering bitmap in RGB 565 format, which reduces memory usage (about twice)

## 1.6.1 (2017-06-09)
* Fix bug from 1.6.0 - not embedded fonts was not rendered

## 1.6.0 (2017-03-22)
* Pdfium updated to newest version, from Android 7.1.1.
It should fix many rendering issues and (thanks to freetype support) fix problems with fonts.

## 1.5.0 (2016-11-18)
* Add method `PdfiumCore#newDocument(byte[])` for reading PDF documents from memory
* Cleanup AndroidManifest.xml to solve problems with manifest merger

## 1.4.0 (2016-07-12)
* merge pull request by [usef](https://github.com/usef) with added support for rendering annotations. Due to limitations of _Pdfium_, messages from comments cannot be read and are rendered only as speech balloons.

## 1.3.1 (2016-07-11)
* `PdfiumCore#newDocument()` may throw `PdfPasswordException` to help with recognition of password requirement or incorrect password.

## 1.3.0 (2016-07-10)
* added support for opening documents with password
* fixed bug with SIGSEV when closing document

## 1.2.0 (2016-07-06)
* `libmodpdfium` compiled with methods for retrieving bookmarks and metadata
* added `PdfiumCore#getDocumentMeta()` for retrieving document metadata
* added `PdfiumCore#getTableOfContents()` for reading whole tree of bookmarks
* comment out native rendering debug

## 1.1.0 (2016-06-27)
* fixed rendering multiple PDFs at the same time thanks to synchronization between instances
* compile Pdfium with SONAME `libmodpdfium` to prevent loading `libpdfium` from Lollipop and higher
* `newDocument()` requires `ParcelFileDescriptor` instead of `FileDescriptor` to keep file descriptor open
* changed method of loading PDFs, which should be more stable

## 1.0.3 (2016-06-17)
* probably fixed bug when pdf should open as normal but was throwing exception
* added much more descriptive exception messages

## 1.0.2 (2016-06-04)
* `newDocument()` throws IOException

## 1.0.1 (2016-06-04)
* fix loading `libpdfium` on devices with < Lollipop

## Initial changes
* Added method for rendering PDF page on bitmap

    ``` java
    void renderPageBitmap(PdfDocument doc, Bitmap bitmap, int pageIndex,
                          int startX, int startY, int drawSizeX, int drawSizeY);
    ```
* Added methods to get width and height of page in points (1/72") (like in `PdfRenderer.Page` class):
    * `int getPageWidthPoint(PdfDocument doc, int index);`
    * `int getPageHeightPoint(PdfDocument doc, int index);`
