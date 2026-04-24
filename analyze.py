import pandas as pd

def analyze():
    try:
        specs = pd.read_csv('cpu_specs.csv').drop_duplicates(subset=['label'])
        data = pd.read_csv('latency_data.csv')
    except FileNotFoundError:
        print("Data files not found.")
        return

    # Use a median filter to remove random blips
    data['smooth'] = data['latency'].rolling(window=3, center=True).median().fillna(data['latency'])

    print("\n--- Intel Architecture Performance Analysis ---")

    # 1. Define Architectural Thresholds (for modern Intel CPUs)
    L3_MAX_EXPECTED = 65  # Cycles (L3 should never be slower than this)
    RAM_MIN_EXPECTED = 90 # Cycles (Anything above this is effectively System RAM)

    for _, row in specs.iterrows():
        label, spec_kb = row['label'], row['size_kb']

        # We only care about the L3 and RAM boundary for anomalies
        if "L3" in label:
            # CHECK A: Is the L3 plateau actually missing?
            # Look at a sample well within L3 range (e.g., 4MB)
            mid_l3 = data[(data['size_kb'] > 2048) & (data['size_kb'] < 5120)]
            if not mid_l3.empty and mid_l3['smooth'].mean() > RAM_MIN_EXPECTED:
                print(f"[!] CRITICAL ANOMALY: {label} Plateau Missing.")
                print(f"    Latency at 4MB is {int(mid_l3['smooth'].mean())} cycles (RAM speeds).")
                continue

            # CHECK B: Early Shift to RAM
            # Find the first point where latency crosses into RAM territory
            early_ram = data[(data['smooth'] >= RAM_MIN_EXPECTED) &
                             (data['size_kb'] < spec_kb * 0.95)]

            if not early_ram.empty:
                first_spike = early_ram.iloc[0]
                print(f"[!] ANOMALY: Early RAM Shift detected.")
                print(f"    {label} Spec is {spec_kb}KB, but hit RAM speeds at {int(first_spike['size_kb'])}KB.")
                print(f"    Measured Latency: {int(first_spike['smooth'])} cycles.")
                print("    Cause: Cache contention or 'Noisy Neighbors' stealing lines.")
            else:
                print(f"[OK] {label} ({spec_kb}KB) is reaching its full capacity at L3 speeds.")

        elif "L1" in label or "L2" in label:
            # Standard sanity check for L1/L2
            limit_data = data[data['size_kb'] <= spec_kb].tail(1)
            if not limit_data.empty and limit_data['smooth'].iloc[0] < L3_MAX_EXPECTED:
                print(f"[OK] {label} ({spec_kb}KB) performing normally.")

if __name__ == "__main__":
    analyze()
