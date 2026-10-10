package com.ocellus.fieldglass.companion;

import android.location.Location;
import android.os.Build;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.UUID;

public final class FieldglassProtocol {
    private FieldglassProtocol() {}

    public static final UUID SERVICE_UUID =
            UUID.fromString("7b6f0001-8f60-4d55-a8b4-4649454c4447");
    public static final UUID FIX_UUID =
            UUID.fromString("7b6f0002-8f60-4d55-a8b4-4649454c4447");

    // BLE minimum ATT MTU is 23 bytes, which leaves a 20-byte characteristic payload.
    public static byte[] encodeFix(Location location) {
        ByteBuffer b = ByteBuffer.allocate(20).order(ByteOrder.LITTLE_ENDIAN);

        int flags = 0x01; // valid
        if (location.hasSpeed()) flags |= 0x02;
        if (location.hasBearing()) flags |= 0x04;
        boolean mock = Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                ? location.isMock()
                : location.isFromMockProvider();
        if (mock) flags |= 0x08;

        long unixSeconds = Math.max(0L, location.getTime() / 1000L);
        int latE7 = (int) Math.round(location.getLatitude() * 10_000_000.0);
        int lonE7 = (int) Math.round(location.getLongitude() * 10_000_000.0);
        int accuracyDm = clamp(Math.round(location.getAccuracy() * 10.0f), 0, 65535);
        int speedCms = location.hasSpeed()
                ? clamp(Math.round(location.getSpeed() * 100.0f), 0, 65535)
                : 0;
        int bearingCdeg = location.hasBearing()
                ? clamp(Math.round(location.getBearing() * 100.0f), 0, 35999)
                : 0;

        b.put((byte) 0x11); // protocol v1 / primary fix packet
        b.put((byte) flags);
        b.putInt((int) Math.min(unixSeconds, 0xFFFFFFFFL));
        b.putInt(latE7);
        b.putInt(lonE7);
        b.putShort((short) accuracyDm);
        b.putShort((short) speedCms);
        b.putShort((short) bearingCdeg);
        return b.array();
    }

    private static int clamp(int v, int lo, int hi) {
        return Math.max(lo, Math.min(hi, v));
    }
}