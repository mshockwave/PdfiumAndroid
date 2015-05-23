#include "util.hpp"

extern "C" {
    #include <unistd.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <string.h>
}

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <utils/Mutex.h>
using namespace android;

#include <fpdfview.h>


static Mutex sLibraryLock;

static int sLibraryReferenceCount = 0;

static void initLibraryIfNeed(){
    Mutex::Autolock lock(sLibraryLock);
    if(sLibraryReferenceCount == 0){
        LOGD("Init FPDF library");
        FPDF_InitLibrary(NULL);
    }
    sLibraryReferenceCount++;
}

static void destroyLibraryIfNeed(){
    Mutex::Autolock lock(sLibraryLock);
    sLibraryReferenceCount--;
    if(sLibraryReferenceCount == 0){
        LOGD("Destroy FPDF library");
        FPDF_DestroyLibrary();
    }
}

class DocumentFile {
    private:
    void *fileMappedBuffer;
    int fileFd;

    public:
    FPDF_DOCUMENT pdfDocument;
    size_t fileSize;
    void setFile(int fd, void *buffer, size_t fileLength){
        fileFd = fd;
        fileSize = fileLength;
        fileMappedBuffer = buffer;
        LOGD("File Size: %d", (int)fileSize);
    }
    void* getFileMap() { return fileMappedBuffer; }

    DocumentFile() :  pdfDocument(NULL),
                      fileMappedBuffer(NULL) { initLibraryIfNeed(); }
    ~DocumentFile();
};
DocumentFile::~DocumentFile(){
    if(pdfDocument != NULL){
        FPDF_CloseDocument(pdfDocument);
    }

    if(fileMappedBuffer != NULL){
        munmap(fileMappedBuffer, fileSize);
        //Leave the file closing work to Java
        //close(fileFd);
    }

    destroyLibraryIfNeed();
}

inline long getFileSize(int fd){
    struct stat file_state;

    if(fstat(fd, &file_state) >= 0){
        return (long)(file_state.st_size);
    }else{
        LOGE("Error getting file size");
        return 0;
    }
}

extern "C" { //For JNI support

JNI_FUNC(jlong, PdfiumCore, nativeOpenDocument)(JNI_ARGS, jint fd){

    size_t fileLength = (size_t)getFileSize(fd);
    if(fileLength <= 0) return -1;

    DocumentFile *docFile = new DocumentFile();

    try{
        void *map;
        if( (map = mmap( docFile->getFileMap(), fileLength, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0 )) == NULL){
            throw "Error mapping file";
        }
        docFile->setFile(fd, map, fileLength);

        if( (docFile->pdfDocument = FPDF_LoadMemDocument( reinterpret_cast<const void*>(docFile->getFileMap()),
                                                          (int)docFile->fileSize, NULL)) == NULL) {
            throw "Error loading document from file map";
        }

        return reinterpret_cast<jlong>(docFile);

    }catch(const char* msg){
        delete docFile;
        LOGE("%s", msg);
        LOGE("Last Error: %ld", FPDF_GetLastError());

        return -1;
    }
}

JNI_FUNC(jint, PdfiumCore, nativeGetPageCount)(JNI_ARGS, jlong documentPtr){
    DocumentFile *doc = reinterpret_cast<DocumentFile*>(documentPtr);
    return (jint)FPDF_GetPageCount(doc->pdfDocument);
}

JNI_FUNC(void, PdfiumCore, nativeCloseDocument)(JNI_ARGS, jlong documentPtr){
    DocumentFile *doc = reinterpret_cast<DocumentFile*>(documentPtr);
    delete doc;
}

static jlong loadPageInternal(DocumentFile *doc, int pageIndex){
    try{
        if(doc == NULL) throw "Get page document null";

        FPDF_DOCUMENT pdfDoc = doc->pdfDocument;
        if(pdfDoc != NULL){
            return reinterpret_cast<jlong>( FPDF_LoadPage(pdfDoc, pageIndex) );
        }else{
            throw "Get page pdf document null";
        }

    }catch(const char *msg){
        LOGE("%s", msg);
        return -1;
    }
}
static void closePageInternal(jlong pagePtr) { FPDF_ClosePage(reinterpret_cast<FPDF_PAGE>(pagePtr)); }

JNI_FUNC(jlong, PdfiumCore, nativeLoadPage)(JNI_ARGS, jlong docPtr, jint pageIndex){
    DocumentFile *doc = reinterpret_cast<DocumentFile*>(docPtr);
    return loadPageInternal(doc, (int)pageIndex);
}
JNI_FUNC(jlongArray, PdfiumCore, nativeLoadPages)(JNI_ARGS, jlong docPtr, jint fromIndex, jint toIndex){
    DocumentFile *doc = reinterpret_cast<DocumentFile*>(docPtr);

    if(toIndex < fromIndex) return NULL;
    jlong pages[ toIndex - fromIndex + 1 ];

    int i;
    for(i = 0; i <= (toIndex - fromIndex); i++){
        pages[i] = loadPageInternal(doc, (int)(i + fromIndex));
    }

    jlongArray javaPages = env -> NewLongArray( (jsize)(toIndex - fromIndex + 1) );
    env -> SetLongArrayRegion(javaPages, 0, (jsize)(toIndex - fromIndex + 1), (const jlong*)pages);

    return javaPages;
}

JNI_FUNC(void, PdfiumCore, nativeClosePage)(JNI_ARGS, jlong pagePtr){ closePageInternal(pagePtr); }
JNI_FUNC(void, PdfiumCore, nativeClosePages)(JNI_ARGS, jlongArray pagesPtr){
    int length = (int)(env -> GetArrayLength(pagesPtr));
    jlong *pages = env -> GetLongArrayElements(pagesPtr, NULL);

    int i;
    for(i = 0; i < length; i++){ closePageInternal(pages[i]); }
}

JNI_FUNC(jint, PdfiumCore, nativeGetPageWidth)(JNI_ARGS, jlong pagePtr){
    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);
    return (jint)FPDF_GetPageWidth(page);
}
JNI_FUNC(jint, PdfiumCore, nativeGetPageHeight)(JNI_ARGS, jlong pagePtr){
    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);
    return (jint)FPDF_GetPageHeight(page);
}

