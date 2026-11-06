package org.ronnie.zarecomp;

import android.app.Activity;
import android.content.Intent;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.os.Build;
import android.graphics.Typeface;
import android.os.Bundle;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

/** Java-only host screen; remains independent from the SDL diagnostic process. */
public final class MainActivity extends Activity {
    private TextView status;
    private int launchCount;

    private void recordHostEvent(String event) {
        String line = System.currentTimeMillis() + " HOST_" + event + "\n";
        try (FileOutputStream out = new FileOutputStream(
                new File(getFilesDir(), "za_host_activity.log"), true)) {
            out.write(line.getBytes(StandardCharsets.UTF_8));
            out.getFD().sync();
        } catch (Exception error) {
            android.util.Log.e("ZA_HOST_DIAG", "Could not persist host lifecycle", error);
        }
    }

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        recordHostEvent("ON_CREATE");
        buildDiagnosticScreen();
        status.setText("Host Activity: created. Main process is alive.");
    }

    private void buildDiagnosticScreen() {
        int pad = (int) (getResources().getDisplayMetrics().density * 20);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER_HORIZONTAL);
        root.setPadding(pad, pad, pad, pad);
        root.setBackgroundColor(0xFF101827);

        TextView title = new TextView(this);
        title.setText("POKÉMON Z-A · ANDROID DIAGNOSTICS");
        title.setTextColor(0xFFFFFFFF);
        title.setTextSize(20);
        title.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        root.addView(title);

        TextView intro = new TextView(this);
        intro.setText("\nThis is a diagnostic host screen, not the game.\n"
                + "SDL runs in a separate Android process. SDL and Vulkan logs are saved locally.");
        intro.setTextColor(0xFFD7E0EE);
        intro.setTextSize(15);
        root.addView(intro);

        status = new TextView(this);
        status.setTextColor(0xFF8FE3A5);
        status.setTextSize(12);
        status.setTypeface(Typeface.MONOSPACE);
        status.setTextIsSelectable(true);
        status.setGravity(Gravity.TOP | Gravity.START);
        status.setPadding(0, pad, 0, pad);
        root.addView(status);

        Button nativeButton = new Button(this);
        nativeButton.setText("Check Android native probe");
        nativeButton.setOnClickListener(v -> {
            try {
                System.loadLibrary("za_probe");
                status.setText(nativeGetProbeInfo());
            } catch (Throwable error) {
                status.setText("NATIVE PROBE LOAD FAILED\n"
                        + error.getClass().getSimpleName() + ": " + error.getMessage());
            }
        });
        root.addView(nativeButton);

        Button sdlButton = new Button(this);
        sdlButton.setText("Launch SDL3 diagnostic");
        sdlButton.setOnClickListener(v -> {
            launchCount++;
            status.setText("SDL launch #" + launchCount + " requested.");
            try {
                startActivity(new Intent(this, SDLProbeActivity.class));
            } catch (Throwable error) {
                status.setText("SDL ACTIVITY LAUNCH FAILED\n"
                        + error.getClass().getSimpleName() + ": " + error.getMessage());
            }
        });
        root.addView(sdlButton);

        Button logButton = new Button(this);
        logButton.setText("Show combined SDL + Vulkan diagnostic log");
        logButton.setOnClickListener(v -> showSdlLog());
        root.addView(logButton);

        Button copyButton = new Button(this);
        copyButton.setText("COPY FULL DIAGNOSTICS");
        copyButton.setOnClickListener(v -> copyFullDiagnostics());
        root.addView(copyButton);

        ScrollView scroll = new ScrollView(this);
        scroll.addView(root);
        setContentView(scroll);
    }

    /**
     * Copies the complete, untruncated diagnostic report to the Android clipboard.
     * The log is app-private and may contain output from the separate SDL process.
     */
    private void copyFullDiagnostics() {
        try {
            StringBuilder report = new StringBuilder(64 * 1024);
            report.append("POKEMON Z-A ANDROID DIAGNOSTICS\n");
            report.append("Captured: ").append(java.time.Instant.now()).append('\n');
            report.append("Device: ").append(Build.MANUFACTURER).append(' ')
                    .append(Build.MODEL).append('\n');
            report.append("Android: ").append(Build.VERSION.RELEASE)
                    .append(" (SDK ").append(Build.VERSION.SDK_INT).append(")\n");
            report.append("Supported ABIs: ");
            if (Build.SUPPORTED_ABIS != null) {
                for (int i = 0; i < Build.SUPPORTED_ABIS.length; i++) {
                    if (i > 0) report.append(", ");
                    report.append(Build.SUPPORTED_ABIS[i]);
                }
            }
            report.append("\nApp files: ").append(getFilesDir().getAbsolutePath()).append("\n");

            try {
                System.loadLibrary("za_probe");
                report.append("\n=== NATIVE PROBE INFO ===\n")
                        .append(nativeGetProbeInfo()).append('\n');
            } catch (Throwable error) {
                report.append("\n=== NATIVE PROBE INFO UNAVAILABLE ===\n")
                        .append(error.getClass().getSimpleName()).append(": ")
                        .append(error.getMessage()).append('\n');
            }

            File logFile = new File(getFilesDir(), "za_sdl_diagnostics.log");
            report.append("\nManifest runtime permissions: none (no uses-permission declarations).\n");
            report.append("\n=== MAIN ACTIVITY LIFECYCLE ===\n");
            File hostLog = new File(getFilesDir(), "za_host_activity.log");
            if (hostLog.isFile()) {
                report.append(new String(Files.readAllBytes(hostLog.toPath()), StandardCharsets.UTF_8));
            } else {
                report.append("No host Activity lifecycle log yet.\n");
            }

            report.append("\n=== COMPLETE SDL / VULKAN / GUEST MEMORY LOG ===\n");
            if (logFile.isFile()) {
                byte[] bytes = Files.readAllBytes(logFile.toPath());
                report.append(new String(bytes, StandardCharsets.UTF_8));
                report.append("\n=== END LOG (").append(bytes.length)
                        .append(" bytes) ===\n");
            } else {
                report.append("No SDL diagnostic log found. Launch SDL3 diagnostic first.\n");
            }

            ClipboardManager clipboard =
                    (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
            if (clipboard == null) {
                status.setText("COPY FAILED: Android clipboard service unavailable.");
                return;
            }
            clipboard.setPrimaryClip(ClipData.newPlainText(
                    "Pokémon Z-A full diagnostics", report.toString()));
            status.setText("FULL DIAGNOSTICS COPIED\n"
                    + report.length() + " characters copied. Paste the report here in ChatGPT.");
        } catch (Exception error) {
            status.setText("FULL DIAGNOSTICS COPY FAILED\n"
                    + error.getClass().getSimpleName() + ": " + error.getMessage());
        }
    }

    private void showSdlLog() {
        try {
            File logFile = new File(getFilesDir(), "za_sdl_diagnostics.log");
            if (!logFile.exists()) {
                status.setText("No diagnostic log found yet. Launch SDL3 diagnostic first.");
                return;
            }
            String log = new String(Files.readAllBytes(logFile.toPath()), StandardCharsets.UTF_8);
            status.setText(log.length() > 30000 ? log.substring(log.length() - 30000) : log);
        } catch (Exception error) {
            status.setText("LOG READ FAILED\n" + error.getClass().getSimpleName()
                    + ": " + error.getMessage());
        }
    }

    @Override protected void onStart() {
        super.onStart();
        recordHostEvent("ON_START");
    }

    @Override protected void onResume() {
        super.onResume();
        recordHostEvent("ON_RESUME");
    }

    @Override protected void onPause() {
        recordHostEvent("ON_PAUSE");
        super.onPause();
    }

    @Override protected void onStop() {
        recordHostEvent("ON_STOP");
        super.onStop();
    }

    @Override protected void onDestroy() {
        recordHostEvent("ON_DESTROY");
        super.onDestroy();
    }

    private static native String nativeGetProbeInfo();
}
