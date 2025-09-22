// This is the "backend" for the webpage
// It handles the communication with the server and updates the UI

// Wait until the HTML has loaded

document.addEventListener('DOMContentLoaded', function() {
    // Reference to the temperature span
    const tempDisplay = document.getElementById('temperature-value');
    
    // Set a temperature variable here
    const temperature = 75; // <-- you can change this number to test

    // Display it on the page
    tempDisplay.textContent = temperature + "°F";
});


// Wait until the page loads before running JS
document.addEventListener('DOMContentLoaded', function() {
    const toggleButton = document.getElementById('toggle-button');
    const tempDisplay = document.getElementById('temperature-value');

    // Define the function we want to call
    async function getTemperature() {
        try {
            const response = await fetch("http://192.168.1.1/temp"); 
            const data = await response.json(); 
            console.log("Received data: ", data);
            tempDisplay.textContent = data.temp + "°F";
        } catch (error) {
            console.error("Error fetching temperature:", error);
        }
    }

    // Attach the function to the button click
    toggleButton.addEventListener('click', getTemperature);
});


// Update every second
//setInterval(getTemperature, 1000);


