package com.example.app_08;

import android.annotation.SuppressLint;
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
import android.view.MotionEvent;
import android.view.View;
import android.content.res.Configuration;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.EditText;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import com.github.mikephil.charting.charts.LineChart;
import com.github.mikephil.charting.components.XAxis;
import com.github.mikephil.charting.data.Entry;
import com.github.mikephil.charting.data.LineData;
import com.github.mikephil.charting.data.LineDataSet;
import com.github.mikephil.charting.utils.MPPointD;
import com.hoho.android.usbserial.driver.UsbSerialDriver;
import com.hoho.android.usbserial.driver.UsbSerialPort;
import com.hoho.android.usbserial.driver.UsbSerialProber;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

import org.jtransforms.fft.DoubleFFT_1D;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.appcompat.app.ActionBarDrawerToggle;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.app.AppCompatDelegate;
import androidx.core.view.GravityCompat;
import androidx.drawerlayout.widget.DrawerLayout;
import android.os.Bundle;
import android.view.MenuItem;
import android.widget.Toast;
import androidx.appcompat.widget.Toolbar;
import com.google.android.material.navigation.NavigationView;
import android.widget.RadioGroup;
import com.github.mikephil.charting.listener.OnChartValueSelectedListener;
import com.github.mikephil.charting.highlight.Highlight;

public class MainActivity extends AppCompatActivity {

    private static final String ACTION_USB_PERMISSION = "com.example.app_08.USB_PERMISSION";
    private static int BAUD_RATE = 921600; // default is now 921600 was 115200
    private static final int WINDOW_SIZE = 500;
    private static final float SAMPLING_FREQ = 1_231_000f;// was 1_250_000f; updated according to actual sampling freq
    private static final float SAMPLING_PERIOD = 1f/SAMPLING_FREQ;
    private static final int MAX_SAMPLE_VALUE_12_BIT_ADC = 4096;
    private static final float V_REF = 3.3f;

    float cursorX = Float.NaN;
    float cursorY = Float.NaN;

    UsbManager usbManager;
    UsbSerialPort serialPort;
    PendingIntent permissionIntent;

    TextView textViewThreshold, textViewStats, textViewConnection;
    LineChart lineChart;
    SeekBar seekBarThreshold;
    Button buttonTrigger, buttonReset;

    List<Entry> entries = new ArrayList<>();
    int sampleCount = 0;
    float maxSample = 0;
    float minSample = 4095;
    private static int BUFFER_SIZE = 10000; // was 1000
    private int sampleIndex = 0;
    float v_threshold = 0;
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

    // ADDED: Variables for the side menu
    private DrawerLayout drawerLayout;
    private ActionBarDrawerToggle actionBarDrawerToggle;
    private NavigationView navigationView;
    private Toolbar toolbar;
    private TextView drawerThresholdTextView;
    private SeekBar drawerThresholdSeekBar;
    TextView textViewCursor;
    Button buttonHomeView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        usbManager = (UsbManager) getSystemService(Context.USB_SERVICE);

        permissionIntent = PendingIntent.getBroadcast(
                this, 0, new Intent(ACTION_USB_PERMISSION), PendingIntent.FLAG_IMMUTABLE
        );

        // Initialize UI
//        AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_YES); // for testing dark mode
        textViewThreshold = findViewById(R.id.textViewThreshold);
        textViewStats = findViewById(R.id.textViewStats);
        textViewConnection = findViewById(R.id.textViewConnection);
        lineChart = findViewById(R.id.lineChart);
        seekBarThreshold = findViewById(R.id.seekBarThreshold);
        buttonTrigger = findViewById(R.id.buttonTrigger);
        buttonReset = findViewById(R.id.buttonReset);
        textViewCursor = findViewById(R.id.textViewCursor);
        buttonHomeView = findViewById(R.id.buttonHomeView);

        // MODIFIED: Initializing and setting up the side menu components
        drawerLayout = findViewById(R.id.drawer_layout);
        toolbar = findViewById(R.id.toolbar);
        navigationView = findViewById(R.id.navigationView); // Corrected ID
        setSupportActionBar(toolbar);

        actionBarDrawerToggle = new ActionBarDrawerToggle(
                this,
                drawerLayout,
                toolbar,
                R.string.open_drawer,
                R.string.close_drawer
        );
        drawerLayout.addDrawerListener(actionBarDrawerToggle);
        actionBarDrawerToggle.syncState();

