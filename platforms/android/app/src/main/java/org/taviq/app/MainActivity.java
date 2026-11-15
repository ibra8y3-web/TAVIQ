package org.taviq.app;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.graphics.Typeface;
import android.view.Gravity;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.ByteArrayOutputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class MainActivity extends Activity {
    private static final String PROJECT_FILE = "main.taviq";
    private static final int MAX_SOURCE_BYTES = 1024 * 1024;
    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private final Handler mainThread = new Handler(Looper.getMainLooper());
    private EditText source;
    private TextView output;
    private Button runButton;

    private boolean isArabic() {
        return "ar".equals(Locale.getDefault().getLanguage());
    }

    private String text(String english, String arabic) {
        return isArabic() ? arabic : english;
    }

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        final boolean rtl = isArabic();

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(20, 24, 20, 20);
        root.setBackgroundColor(0xff101722);

        TextView heading = new TextView(this);
        heading.setText("TAVIQ Pocket Runtime");
        heading.setTextColor(0xffe9f0fa);
        heading.setTextSize(22);
        heading.setGravity(rtl ? Gravity.RIGHT : Gravity.LEFT);
        heading.setTypeface(Typeface.DEFAULT_BOLD);
        root.addView(heading);

        TextView subtitle = new TextView(this);
        subtitle.setText(text("Native C++ compiler · QVM v2",
                              "مترجم C++ الأصلي · آلة QVM v2"));
        subtitle.setTextColor(0xffaab8cc);
        subtitle.setGravity(rtl ? Gravity.RIGHT : Gravity.LEFT);
        root.addView(subtitle);

        TextView project = new TextView(this);
        project.setText(text("Project: " + PROJECT_FILE,
                             "المشروع: " + PROJECT_FILE));
        project.setTextColor(0xffaab8cc);
        project.setPadding(0, 8, 0, 8);
        root.addView(project);

        source = new EditText(this);
        source.setGravity(Gravity.TOP | (rtl ? Gravity.RIGHT : Gravity.LEFT));
        source.setTextDirection(rtl ? android.view.View.TEXT_DIRECTION_RTL
                                   : android.view.View.TEXT_DIRECTION_LTR);
        source.setTypeface(Typeface.MONOSPACE);
        source.setTextColor(0xffe9f0fa);
        source.setHintTextColor(0xff75839a);
        source.setHint(text("Write a TAVIQ flow...", "اكتب برنامج TAVIQ..."));
        source.setMinLines(12);
        source.setText("flow main {\n" +
                "    var total: number = 0;\n" +
                "    var i: number = 0;\n" +
                "    while (i < 5) {\n" +
                "        i = i + 1;\n" +
                "        total = total + i;\n" +
                "    }\n" +
                "    display total;\n" +
                "    display \"TAVIQ on Android\";\n" +
                "}\n");
        root.addView(source, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f));

        LinearLayout projectActions = new LinearLayout(this);
        projectActions.setOrientation(LinearLayout.HORIZONTAL);
        Button save = new Button(this);
        save.setText(text("Save", "حفظ"));
        Button open = new Button(this);
        open.setText(text("Open", "فتح"));
        projectActions.addView(save, new LinearLayout.LayoutParams(0,
                LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
        projectActions.addView(open, new LinearLayout.LayoutParams(0,
                LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
        root.addView(projectActions);

        runButton = new Button(this);
        runButton.setText(text("Compile and Run", "ترجمة وتشغيل"));
        root.addView(runButton);

        TextView outputLabel = new TextView(this);
        outputLabel.setText(text("Output", "النتيجة"));
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
                LinearLayout.LayoutParams.MATCH_PARENT, 150));

        save.setOnClickListener(v -> saveProject());
        open.setOnClickListener(v -> openProject());
        runButton.setOnClickListener(v -> compileAndRun());
        setContentView(root);
    }

    private void saveProject() {
        byte[] bytes = source.getText().toString().getBytes(StandardCharsets.UTF_8);
        if (bytes.length > MAX_SOURCE_BYTES) {
            output.setText(text("Source exceeds 1 MiB", "حجم البرنامج أكبر من 1 ميجابايت"));
            return;
        }
        try (FileOutputStream stream = openFileOutput(PROJECT_FILE, MODE_PRIVATE)) {
            stream.write(bytes);
            output.setText(text("Saved " + PROJECT_FILE, "تم حفظ " + PROJECT_FILE));
        } catch (IOException ex) {
            output.setText(text("Save failed: ", "فشل الحفظ: ") + ex.getMessage());
        }
    }

    private void openProject() {
        try (FileInputStream stream = openFileInput(PROJECT_FILE);
             ByteArrayOutputStream buffer = new ByteArrayOutputStream()) {
            byte[] chunk = new byte[8192];
            int count;
            while ((count = stream.read(chunk)) != -1) {
                if (buffer.size() + count > MAX_SOURCE_BYTES) {
                    throw new IOException("project exceeds 1 MiB");
                }
                buffer.write(chunk, 0, count);
            }
            source.setText(buffer.toString(StandardCharsets.UTF_8.name()));
            output.setText(text("Opened " + PROJECT_FILE, "تم فتح " + PROJECT_FILE));
        } catch (IOException ex) {
            output.setText(text("Open failed: ", "فشل الفتح: ") + ex.getMessage());
        }
    }

    private void compileAndRun() {
        final String program = source.getText().toString();
        if (program.getBytes(StandardCharsets.UTF_8).length > MAX_SOURCE_BYTES) {
            output.setText(text("Source exceeds 1 MiB", "حجم البرنامج أكبر من 1 ميجابايت"));
            return;
        }
        runButton.setEnabled(false);
        output.setText(text("Compiling...", "جارٍ الترجمة..."));
        executor.execute(() -> {
            final String result;
            try {
                result = NativeRuntime.run(program);
            } catch (RuntimeException ex) {
                mainThread.post(() -> output.setText(
                        text("Runtime error: ", "خطأ تشغيل: ") + ex.getMessage()));
                mainThread.post(() -> runButton.setEnabled(true));
                return;
            }
            mainThread.post(() -> {
                output.setText(result);
                runButton.setEnabled(true);
            });
        });
    }

    @Override protected void onDestroy() {
        executor.shutdownNow();
        super.onDestroy();
    }
}
