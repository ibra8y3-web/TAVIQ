package org.taviq.app;

import android.test.InstrumentationTestCase;

public final class RuntimeInstrumentedTest extends InstrumentationTestCase {
    public void testNativeCompilerAndQvmRunOnAndroid() {
        final String source =
                "flow main { var n: number = 6; display n * 7; display true; }";
        assertEquals("42\ntrue\n", NativeRuntime.run(source));
    }

    public void testInvalidProgramReturnsDiagnostics() {
        final String source = "flow main { display missing; }";
        final String result = NativeRuntime.run(source);
        assertTrue(result.contains("undefined variable"));
    }
}