        // Initialize the controls from the drawer layout
        View headerView = navigationView.getHeaderView(0);
        Button drawerButtonTrigger = headerView.findViewById(R.id.drawerButtonTrigger);
        Button drawerButtonReset = headerView.findViewById(R.id.drawerButtonReset);
        Button drawerButtonHomeView = headerView.findViewById(R.id.drawerButtonHomeView);
        // Drawer "Arm Capture" button
        drawerButtonTrigger.setOnClickListener(v -> {
            buttonTrigger.performClick(); // just reuse the existing handler
            drawerLayout.closeDrawer(GravityCompat.START); // optional: auto-close drawer
        });

// Drawer "Clear Graph" button
        drawerButtonReset.setOnClickListener(v -> {
            buttonReset.performClick();
            drawerLayout.closeDrawer(GravityCompat.START);
        });

// Drawer "Home View" button
        drawerButtonHomeView.setOnClickListener(v -> {
            buttonHomeView.performClick();
            drawerLayout.closeDrawer(GravityCompat.START);
        });
        drawerThresholdTextView = headerView.findViewById(R.id.textViewDrawerThreshold);
        drawerThresholdSeekBar = headerView.findViewById(R.id.seekBarDrawerThreshold);
        // ADDED: Initialize the RadioGroup from the drawer header
        RadioGroup radioGroupTrigger = headerView.findViewById(R.id.radioGroupTrigger);
        radioGroupTrigger.setOnCheckedChangeListener((group, checkedId) -> {
            String command;
            if (checkedId == R.id.radioRising) {
                command = "TYPE=RISING\n";
            } else {
                command = "TYPE=FALLING\n";
            }
            if (serialPort != null) {
                try {
                    serialPort.write(command.getBytes(StandardCharsets.UTF_8), 1000);
                    Toast.makeText(this, "Trigger type set to: " + (checkedId == R.id.radioRising ? "Rising" : "Falling"), Toast.LENGTH_SHORT).show();
                } catch (IOException e) {
                    Toast.makeText(this, "Failed to set trigger type", Toast.LENGTH_SHORT).show();
                }
            }
        });

