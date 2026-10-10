package com.ocellus.fieldglass.companion;

import android.Manifest;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Build;
import android.os.IBinder;
import android.os.Looper;

import java.util.Objects;

public class LocationShareService extends Service implements LocationListener {
    public static final String ACTION_START =
            "com.ocellus.fieldglass.companion.START";
    public static final String EXTRA_ADDRESS = "address";

    private static final int NOTIFICATION_ID = 4421;
    private static final String CHANNEL_ID = "fieldglass_location";

    private LocationManager locationManager;
    private BluetoothDevice device;
    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic fixCharacteristic;
    private boolean gattConnected = false;
    private boolean readyToWrite = false;
    private boolean stopping = false;

    @Override
    public void onCreate() {
        super.onCreate();
        createNotificationChannel();
        IntentFilter filter = new IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
        if (Build.VERSION.SDK_INT >= 33) {
            registerReceiver(bondReceiver, filter, Context.RECEIVER_NOT_EXPORTED);
        } else {
            registerReceiver(bondReceiver, filter);
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        startForeground(NOTIFICATION_ID, notification("Connecting to Fieldglass…"));

        if (intent == null || !ACTION_START.equals(intent.getAction())) {
            return START_NOT_STICKY;
        }
        String address = intent.getStringExtra(EXTRA_ADDRESS);
        if (address == null) {
            stopSelf();
            return START_NOT_STICKY;
        }

        if (!hasRequiredPermissions()) {
            updateNotification("Missing Bluetooth/location permission.");
            stopSelf();
            return START_NOT_STICKY;
        }

        BluetoothManager manager = (BluetoothManager) getSystemService(BLUETOOTH_SERVICE);
        device = manager.getAdapter().getRemoteDevice(address);
        connectGatt();
        startLocationUpdates();
        return START_STICKY;
    }

    private void connectGatt() {
        closeGatt();
        if (Build.VERSION.SDK_INT >= 23) {
            gatt = device.connectGatt(this, false, gattCallback,
                    BluetoothDevice.TRANSPORT_LE);
        } else {
            gatt = device.connectGatt(this, false, gattCallback);
        }
    }

    private final BluetoothGattCallback gattCallback = new BluetoothGattCallback() {
        @Override
        public void onConnectionStateChange(BluetoothGatt g, int status, int newState) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                gattConnected = true;
                updateNotification("Connected. Securing link…");

                if (device.getBondState() == BluetoothDevice.BOND_NONE) {
                    device.createBond();
                } else if (device.getBondState() == BluetoothDevice.BOND_BONDED) {
                    g.discoverServices();
                }
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                gattConnected = false;
                readyToWrite = false;
                fixCharacteristic = null;
                if (!stopping) {
                    updateNotification("Disconnected. Reconnecting…");
                    mainHandler.postDelayed(LocationShareService.this::connectGatt, 2500L);
                }
            }
        }

        @Override
        public void onServicesDiscovered(BluetoothGatt g, int status) {
            BluetoothGattService service = g.getService(FieldglassProtocol.SERVICE_UUID);
            if (service == null) {
                updateNotification("Connected device is not a compatible Fieldglass.");
                return;
            }
            fixCharacteristic = service.getCharacteristic(FieldglassProtocol.FIX_UUID);
            readyToWrite = fixCharacteristic != null &&
                    device.getBondState() == BluetoothDevice.BOND_BONDED;
            updateNotification(readyToWrite
                    ? "Sharing phone location to Fieldglass."
                    : "Waiting for secure pairing.");
        }
    };

    private final android.os.Handler mainHandler =
            new android.os.Handler(Looper.getMainLooper());

    private final BroadcastReceiver bondReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            BluetoothDevice changed = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            if (device == null || changed == null ||
                    !Objects.equals(changed.getAddress(), device.getAddress())) return;

            int state = intent.getIntExtra(
                    BluetoothDevice.EXTRA_BOND_STATE, BluetoothDevice.ERROR);
            if (state == BluetoothDevice.BOND_BONDED && gattConnected && gatt != null) {
                gatt.discoverServices();
                updateNotification("Paired. Preparing location stream…");
            } else if (state == BluetoothDevice.BOND_NONE) {
                readyToWrite = false;
                updateNotification("Pairing was cancelled or removed.");
            }
        }
    };

    private void startLocationUpdates() {
        locationManager = (LocationManager) getSystemService(LOCATION_SERVICE);
        if (checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)
                != PackageManager.PERMISSION_GRANTED) return;

        if (locationManager.getAllProviders().contains(LocationManager.GPS_PROVIDER)
                && locationManager.isProviderEnabled(LocationManager.GPS_PROVIDER)) {
            locationManager.requestLocationUpdates(
                    LocationManager.GPS_PROVIDER, 1000L, 0f, this, Looper.getMainLooper());
        } else if (locationManager.getAllProviders().contains(LocationManager.NETWORK_PROVIDER)
                && locationManager.isProviderEnabled(LocationManager.NETWORK_PROVIDER)) {
            // Network location is a fallback only. Do not let a coarse network
            // fix overwrite an active GNSS stream.
            locationManager.requestLocationUpdates(
                    LocationManager.NETWORK_PROVIDER, 1000L, 0f, this, Looper.getMainLooper());
        }
    }

    @Override
    public void onLocationChanged(Location location) {
        if (!readyToWrite || gatt == null || fixCharacteristic == null) return;
        byte[] packet = FieldglassProtocol.encodeFix(location);

        try {
            if (Build.VERSION.SDK_INT >= 33) {
                gatt.writeCharacteristic(
                        fixCharacteristic,
                        packet,
                        BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE);
            } else {
                fixCharacteristic.setWriteType(
                        BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE);
                fixCharacteristic.setValue(packet);
                gatt.writeCharacteristic(fixCharacteristic);
            }
            updateNotification(
                    String.format("Sharing %.1f m fix to Fieldglass.", location.getAccuracy()));
        } catch (SecurityException ignored) {
            readyToWrite = false;
            updateNotification("Bluetooth permission was revoked.");
        }
    }

    private boolean hasRequiredPermissions() {
        if (checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)
                != PackageManager.PERMISSION_GRANTED) return false;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            return checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT)
                    == PackageManager.PERMISSION_GRANTED;
        }
        return true;
    }

    private void createNotificationChannel() {
        NotificationManager nm = getSystemService(NotificationManager.class);
        NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID, "Fieldglass location sharing",
                NotificationManager.IMPORTANCE_LOW);
        nm.createNotificationChannel(channel);
    }

    private Notification notification(String text) {
        Intent open = new Intent(this, MainActivity.class);
        PendingIntent pi = PendingIntent.getActivity(
                this, 0, open,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);

        return new Notification.Builder(this, CHANNEL_ID)
                .setContentTitle("Ocellus Fieldglass")
                .setContentText(text)
                .setSmallIcon(android.R.drawable.ic_menu_mylocation)
                .setContentIntent(pi)
                .setOngoing(true)
                .build();
    }

    private void updateNotification(String text) {
        getSystemService(NotificationManager.class)
                .notify(NOTIFICATION_ID, notification(text));
    }

    private void closeGatt() {
        readyToWrite = false;
        gattConnected = false;
        fixCharacteristic = null;
        if (gatt != null) {
            try {
                gatt.disconnect();
                gatt.close();
            } catch (Exception ignored) {}
            gatt = null;
        }
    }

    @Override
    public void onDestroy() {
        stopping = true;
        mainHandler.removeCallbacksAndMessages(null);
        if (locationManager != null) locationManager.removeUpdates(this);
        closeGatt();
        unregisterReceiver(bondReceiver);
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}