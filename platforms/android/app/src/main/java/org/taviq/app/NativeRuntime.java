package org.taviq.app;

public final class NativeRuntime {
    static {
        System.loadLibrary("taviq_android");
    }

    private NativeRuntime() {}

    public static native String run(String source);
}
