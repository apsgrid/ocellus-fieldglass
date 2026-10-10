import struct

# Protocol fixture mirrored by Android FieldglassProtocol.encodeFix() and
# firmware companion_location.cpp.
def encode(unix_s, lat, lon, accuracy_m, speed_mps=None, bearing_deg=None, mock=False):
    flags=0x01
    if speed_mps is not None: flags |= 0x02
    if bearing_deg is not None: flags |= 0x04
    if mock: flags |= 0x08
    return struct.pack(
        "<BBIiiHHH",
        0x11,
        flags,
        unix_s,
        round(lat*10_000_000),
        round(lon*10_000_000),
        max(0,min(65535,round(accuracy_m*10))),
        0 if speed_mps is None else max(0,min(65535,round(speed_mps*100))),
        0 if bearing_deg is None else max(0,min(35999,round(bearing_deg*100))),
    )

def decode(packet):
    assert len(packet)==20
    proto,flags,unix_s,lat_e7,lon_e7,acc_dm,speed_cms,bearing_cdeg=struct.unpack("<BBIiiHHH",packet)
    return {
        "proto":proto,
        "flags":flags,
        "unix_s":unix_s,
        "lat":lat_e7/10_000_000,
        "lon":lon_e7/10_000_000,
        "accuracy_m":acc_dm/10,
        "speed_mps":speed_cms/100,
        "bearing_deg":bearing_cdeg/100,
    }

def main():
    packet=encode(
        1760000000,
        51.5074,
        -0.1278,
        3.2,
        speed_mps=1.25,
        bearing_deg=270.5,
        mock=False,
    )
    assert len(packet)==20
    d=decode(packet)
    assert d["proto"]==0x11
    assert d["flags"]==0x07
    assert d["unix_s"]==1760000000
    assert abs(d["lat"]-51.5074)<1e-7
    assert abs(d["lon"]+0.1278)<1e-7
    assert d["accuracy_m"]==3.2
    assert d["speed_mps"]==1.25
    assert d["bearing_deg"]==270.5
    print("PASS: Fieldglass companion v1 packet is 20 bytes and round-trips")

if __name__=="__main__":
    main()