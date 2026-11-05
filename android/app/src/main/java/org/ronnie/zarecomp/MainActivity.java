package org.ronnie.zarecomp;

import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;

public final class MainActivity extends Activity {
    static {
        System.loadLibrary("za_android_probe");
    }

    private static native String nativeGetProbeInfo();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        TextView view = new TextView(this);
        view.setText(nativeGetProbeInfo()
                + "\n\nThis is only an Android NDK smoke-test shell."
                + "\nNo game engine, game data, SDL3 or Vulkan renderer is included yet.");
        view.setTextSize(16);
        view.setPadding(32, 48, 32, 32);
        setContentView(view);
    }
}