/*
JNI_FUNC(jlong, PdfiumCore, nativeGetNativeWindow)(JNI_ARGS, jobject objSurface){
    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, objSurface);
    //ANativeWindow_acquire(nativeWindow);
    return reinterpret_cast<jlong>(nativeWindow);
}
*/

static void renderPageInternal( FPDF_PAGE page,
                                ANativeWindow_Buffer *windowBuffer,
                                int dpi,
                                int startX, int startY,
                                int sizeHorizontal, int sizeVertical ){

    FPDF_BITMAP pdfBitmap = FPDFBitmap_CreateEx( sizeHorizontal, sizeVertical,
                                                 FPDFBitmap_BGRA,
                                                 windowBuffer->bits, (int)(windowBuffer->stride) * 4);

    LOGD("Page Width(point): %lf", FPDF_GetPageWidth(page));
    LOGD("Page Height(point): %lf", FPDF_GetPageHeight(page));
    LOGD("PDF Width: %d", FPDFBitmap_GetWidth(pdfBitmap));
    LOGD("PDF Height: %d", FPDFBitmap_GetHeight(pdfBitmap));
    LOGD("PDF Stride: %d", FPDFBitmap_GetStride(pdfBitmap));
    LOGD("Dpi: %d", dpi);

    int width = (int)(FPDF_GetPageWidth(page) * dpi / 72);
    int height = (int)(FPDF_GetPageHeight(page) * dpi / 72);

    FPDFBitmap_FillRect( pdfBitmap, 0, 0, sizeHorizontal, sizeVertical,
                         255, 255, 255, 255);

    FPDF_RenderPageBitmap( pdfBitmap, page,
                           startX, startY,
                           sizeHorizontal, sizeVertical,
                           0, FPDF_REVERSE_BYTE_ORDER );
}

JNI_FUNC(void, PdfiumCore, nativeRenderPage)(JNI_ARGS, jlong pagePtr, jobject objSurface, jint dpi){
    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, objSurface);
    if(nativeWindow == NULL){
        LOGE("native window pointer null");
        return;
    }
    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);

    if(page == NULL || nativeWindow == NULL){
        LOGE("Render page pointers invalid");
        return;
    }


    ANativeWindow_setBuffersGeometry( nativeWindow,
                                      ANativeWindow_getWidth(nativeWindow),
                                      ANativeWindow_getHeight(nativeWindow),
                                      WINDOW_FORMAT_RGBA_8888 );


    ANativeWindow_Buffer buffer;
    int ret;
    if( (ret = ANativeWindow_lock(nativeWindow, &buffer, NULL)) != 0 ){
        LOGE("Locking native window failed: %s", strerror(ret * -1));
        return;
    }

    int height = (int)buffer.height;
    int width = (int)buffer.width;
    LOGD("Height: %d", height);
    LOGD("Width: %d", width);
    LOGD("Stride: %d", (int)buffer.stride);
    LOGD("Buffer format: %d", (int)buffer.format);

    renderPageInternal(page, &buffer, dpi, 0, 0, width, height); //Render the whole page

    ANativeWindow_unlockAndPost(nativeWindow);
    ANativeWindow_release(nativeWindow);
}

}//extern C
