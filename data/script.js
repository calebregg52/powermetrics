document.addEventListener('DOMContentLoaded', () => {
  const voltageCtx = document.getElementById('voltageGraph').getContext('2d');
  const currentCtx = document.getElementById('currentGraph').getContext('2d');
  const voltageGauge = document.getElementById('voltage-value');
  const currentGauge = document.getElementById('current-value');
  const voltageAlert = document.getElementById('volt-alert');
  const currentAlert = document.getElementById('curr-alert');

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

  const currentData = {
      labels: Array(50).fill(''),
      datasets: [{
          label: 'Current',
          data: Array(50).fill(0),
          borderColor: 'green',
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

  const currentChart = new Chart(currentCtx, {
      type: 'line',
      data: currentData,
      options: {
          responsive: true,
          scales: {
              x: { display: false },
              y: { beginAtZero: true }
          }
      }
  });

  function updateData() {
      const randomVoltage = Math.random() * 60 - 30;
      const randomCurrent = Math.random() * 3;

      // Update the charts
      voltageData.datasets[0].data.shift();
      voltageData.datasets[0].data.push(randomVoltage);
      voltageChart.update();

      currentData.datasets[0].data.shift();
      currentData.datasets[0].data.push(randomCurrent);
      currentChart.update();

      // Update gauges
      voltageGauge.textContent = `${randomVoltage.toFixed(2)} V`;
      currentGauge.textContent = `${randomCurrent.toFixed(2)} A`;

      // Overvoltage detection
      if (randomVoltage > 30 || randomVoltage < -30) {
          voltageAlert.textContent = 'OVERLOAD!';
          voltageAlert.style.color = 'red';
      } else {
          voltageAlert.textContent = '';
      }

      // Overcurrent detection
      if (randomCurrent > 3) {
          currentAlert.textContent = 'OVERLOAD!';
          currentAlert.style.color = 'red';
      } else {
          currentAlert.textContent = '';
      }
  }

  setInterval(updateData, 1000);
});