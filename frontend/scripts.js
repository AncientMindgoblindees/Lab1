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

    // ========= Alert UI Elements ========
    const minTempInput = document.getElementById('min_temp');
    const maxTempInput = document.getElementById('max_temp');
    const recipientInput = document.getElementById('recipient');
    const alertMessageInput = document.getElementById('alert_message');

    // ======== State ========
    // Will be updated from server on load
    let serverState = 'F';
    let clientState = 'F';
    let alertSent = false;

    // EmailJS configuration
    function sendEmailAlert(recipient, message, sensor1, sensor2){
        emailjs.send("service_j2b0teh","template_6dhvt9g",{
            to_email: recipient,
            message: message,
            date: new Date().toLocaleString(),
            sensor1: sensor1 !== null ? sensor1.toFixed(2) : "no data",
            sensor2: sensor2 !== null ? sensor2.toFixed(2) : "no data"
        })
        .then(() => console.log("Alert email sent"))
        .catch(err => console.error("Error sending alert email:", err));
    }

    //Alert checking function
    function checkAlerts(last){
        const min = parseFloat(minTempInput.value);
        const max = parseFloat(maxTempInput.value);
        const recipient = recipientInput.value.trim();
        const message = alertMessageInput.value.trim();

        if (!recipient || !message || isNaN(min) || isNaN(max)) return;
        
        const outOfRange = 
            (last.sensor1Celsius !== null && (last.sensor1Celsius < min || last.sensor1Celsius > max)) ||
            (last.sensor2Celsius !== null && (last.sensor2Celsius < min || last.sensor2Celsius > max));

        if (outOfRange && !alertSent) {
            sendEmailAlert(recipient, message, last.sensor1Celsius, last.sensor2Celsius);
            alertSent = true;
        } else if (!outOfRange) {
            alertSent = false; // Reset alert if back in range
        }
    }

    // Helper function to sync the local state and server state
    async function syncStateWithServer() {
        try {
            const response = await fetch(`${BASE_URL}/status`, { cache: 'no-store' });
            if (!response.ok) return;
            
            const data = await response.json();
            
            // Update state based on server's tempType (0 = C, 1 = F)
            if (data.tempType === 0 || data.tempType === "0") {
                serverState = 'C';
                clientState = 'C';
                cfSwitchBtn.textContent = 'Switch to °F';
            } else if (data.tempType === 1 || data.tempType === "1") {
                serverState = 'F';
                clientState = 'F';
                cfSwitchBtn.textContent = 'Switch to °C';
            }
        } catch (err) {
            console.error("Error syncing state with server:", err);
        }
    }

    // Fixed-size ring buffer for { ts, celsius }
    // Temperatures stored in tempStore are always converted to Celsius for consistency
    const tempStore = {
        buf: new Array(MAX_POINTS),
        start: 0,        // index of oldest element
        size: 0,         // number of elements (<= MAX_POINTS)
        push(sample) {   // sample format = { ts: number (ms), celsius: number }
            if (this.size < MAX_POINTS) {
                this.buf[(this.start + this.size) % MAX_POINTS] = sample;
                this.size++;
            } else {
                // Overwrite oldest (i.e. ring buffer)
                this.buf[this.start] = sample;
                this.start = (this.start + 1) % MAX_POINTS;
            }
        },
        // Get latest sample or null if empty
        latest() {
            if (this.size === 0) return null;
            const idx = (this.start + this.size - 1) % MAX_POINTS;
            return this.buf[idx];
        },
        // Get all samples in chronological order
        toArray() {
            const out = [];
            for (let i = 0; i < this.size; i++) {
                out.push(this.buf[(this.start + i) % MAX_POINTS]);
            }
            return out;
        }
    };

    // This line allows you to use tempStore methods from the console for testing
    //window.tempStore = tempStore;

    // ======== Helpers ========
    const fToC = f => (f - 32) * (5/9);
    const cToF = c => (c * (9/5)) + 32;

    function displayLatest() {
        const last = tempStore.latest();
        const unit = serverState === 'F' ? '°F' : '°C';
        
        // Check for unplugged sensors (-196.6°F or -127.0°C)
        const sensor1Unplugged = last.rawSensor1 !== null && 
       ((serverState === 'F' && Math.abs(last.rawSensor1 - (-196.6)) < 0.1) || 
        (serverState === 'C' && Math.abs(last.rawSensor1 - (-127.0)) < 0.1));
        
        const sensor2Unplugged = last.rawSensor2 !== null && 
       ((serverState === 'F' && Math.abs(last.rawSensor2 - (-196.6)) < 0.1) || 
        (serverState === 'C' && Math.abs(last.rawSensor2 - (-127.0)) < 0.1));    
        
        // Display sensor 1
        if (sensor1Unplugged) {
            sensor1Display.textContent = 'unplugged sensor';
        } else if (!last || last.sensor1 === null) {
            sensor1Display.textContent = 'no data available';
        } else {
            sensor1Display.textContent = `${last.sensor1.toFixed(2)}${unit}`;
        }
        
        // Display sensor 2
        if (sensor2Unplugged) {
            sensor2Display.textContent = 'unplugged sensor';
        } else if (!last || last.sensor2 === null) {
            sensor2Display.textContent = 'no data available';
        } else {
            sensor2Display.textContent = `${last.sensor2.toFixed(2)}${unit}`;
        }
    }

    function pushMissingSample() {
        tempStore.push({ 
            ts: Date.now(), 
            celsius: null,
            sensor1: null,
            sensor2: null,
            sensor1Celsius: null,
            sensor2Celsius: null
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

            // Check for unplugged sensors (-196.6°F or -127.0°C)
            const sensor1Unplugged = rawSensor1 !== null && 
                ((serverState === 'F' && Math.abs(rawSensor1 - (-196.6)) < 0.1) || 
                 (serverState === 'C' && Math.abs(rawSensor1 - (-127.0)) < 0.1));
            
            const sensor2Unplugged = rawSensor2 !== null && 
                ((serverState === 'F' && Math.abs(rawSensor2 - (-196.6)) < 0.1) || 
                 (serverState === 'C' && Math.abs(rawSensor2 - (-127.0)) < 0.1));

            // Convert to celsius for tempStore if data is in fahrenheit
            const sensor1Celsius = !sensor1Unplugged && rawSensor1 !== null && Number.isFinite(rawSensor1) 
                ? (serverState === 'F' ? fToC(rawSensor1) : rawSensor1) 
                : null;
            
            const sensor2Celsius = !sensor2Unplugged && rawSensor2 !== null && Number.isFinite(rawSensor2)
                ? (serverState === 'F' ? fToC(rawSensor2) : rawSensor2)
                : null;
            
            // Calculate average for graphing:
            // 1. Use server's average if provided
            // 2. If only one sensor active, use that sensor
            // 3. If both active but no average, calculate it
            // 4. If neither active, null
            let averageCelsius = null;
            if (rawAverage !== null && Number.isFinite(rawAverage)) {
                averageCelsius = serverState === 'F' ? fToC(rawAverage) : rawAverage;
            } else if (sensor1Celsius !== null && sensor2Celsius !== null) {
                averageCelsius = (sensor1Celsius + sensor2Celsius) / 2;
            } else if (sensor1Celsius !== null) {
                averageCelsius = null; // Change to sensor1Celsius if you want to show single sensor as average
            } else if (sensor2Celsius !== null) {
                averageCelsius = null; // Change to sensor2Celsius if you want to show single sensor as average
            }

            // Store in buffer (celsius = average for backward compatibility)
            tempStore.push({ 
                ts: Date.now(), 
                celsius: averageCelsius,
                sensor1: rawSensor1,  // Store raw values for display
                sensor2: rawSensor2,
                sensor1Celsius: sensor1Celsius,  // Store Celsius for graphing
                sensor2Celsius: sensor2Celsius   // Store Celsius for graphing
            });
            
            // Display the raw sensor values
            displayLatest();

            // Check alerts
            const last = tempStore.latest();
            if (last) checkAlerts(last);

        } catch (err) {
            // Network/timeout/parse error -> store "missing"
            pushMissingSample();
        }
    }

    // Toggle °F/°C - tells server to switch units
    function toggleUnit() {
        // Determine next mode (0 = Celsius, 1 = Fahrenheit)
        const newType = (clientState === 'F') ? 0 : 1;

        // Send request to server
        fetch(`${BASE_URL}/toggle?tempType=${newType}`, { method: "POST" })
            .then(response => {
                if (!response.ok) throw new Error("Failed to toggle unit on server");
                return response.json();
            })
            .then(data => {
                // Update local state based on server response
                if (data.temp_type === "0" || data.temp_type === 0) {
                    serverState = 'C';
                    clientState = 'C';
                    cfSwitchBtn.textContent = 'Switch to °F';
                } else if (data.temp_type === "1" || data.temp_type === 1) {
                    serverState = 'F';
                    clientState = 'F';
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

        // Then fetch every second to fill up tempStore and update display
        let timerId = setInterval(() => {
            fetchTemperatureOnce();
            drawGraph();
        }, 1000);

        // Optional: pause when tab hidden to be nice to the ESP
        document.addEventListener('visibilitychange', () => {
            if (document.hidden) {
                clearInterval(timerId);
            } else {
                // Fetch temperature once on return, then resume polling every second
                fetchTemperatureOnce();
                drawGraph();
                timerId = setInterval(() => {
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
        const minTemp = clientState === 'F' ? 50 : 10;
        const maxTemp = clientState === 'F' ? 122 : 50;
        const unit = clientState === 'F' ? '°F' : '°C';

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

        const tempStep = clientState === 'F' ? 20 : 10;
        for (let temp = minTemp; temp <= maxTemp; temp += tempStep) {
            // Convert display temp back to celsius for Y calculation
            const celsiusForY = clientState === 'F' ? fToC(temp) : temp;
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

        // Draw temperature lines for both sensors and average
        // Sensor 1 - Blue line
        drawSensorLine(dataPoints, now, '#2196F3', 'sensor1Celsius');
        
        // Sensor 2 - Orange line
        drawSensorLine(dataPoints, now, '#FF9800', 'sensor2Celsius');
        
        // Average - Green line (only when both sensors active)
        drawSensorLine(dataPoints, now, '#4CAF50', 'celsius');

        // Draw legend
        const legendX = margin.left + 20;
        const legendY = margin.top + 20;
        
        // Sensor 1
        ctx.strokeStyle = '#2196F3';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(legendX, legendY);
        ctx.lineTo(legendX + 30, legendY);
        ctx.stroke();
        ctx.fillStyle = '#333';
        ctx.font = '12px Arial';
        ctx.textAlign = 'left';
        ctx.fillText('Sensor 1', legendX + 35, legendY + 4);
        
        // Sensor 2
        ctx.strokeStyle = '#FF9800';
        ctx.beginPath();
        ctx.moveTo(legendX, legendY + 20);
        ctx.lineTo(legendX + 30, legendY + 20);
        ctx.stroke();
        ctx.fillText('Sensor 2', legendX + 35, legendY + 24);
        
        // Average
        ctx.strokeStyle = '#4CAF50';
        ctx.beginPath();
        ctx.moveTo(legendX, legendY + 40);
        ctx.lineTo(legendX + 30, legendY + 40);
        ctx.stroke();
        ctx.fillText('Average', legendX + 35, legendY + 44);
    }

    // Helper function to draw a single sensor line
    function drawSensorLine(dataPoints, now, color, sensorField) {
        const margin = { top: 30, right: 30, bottom: 50, left: 60 };
        const width = canvas.width - margin.left - margin.right;
        const height = canvas.height - margin.top - margin.bottom;

        function tempToY(celsius) {
            if (celsius < 10 || celsius > 50) return null;
            const ratio = (celsius - 10) / (50 - 10);
            return margin.top + height - (ratio * height);
        }

        function secondsToX(secondsAgo) {
            const ratio = (300 - secondsAgo) / 300;
            return margin.left + (ratio * width);
        }

        ctx.strokeStyle = color;
        ctx.lineWidth = 2;
        ctx.lineJoin = 'round';

        let lastValidPoint = null;

        for (let i = 0; i < dataPoints.length; i++) {
            const point = dataPoints[i];
            const secondsAgo = Math.floor((now - point.ts) / 1000);
            
            if (secondsAgo > 300) continue;
            
            const x = secondsToX(secondsAgo);
            const celsius = point[sensorField];

            // Check if point is valid (not null and in range)
            const isValid = celsius !== null && 
                          celsius >= 10 && 
                          celsius <= 50;

            if (isValid) {
                const y = tempToY(celsius);
                
                if (lastValidPoint !== null) {
                    ctx.beginPath();
                    ctx.moveTo(lastValidPoint.x, lastValidPoint.y);
                    ctx.lineTo(x, y);
                    ctx.stroke();
                }

                // Draw point
                ctx.fillStyle = color;
                ctx.beginPath();
                ctx.arc(x, y, 2, 0, Math.PI * 2);
                ctx.fill();

                lastValidPoint = { x, y };
            } else {
                lastValidPoint = null;
            }
        }
    }

    // Initial draw
    drawGraph();
});