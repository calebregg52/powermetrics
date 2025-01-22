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
                y: { beginAtZero: true }
            }
        }
    });

    // WebSocket setup
    const socket = new WebSocket(`ws://${window.location.hostname}:81`);

    socket.onmessage = (event) => {
        const data = JSON.parse(event.data);
        const voltage = data.voltage;

        // Update the voltage chart
        voltageData.datasets[0].data.shift();
        voltageData.datasets[0].data.push(voltage);
        voltageChart.update();

        // Update the voltage gauge
        voltageGauge.textContent = `${voltage.toFixed(2)} V`;

        // Overvoltage detection
        if (voltage > 30 || voltage < -30) {
            voltageAlert.textContent = 'OVERLOAD!';
            voltageAlert.style.color = 'red';
        } else {
            voltageAlert.textContent = '';
        }
    };

    socket.onopen = () => {
        console.log('WebSocket connection established');
    };

    socket.onerror = (error) => {
        console.error('WebSocket error:', error);
    };

    socket.onclose = () => {
        console.warn('WebSocket connection closed');
    };
});
