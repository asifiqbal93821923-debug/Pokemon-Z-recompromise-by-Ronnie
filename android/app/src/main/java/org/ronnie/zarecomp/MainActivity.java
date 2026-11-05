package org.ronnie.zarecomp;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Typeface;
import android.os.Bundle;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

/**
 * Java-only host screen. It stays in the main process while SDL runs in a
 * separate process, so an SDL/native crash returns here instead of closing
 * the whole diagnostic app.
 */
public final class MainActivity extends Activity {
    private TextView status;
    private int launchCount;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
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
                + "The SDL test now runs in a separate Android process. "
                + "If SDL crashes, this screen should remain available.");
        intro.setTextColor(0xFFD7E0EE);
        intro.setTextSize(15);
        root.addView(intro);

        status = new TextView(this);
        status.setTextColor(0xFF8FE3A5);
        status.setTextSize(15);
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
            status.setText("SDL launch #" + launchCount
                    + " requested. If SDL exits/crashes, return here and read this status.");
            try {
                startActivity(new Intent(this, SDLProbeActivity.class));
            } catch (Throwable error) {
                status.setText("SDL ACTIVITY LAUNCH FAILED\n"
                        + error.getClass().getSimpleName() + ": " + error.getMessage());
            }
        });
        root.addView(sdlButton);

        ScrollView scroll = new ScrollView(this);
        scroll.addView(root);
        setContentView(scroll);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (status != null && launchCount > 0) {
            status.setText("Returned to host after SDL launch #" + launchCount
                    + ". Host process is still alive.");
        }
    }

    private static native String nativeGetProbeInfo();
}
