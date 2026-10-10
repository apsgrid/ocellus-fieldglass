import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
SAMPLE=ROOT/"sim"/"sample-fieldglass.jsonl"

required={"schema","event_id","session_id","sequence","kind","observer","time","privacy","payload"}
valid_kinds={"session.start","session.end","system.status","gnss.fix","location.fix","wifi.ap","wifi.station","wifi.frame_summary","wifi.channel_metric","wifi.csi_ref","ble.advertisement","bt.discovery","artifact.ref","annotation"}

def main():
    rows=[json.loads(x) for x in SAMPLE.read_text(encoding="utf-8").splitlines() if x.strip()]
    assert rows
    session=rows[0]["session_id"]
    for i,row in enumerate(rows):
        missing=required-set(row)
        assert not missing,(i,missing)
        assert row["schema"]=="fieldglass.event/1.0"
        assert row["kind"] in valid_kinds
        assert row["session_id"]==session
        assert row["sequence"]==i
        assert row["time"]["monotonic_ms"]>=0
    assert rows[0]["kind"]=="session.start"
    assert rows[-1]["kind"]=="session.end"
    print(f"PASS: {len(rows)} events, one coherent session")

if __name__=="__main__":
    main()