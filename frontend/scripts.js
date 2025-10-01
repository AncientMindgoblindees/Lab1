// Client-side "data layer" + polling for temperature readings

document.addEventListener('DOMContentLoaded', () => {
    // ======== Config ========
    const TEMP_URL = "http://192.168.1.1/temp"; 
    const MAX_POINTS = 300; // last 300 seconds

    // ======== Elements ========
    const cfSwitchBtn  = document.getElementById('celsius_fahrenheit');
    const getTempBtn   = document.getElementById('get_temp_button');
    const tempDisplay  = document.getElementById('temperature-value');

    // ======== State ========
    // We'll store temperatures internally in Celsius so conversions are consistent.
    let degState = 'F'; // UI display state: 'F' or 'C'

    // Fixed-size circular buffer for { ts, celsius }
    const tempStore = {
        buf: new Array(MAX_POINTS),
        start: 0,        // index of oldest element
        size: 0,         // number of valid elements (<= MAX_POINTS)
        push(sample) {   // sample: { ts: number (ms), celsius: number }
            if (this.size < MAX_POINTS) {
                this.buf[(this.start + this.size) % MAX_POINTS] = sample;
                this.size++;
            } else {
                // Overwrite oldest (ring buffer)
                this.buf[this.start] = sample;
                this.start = (this.start + 1) % MAX_POINTS;
            }
        },
        latest() {
            if (this.size === 0) return null;
            const idx = (this.start + this.size - 1) % MAX_POINTS;
            return this.buf[idx];
        },
        // Optional helper if you want the whole window of points later (for graphing)
        toArray() {
            const out = [];
            for (let i = 0; i < this.size; i++) {
                out.push(this.buf[(this.start + i) % MAX_POINTS]);
            }
            return out;
        }
    };

    // Expose for quick debugging in console
    window.tempStore = tempStore;

    // ======== Helpers ========
    const fToC = f => (f - 32) * (5/9);
    const cToF = c => (c * (9/5)) + 32;

    function displayLatest() {
        const last = tempStore.latest();
        if (!last || last.celsius == null || !Number.isFinite(last.celsius)) {
            tempDisplay.textContent = '--';
            return;
        }
        const value = (degState === 'F') ? cToF(last.celsius) : last.celsius;
        tempDisplay.textContent = value.toFixed(2) + (degState === 'F' ? '°F' : '°C');
    }

    function pushMissingSample() {
        tempStore.push({ ts: Date.now(), celsius: null });
        displayLatest();
    }

    // Fetch once (manual button) — still stores to the buffer
    async function fetchTemperatureOnce() {
        try {
            const response = await fetch(TEMP_URL, { cache: 'no-store'});

            if (!response.ok) {  // HTTP error
                pushMissingSample();
                return;
            }

            const data = await response.json();

            // If your device sends Fahrenheit instead, convert here:
            // const celsius = fToC(parseFloat(data.temp));
            const celsius = parseFloat(data.temp);

            if (!Number.isFinite(celsius)) {
                pushMissingSample();         // malformed number -> missing
                return;
            }

            tempStore.push({ ts: Date.now(), celsius });
            displayLatest();

        } catch (err) {
            // Network/timeout/parse error -> store "missing" instead of logging noise
            pushMissingSample();
        }
    }

    // Toggle °F/°C without changing stored data
    function toggleUnit() {
        degState = (degState === 'F') ? 'C' : 'F';
        cfSwitchBtn.textContent = (degState === 'F') ? 'Switch to °C' : 'Switch to °F';
        displayLatest();
    }

    // ======== Wire up UI ========
    cfSwitchBtn.addEventListener('click', toggleUnit);
    getTempBtn.addEventListener('click', fetchTemperatureOnce);

    // ======== Auto-fetch on connect + every second ========
    // Immediately request once on page load:
    fetchTemperatureOnce();

    // Then poll every second to build the rolling 300-second window
    const timerId = setInterval(fetchTemperatureOnce, 1000);

    // Optional: pause when tab hidden to be nice to the ESP
    document.addEventListener('visibilitychange', () => {
        if (document.hidden) {
            clearInterval(timerId);
        } else {
            // Kick one fetch on return, then resume polling
            fetchTemperatureOnce();
            setInterval(fetchTemperatureOnce, POLL_MS);
        }
    });
});


