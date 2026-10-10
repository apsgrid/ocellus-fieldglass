package com.ocellus.fieldglass.companion;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelUuid;
import android.location.LocationManager;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Collections;

public class MainActivity extends Activity {
    private static final int PERMISSION_REQUEST = 1001;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private final Map<String, BluetoothDevice> devices = new LinkedHashMap<>();
    private final List<String> labels = new ArrayList<>();

    private ArrayAdapter<String> listAdapter;
    private BluetoothLeScanner scanner;
    private String selectedAddress;
    private TextView status;
    private Button shareButton;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        int pad = dp(16);
        root.setPadding(pad, pad, pad, pad);

        TextView title = new TextView(this);
        title.setText("Ocellus Fieldglass Companion");
        title.setTextSize(24f);
        root.addView(title);

        TextView explain = new TextView(this);
        explain.setText(
                "Shares this phone's live location to a nearby Fieldglass over encrypted BLE. " +
                "Fieldglass shows a six-digit pairing code; Android will ask for it the first time.");
        explain.setPadding(0, dp(8), 0, dp(12));
        root.addView(explain);

        status = new TextView(this);
        status.setText("Grant permissions, then scan.");
        root.addView(status);

        Button scanButton = new Button(this);
        scanButton.setText("Scan for Fieldglass");
        scanButton.setOnClickListener(v -> startScan());
        root.addView(scanButton);

        ListView list = new ListView(this);
        listAdapter = new ArrayAdapter<>(this, android.R.layout.simple_list_item_single_choice, labels);
        list.setChoiceMode(ListView.CHOICE_MODE_SINGLE);
        list.setAdapter(listAdapter);
        list.setOnItemClickListener((parent, view, position, id) -> {
            String label = labels.get(position);
            int split = label.lastIndexOf("\n");
            selectedAddress = split >= 0 ? label.substring(split + 1) : null;
            status.setText("Selected " + label.replace("\n", " "));
            shareButton.setEnabled(selectedAddress != null);
        });
        root.addView(list, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f));

        shareButton = new Button(this);
        shareButton.setText("Start sharing location");
        shareButton.setEnabled(false);
        shareButton.setOnClickListener(v -> startSharing());
        root.addView(shareButton);

        Button stopButton = new Button(this);
        stopButton.setText("Stop sharing");
        stopButton.setOnClickListener(v -> {
            stopService(new Intent(this, LocationShareService.class));
            status.setText("Location sharing stopped.");
        });
        root.addView(stopButton);

        setContentView(root);
        requestRequiredPermissions();
    }

    private void requestRequiredPermissions() {
        List<String> wanted = new ArrayList<>();
        wanted.add(Manifest.permission.ACCESS_FINE_LOCATION);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            wanted.add(Manifest.permission.BLUETOOTH_SCAN);
            wanted.add(Manifest.permission.BLUETOOTH_CONNECT);
        }
        if (Build.VERSION.SDK_INT >= 33) {
            wanted.add(Manifest.permission.POST_NOTIFICATIONS);
        }

        List<String> missing = new ArrayList<>();
        for (String p : wanted) {
            if (checkSelfPermission(p) != PackageManager.PERMISSION_GRANTED) missing.add(p);
        }
        if (!missing.isEmpty()) {
            requestPermissions(missing.toArray(new String[0]), PERMISSION_REQUEST);
        }
    }

    private boolean permissionsReady() {
        if (checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)
                != PackageManager.PERMISSION_GRANTED) return false;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN)
                    != PackageManager.PERMISSION_GRANTED) return false;
            if (checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT)
                    != PackageManager.PERMISSION_GRANTED) return false;
        }
        return true;
    }

    private void startScan() {
        if (!permissionsReady()) {
            requestRequiredPermissions();
            return;
        }

        BluetoothManager manager = (BluetoothManager) getSystemService(Context.BLUETOOTH_SERVICE);
        BluetoothAdapter adapter = manager.getAdapter();
        if (adapter == null || !adapter.isEnabled()) {
            Toast.makeText(this, "Enable Bluetooth first.", Toast.LENGTH_LONG).show();
            return;
        }

        scanner = adapter.getBluetoothLeScanner();
        devices.clear();
        labels.clear();
        listAdapter.notifyDataSetChanged();
        selectedAddress = null;
        shareButton.setEnabled(false);

        ScanFilter filter = new ScanFilter.Builder()
                .setServiceUuid(new ParcelUuid(FieldglassProtocol.SERVICE_UUID))
                .build();
        ScanSettings settings = new ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                .build();

        status.setText("Scanning…");
        scanner.startScan(Collections.singletonList(filter), settings, scanCallback);
        handler.postDelayed(this::stopScan, 8000L);
    }

    private void stopScan() {
        if (scanner != null && permissionsReady()) {
            try {
                scanner.stopScan(scanCallback);
            } catch (SecurityException ignored) {}
        }
        if (devices.isEmpty()) status.setText("No Fieldglass found. Check that it is powered on.");
        else status.setText("Tap a Fieldglass, then start sharing.");
    }

    private final ScanCallback scanCallback = new ScanCallback() {
        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            BluetoothDevice d = result.getDevice();
            String address = d.getAddress();
            if (devices.containsKey(address)) return;
            devices.put(address, d);

            String name = null;
            try {
                name = d.getName();
            } catch (SecurityException ignored) {}
            if (name == null || name.isBlank()) name = "Fieldglass";
            labels.add(name + "  RSSI " + result.getRssi() + " dBm\n" + address);
            listAdapter.notifyDataSetChanged();
        }

        @Override
        public void onScanFailed(int errorCode) {
            status.setText("BLE scan failed: " + errorCode);
        }
    };

    private void startSharing() {
        if (selectedAddress == null) return;
        if (!permissionsReady()) {
            requestRequiredPermissions();
            return;
        }

        LocationManager lm = (LocationManager) getSystemService(LOCATION_SERVICE);
        if (lm == null || !lm.isProviderEnabled(LocationManager.GPS_PROVIDER)) {
            Toast.makeText(this,
                    "GPS is currently disabled; Fieldglass can still use network fixes if available.",
                    Toast.LENGTH_LONG).show();
        }

        Intent i = new Intent(this, LocationShareService.class);
        i.setAction(LocationShareService.ACTION_START);
        i.putExtra(LocationShareService.EXTRA_ADDRESS, selectedAddress);
        if (Build.VERSION.SDK_INT >= 26) startForegroundService(i);
        else startService(i);
        status.setText("Sharing started. Follow Android's pairing prompt if this is a new Fieldglass.");
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }
}