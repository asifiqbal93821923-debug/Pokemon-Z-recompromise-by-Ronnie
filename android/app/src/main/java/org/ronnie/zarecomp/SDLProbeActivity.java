package org.ronnie.zarecomp;

import android.os.Bundle;
import android.util.Log;
import org.libsdl.app.SDLActivity;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;

/** SDL diagnostic runs in its own process; lifecycle and native logs survive its exit. */
public final class SDLProbeActivity extends SDLActivity {
    private static final String TAG = "ZA_SDL_DIAG";
    private static final String LOG_FILE = "za_sdl_diagnostics.log";

    private void resetLog() {
        try (FileOutputStream out = new FileOutputStream(
                new File(getFilesDir(), LOG_FILE), false)) {
            out.write(("DIAGNOSTIC_SESSION_START " + System.currentTimeMillis() + "\n")
                    .getBytes(StandardCharsets.UTF_8));
            out.getFD().sync();
        } catch (Exception error) {
            Log.e(TAG, "Could not reset diagnostic log", error);
        }
    }

    private void record(String event) {
        String line = System.currentTimeMillis() + " " + event + "\n";
        Log.i(TAG, event);
        try (FileOutputStream out = new FileOutputStream(
                new File(getFilesDir(), LOG_FILE), true)) {
            out.write(line.getBytes(StandardCharsets.UTF_8));
            out.getFD().sync();
        } catch (Exception error) {
            Log.e(TAG, "Could not persist lifecycle log", error);
        }
    }

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        resetLog();
        record("ACTIVITY_ON_CREATE");
    }
    @Override protected void onStart() {
        super.onStart();
        record("ACTIVITY_ON_START");
    }
    @Override protected void onResume() {
        super.onResume();
        record("ACTIVITY_ON_RESUME");
    }
    @Override protected void onPause() {
        record("ACTIVITY_ON_PAUSE");
        super.onPause();
    }
    @Override protected void onStop() {
        super.onStop();
        record("ACTIVITY_ON_STOP");
    }
    @Override protected void onDestroy() {
        record("ACTIVITY_ON_DESTROY");
        super.onDestroy();
    }
    @Override public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        record("WINDOW_FOCUS=" + hasFocus);
    }
}