        // ADDED: Initialize the Baud Rate Spinner
        Spinner spinnerBaudRate = headerView.findViewById(R.id.spinnerBaudRate);
        spinnerBaudRate.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                int newBaudRate = Integer.parseInt(parent.getItemAtPosition(position).toString());
                if (newBaudRate != BAUD_RATE) {
                    changeBaudRate(newBaudRate);
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
                // Do nothing
            }
        });
        // ADDED: Initialize the Buffer Size controls
        EditText editTextBufferSize = headerView.findViewById(R.id.editTextBufferSize);
        Button buttonSetBufferSize = headerView.findViewById(R.id.buttonSetBufferSize);

        // Set the initial value in the EditText to the current BUFFER_SIZE
        editTextBufferSize.setText(String.valueOf(BUFFER_SIZE));

        buttonSetBufferSize.setOnClickListener(v -> {
            try {
                int newBufferSize = Integer.parseInt(editTextBufferSize.getText().toString());
                if (newBufferSize > 0) {
                    BUFFER_SIZE = newBufferSize;
                    Toast.makeText(this, "Buffer size set to " + BUFFER_SIZE, Toast.LENGTH_SHORT).show();
                } else {
                    Toast.makeText(this, "Buffer size must be a positive number", Toast.LENGTH_SHORT).show();
                }
            } catch (NumberFormatException e) {
                Toast.makeText(this, "Invalid buffer size", Toast.LENGTH_SHORT).show();
            }
        });

        buttonHomeView.setOnClickListener(v -> {
            if (lineChart.getData() != null && lineChart.getData().getEntryCount() > 0) {
                lineChart.fitScreen(); // resets zoom and pan
                lineChart.moveViewToX(0f); // centers around x=0
                lineChart.invalidate();
            }
        });

        // Set up the listener for the drawer's SeekBar
        drawerThresholdSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                float v_threshold_drawer = (progress / 4095f) * 3.3f;
                drawerThresholdTextView.setText(String.format("V Threshold: %.2f V", (double)v_threshold_drawer));
                v_threshold = v_threshold_drawer;
                // You can add code here to update the main seekBar if needed
                if (fromUser) {
                    // This is the key line to add
                    seekBarThreshold.setProgress(progress);
                }
            }


            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        setupChart();

        // MODIFIED: SeekBar listener with sync to drawer
        seekBarThreshold.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                v_threshold = (progress / 4095f) * 3.3f;
                textViewThreshold.setText(String.format("V Threshold: %.2f V", (double)v_threshold));
                // Sync the drawer's seekbar as well
                if (fromUser) {
                    drawerThresholdSeekBar.setProgress(progress);
                }
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
                    int v_threshold_sample_value = (int)((v_threshold * MAX_SAMPLE_VALUE_12_BIT_ADC) / V_REF);
                    String v_threshold_command = "V_THRESHOLD=" + v_threshold_sample_value + "\n";
                    serialPort.write(v_threshold_command.getBytes(StandardCharsets.UTF_8), 1000);
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

    // ADDED: Override onBackPressed to close the drawer
    @Override
    public void onBackPressed() {
        if (drawerLayout.isDrawerOpen(GravityCompat.START)) {
            drawerLayout.closeDrawer(GravityCompat.START);
        } else {
            super.onBackPressed();
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    private void setupChart() {
        lineChart.setBackgroundColor(Color.BLACK);
        lineChart.setDrawGridBackground(true);
        lineChart.setGridBackgroundColor(Color.BLACK);
        lineChart.getXAxis().setPosition(XAxis.XAxisPosition.BOTTOM);
        lineChart.getXAxis().setTextColor(Color.WHITE);
        lineChart.getXAxis().setDrawGridLines(true);
        lineChart.getXAxis().setGridColor(Color.DKGRAY);
        lineChart.getXAxis().setValueFormatter(new com.github.mikephil.charting.formatter.ValueFormatter() {
            @Override
            public String getAxisLabel(float value, com.github.mikephil.charting.components.AxisBase axis) {
                return String.format("%.0f µs", value);
            }
        });
        lineChart.getAxisLeft().setAxisMinimum(0);
        lineChart.getAxisLeft().setAxisMaximum(3.3f);
        lineChart.getAxisLeft().setTextColor(Color.WHITE);
        lineChart.getAxisLeft().setDrawGridLines(true);
        lineChart.getAxisLeft().setGridColor(Color.DKGRAY);

        lineChart.getAxisRight().setEnabled(false);
        lineChart.getLegend().setEnabled(false);
        lineChart.getDescription().setEnabled(false);

        // Enable zooming and scrolling
        lineChart.setTouchEnabled(true);
        lineChart.setDragEnabled(true);
        lineChart.setScaleEnabled(true);
        lineChart.setPinchZoom(false);


        // 🔹 Add the cursor listener here:
        lineChart.setOnChartValueSelectedListener(new OnChartValueSelectedListener() {
            @Override
            public void onValueSelected(Entry e, Highlight h) {
                cursorX = e.getX();
                cursorY = e.getY();
                updateCursorText();
            }

            @Override
            public void onNothingSelected() {
                cursorX = Float.NaN;
                cursorY = Float.NaN;
                updateCursorText();
            }
        });
//        // --- touch listener: convert touch pixel -> chart values, map to nearest sample index
//        lineChart.setOnTouchListener((v, event) -> {
//            // require data & at least one dataset
//            if (lineChart.getData() == null || lineChart.getData().getDataSetCount() == 0) {
//                return false;
//            }
//
//            if (event.getAction() == MotionEvent.ACTION_DOWN || event.getAction() == MotionEvent.ACTION_MOVE) {
//                try {
//                    // get chart x/y values (data-space) from touch pixel coordinates
//                    MPPointD values = lineChart.getTransformer(lineChart.getAxisLeft().getAxisDependency())
//                            .getValuesByTouchPoint(event.getX(), event.getY());
//                    double touchX = values.x; // in µs (because chart X entries use µs)
//                    // map touchX to nearest sample index in sampleBuffer
//                    synchronized (bufferLock) {
//                        int size = sampleBuffer.size();
//                        if (size > 0) {
//                            int triggerIndex = size / 2;
//                            float samplePeriodUs = SAMPLING_PERIOD * 1_000_000f;
//                            double idxD = touchX / samplePeriodUs + triggerIndex;
//                            int idx = (int) Math.round(idxD);
//                            if (idx < 0) idx = 0;
//                            if (idx >= size) idx = size - 1;
//                            float yVal = sampleBuffer.get(idx); // in volts
//                            float xForDisplay = (idx - triggerIndex) * samplePeriodUs; // aligned to sample grid
//                            cursorX = xForDisplay;
//                            cursorY = yVal;
//                            runOnUiThread(this::updateCursorText);
//                        }
//                    }
//                    // no explicit recycle call for MPPointD (safe)
//                } catch (Exception ex) {
//                    // ignore mapping errors
//                }
//            }
//            return false; // allow chart to also handle the touch (zooming/panning)
//        });

    }

    private void updateCursorText() {
        runOnUiThread(() -> {
            if (!Float.isNaN(cursorX) && !Float.isNaN(cursorY)) {
                textViewCursor.setText(String.format("Cursor: X=%.1f µs, Y=%.2f V", cursorX, cursorY));
            } else {
                textViewCursor.setText("Cursor: —");
            }
        });
    }

    private void resetGraph() {
        entries.clear();
        sampleCount = 0;
        maxSample = 0;
        minSample = 4095;
        synchronized (bufferLock) {
            sampleBuffer.clear();
        }
        lineChart.clear();
        textViewStats.setText("Stats: ");
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
    private final Object bufferLock = new Object();

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
                                            handleIncomingSample(val);
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

    private void handleIncomingSample(int sample) {
        // If the buffer size has changed, clear the buffer and reset the index
        if (sampleBuffer.size() > BUFFER_SIZE) {
            sampleBuffer.clear();
            sampleIndex = 0;
        }
        if (sampleIndex == 0) {
            // New buffer just started
            resetGraph();
        }

        float v_sample = (sample / 4095.0f) * 3.3f; // convert to volts
        sampleBuffer.add(v_sample);

        sampleIndex++;
        if (sampleIndex >= BUFFER_SIZE) {
            // Completed one buffer from trigger
            sampleIndex = 0;
        }
    }

    private void updateUI() {
        List<Entry> chartEntries = new ArrayList<>();
        float maxVal = 0;
        float minVal = 4;
        int triggerIndex = sampleBuffer.size() / 2;

        synchronized (bufferLock) {
            for (int i = 0; i < sampleBuffer.size(); i++) {
                float time = (i - triggerIndex) * SAMPLING_PERIOD * 1_000_000f; // time in µs
                float val = sampleBuffer.get(i);
                chartEntries.add(new Entry(time, val));
                if (val > maxVal) maxVal = val;
                if (val < minVal) minVal = val;
            }
        }

        LineDataSet dataSet = new LineDataSet(chartEntries, "ADC Data");
        dataSet.setDrawCircles(false);
        dataSet.setDrawValues(false);
        dataSet.setLineWidth(2f);
        dataSet.setColor(Color.RED);

        LineData lineData = new LineData(dataSet);
        lineChart.setData(lineData);

        if (chartEntries.size() > WINDOW_SIZE) {
            lineChart.setVisibleXRangeMaximum(WINDOW_SIZE);
            lineChart.moveViewToX(chartEntries.size() - WINDOW_SIZE*1.5f);
        }

        lineChart.invalidate();
        double fSig = SignalAnalyzer.estimateFrequencyFFT(sampleBuffer, SAMPLING_FREQ);
        String fStr = Double.isNaN(fSig) ? "—" : String.format("%.1f Hz", fSig);

        textViewStats.setText(String.format(
                "Samples=%d  Max=%.2f  Min=%.2f  Pk-Pk=%.2f  Fs=%.2f MHz  Fsig=%s",
                chartEntries.size(), maxVal, minVal, maxVal - minVal,
                SAMPLING_FREQ / 1_000_000f, fStr));
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

    private void changeBaudRate(int newBaudRate) {
        // This method handles closing the current serial port and reconnecting with the new baud rate.
        if (serialPort != null) {
            closeSerialPort();
            BAUD_RATE = newBaudRate; // Update the class variable
            detectDevice(); // Reconnect to the device
        } else {
            BAUD_RATE = newBaudRate;
        }
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
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        if (newConfig.orientation == Configuration.ORIENTATION_LANDSCAPE) {
            textViewStats.setVisibility(View.GONE);
            seekBarThreshold.setVisibility(View.GONE);
            buttonTrigger.setVisibility(View.GONE);
            buttonReset.setVisibility(View.GONE);

            getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
            );

        } else {
            textViewStats.setVisibility(View.VISIBLE);
            seekBarThreshold.setVisibility(View.VISIBLE);
            buttonTrigger.setVisibility(View.VISIBLE);
            buttonReset.setVisibility(View.VISIBLE);

            getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_VISIBLE);
        }

        lineChart.invalidate();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        unregisterReceiver(usbReceiver);
        closeSerialPort();
    }
}