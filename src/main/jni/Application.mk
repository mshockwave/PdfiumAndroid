APP_STL := gnustl_static
APP_CPPFLAGS += -fexceptions

#For ANativeWindow support
APP_PLATFORM = android-9

APP_ABI :=  armeabi \
            armeabi-v7a \
            arm64-v8a \
            mips \
            x86 \
            x86_64