package com.shockwave.pdfium;

import android.content.Context;
import android.util.Log;
import android.view.Surface;

import java.io.FileDescriptor;
import java.lang.reflect.Field;

public class PdfiumCore {
    private static final String TAG = PdfiumCore.class.getName();

    static{
        System.loadLibrary("jniPdfium");
    }

    private native long nativeOpenDocument(int fd);
    private native void nativeCloseDocument(long docPtr);
    private native int nativeGetPageCount(long docPtr);
    private native long nativeLoadPage(long docPtr, int pageIndex);
    private native long[] nativeLoadPages(long docPtr, int fromIndex, int toIndex);
    private native void nativeClosePage(long pagePtr);
    private native void nativeClosePages(long[] pagesPtr);
    private native int nativeGetPageWidthPixel(long pagePtr, int dpi);
    private native int nativeGetPageHeightPixel(long pagePtr, int dpi);
    //private native long nativeGetNativeWindow(Surface surface);
    //private native void nativeRenderPage(long pagePtr, long nativeWindowPtr);
    private native void nativeRenderPage(long pagePtr, Surface surface, int dpi,
                                         int startX, int startY,
                                         int drawSizeHor, int drawSizeVer);

    private static final Class FD_CLASS = FileDescriptor.class;
    private static final String FD_FIELD_NAME = "descriptor";
    private static Field mFdField = null;

    private int mCurrentDpi;

    public PdfiumCore(Context ctx){
        mCurrentDpi = ctx.getResources().getDisplayMetrics().densityDpi;
    }

    public static int getNumFd(FileDescriptor fdObj){
        try{
            if(mFdField == null){
                mFdField = FD_CLASS.getDeclaredField(FD_FIELD_NAME);
                mFdField.setAccessible(true);
            }

            return mFdField.getInt(fdObj);
        }catch(NoSuchFieldException e){
            e.printStackTrace();
            return -1;
        } catch (IllegalAccessException e) {
            e.printStackTrace();
            return -1;
        }
    }

    public PdfDocument newDocument(FileDescriptor fd){
        PdfDocument document = new PdfDocument();

        document.mNativeDocPtr = nativeOpenDocument(getNumFd(fd));
        if(document.mNativeDocPtr <= 0) Log.e(TAG, "Open document failed");

        return document;
    }
    public int getPageCount(PdfDocument doc){
        synchronized (doc.Lock){
            return nativeGetPageCount(doc.mNativeDocPtr);
        }
    }

    public long openPage(PdfDocument doc, int pageIndex){
        synchronized (doc.Lock){
            long pagePtr = nativeLoadPage(doc.mNativeDocPtr, pageIndex);
            doc.mNativePagesPtr.put(pageIndex, pagePtr);
            return pagePtr;
        }
    }
    public long[] openPage(PdfDocument doc, int fromIndex, int toIndex){
        synchronized (doc.Lock){
            long[] pagesPtr = nativeLoadPages(doc.mNativeDocPtr, fromIndex, toIndex);
            int pageIndex = fromIndex;
            for(long page : pagesPtr){
                if(pageIndex > toIndex) break;
                doc.mNativePagesPtr.put(pageIndex, page);
                pageIndex++;
            }

            return pagesPtr;
        }
    }
    public int getPageWidth(PdfDocument doc, int index){
        synchronized (doc.Lock){
            Long pagePtr;
            if( (pagePtr = doc.mNativePagesPtr.get(index)) != null ){
                return nativeGetPageWidthPixel(pagePtr, mCurrentDpi);
            }
            return 0;
        }
    }
    public int getPageHeight(PdfDocument doc, int index){
        synchronized (doc.Lock){
            Long pagePtr;
            if( (pagePtr = doc.mNativePagesPtr.get(index)) != null ){
                return nativeGetPageHeightPixel(pagePtr, mCurrentDpi);
            }
            return 0;
        }
    }

    public void renderPage(PdfDocument doc, Surface surface, int pageIndex,
                           int startX, int startY, int drawSizeX, int drawSizeY){
        synchronized (doc.Lock){
            try{
                //nativeRenderPage(doc.mNativePagesPtr.get(pageIndex), surface, mCurrentDpi);
                nativeRenderPage(doc.mNativePagesPtr.get(pageIndex), surface, mCurrentDpi,
                                    startX, startY, drawSizeX, drawSizeY);
            }catch(NullPointerException e){
                Log.e(TAG, "mContext may be null");
                e.printStackTrace();
            }catch(Exception e){
                Log.e(TAG, "Exception throw from native");
                e.printStackTrace();
            }
        }
    }

    public void closeDocument(PdfDocument doc){
        synchronized (doc.Lock){
            for(Integer index : doc.mNativePagesPtr.keySet()){
                nativeClosePage(doc.mNativePagesPtr.get(index));
            }
            doc.mNativePagesPtr.clear();

            nativeCloseDocument(doc.mNativeDocPtr);
        }
    }
}
