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
    let serverUnit = 'F'; // Will be updated from server on load
    
    // Track what unit the UI should display (matches serverUnit)
    let degState = 'F'; // Will be updated from server on load

    // Function to sync state with server
    async function syncStateWithServer() {
        try {
            const response = await fetch(`${BASE_URL}/status`, { cache: 'no-store' });
            if (!response.ok) return;
            
            const data = await response.json();
            
            // Update state based on server's tempType (0 = C, 1 = F)
            if (data.tempType === 0 || data.tempType === "0") {
                serverUnit = 'C';
                degState = 'C';
                cfSwitchBtn.textContent = 'Switch to °F';
            } else if (data.tempType === 1 || data.tempType === "1") {
                serverUnit = 'F';
                degState = 'F';
                cfSwitchBtn.textContent = 'Switch to °C';
            }
        } catch (err) {
            console.error("Error syncing state with server:", err);
        }
    }

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
            
            // Calculate average for graphing:
            // 1. Use server's average if provided
            // 2. If only one sensor active, use that sensor
            // 3. If both active but no average, calculate it
            // 4. If neither active, null
            let averageCelsius = null;
            if (rawAverage !== null && Number.isFinite(rawAverage)) {
                averageCelsius = serverUnit === 'F' ? fToC(rawAverage) : rawAverage;
            } else if (sensor1Celsius !== null && sensor2Celsius !== null) {
                averageCelsius = (sensor1Celsius + sensor2Celsius) / 2;
            } else if (sensor1Celsius !== null) {
                averageCelsius = sensor1Celsius;
            } else if (sensor2Celsius !== null) {
                averageCelsius = sensor2Celsius;
            }

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
    // First, sync state with server, then start fetching
    syncStateWithServer().then(() => {
        // Immediately request once on page load:
        fetchTemperatureOnce();

        // Then poll every second to build the rolling 300-second window
        const timerId = setInterval(() => {
            fetchTemperatureOnce();
            drawGraph();
        }, 1000);

        // Optional: pause when tab hidden to be nice to the ESP
        document.addEventListener('visibilitychange', () => {
            if (document.hidden) {
                clearInterval(timerId);
            } else {
                // Kick one fetch on return, then resume polling
                fetchTemperatureOnce();
                setInterval(() => {
                    fetchTemperatureOnce();
                    drawGraph();
                }, 1000);
            }
        });
    });

    // ======== GRAPH FUNCTIONALITY ========
    const canvas = document.getElementById('tempGraph');
    const ctx = canvas.getContext('2d');

    // Size canvas to full width
    function resizeCanvas() {
        canvas.width = window.innerWidth - 40; // Leave some margin
        canvas.height = 400;
        drawGraph();
    }

    // Call on load and resize
    resizeCanvas();
    window.addEventListener('resize', resizeCanvas);

    function drawGraph() {
        if (!canvas || !ctx) return;

        // Clear canvas
        ctx.fillStyle = '#fafafa';
        ctx.fillRect(0, 0, canvas.width, canvas.height);

        // Margins
        const margin = { top: 30, right: 30, bottom: 50, left: 60 };
        const width = canvas.width - margin.left - margin.right;
        const height = canvas.height - margin.top - margin.bottom;

        // Temperature range based on current unit
        const minTemp = degState === 'F' ? 50 : 10;
        const maxTemp = degState === 'F' ? 122 : 50;
        const unit = degState === 'F' ? '°F' : '°C';

        // Helper functions
        function tempToY(celsius) {
            // celsius is stored value, need to check range in celsius
            if (celsius < 10 || celsius > 50) return null; // Off-scale
            
            const ratio = (celsius - 10) / (50 - 10);
            return margin.top + height - (ratio * height);
        }

        function secondsToX(secondsAgo) {
            const ratio = (300 - secondsAgo) / 300;
            return margin.left + (ratio * width);
        }

        // Get all data points
        const dataPoints = tempStore.toArray();
        const now = Date.now();

        // Draw title
        ctx.fillStyle = '#333';
        ctx.font = 'bold 16px Arial';
        ctx.textAlign = 'center';
        ctx.fillText(`Temperature vs Time (${unit})`, canvas.width / 2, 20);

        // Draw axes
        ctx.strokeStyle = '#333';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(margin.left, margin.top);
        ctx.lineTo(margin.left, margin.top + height);
        ctx.lineTo(margin.left + width, margin.top + height);
        ctx.stroke();

        // Draw Y-axis labels and grid
        ctx.fillStyle = '#666';
        ctx.font = '12px Arial';
        ctx.textAlign = 'right';
        ctx.strokeStyle = '#e0e0e0';
        ctx.lineWidth = 1;

        const tempStep = degState === 'F' ? 20 : 10;
        for (let temp = minTemp; temp <= maxTemp; temp += tempStep) {
            // Convert display temp back to celsius for Y calculation
            const celsiusForY = degState === 'F' ? fToC(temp) : temp;
            const y = tempToY(celsiusForY);
            
            if (y !== null) {
                ctx.fillText(temp + unit, margin.left - 10, y + 4);
                
                // Grid line
                ctx.beginPath();
                ctx.moveTo(margin.left, y);
                ctx.lineTo(margin.left + width, y);
                ctx.stroke();
            }
        }

        // Y-axis label
        ctx.save();
        ctx.translate(15, canvas.height / 2);
        ctx.rotate(-Math.PI / 2);
        ctx.textAlign = 'center';
        ctx.fillStyle = '#333';
        ctx.font = 'bold 14px Arial';
        ctx.fillText(`Temperature (${unit})`, 0, 0);
        ctx.restore();

        // Draw X-axis labels
        ctx.fillStyle = '#666';
        ctx.font = '12px Arial';
        ctx.textAlign = 'center';
        ctx.strokeStyle = '#333';

        for (let seconds = 0; seconds <= 300; seconds += 60) {
            const x = secondsToX(seconds);
            const label = seconds === 0 ? 'now' : seconds;
            ctx.fillText(label, x, margin.top + height + 25);

            // Tick mark
            ctx.beginPath();
            ctx.moveTo(x, margin.top + height);
            ctx.lineTo(x, margin.top + height + 5);
            ctx.stroke();
        }

        // X-axis label
        ctx.fillStyle = '#333';
        ctx.font = 'bold 14px Arial';
        ctx.fillText('Seconds Ago', canvas.width / 2, canvas.height - 5);

        // Draw temperature line
        ctx.strokeStyle = '#2196F3';
        ctx.lineWidth = 2;
        ctx.lineJoin = 'round';

        let lastValidPoint = null;

        for (let i = 0; i < dataPoints.length; i++) {
            const point = dataPoints[i];
            const secondsAgo = Math.floor((now - point.ts) / 1000);
            
            // Skip if beyond 300 seconds
            if (secondsAgo > 300) continue;
            
            const x = secondsToX(secondsAgo);

            // Check if point is valid (not null and in range)
            const isValid = point.celsius !== null && 
                          point.celsius >= 10 && 
                          point.celsius <= 50;

            if (isValid) {
                const y = tempToY(point.celsius);
                
                if (lastValidPoint !== null) {
                    // Draw line from last valid point to this one
                    ctx.beginPath();
                    ctx.moveTo(lastValidPoint.x, lastValidPoint.y);
                    ctx.lineTo(x, y);
                    ctx.stroke();
                }

                // Draw point
                ctx.fillStyle = '#2196F3';
                ctx.beginPath();
                ctx.arc(x, y, 2, 0, Math.PI * 2);
                ctx.fill();

                lastValidPoint = { x, y };
            } else {
                // Invalid point (null or off-scale) - creates gap
                lastValidPoint = null;
            }
        }
    }

    // Initial draw
    drawGraph();
});