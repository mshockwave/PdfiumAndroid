package com.shockwave.pdfium;

import java.util.HashMap;
import java.util.Map;

public class PdfDocument {

    /*package*/ PdfDocument(){}

    /*package*/ long mNativeDocPtr;

    /*package*/ final Map<Integer, Long> mNativePagesPtr = new HashMap<>();
}
