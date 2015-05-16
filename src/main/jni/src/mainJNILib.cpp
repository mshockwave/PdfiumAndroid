#include "util.hpp"

extern "C" {
    #include <unistd.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
}

#include <android/native_window.h>
#include <android/native_window_jni.h>

#include <fpdfview.h>


static int sLibraryReferenceCount = 0;

static void initLibraryIfNeed(){
    if(sLibraryReferenceCount == 0){
        FPDF_InitLibrary(NULL);
    }
    sLibraryReferenceCount++;
}

static void destroyLibraryIfNeed(){
    sLibraryReferenceCount--;
    if(sLibraryReferenceCount == 0){
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
    void setFile(int fd, void *buffer){
        fileFd = fd;
        fileMappedBuffer = buffer;
        fileSize = sizeof(fileMappedBuffer);
    }
    void* getFileMap() { return fileMappedBuffer; }

    DocumentFile() :  pdfDocument(NULL),
                      fileMappedBuffer(NULL) {}
    ~DocumentFile();
};
DocumentFile::~DocumentFile(){
    if(pdfDocument != NULL){
        FPDF_CloseDocument(pdfDocument);
    }

    if(fileMappedBuffer != NULL){
        munmap(fileMappedBuffer, fileSize);
        close(fileFd);
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

JNI_FUNC(jlong, NativeHandler, nativeOpenDocument)(JNI_ARGS, jint fd){

    size_t fileLength = (size_t)getFileSize(fd);
    if(fileLength <= 0) return -1;

    DocumentFile *docFile = new DocumentFile();

    try{
        void *map;
        if( (map = mmap( docFile->getFileMap(), fileLength, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0 )) == NULL){
            throw "Error mapping file";
        }
        docFile->setFile(fd, map);

        if( (docFile->pdfDocument = FPDF_LoadMemDocument( reinterpret_cast<const void*>(docFile->getFileMap()),
                                                          (int)docFile->fileSize, NULL)) == NULL) {
            throw "Error loading document from file map";
        }

        return reinterpret_cast<jlong>(docFile);

    }catch(const char* msg){
        delete docFile;
        LOGE("%s", msg);

        return -1;
    }
}

JNI_FUNC(void, NativeHandler, nativeCloseDocument)(JNI_ARGS, jlong documentPtr){
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

JNI_FUNC(jlong, NativeHandler, nativeLoadPage)(JNI_ARGS, jlong docPtr, jint pageIndex){
    DocumentFile *doc = reinterpret_cast<DocumentFile*>(docPtr);
    return loadPageInternal(doc, (int)pageIndex);
}
JNI_FUNC(jlongArray, NativeHandler, nativeLoadPages)(JNI_ARGS, jlong docPtr, jint fromIndex, jint toIndex){
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

JNI_FUNC(void, NativeHandler, nativeClosePage)(JNI_ARGS, jlong pagePtr){ closePageInternal(pagePtr); }
JNI_FUNC(void, NativeHandler, nativeClosePages)(JNI_ARGS, jlongArray pagesPtr){
    int length = (int)(env -> GetArrayLength(pagesPtr));
    jlong *pages = env -> GetLongArrayElements(pagesPtr, NULL);

    int i;
    for(i = 0; i < length; i++){ closePageInternal(pages[i]); }
}

JNI_FUNC(jlong, NativeHandler, nativeGetNativeWindow)(JNI_ARGS, jobject objSurface){
    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, objSurface);
    ANativeWindow_acquire(nativeWindow);
    return reinterpret_cast<jlong>(nativeWindow);
}

static void renderPageInternal( FPDF_PAGE page,
                                ANativeWindow_Buffer *windowBuffer,
                                int startX, int startY,
                                int sizeHorizontal, int sizeVertical ){

    FPDF_BITMAP pdfBitmap = FPDFBitmap_CreateEx( sizeHorizontal, sizeVertical,
                                                 4, windowBuffer->bits, (int)windowBuffer->stride);

    FPDF_RenderPageBitmap( pdfBitmap, page,
                           startX, startY,
                           sizeHorizontal, sizeVertical,
                           0, FPDF_REVERSE_BYTE_ORDER );
}

JNI_FUNC(void, NativeHandler, nativeRenderPage)(JNI_ARGS, jlong pagePtr, jlong nativeWindowPtr){
    FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);
    ANativeWindow *nativeWindow = reinterpret_cast<ANativeWindow*>(nativeWindowPtr);

    if(page == NULL || nativeWindow == NULL){
        LOGE("Render page pointers invalid");
        return;
    }

    ANativeWindow_Buffer buffer;
    if( ANativeWindow_lock(nativeWindow, &buffer, NULL) != 0 ){
        LOGE("Locking native window failed");
        return;
    }

    int height = (int)buffer.height;
    int width = (int)buffer.width;

    renderPageInternal(page, &buffer, 0, 0, width, height); //Render the whole page

    ANativeWindow_unlockAndPost(nativeWindow);
}
