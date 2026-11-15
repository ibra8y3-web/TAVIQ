package org.taviq.app;

import android.test.InstrumentationTestCase;

public final class RuntimeInstrumentedTest extends InstrumentationTestCase {
    public void testNativeCompilerAndQvmV3RecursiveCallOnAndroid() {
        final String source =
                "fn factorial(n: number) -> number { " +
                "if (n <= 1) { return 1; } else { return n * factorial(n - 1); } } " +
                "flow main { display factorial(6); display true; }";
        assertEquals("720\ntrue\n", NativeRuntime.run(source));
    }

    public void testInvalidProgramReturnsDiagnostics() {
        final String source = "flow main { display missing; }";
        final String result = NativeRuntime.run(source);
        assertTrue(result.contains("undefined variable"));
    }
}
