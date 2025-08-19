package com.example.app_08;

import org.jtransforms.fft.DoubleFFT_1D;
import java.util.List;

/**
 * Utility class for analyzing sampled signals.
 */
public class SignalAnalyzer {

    // Must be <= buffer size and even
    private static final int FFT_SIZE = 2048;

    // You can configure your sample rate from outside
    public static double estimateFrequencyFFT(List<Float> buf, double samplingFreqHz) {
        int N = Math.min(buf.size(), FFT_SIZE);
        if (N < 32) return Double.NaN; // too short

        // Copy the LAST N samples
        double[] a = new double[N];
        int start = buf.size() - N;
        double mean = 0;
        for (int i = 0; i < N; i++) {
            double v = buf.get(start + i);
            mean += v;
            a[i] = v;
        }
        mean /= N;

        // Remove DC and apply Hann window
        for (int i = 0; i < N; i++) {
            double w = 0.5 * (1 - Math.cos(2 * Math.PI * i / (N - 1)));
            a[i] = (a[i] - mean) * w;
        }

        // FFT (real, in-place, packed format)
        DoubleFFT_1D fft = new DoubleFFT_1D(N);
        fft.realForward(a);

        // Find peak bin magnitude (skip DC at k=0)
        int half = N / 2;
        int peakK = 1;
        double peakPow = 0;

        for (int k = 1; k < half; k++) {
            double re = a[2 * k];
            double im = a[2 * k + 1];
            double pow = re * re + im * im;
            if (pow > peakPow) {
                peakPow = pow;
                peakK = k;
            }
        }

        // Handle Nyquist (k = half) for even N: real part stored at a[1], imag=0
        double nyqRe = a[1];
        double nyqPow = nyqRe * nyqRe;
        if (nyqPow > peakPow) {
            peakPow = nyqPow;
            peakK = half;
        }

        // Optional: quadratic interpolation around the peak for sub-bin accuracy
        if (peakK > 1 && peakK < half - 1) {
            double p0 = magPowAt(a, peakK - 1);
            double p1 = magPowAt(a, peakK);
            double p2 = magPowAt(a, peakK + 1);
            double denom = (p0 - 2 * p1 + p2);
            if (denom != 0) {
                double delta = 0.5 * (p0 - p2) / denom; // -0.5..+0.5
                return ((peakK + delta) * samplingFreqHz) / N;
            }
        }

        return (peakK * samplingFreqHz) / N;
    }

    private static double magPowAt(double[] a, int k) {
        int N = a.length;
        int half = N / 2;
        if (k == 0) {
            double re0 = a[0];
            return re0 * re0;
        } else if (k == half) { // Nyquist (even N)
            double re = a[1];
            return re * re;
        } else {
            double re = a[2 * k];
            double im = a[2 * k + 1];
            return re * re + im * im;
        }
    }
}