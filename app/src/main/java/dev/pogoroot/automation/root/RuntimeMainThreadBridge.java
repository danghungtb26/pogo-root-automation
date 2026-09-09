package dev.pogoroot.automation.root;

import android.os.Handler;
import android.os.Looper;

/** Posts a tokenized callback onto the Android/Unity main thread for Zygisk. */
public final class RuntimeMainThreadBridge {
    private static final Handler MAIN_HANDLER = new Handler(Looper.getMainLooper());

    private RuntimeMainThreadBridge() {
    }

    public static boolean post(long token) {
        return MAIN_HANDLER.post(() -> nativeRun(token));
    }

    private static native void nativeRun(long token);
}
