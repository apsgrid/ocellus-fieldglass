import json, random, time, uuid
from datetime import datetime, timezone
from pathlib import Path

OUT = Path(__file__).with_name("sample-fieldglass.jsonl")
session = str(uuid.uuid4())
observer = {"id":"fieldglass-sim-01","hardware":"simulator","firmware":"0.1.0"}

def now():
    return datetime.now(timezone.utc).isoformat().replace("+00:00","Z")

def base(seq, kind):
    return {
        "schema":"fieldglass.event/1.0",
        "event_id":str(uuid.uuid4()),
        "session_id":session,
        "sequence":seq,
        "kind":kind,
        "observer":observer,
        "time":{"utc":now(),"monotonic_ms":seq*850,"quality":"gnss"},
        "location":{
            "lat":0.0 + random.uniform(-.004,.004),
            "lon":0.0 + random.uniform(-.004,.004),
            "alt_m":40.0,
            "accuracy_m":3.5,
            "speed_mps":11.0,
            "heading_deg":90.0,
            "satellites":12,
            "hdop":0.8,
            "source":"gnss"
        },
        "privacy":{"mode":"raw","location":"precise","ssid_redacted":False}
    }

events=[]
e=base(0,"session.start"); e["payload"]={"profile":"DRIVE"}; events.append(e)
for i in range(1,26):
    kind=["wifi.ap","ble.advertisement","bt.discovery"][i%3]
    e=base(i,kind)
    e["signal"]={"rssi_dbm":random.randint(-85,-35),"channel":random.choice([1,6,11]),"frequency_mhz":2437}
    if kind=="wifi.ap":
        e["payload"]={"bssid":f"02:00:00:00:00:{i:02x}","ssid":f"Sample-{i%5}","security":"WPA2","locally_administered":True,"ie_fingerprint":f"{random.getrandbits(32):08x}"}
    elif kind=="ble.advertisement":
        e["payload"]={"address":f"DA:7A:00:00:00:{i:02x}","address_type":"random","name":f"BLE-{i%4}","manufacturer_id":76,"adv_fingerprint":f"{random.getrandbits(32):08x}"}
    else:
        e["payload"]={"address":f"00:11:22:33:44:{i:02x}","name":f"BT-{i%3}","class_of_device":2360324}
    events.append(e)
e=base(26,"session.end"); e["payload"]={"reason":"normal"}; events.append(e)

OUT.write_text("\n".join(json.dumps(x,separators=(",",":")) for x in events)+"\n",encoding="utf-8")
print(f"wrote {len(events)} events to {OUT}")