package org.ronnie.zarecomp;

import android.graphics.Color;
import android.os.Bundle;
import android.util.Log;
import android.view.Gravity;
import android.widget.Button;
import android.widget.RelativeLayout;
import org.libsdl.app.SDLActivity;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;

/** SDL diagnostic with an always-visible in-app exit control. */
public final class SDLProbeActivity extends SDLActivity {
    private static final String TAG = "ZA_SDL_DIAG";
    private static final String LOG_FILE = "za_sdl_diagnostics.log";
    private Button backButton;

    private int dp(float value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

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

    private void addBackButton() {
        if (mLayout == null) {
            record("BACK_BUTTON result=UNAVAILABLE layout_missing");
            return;
        }

        backButton = new Button(this);
        backButton.setText("← BACK TO DIAGNOSTICS");
        backButton.setAllCaps(false);
        backButton.setTextSize(14);
        backButton.setTextColor(Color.WHITE);
        backButton.setGravity(Gravity.CENTER);
        backButton.setMinHeight(dp(48));
        backButton.setPadding(dp(18), dp(8), dp(18), dp(8));
        backButton.setBackgroundTintList(
                android.content.res.ColorStateList.valueOf(0xE6263854));
        backButton.setElevation(dp(10));
        backButton.setContentDescription("Back to Pokémon Z-A diagnostics screen");
        backButton.setOnClickListener(view -> {
            record("BACK_BUTTON result=TAPPED action=finish_sdl_activity");
            finish();
        });

        RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(
                RelativeLayout.LayoutParams.WRAP_CONTENT,
                RelativeLayout.LayoutParams.WRAP_CONTENT);
        params.addRule(RelativeLayout.ALIGN_PARENT_TOP);
        params.addRule(RelativeLayout.ALIGN_PARENT_START);
        params.setMargins(dp(14), dp(14), dp(8), dp(8));
        mLayout.addView(backButton, params);
        backButton.bringToFront();
        backButton.setVisibility(android.view.View.VISIBLE);
        record("BACK_BUTTON result=READY overlay=top_start");
    }

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        resetLog();
        record("ACTIVITY_ON_CREATE");
        addBackButton();
    }
    @Override protected void onStart() {
        super.onStart();
        record("ACTIVITY_ON_START");
    }
    @Override protected void onResume() {
        super.onResume();
        record("ACTIVITY_ON_RESUME");
        if (backButton != null) {
            backButton.post(backButton::bringToFront);
        }
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
        if (hasFocus && backButton != null) {
            backButton.post(backButton::bringToFront);
        }
    }

    @Override public void onBackPressed() {
        record("ANDROID_BACK_PRESSED action=finish_sdl_activity");
        finish();
    }
}
