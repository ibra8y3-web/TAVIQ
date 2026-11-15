package org.taviq.app;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import org.junit.Test;
import org.junit.runner.RunWith;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

@RunWith(AndroidJUnit4.class)
public final class RuntimeInstrumentedTest {
    @Test
    public void testNativeCompilerAndQvmControlFlowOnAndroid() {
        final String source =
                "flow main { " +
                "var i: number = 0; var total: number = 0; " +
                "while (i < 5) { i = i + 1; " +
                "if (i == 3) { total = total + 10; } " +
                "else { total = total + i; } } " +
                "display total; display true; }";
        assertEquals("22\ntrue\n", NativeRuntime.run(source));
    }

    @Test
    public void testInvalidProgramReturnsDiagnostics() {
        final String source = "flow main { display missing; }";
        final String result = NativeRuntime.run(source);
        assertTrue(result.contains("undefined variable"));
    }
}
