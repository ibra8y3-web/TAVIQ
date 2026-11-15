package org.taviq.app;

import android.app.Activity;
import android.os.Bundle;
import android.graphics.Typeface;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

public final class MainActivity extends Activity {
    static { System.loadLibrary("taviq_android"); }
    private EditText source;
    private TextView output;
    private native String runTaviq(String program);

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(20, 24, 20, 20);
        root.setBackgroundColor(0xff101722);

        TextView heading = new TextView(this);
        heading.setText("TAVIQ Pocket Runtime");
        heading.setTextColor(0xffe9f0fa);
        heading.setTextSize(22);
        heading.setTypeface(Typeface.DEFAULT_BOLD);
        root.addView(heading);

        TextView subtitle = new TextView(this);
        subtitle.setText("Native C++ compiler · QVM v2");
        subtitle.setTextColor(0xffaab8cc);
        root.addView(subtitle);

        source = new EditText(this);
        source.setGravity(View.TEXT_ALIGNMENT_VIEW_START);
        source.setTypeface(Typeface.MONOSPACE);
        source.setTextColor(0xffe9f0fa);
        source.setHintTextColor(0xff75839a);
        source.setHint("Write a TAVIQ flow...");
        source.setMinLines(12);
        source.setText("flow main {\\n    var total: number = 0;\\n    var i: number = 0;\\n    while (i < 5) {\\n        i = i + 1;\\n        total = total + i;\\n    }\\n    display total;\\n    display \\"TAVIQ on Android\\";\\n}\\n");
        root.addView(source, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f));

        Button run = new Button(this);
        run.setText("Compile and Run");
        root.addView(run);

        TextView outputLabel = new TextView(this);
        outputLabel.setText("Output");
        outputLabel.setTextColor(0xffe9f0fa);
        outputLabel.setTypeface(Typeface.DEFAULT_BOLD);
        root.addView(outputLabel);

        output = new TextView(this);
        output.setTextColor(0xffb8f0cf);
        output.setTypeface(Typeface.MONOSPACE);
        output.setTextIsSelectable(true);
        ScrollView scroll = new ScrollView(this);
        scroll.addView(output);
        root.addView(scroll, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 160));

        run.setOnClickListener(v -> {
            run.setEnabled(false);
            output.setText("Compiling...");
            try {
                output.setText(runTaviq(source.getText().toString()));
            } catch (RuntimeException ex) {
                output.setText("Runtime error: " + ex.getMessage());
            } finally {
                run.setEnabled(true);
            }
        });
        setContentView(root);
    }
}
