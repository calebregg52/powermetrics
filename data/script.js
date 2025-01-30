document.addEventListener('DOMContentLoaded', () => {
    const voltageCtx = document.getElementById('voltageGraph').getContext('2d');
    const voltageGauge = document.getElementById('voltage-value');
    const voltageAlert = document.getElementById('volt-alert');

    const voltageData = {
        labels: Array(50).fill(''),
        datasets: [{
            label: 'Voltage',
            data: Array(50).fill(0),
            borderColor: 'blue',
            borderWidth: 1,
            fill: false,
        }]
    };

    const voltageChart = new Chart(voltageCtx, {
        type: 'line',
        data: voltageData,
        options: {
            responsive: true,
            scales: {
                x: { display: false },
                y: {
                    beginAtZero: false, // Allow dynamic range
                }
            }
        }
    });

    // Establish WebSocket connection
    const ws = new WebSocket(`ws://${window.location.hostname}:81/`);

    ws.onopen = () => {
        console.log("WebSocket connected.");
    };

    ws.onmessage = (event) => {
        try {
            const data = JSON.parse(event.data);
            const voltage = parseFloat(data.voltage).toFixed(3);

            // Update the voltage chart
            voltageData.datasets[0].data.shift();
            voltageData.datasets[0].data.push(voltage);

            // Dynamically adjust Y-axis limits
            const minVoltage = Math.min(...voltageData.datasets[0].data);
            const maxVoltage = Math.max(...voltageData.datasets[0].data);

            if (maxVoltage < 1 && minVoltage > 0) {
                voltageChart.options.scales.y.min = 0;
                voltageChart.options.scales.y.max = 1;
            } else {
                const padding = 2; // Add padding for better visualization
                voltageChart.options.scales.y.min = Math.floor(minVoltage) - padding;
                voltageChart.options.scales.y.max = Math.ceil(maxVoltage) + padding;
            }

            voltageChart.update();

            // Update the voltage gauge
            voltageGauge.textContent = `${voltage} V`;

            // Overvoltage detection
            if (voltage > 30 || voltage < -30) {
                voltageAlert.textContent = 'OVERLOAD!';
                voltageAlert.style.color = 'red';
            } else {
                voltageAlert.textContent = '';
            }
        } catch (error) {
            console.error("Error parsing WebSocket data:", error);
        }
    };

    ws.onclose = () => {
        console.log("WebSocket disconnected.");
    };
});
