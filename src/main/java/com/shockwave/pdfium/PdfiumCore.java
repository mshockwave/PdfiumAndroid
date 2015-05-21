package com.shockwave.pdfium;

import android.content.Context;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Surface;
import android.view.WindowManager;

import java.io.FileDescriptor;
import java.lang.reflect.Field;
import java.util.List;

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
    //private native long nativeGetNativeWindow(Surface surface);
    //private native void nativeRenderPage(long pagePtr, long nativeWindowPtr);
    private native void nativeRenderPage(long pagePtr, Surface surface, int dpi);

    private static final Class FD_CLASS = FileDescriptor.class;
    private static final String FD_FIELD_NAME = "descriptor";
    private static Field mFdField = null;

    private Context mContext;

    public PdfiumCore(Context ctx){
        mContext = ctx;
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

    public void renderPage(PdfDocument doc, Surface surface, int pageIndex){
        synchronized (doc.Lock){
            try{
            /*Get real time density*/
                int dpi = mContext.getResources().getDisplayMetrics().densityDpi;
                nativeRenderPage(doc.mNativePagesPtr.get(pageIndex), surface, dpi);

            }catch(NullPointerException e){
                Log.e(TAG, "mContext may be null");
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
