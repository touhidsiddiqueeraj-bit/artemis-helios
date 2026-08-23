import serial, time, re, csv, pathlib

port="/dev/ttyUSB0"
baud=115200
N=400
s=serial.Serial(port, baud, timeout=1.2)
s.dtr=False
s.rts=False
time.sleep(0.6)
s.reset_input_buffer()
print(f"Capturing {N} HEL->ART at {baud} 8N1...")
# Store cycles
records=[]
for i in range(N):
    s.write(b"HEL:VP=16.80,GP=642.0,AL=0.350\r\n")
    s.flush()
    # wait for ART and CYCLES (two lines)
    t0=time.time()
    buf=b""
    while time.time()-t0 < 1.5:
        if s.in_waiting:
            buf+=s.read(s.in_waiting)
            if b"CYCLES" in buf and b"ART:" in buf:
                # ensure we have complete line ending with \n
                if buf.count(b"\n")>=2:
                    break
        time.sleep(0.01)
    txt=buf.decode(errors='ignore')
    # parse CYCLES line
    m=re.search(r"CYCLES parse=(\d+) calc=(\d+) pwm=(\d+) ina=(\d+) tx=(\d+) tick=(\d+)", txt)
    if m:
        parse,calc,pwm,ina,tx,tick = map(int,m.groups())
        records.append((parse,calc,pwm,ina,tx,tick))
        if i%50==49:
            print(f"{i+1}/{N} tick {tick} cycles")
    else:
        print(f"[{i}] no CYCLES in {repr(txt[:120])}")
        # retry?
    time.sleep(0.08)  # 80ms gap + processing ~9ms = ~89ms, close to 100ms budget

s.close()
print(f"Captured {len(records)}/{N}")
if records:
    # convert cycles to ms at 72MHz
    def to_ms(c): return c/72000.0
    # compute stats
    import numpy as np
    arr=np.array(records, dtype=float)
    # arr columns: parse,calc,pwm,ina,tx,tick
    names=["parse","calc","pwm","ina","tx","tick"]
    for idx,name in enumerate(names):
        col=arr[:,idx]/72000*1000  # wait cycles/72 = us, /1000 = ms? Actually cycles/72 = us, /1000 = ms
        # Let's compute correctly: us = cycles/72, ms = us/1000 = cycles/72000
        ms = arr[:,idx]/72000.0
        print(f"{name:6s} mean {ms.mean():.4f} ms p99 {np.percentile(ms,99):.4f} max {ms.max():.4f}")
    # write csv in ms
    out=pathlib.Path("Code/Python/results/artemis_timing_real.csv")
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out,"w",newline="") as f:
        w=csv.writer(f)
        w.writerow(["parse_ms","calc_ms","pwm_ms","ina_ms","tx_ms","tick_ms"])
        for r in records:
            w.writerow([f"{c/72000:.6f}" for c in r])
    print(f"Wrote {out}")
    # also write summary for paper
    import numpy as np
    ms_arr=arr/72000.0
    summary_path=pathlib.Path("Code/Python/results/artemis_timing_summary_real.txt")
    with open(summary_path,"w") as f:
        for idx,name in enumerate(names):
            ms=ms_arr[:,idx]
            f.write(f"{name} mean {ms.mean():.6f} p50 {np.percentile(ms,50):.6f} p99 {np.percentile(ms,99):.6f} max {ms.max():.6f}\n")
    print(f"Wrote {summary_path}")

