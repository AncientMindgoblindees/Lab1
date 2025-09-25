// This is the "backend" for the webpage
// It handles the communication with the server and updates the UI

// Wait until the page loads before running JS
document.addEventListener('DOMContentLoaded', function() {
    const cf_switch = document.getElementById('celsius_fahrenheit');
    const tempButton = document.getElementById('get_temp_button');
    const tempDisplay = document.getElementById('temperature-value');
    
    // Set a temperature variable here
    const temperature = 75; // <-- you can change this number to test

    // Display it on the page
    tempDisplay.textContent = temperature + "°F";

    const degState = "F"; // "F" or "C

    
    // Define the function we want to call
    async function getTemperature() {
        try {
            const response = await fetch("http://192.168.1.1/temp/stream"); 
            const data = await response.json(); 
            console.log("Received data: ", data);
            tempDisplay.textContent = data.temp + "°F";
        } catch (error) {
            console.error("Error fetching temperature:", error);
        }
    }



    async function convertTemperature() {
        if (degState === "F") {
            let currentTemp = parseFloat(tempDisplay.textContent);
            let celsiusTemp = ((currentTemp - 32) * (5/9)).toFixed(2);
            tempDisplay.textContent = celsiusTemp + "°C";
            degState = "C";
            cf_switch.textContent = "Switch to °F";
        } else {
            let currentTemp = parseFloat(tempDisplay.textContent);
            let fahrenheitTemp = ((currentTemp * (9/5)) + 32).toFixed(2);
            tempDisplay.textContent = fahrenheitTemp + "°F";
            degState = "F";
            cf_switch.textContent = "Switch to °C";
        }
    }
        
    // Attach the function to the button click
    tempButton.addEventListener('click', getTemperature);
    cf_switch.addEventListener('click', convertTemperature);
});


// Update every second
//setInterval(getTemperature, 1000);


