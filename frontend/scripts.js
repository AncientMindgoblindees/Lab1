// Client-side "data layer" + polling for temperature readings

document.addEventListener('DOMContentLoaded', () => {
    // ======== Config ========
    const BASE_URL = "http://192.168.1.1"; 
    const MAX_POINTS = 300; // last 300 seconds

    // ======== Elements ========
    const cfSwitchBtn  = document.getElementById('celsius_fahrenheit');
    const sensor1Btn   = document.getElementById('sensor1_button');
    const sensor2Btn   = document.getElementById('sensor2_button');
    const sensor1Display = document.getElementById('sensor1_value');
    const sensor2Display = document.getElementById('sensor2_value');

    // ======== State ========
    // Track what unit the SERVER is currently sending
    let serverUnit = 'F'; // Server starts in Fahrenheit
    
    // Track what unit the UI should display (matches serverUnit)
    let degState = 'F';

    // Fixed-size circular buffer for { ts, celsius }
    // NOTE: Always stores temperature in Celsius regardless of what server sends
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
        const unit = serverUnit === 'F' ? '°F' : '°C';
        
        if (!last || !last.sensor1 || !last.sensor2) {
            // Handle missing data
            sensor1Display.textContent = last?.sensor1 ? `${last.sensor1.toFixed(2)}${unit}` : '--';
            sensor2Display.textContent = last?.sensor2 ? `${last.sensor2.toFixed(2)}${unit}` : '--';
            return;
        }
        
        // Display both sensor values
        sensor1Display.textContent = `${last.sensor1.toFixed(2)}${unit}`;
        sensor2Display.textContent = `${last.sensor2.toFixed(2)}${unit}`;
    }

    function pushMissingSample() {
        tempStore.push({ 
            ts: Date.now(), 
            celsius: null,
            sensor1: null,
            sensor2: null 
        });
        displayLatest();
    }

    // Fetch temperature from server
    async function fetchTemperatureOnce() {
        try {
            const response = await fetch(`${BASE_URL}/temp`, { cache: 'no-store'});

            if (!response.ok) {  // HTTP error
                pushMissingSample();
                return;
            }

            const data = await response.json();

            // Get individual sensor values (may be undefined if sensor is off/unplugged)
            const rawSensor1 = data.sensor1 !== undefined ? parseFloat(data.sensor1) : null;
            const rawSensor2 = data.sensor2 !== undefined ? parseFloat(data.sensor2) : null;
            const rawAverage = data.average !== undefined ? parseFloat(data.average) : null;

            // Convert to Celsius for storage if server is sending Fahrenheit
            const sensor1Celsius = rawSensor1 !== null && Number.isFinite(rawSensor1) 
                ? (serverUnit === 'F' ? fToC(rawSensor1) : rawSensor1) 
                : null;
            
            const sensor2Celsius = rawSensor2 !== null && Number.isFinite(rawSensor2)
                ? (serverUnit === 'F' ? fToC(rawSensor2) : rawSensor2)
                : null;
            
            const averageCelsius = rawAverage !== null && Number.isFinite(rawAverage)
                ? (serverUnit === 'F' ? fToC(rawAverage) : rawAverage)
                : null;

            // Store in buffer (celsius = average for graphing)
            tempStore.push({ 
                ts: Date.now(), 
                celsius: averageCelsius,
                sensor1: rawSensor1,  // Store raw values for display
                sensor2: rawSensor2
            });
            
            // Display the raw sensor values
            displayLatest();

        } catch (err) {
            // Network/timeout/parse error -> store "missing"
            pushMissingSample();
        }
    }

    // Toggle °F/°C - tells server to switch units
    function toggleUnit() {
        // Determine next mode (0 = Celsius, 1 = Fahrenheit)
        const newType = (degState === 'F') ? 0 : 1;

        // Send request to server
        fetch(`${BASE_URL}/toggle?tempType=${newType}`, { method: "POST" })
            .then(response => {
                if (!response.ok) throw new Error("Failed to toggle unit on server");
                return response.json();
            })
            .then(data => {
                // Update local state based on server response
                if (data.temp_type === "0" || data.temp_type === 0) {
                    serverUnit = 'C';
                    degState = 'C';
                    cfSwitchBtn.textContent = 'Switch to °F';
                } else if (data.temp_type === "1" || data.temp_type === 1) {
                    serverUnit = 'F';
                    degState = 'F';
                    cfSwitchBtn.textContent = 'Switch to °C';
                }
                displayLatest();
            })
            .catch(err => {
                console.error("Error toggling unit:", err);
            });
    }

    function toggleSensor(sensorId) {
        fetch(`${BASE_URL}/toggle?sensor=${sensorId}`, { method: "POST" })
            .catch(err => console.error(`Error toggling sensor ${sensorId}:`, err));
    }

    // ======== Wire up UI ========
    cfSwitchBtn.addEventListener('click', toggleUnit);
    sensor1Btn.addEventListener('click', () => toggleSensor(1));
    sensor2Btn.addEventListener('click', () => toggleSensor(2));

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
            setInterval(fetchTemperatureOnce, 1000);
        }
    });
});