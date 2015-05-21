package com.shockwave.pdfium;

import android.support.v4.util.ArrayMap;

import java.util.Map;

public class PdfDocument {
    public final Object Lock = new Object();

    /*package*/ PdfDocument(){}

    /*package*/ long mNativeDocPtr;

    /*package*/ final Map<Integer, Long> mNativePagesPtr = new ArrayMap<>();
    public boolean hasPage(int index){ return mNativePagesPtr.containsKey(index); }
}
