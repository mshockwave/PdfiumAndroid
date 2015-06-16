#include "util.hpp"

extern "C" {
    #include <unistd.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <string.h>
}

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/bitmap.h>
#include <utils/Mutex.h>
using namespace android;


#include <fpdfview.h>
#include <fpdfdoc.h>
#include <fpdftext.h>


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


// Text Module API

JNI_FUNC(jint*, PdfiumCore, textLoadPage)(JNI_ARGS, jlong pagePtr){
	FPDF_PAGE page = reinterpret_cast<FPDF_PAGE>(pagePtr);
	return (jint*)FPDFText_LoadPage(page);
}

JNI_FUNC(void, PdfiumCore, textClosePage)(JNI_ARGS, jint textpage){
	FPDF_TEXTPAGE pTextPage = reinterpret_cast<FPDF_TEXTPAGE>(textpage);
	FPDFText_ClosePage(pTextPage);
}

JNI_FUNC(jint*, PdfiumCore, textFindStart)(JNI_ARGS, jint textpage, jstring findwhat, jlong flag, jint startindex){

    int length = env->GetStringLength(findwhat);
    const FPDF_WCHAR* wcFind = env->GetStringChars(findwhat, 0);

    FPDF_TEXTPAGE pTextPage = reinterpret_cast<FPDF_TEXTPAGE>(textpage);
    FPDF_SCHHANDLE searchHandle = NULL;
    LOGI("wcFind is %x %x %x %x",wcFind[0],wcFind[1],wcFind[2],wcFind[3]);

    searchHandle = FPDFText_FindStart(pTextPage,(FPDF_WCHAR*)wcFind, flag, startindex);

    if(searchHandle == NULL){
        LOGE("FPDFTextFindStart: FPDFTextFindStart did not return success");
    }

    return (jint*)searchHandle;
}

JNI_FUNC(jint, PdfiumCore, textFindNext)(JNI_ARGS, jint searchHandle){

    FPDF_SCHHANDLE pSearchHandle = reinterpret_cast<FPDF_SCHHANDLE>(searchHandle);
	FPDF_BOOL isMatch = 0;
	isMatch = FPDFText_FindNext(pSearchHandle);
	LOGD("FPDFText_FindNext Match is %x",isMatch);
	return isMatch;
}

// TODO: incomplete
JNI_FUNC(jstring, PdfiumCore, textGetText)(JNI_ARGS, jint textpage, jint nStart, jint nCount){

	FPDF_DWORD bufflen = 0;

	FPDF_TEXTPAGE pTextPage = reinterpret_cast<FPDF_TEXTPAGE>(textpage);

    //TODO: How to fix this ????
	FPDF_WCHAR* pBuff = new FPDF_WCHAR[bufflen+1];
	pBuff[bufflen] = 0;

	int ret = FPDFText_GetText(pTextPage, nStart, nCount, pBuff);

	if(ret == 0){
        LOGE("FPDFTextGetText: FPDFTextGetText did not return success");
    }

	return env->NewString(pBuff, bufflen);
}

JNI_FUNC(jint, PdfiumCore, textCountChars)(JNI_ARGS, jint textPage){

	FPDF_TEXTPAGE pTextPage = reinterpret_cast<FPDF_TEXTPAGE>(textPage);
	int count = 0;
	count = FPDFText_CountChars(pTextPage);
	return count;
}

JNI_FUNC(jint, PdfiumCore, textCountRects)(JNI_ARGS, jint textPage, jint start, jint count){

	FPDF_TEXTPAGE pTextPage = reinterpret_cast<FPDF_TEXTPAGE>(textPage);
	int rectCount = 0;
	rectCount = FPDFText_CountRects(pTextPage, start, count);
	return rectCount;
}


JNI_FUNC(jobject, PdfiumCore, textGetRect)(JNI_ARGS, jint textpage, jint index){

    jclass cls_r;
    double rectLeft, rectTop, rectRight, rectBottom;
    FPDF_TEXTPAGE pTextPage = reinterpret_cast<FPDF_TEXTPAGE>(textpage);

    FPDFText_GetRect(pTextPage, index, &rectLeft, &rectTop, &rectRight, &rectBottom);

    // get android RectF
    cls_r = env->FindClass((const char*)"android/graphics/RectF");
    if (cls_r == NULL){
        return NULL;
    }

    jobject obj = env->AllocObject(cls_r);
    jfieldID left = env->GetFieldID( cls_r, (const char*)"left", "F");
    jfieldID right = env->GetFieldID(cls_r, (const char*)"right", "F");
    jfieldID top = env->GetFieldID(cls_r, (const char*)"top", "F");
    jfieldID bottom = env->GetFieldID( cls_r, (const char*)"bottom", "F");

    env->SetFloatField( obj, left, rectLeft);
    env->SetFloatField( obj, right, rectRight);
    env->SetFloatField( obj, top, rectTop);
    env->SetFloatField( obj, bottom, rectBottom);
    return obj;
}

JNI_FUNC(jint, PdfiumCore, textGetSchResultIndex)(JNI_ARGS, jint searchHandle){
	FPDF_SCHHANDLE pSearchHandle = reinterpret_cast<FPDF_SCHHANDLE>(searchHandle);
	int index = -1;
	index = FPDFText_GetSchResultIndex(pSearchHandle);
	if(index == -1){
        LOGE("FPDFTextGetSchResultIndex: FPDFTextGetSchResultIndex did not return success");
    }
	return index;
}

JNI_FUNC(jint, PdfiumCore, textGetSchCount)(JNI_ARGS, jint searchHandle){
	FPDF_SCHHANDLE pSearchHandle = reinterpret_cast<FPDF_SCHHANDLE>(searchHandle);
	int count = -1;
	count = FPDFText_GetSchCount(pSearchHandle);
	if(count == -1){
        LOGE("FPDFTextGetSchCount: FPDFTextGetSchCount did not return success");
    }
	return count;
}

JNI_FUNC(void, PdfiumCore, textFindClose)(JNI_ARGS, jint searchHandle){
	FPDF_SCHHANDLE pSearchHandle = reinterpret_cast<FPDF_SCHHANDLE>(searchHandle);
	FPDFText_FindClose(pSearchHandle);
}



}//extern C
