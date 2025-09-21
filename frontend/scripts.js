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

