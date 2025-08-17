package com.example.app_08;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Color;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbManager;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import com.github.mikephil.charting.charts.LineChart;
import com.github.mikephil.charting.components.XAxis;
import com.github.mikephil.charting.data.Entry;
import com.github.mikephil.charting.data.LineData;
import com.github.mikephil.charting.data.LineDataSet;
import com.hoho.android.usbserial.driver.UsbSerialDriver;
import com.hoho.android.usbserial.driver.UsbSerialPort;
import com.hoho.android.usbserial.driver.UsbSerialProber;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;


public class MainActivity extends AppCompatActivity {

    private static final String ACTION_USB_PERMISSION = "com.example.app_08.USB_PERMISSION";
    private static final int BAUD_RATE = 115200;

    UsbManager usbManager;
    UsbSerialPort serialPort;
    PendingIntent permissionIntent;

    TextView textViewData, textViewThreshold, textViewStats, textViewConnection;
    LineChart lineChart;
    SeekBar seekBarThreshold;
    Button buttonTrigger, buttonReset;

    List<Entry> entries = new ArrayList<>();
    int sampleCount = 0;
    float maxSample = 0;
    float minSample = 4095;

    private final BroadcastReceiver usbReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (ACTION_USB_PERMISSION.equals(action)) {
                UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                boolean granted = intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false);
                if (granted && device != null) {
                    Log.d("Serial", "Permission granted, connecting...");
                    connectToDevice(device);
                } else {
                    Toast.makeText(context, "USB Permission denied", Toast.LENGTH_SHORT).show();
                    updateConnectionStatus("USB Permission denied");
                }

            } else if (UsbManager.ACTION_USB_DEVICE_ATTACHED.equals(action)) {
                UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                if (device != null) {
                    usbManager.requestPermission(device, permissionIntent);
                }

            } else if (UsbManager.ACTION_USB_DEVICE_DETACHED.equals(action)) {
                UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                if (device != null && serialPort != null &&
                        device.equals(serialPort.getDriver().getDevice())) {
                    closeSerialPort();
                    updateConnectionStatus("Device disconnected");
                }
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        usbManager = (UsbManager) getSystemService(Context.USB_SERVICE);

        permissionIntent = PendingIntent.getBroadcast(
                this, 0, new Intent(ACTION_USB_PERMISSION), PendingIntent.FLAG_IMMUTABLE
        );

        // Initialize UI
        textViewData = findViewById(R.id.textViewData);
        textViewThreshold = findViewById(R.id.textViewThreshold);
        textViewStats = findViewById(R.id.textViewStats);
        textViewConnection = findViewById(R.id.textViewConnection);
        lineChart = findViewById(R.id.lineChart);
        seekBarThreshold = findViewById(R.id.seekBarThreshold);
        buttonTrigger = findViewById(R.id.buttonTrigger);
        buttonReset = findViewById(R.id.buttonReset);

        setupChart();

        // SeekBar listener
        seekBarThreshold.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                float voltage = (progress / 4095f) * 3.3f;
                textViewThreshold.setText(String.format("V Threshold: %.2f V", (double)voltage));
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        // Buttons
        buttonReset.setOnClickListener(v -> resetGraph());
        buttonTrigger.setOnClickListener(v -> {
            if (serialPort != null) {
                try {
                    serialPort.write("TRIGGER\n".getBytes(StandardCharsets.UTF_8), 1000);
                } catch (IOException e) {
                    Toast.makeText(this, "Failed to send trigger command", Toast.LENGTH_SHORT).show();
                    Log.e("MainActivity", "Trigger error", e);
                }
            }
        });

        // Register USB receiver
        IntentFilter filter = new IntentFilter();
        filter.addAction(ACTION_USB_PERMISSION);
        filter.addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED);
        filter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(usbReceiver, filter, Context.RECEIVER_NOT_EXPORTED);
        } else {
            registerReceiver(usbReceiver, filter);
        }

        detectDevice();
    }

    private void setupChart() {
        lineChart.setData(new LineData());
        lineChart.getXAxis().setPosition(XAxis.XAxisPosition.BOTTOM);
        lineChart.getAxisLeft().setAxisMinimum(0);
        lineChart.getAxisLeft().setAxisMaximum(4);
        lineChart.getAxisRight().setEnabled(false);
    }

    private void resetGraph() {
        entries.clear();
        sampleCount = 0;
        maxSample = 0;
        minSample = 4095;
        sampleBuffer.clear();

        lineChart.clear();
        textViewStats.setText("Stats: ");
        textViewData.setText("");
    }

    private void detectDevice() {
        List<UsbSerialDriver> drivers = UsbSerialProber.getDefaultProber().findAllDrivers(usbManager);
        if (drivers.isEmpty()) {
            updateConnectionStatus("No device connected");
            Log.d("USB", "No USB drivers found");
            return;
        }

        for (UsbSerialDriver driver : drivers) {
            UsbDevice device = driver.getDevice();
            Log.d("USB", "Found device VID=" + device.getVendorId() + " PID=" + device.getProductId());
            if (usbManager.hasPermission(device)) {
                connectToDevice(device);
            } else {
                usbManager.requestPermission(device, permissionIntent);
            }
        }
    }

    private void connectToDevice(UsbDevice device) {
        UsbSerialDriver driver = UsbSerialProber.getDefaultProber().probeDevice(device);
        if (driver == null) {
            updateConnectionStatus("No driver for device");
            return;
        }

        try {
            serialPort = driver.getPorts().get(0);
            serialPort.open(usbManager.openDevice(device));
            serialPort.setParameters(BAUD_RATE, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE);
            updateConnectionStatus("Connected to " + device.getDeviceName());

            new Thread(this::readSerial).start();
        } catch (IOException e) {
            updateConnectionStatus("Connection failed: " + e.getMessage());
            Log.e("MainActivity", "Serial open error", e);
        }
    }

    private final List<Float> sampleBuffer = new ArrayList<>();
    private final Object bufferLock = new Object(); // to avoid race conditions
    private static final int BUFFER_SIZE = 1000;
    private final circularBuffer circularBuffer = new circularBuffer(BUFFER_SIZE);
    private void readSerial() {
        byte[] buffer = new byte[1024];
        StringBuilder partialLine = new StringBuilder();

        while (serialPort != null) {
            try {
                int numBytes = serialPort.read(buffer, 100);
                if (numBytes > 0) {
                    String data = new String(buffer, 0, numBytes, StandardCharsets.UTF_8);
                    partialLine.append(data);

                    // process complete lines
                    int newlineIndex;
                    while ((newlineIndex = partialLine.indexOf("\n")) >= 0) {
                        String line = partialLine.substring(0, newlineIndex).trim();
                        partialLine.delete(0, newlineIndex + 1);

                        if (!line.isEmpty()) {
                            for (String num : line.split("\\s*,\\s*")) {
                                try {
                                    int val = Integer.parseInt(num);
                                    if (val >= 0 && val <= 4095) {
                                        synchronized (bufferLock) {
                                            float v_sample = (val / 4095.0f) * 3.3f; // convert to volts
                                            sampleBuffer.add(v_sample);
                                        }
                                    }
                                } catch (NumberFormatException ignored) {}
                            }
                        }
                    }

                    // update UI after new data arrived
                    runOnUiThread(this::updateUI);
                }
            } catch (IOException e) {
                runOnUiThread(() -> updateConnectionStatus("Disconnected"));
                closeSerialPort();
                break;
            }
        }
    }

    private void updateUI() {
        List<Entry> entries = new ArrayList<>();
        float maxVal = 0;
        float minVal = 4;

        synchronized (bufferLock) {
            for (int i = 0; i < sampleBuffer.size(); i++) {
                float val = sampleBuffer.get(i);
                entries.add(new Entry(i, val));
                if (val > maxVal) maxVal = val;
                if (val < minVal) minVal = val;
            }
        }

        LineDataSet dataSet = new LineDataSet(entries, "ADC Data");
        dataSet.setDrawCircles(false);
        dataSet.setDrawValues(false);
        dataSet.setLineWidth(2f);
        dataSet.setColor(Color.RED);
        lineChart.setData(new LineData(dataSet));
        lineChart.invalidate();

        textViewStats.setText(String.format(
                "Samples=%d  Max=%.00f  Min=%.00f  Pk-Pk=%.00f",
                entries.size(), maxVal, minVal, maxVal - minVal
        ));
    }



    private void updateUI(String data) {
            for (String line : data.split("\n")) {
                line = line.trim();
                if (line.isEmpty()) continue;

                String[] nums = line.split("\\s*,\\s*");
                for (String num : nums) {
                    if (num.isEmpty()) continue;
                    try {
                        int val = Integer.parseInt(num);
                        if (val >= 0 && val <= 4095) {
                            circularBuffer.add(val);
                        }
                    } catch (NumberFormatException ignored) {}
                }
            }
        }

    private void closeSerialPort() {
        if (serialPort != null) {
            try {
                serialPort.close();
            } catch (IOException e) {
                Log.e("MainActivity", "Error closing serial port", e);
            } finally {
                serialPort = null;
            }
        }
    }


    private void updateChart() {
        float[] samples = circularBuffer.getData();
        List<Entry> entries = new ArrayList<>();
        for (int i = 0; i < samples.length; i++) {
            entries.add(new Entry(i, samples[i]));
        }

        LineDataSet dataSet = new LineDataSet(entries, "Signal");
        dataSet.setDrawCircles(false);
        dataSet.setDrawValues(false);
//        dataSet.setLineWidth(2f);
        dataSet.setColor(Color.BLUE);

        LineData lineData = new LineData(dataSet);
        lineChart.setData(lineData);
        lineChart.invalidate();
    }


    private void updateConnectionStatus(String status) {
        runOnUiThread(() -> {
            textViewConnection.setText(status);
            int color = status.toLowerCase().contains("connected") ?
                    getResources().getColor(android.R.color.holo_green_dark) :
                    getResources().getColor(android.R.color.holo_red_dark);
            textViewConnection.setTextColor(color);
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        unregisterReceiver(usbReceiver);
        closeSerialPort();
    }
}
