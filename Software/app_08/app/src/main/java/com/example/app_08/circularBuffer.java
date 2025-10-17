package com.example.app_08;

public final class circularBuffer {
    private final float[] buf;
    private int writePos = 0;   // next position to write
    private int count = 0;      // how many valid samples are stored (<= capacity)

    public circularBuffer(int capacity) {
        if (capacity <= 0) throw new IllegalArgumentException("capacity must be > 0");
        this.buf = new float[capacity];
    }

    public synchronized void add(float v) {
        buf[writePos] = v;
        writePos = (writePos + 1) % buf.length;
        if (count < buf.length) count++;
    }

    /** Returns a snapshot copy of the current contents, oldest -> newest. */
    public synchronized float[] getData() {
        float[] out = new float[count];
        // start index of the oldest element
        int start = writePos - count;
        if (start < 0) start += buf.length;

        int firstChunk = Math.min(count, buf.length - start);
        System.arraycopy(buf, start, out, 0, firstChunk);
        if (firstChunk < count) {
            System.arraycopy(buf, 0, out, firstChunk, count - firstChunk);
        }
        return out;
    }

    public synchronized int size() { return count; }
    public synchronized int capacity() { return buf.length; }
    public synchronized boolean isFull() { return count == buf.length; }
}



