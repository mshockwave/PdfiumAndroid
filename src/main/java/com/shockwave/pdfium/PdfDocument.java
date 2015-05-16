package com.shockwave.pdfium;

import java.util.HashMap;
import java.util.Map;

public class PdfDocument {

    /*package*/ PdfDocument(){}

    /*package*/ long mNativeDocPtr;

    /*package*/ Map<Integer, Long> mNativePagesPtr = new HashMap<>();

    /*package*/ long mNativeWindowPtr;
}
