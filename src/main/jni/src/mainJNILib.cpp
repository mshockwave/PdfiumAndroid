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

JNI_FUNC(jint, PdfiumCore, nativeGetPageWidthPixel)(JNI_ARGS, jlong pagePtr, jint dpi){
    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);
    return (jint)(FPDF_GetPageWidth(page) * dpi / 72);
}
JNI_FUNC(jint, PdfiumCore, nativeGetPageHeightPixel)(JNI_ARGS, jlong pagePtr, jint dpi){
    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);
    return (jint)(FPDF_GetPageHeight(page) * dpi / 72);
}

static void renderPageInternal( FPDF_PAGE page,
                                ANativeWindow_Buffer *windowBuffer,
                                int startX, int startY,
                                int canvasHorSize, int canvasVerSize,
                                int drawSizeHor, int drawSizeVer){

    FPDF_BITMAP pdfBitmap = FPDFBitmap_CreateEx( canvasHorSize, canvasVerSize,
                                                 FPDFBitmap_BGRA,
                                                 windowBuffer->bits, (int)(windowBuffer->stride) * 4);

    LOGD("Start X: %d", startX);
    LOGD("Start Y: %d", startY);
    LOGD("Canvas Hor: %d", canvasHorSize);
    LOGD("Canvas Ver: %d", canvasVerSize);
    LOGD("Draw Hor: %d", drawSizeHor);
    LOGD("Draw Ver: %d", drawSizeVer);

    if(drawSizeHor < canvasHorSize || drawSizeVer < canvasVerSize){
        FPDFBitmap_FillRect( pdfBitmap, 0, 0, canvasHorSize, canvasVerSize,
                             0x84, 0x84, 0x84, 255); //Gray
    }

    int baseHorSize = (canvasHorSize < drawSizeHor)? canvasHorSize : drawSizeHor;
    int baseVerSize = (canvasVerSize < drawSizeVer)? canvasVerSize : drawSizeVer;
    int baseX = (startX < 0)? 0 : startX;
    int baseY = (startY < 0)? 0 : startY;
    FPDFBitmap_FillRect( pdfBitmap, baseX, baseY, baseHorSize, baseVerSize,
                         255, 255, 255, 255); //White

    FPDF_RenderPageBitmap( pdfBitmap, page,
                           startX, startY,
                           drawSizeHor, drawSizeVer,
                           0, FPDF_REVERSE_BYTE_ORDER );
}

JNI_FUNC(void, PdfiumCore, nativeRenderPage)(JNI_ARGS, jlong pagePtr, jobject objSurface,
                                             jint dpi, jint startX, jint startY,
                                             jint drawSizeHor, jint drawSizeVer){
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

    if(ANativeWindow_getFormat(nativeWindow) != WINDOW_FORMAT_RGBA_8888){
        LOGD("Set format to RGBA_8888");
        ANativeWindow_setBuffersGeometry( nativeWindow,
                                          ANativeWindow_getWidth(nativeWindow),
                                          ANativeWindow_getHeight(nativeWindow),
                                          WINDOW_FORMAT_RGBA_8888 );
    }

    ANativeWindow_Buffer buffer;
    int ret;
    if( (ret = ANativeWindow_lock(nativeWindow, &buffer, NULL)) != 0 ){
        LOGE("Locking native window failed: %s", strerror(ret * -1));
        return;
    }

    renderPageInternal(page, &buffer,
                       (int)startX, (int)startY,
                       buffer.width, buffer.height,
                       (int)drawSizeHor, (int)drawSizeVer);

    ANativeWindow_unlockAndPost(nativeWindow);
    ANativeWindow_release(nativeWindow);
}

}//extern C
