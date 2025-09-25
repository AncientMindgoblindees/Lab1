// This is the "backend" for the webpage
// It handles the communication with the server and updates the UI

// Wait until the page loads before running JS
document.addEventListener('DOMContentLoaded', function() {
    const cf_switch = document.getElementById('celsius_fahrenheit');
    const tempButton = document.getElementById('get_temp_button');
    const tempDisplay = document.getElementById('temperature_value');
    
    // State variables
    let degState = "F";
    let currentTemp = null;
    let tempHist = []; // Array to hold temperature history, max size 300
    

    // Helper functions
    // Define the function we want to call
    async function getTemperature() {
        try {
            const response = await fetch("http://192.168.1.1/temp/stream"); 
            const data = await response.json();

            console.log("Received data: ", data);
            currentTemp = data.temp;


            tempHist.push(currentTemp);

            if(tempHist.length > 300){
                tempHist.shift(); // Remove oldest entry to maintain size
            }

            updateTemperatureDisplay();
        } catch (error) {
            console.error("Error fetching temperature:", error);
        }
    }

    function updateTemperatureDisplay() {
        if (currentTemp === null) {
            tempDisplay.textContent = "--";
            return;
        }

        let displayValue = currentTemp;

        if (degState === "C") {
            displayValue = ((currentTemp - 32) * (5/9)).toFixed(2);
            tempDisplay.textContent = displayValue + "°C";
        } else {
            displayValue = currentTemp.toFixed(2);
            tempDisplay.textContent = displayValue + "°F";
        }
    }

    async function convertTemperature() {
        if (degState === "F") {
            degState = "C";
            cf_switch.textContent = "Switch to °F";
        } else {
            degState = "F";
            cf_switch.textContent = "Switch to °C";
        }
        updateTemperatureDisplay();
    }
        
    // Attach the function to the button click
    tempButton.addEventListener('click', getTemperature);
    cf_switch.addEventListener('click', convertTemperature);
});


// Update every second
//setInterval(getTemperature, 1000);


