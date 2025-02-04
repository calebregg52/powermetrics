document.addEventListener('DOMContentLoaded', () => {
    // Configuration
    const voltageCount = 3;
    const currentCount = 2;
    const maxDataPoints = 50; // Number of data points per dataset
  
    // Define the original color palette for the datasets
    const colors = ['#007bff', '#dc3545', '#28a745', '#fd7e14', '#6f42c1'];
  
    // Global time variable and time step (in seconds)
    let t = 0;
    const dt = 0.5; // 0.5-second increments (update interval)
  
    // ----- Build Voltage Chart (Common Y-Axis on Both Sides) -----
    const voltageDatasets = [];
    // Define x-axis and two y-axes (yLeft and yRight) that share the same scale.
    const voltageScales = {
      x: {
        display: true,
        ticks: { color: '#333' },
        grid: { color: '#dee2e6' }
      },
      yLeft: {
        type: 'linear',
        position: 'left',
        ticks: { color: '#333' },
        grid: { color: '#dee2e6' }
      },
      yRight: {
        type: 'linear',
        position: 'right',
        ticks: { color: '#333' },
        grid: { display: false }
      }
    };
    for (let i = 0; i < voltageCount; i++) {
      voltageDatasets.push({
        label: `Voltage ${i + 1}`,
        data: Array(maxDataPoints).fill(0),
        borderColor: colors[i % colors.length],
        borderWidth: 2,
        fill: false,
        // Attach to the left axis; the right axis will mirror its scale.
        yAxisID: 'yLeft'
      });
    }
    const voltageData = {
      labels: Array(maxDataPoints).fill(''), // Will be updated with time labels
      datasets: voltageDatasets
    };
  
    // ----- Build Current Chart (Common Y-Axis on Both Sides) -----
    const currentDatasets = [];
    const currentScales = {
      x: {
        display: true,
        ticks: { color: '#333' },
        grid: { color: '#dee2e6' }
      },
      yLeft: {
        type: 'linear',
        position: 'left',
        ticks: { color: '#333' },
        grid: { color: '#dee2e6' }
      },
      yRight: {
        type: 'linear',
        position: 'right',
        ticks: { color: '#333' },
        grid: { display: false }
      }
    };
    for (let i = 0; i < currentCount; i++) {
      currentDatasets.push({
        label: `Current ${i + 1}`,
        data: Array(maxDataPoints).fill(0),
        borderColor: colors[i % colors.length],
        borderWidth: 2,
        fill: false,
        // Attach to the left axis.
        yAxisID: 'yLeft'
      });
    }
    const currentData = {
      labels: Array(maxDataPoints).fill(''), // Will be updated with time labels
      datasets: currentDatasets
    };
  
    // ----- Create the Voltage Chart -----
    const voltageCtx = document.getElementById('voltageGraph').getContext('2d');
    const voltageChart = new Chart(voltageCtx, {
      type: 'line',
      data: voltageData,
      options: {
        responsive: true,
        scales: voltageScales,
        plugins: {
          legend: { labels: { color: '#333' } }
        }
      }
    });
  
    // ----- Create the Current Chart -----
    const currentCtx = document.getElementById('currentGraph').getContext('2d');
    const currentChart = new Chart(currentCtx, {
      type: 'line',
      data: currentData,
      options: {
        responsive: true,
        scales: currentScales,
        plugins: {
          legend: { labels: { color: '#333' } }
        }
      }
    });
  
    // ----- Generate Toggle Controls (Using Bootstrap 5 Switches) -----
    function createToggles(containerId, channelCount, chartInstance) {
      const container = document.getElementById(containerId);
      for (let i = 0; i < channelCount; i++) {
        const formCheckDiv = document.createElement('div');
        formCheckDiv.className = 'form-check form-switch d-inline-block me-3';
  
        const checkbox = document.createElement('input');
        checkbox.className = 'form-check-input';
        checkbox.type = 'checkbox';
        checkbox.id = `${containerId}-channel-${i}`;
        checkbox.checked = true;
        checkbox.addEventListener('change', () => {
          chartInstance.data.datasets[i].hidden = !checkbox.checked;
          chartInstance.update();
        });
  
        const label = document.createElement('label');
        label.className = 'form-check-label';
        label.htmlFor = checkbox.id;
        label.style.color = chartInstance.data.datasets[i].borderColor;
        label.textContent = chartInstance.data.datasets[i].label;
  
        formCheckDiv.appendChild(checkbox);
        formCheckDiv.appendChild(label);
        container.appendChild(formCheckDiv);
      }
    }
    createToggles('voltage-toggles', voltageCount, voltageChart);
    createToggles('current-toggles', currentCount, currentChart);
  
    // ----- Dark Mode Toggle -----
    const darkModeToggle = document.getElementById('darkModeToggle');
    if (darkModeToggle) {
      darkModeToggle.addEventListener('change', (e) => {
        const darkMode = e.target.checked;
        document.body.classList.toggle('dark-mode', darkMode);
        const textColor = darkMode ? "#e0e0e0" : "#333";
  
        // Update each scale for the voltage chart.
        Object.keys(voltageChart.options.scales).forEach(key => {
          if (key === 'x') {
            voltageChart.options.scales[key].ticks.color = textColor;
            voltageChart.options.scales[key].grid.color = darkMode ? "#fff" : "#dee2e6";
          } else {
            voltageChart.options.scales[key].ticks.color = textColor;
            if (voltageChart.options.scales[key].grid) {
              voltageChart.options.scales[key].grid.color = darkMode ? "#fff" : "#dee2e6";
            }
          }
        });
        if (voltageChart.options.plugins.legend?.labels) {
          voltageChart.options.plugins.legend.labels.color = textColor;
        }
        voltageChart.update();
  
        // Update each scale for the current chart.
        Object.keys(currentChart.options.scales).forEach(key => {
          if (key === 'x') {
            currentChart.options.scales[key].ticks.color = textColor;
            currentChart.options.scales[key].grid.color = darkMode ? "#fff" : "#dee2e6";
          } else {
            currentChart.options.scales[key].ticks.color = textColor;
            if (currentChart.options.scales[key].grid) {
              currentChart.options.scales[key].grid.color = darkMode ? "#fff" : "#dee2e6";
            }
          }
        });
        if (currentChart.options.plugins.legend?.labels) {
          currentChart.options.plugins.legend.labels.color = textColor;
        }
        currentChart.update();
      });
    }
  
    // ----- Update Function: Generate Waveforms, Update Time Labels, and Dynamically Scale the Y-Axes -----
    function updateChartsAndGauges() {
      // -- Voltage Waveforms --
      // Voltage Channel 1: Sine wave (amplitude 10, frequency 0.2 Hz)
      const v1 = 10 * Math.sin(2 * Math.PI * 0.2 * t);
      // Voltage Channel 2: Square wave (amplitude 10)
      const v2 = (Math.sin(2 * Math.PI * 0.2 * t) >= 0) ? 10 : -10;
      // Voltage Channel 3: Triangle wave (amplitude 10, period = 5 s)
      const v3 = 4 * 10 * Math.abs((t / 5) - Math.floor(t / 5 + 0.5)) - 10;
  
      const voltageValues = [v1, v2, v3];
      for (let i = 0; i < voltageCount; i++) {
        const gaugeElem = document.getElementById(`voltage-value-${i + 1}`);
        if (gaugeElem) gaugeElem.textContent = `${voltageValues[i].toFixed(3)} V`;
        const alertElem = document.getElementById(`volt-alert-${i + 1}`);
        if (alertElem) {
          alertElem.textContent = (voltageValues[i] > 30 || voltageValues[i] < -30) ? 'OVERLOAD!' : '';
        }
        // For each voltage dataset, shift out the oldest data point and add the new value.
        const dataset = voltageChart.data.datasets[i];
        dataset.data.shift();
        dataset.data.push(voltageValues[i]);
      }
      // Update the common y-axis for the voltage chart based on all visible datasets.
      let visibleVoltageData = [];
      voltageChart.data.datasets.forEach(dataset => {
        if (!dataset.hidden) {
          visibleVoltageData = visibleVoltageData.concat(dataset.data);
        }
      });
      if (visibleVoltageData.length) {
        const minVal = Math.min(...visibleVoltageData);
        const maxVal = Math.max(...visibleVoltageData);
        let range = maxVal - minVal;
        let padding = range * 0.1;
        if (padding === 0) padding = 1;
        voltageChart.options.scales.yLeft.min = minVal - padding;
        voltageChart.options.scales.yLeft.max = maxVal + padding;
        // Mirror the same scale on the right axis.
        voltageChart.options.scales.yRight.min = minVal - padding;
        voltageChart.options.scales.yRight.max = maxVal + padding;
      }
      // Update the x-axis time labels.
      voltageChart.data.labels.shift();
      voltageChart.data.labels.push(t.toFixed(1) + ' s');
      voltageChart.update();
  
      // -- Current Waveforms --
      // Current Channel 1: Sine wave (amplitude 5, frequency 0.5 Hz)
      const i1 = 5 * Math.sin(2 * Math.PI * 0.5 * t);
      // Current Channel 2: Cosine wave (amplitude 5, frequency 0.5 Hz)
      const i2 = 5 * Math.cos(2 * Math.PI * 0.5 * t);
  
      const currentValues = [i1, i2];
      for (let i = 0; i < currentCount; i++) {
        const gaugeElem = document.getElementById(`current-value-${i + 1}`);
        if (gaugeElem) gaugeElem.textContent = `${currentValues[i].toFixed(3)} A`;
        const alertElem = document.getElementById(`curr-alert-${i + 1}`);
        if (alertElem) {
          alertElem.textContent = (currentValues[i] > 10 || currentValues[i] < -10) ? 'OVERLOAD!' : '';
        }
        const dataset = currentChart.data.datasets[i];
        dataset.data.shift();
        dataset.data.push(currentValues[i]);
      }
      // Update the common y-axis for the current chart based on all visible datasets.
      let visibleCurrentData = [];
      currentChart.data.datasets.forEach(dataset => {
        if (!dataset.hidden) {
          visibleCurrentData = visibleCurrentData.concat(dataset.data);
        }
      });
      if (visibleCurrentData.length) {
        const minVal = Math.min(...visibleCurrentData);
        const maxVal = Math.max(...visibleCurrentData);
        let range = maxVal - minVal;
        let padding = range * 0.1;
        if (padding === 0) padding = 1;
        currentChart.options.scales.yLeft.min = minVal - padding;
        currentChart.options.scales.yLeft.max = maxVal + padding;

        currentChart.options.scales.yRight.min = minVal - padding;
        currentChart.options.scales.yRight.max = maxVal + padding;
      }
      // Update the x-axis time labels for the current chart.
      currentChart.data.labels.shift();
      currentChart.data.labels.push(t.toFixed(1) + ' s');
      currentChart.update();
  
      // Increment time for the next update.
      t += dt;
    }
  
    // Update the charts every 500ms.
    setInterval(updateChartsAndGauges, 500);
  });
  
  
  
  

  