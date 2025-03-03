document.addEventListener('DOMContentLoaded', () => {
  const voltageCount = 3;
  const currentCount = 2;
  const maxDataPoints = 50;
  const colors = ['#1e90ff', '#ff4757', '#2ed573', '#ff9f43', '#9c88ff'];

  let t = 0;
  let dt = 1;

  const charts = {
    voltage: { chart: null, timescale: dt, zoom: 1, isPaused: false, triggerChannel: 0, triggerLevel: 0, triggered: false },
    current: { chart: null, timescale: dt, zoom: 1, isPaused: false, triggerChannel: 0, triggerLevel: 0, triggered: false }
  };

  const stats = {
    voltage: Array(voltageCount).fill().map(() => ({ min: Infinity, max: -Infinity, sum: 0, count: 0 })),
    current: Array(currentCount).fill().map(() => ({ min: Infinity, max: -Infinity, sum: 0, count: 0 }))
  };

  const socket = new WebSocket(`ws://${window.location.hostname}:81/`);
  const statusIndicator = document.getElementById('websocket-status');

  socket.onopen = () => {
      console.log("WebSocket connected");
      statusIndicator.textContent = "Circuit: Connected";
      statusIndicator.classList.add('connected');
  };

  socket.onclose = () => {
      console.log("WebSocket disconnected");
      statusIndicator.textContent = "Circuit: Disconnected";
      statusIndicator.classList.remove('connected');
  };

  socket.onerror = (error) => console.log("WebSocket error:", error);

  document.getElementById('darkModeToggle').addEventListener('change', () => {
    document.body.classList.toggle('dark-mode');
    updateChartColors();
  });

  charts.voltage.chart = new Chart(document.getElementById('voltageGraph').getContext('2d'), {
    type: 'line',
    data: {
      labels: Array(maxDataPoints).fill(''),
      datasets: [
        { label: 'Voltage 1', data: Array(maxDataPoints).fill(0), borderColor: colors[0], borderWidth: 2, fill: false, yAxisID: 'yLeft' },
        { label: 'Voltage 3', data: Array(maxDataPoints).fill(0), borderColor: colors[1], borderWidth: 2, fill: false, yAxisID: 'yLeft' },
        { label: 'Voltage 4', data: Array(maxDataPoints).fill(0), borderColor: colors[2], borderWidth: 2, fill: false, yAxisID: 'yLeft' }
      ]
    },
    options: getChartOptions()
  });

  charts.current.chart = new Chart(document.getElementById('currentGraph').getContext('2d'), {
    type: 'line',
    data: { labels: Array(maxDataPoints).fill(''), datasets: createDatasets(currentCount, 0, 'Current') },
    options: getChartOptions()
  });

  // Fetch and display channel toggles in the modal
  fetch('/get-channels')
    .then(response => response.json())
    .then(channels => {
      const voltageToggles = document.getElementById('voltage-channel-toggles');
      const currentToggles = document.getElementById('current-channel-toggles');

      channels.forEach((channel, index) => {
        const container = channel.name.startsWith('voltage') ? voltageToggles : currentToggles;
        const div = document.createElement('div');
        div.className = 'form-check form-switch d-inline-block me-3';
        const checkbox = document.createElement('input');
        checkbox.className = 'form-check-input';
        checkbox.type = 'checkbox';
        checkbox.id = `channel-toggle-${channel.name}`;
        checkbox.checked = channel.enabled;
        checkbox.addEventListener('change', () => {
          fetch('/set-channel', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: `name=${channel.name}&enabled=${checkbox.checked}`
          }).then(response => {
            if (response.ok) console.log(`Channel ${channel.name} updated`);
          });
        });
        const label = document.createElement('label');
        label.className = 'form-check-label';
        label.htmlFor = checkbox.id;
        label.textContent = channel.name.replace('_', ' ').toUpperCase();
        div.appendChild(checkbox);
        div.appendChild(label);
        container.appendChild(div);
      });
    });

  // Create only the chart visibility toggles (colored ones)
  createToggles('voltage-toggles', voltageCount, charts.voltage.chart);
  createToggles('current-toggles', currentCount, charts.current.chart);

  socket.onmessage = (event) => {
    try {
      const data = JSON.parse(event.data);
      const voltageValues = [data.voltage_1 || 0, data.voltage_3 || 0, data.voltage_4 || 0];
      const currentValues = [data.current_1 || 0, data.current_2 || 0];

      // Threshold Alerts and Stats for Voltage (fixed mapping)
      voltageValues.forEach((v, i) => {
        const idNum = i === 0 ? 1 : i === 1 ? 3 : 4;  // Map to 1, 3, 4
        const threshold = parseFloat(document.getElementById(`volt-threshold-${idNum}`).value) || Infinity;
        document.getElementById(`volt-alert-${idNum}`).textContent = v > threshold ? "Over Voltage!" : "";
        updateStats(stats.voltage[i], v, `volt-stats-${idNum}`);
      });

      // Threshold Alerts and Stats for Current
      currentValues.forEach((c, i) => {
        const threshold = parseFloat(document.getElementById(`curr-threshold-${i + 1}`).value) || Infinity;
        document.getElementById(`curr-alert-${i + 1}`).textContent = c > threshold ? "Over Current!" : "";
        updateStats(stats.current[i], c, `curr-stats-${i + 1}`);
      });

      if (!charts.voltage.isPaused) {
        const vTrigger = voltageValues[charts.voltage.triggerChannel];
        if (!charts.voltage.triggered && vTrigger > charts.voltage.triggerLevel) charts.voltage.triggered = true;
        if (charts.voltage.triggered) updateChart(charts.voltage, voltageValues, 'voltage-value-');
      }
      if (!charts.current.isPaused) {
        const cTrigger = currentValues[charts.current.triggerChannel];
        if (!charts.current.triggered && cTrigger > charts.current.triggerLevel) charts.current.triggered = true;
        if (charts.current.triggered) updateChart(charts.current, currentValues, 'current-value-');
      }

      t += dt;
    } catch (error) {
      console.error("Error parsing WebSocket data:", error);
    }
  };

  function createDatasets(count, offset, prefix) {
    return Array.from({ length: count }, (_, i) => ({
      label: `${prefix} ${i + 1}`,
      data: Array(maxDataPoints).fill(0),
      borderColor: colors[i + offset],
      borderWidth: 2,
      fill: false,
      yAxisID: 'yLeft'
    }));
  }

  function updateChart(chartObj, values, idPrefix) {
    if (!chartObj.isPaused) {
      const { chart, timescale } = chartObj;
      chart.data.datasets.forEach((dataset, i) => {
        dataset.data.shift();
        dataset.data.push(values[i]);
        const idNum = idPrefix.startsWith('voltage') ? (i === 0 ? 1 : i === 1 ? 3 : 4) : i + 1;
        document.getElementById(`${idPrefix}${idNum}`).textContent = `${values[i].toFixed(3)} ${idPrefix.startsWith('voltage') ? 'V' : 'A'}`;
      });
      chart.data.labels.shift();
      chart.data.labels.push(`${(t * timescale).toFixed(1)} s`);
      chart.update();
    }
  }

  function updateStats(stat, value, id) {
    stat.min = Math.min(stat.min, value);
    stat.max = Math.max(stat.max, value);
    stat.sum += value;
    stat.count++;
    document.getElementById(id).textContent = `Min: ${stat.min.toFixed(2)} | Max: ${stat.max.toFixed(2)} | Avg: ${(stat.sum / stat.count).toFixed(2)}`;
  }

  function getChartOptions() {
    return {
      responsive: true,
      maintainAspectRatio: false,
      scales: {
        x: { ticks: { color: () => document.body.classList.contains('dark-mode') ? '#e0e0e0' : '#333' }, grid: { color: '#dee2e6' } },
        yLeft: { type: 'linear', position: 'left', ticks: { color: () => document.body.classList.contains('dark-mode') ? '#e0e0e0' : '#333' } }
      },
      plugins: { legend: { labels: { color: () => document.body.classList.contains('dark-mode') ? '#e0e0e0' : '#333' } } }
    };
  }

  function updateChartColors() {
    const isDark = document.body.classList.contains('dark-mode');
    Object.values(charts).forEach(({ chart }) => {
      chart.options.scales.x.ticks.color = isDark ? '#e0e0e0' : '#333';
      chart.options.scales.yLeft.ticks.color = isDark ? '#e0e0e0' : '#333';
      chart.options.plugins.legend.labels.color = isDark ? '#e0e0e0' : '#333';
      chart.update();
    });
  }

  function createToggles(containerId, count, chart) {
    const container = document.getElementById(containerId);
    for (let i = 0; i < count; i++) {
      const div = document.createElement('div');
      div.className = 'form-check form-switch d-inline-block me-3';
      const checkbox = document.createElement('input');
      checkbox.className = 'form-check-input';
      checkbox.type = 'checkbox';
      checkbox.id = `${containerId}-channel-${i}`;
      checkbox.checked = true;
      checkbox.addEventListener('change', () => {
        chart.data.datasets[i].hidden = !checkbox.checked;
        chart.update();
      });
      const label = document.createElement('label');
      label.className = 'form-check-label';
      label.htmlFor = checkbox.id;
      label.style.color = chart.data.datasets[i].borderColor;
      label.textContent = chart.data.datasets[i].label;
      div.appendChild(checkbox);
      div.appendChild(label);
      container.appendChild(div);
    }
  }

  window.togglePause = (type) => {
    charts[type].isPaused = !charts[type].isPaused;
    document.getElementById(`${type}-pause-btn`).textContent = charts[type].isPaused ? 'Resume' : 'Pause';
  };

  window.changeTimescale = (type, direction) => {
    const chartObj = charts[type];
    chartObj.timescale = Math.max(0.1, chartObj.timescale + direction * 0.1);
    const chart = chartObj.chart;
    chart.data.labels = chart.data.labels.map((_, i) => `${(i * chartObj.timescale).toFixed(1)} s`);
    chart.update();
  };

  window.zoomChart = (type, factor) => {
    const chartObj = charts[type];
    chartObj.zoom *= factor;
    chartObj.zoom = Math.max(0.1, Math.min(10, chartObj.zoom));
    const chart = chartObj.chart;
    chart.options.scales.yLeft.min = -2 / chartObj.zoom;
    chart.options.scales.yLeft.max = 2 / chartObj.zoom;
    chart.update();
  };

  window.resetChart = (type) => {
    const chartObj = charts[type];
    chartObj.timescale = 1;
    chartObj.zoom = 1;
    const chart = chartObj.chart;
    chart.data.labels = chart.data.labels.map((_, i) => `${(i * chartObj.timescale).toFixed(1)} s`);
    chart.options.scales.yLeft.min = -2;
    chart.options.scales.yLeft.max = 2;
    chart.update();
  };

  window.exportChartCSV = (type) => {
    const chart = charts[type].chart;
    const labels = chart.data.labels;
    const datasets = chart.data.datasets;

    let csvContent = "Time," + datasets.map(ds => ds.label).join(",") + "\n";
    for (let i = 0; i < labels.length; i++) {
      const row = [labels[i]];
      datasets.forEach(ds => row.push(ds.data[i]));
      csvContent += row.join(",") + "\n";
    }

    const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
    const link = document.createElement("a");
    const url = URL.createObjectURL(blob);
    link.setAttribute("href", url);
    link.setAttribute("download", `${type}_chart_data.csv`);
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  };

  window.exportChartPNG = (type) => {
    const chart = charts[type].chart;
    const link = document.createElement("a");
    link.setAttribute("href", chart.toBase64Image());
    link.setAttribute("download", `${type}_chart.png`);
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  };

  ['voltage', 'current'].forEach(type => {
    document.getElementById(`${type}-trigger-channel`).addEventListener('change', (e) => {
      charts[type].triggerChannel = parseInt(e.target.value);
      charts[type].triggered = false;
    });
    document.getElementById(`${type}-trigger-level`).addEventListener('input', (e) => {
      charts[type].triggerLevel = parseFloat(e.target.value) || 0;
      charts[type].triggered = false;
    });
  });

  function saveSettings() {
    const settings = {
      timescale: { voltage: charts.voltage.timescale, current: charts.current.timescale },
      zoom: { voltage: charts.voltage.zoom, current: charts.current.zoom }
    };
    localStorage.setItem('powermetricsSettings', JSON.stringify(settings));
  }

  function loadSettings() {
    const settings = JSON.parse(localStorage.getItem('powermetricsSettings')) || {};
    charts.voltage.timescale = settings.timescale?.voltage || 1;
    charts.current.timescale = settings.timescale?.current || 1;
    charts.voltage.zoom = settings.zoom?.voltage || 1;
    charts.current.zoom = settings.zoom?.current || 1;
    zoomChart('voltage', 1);
    zoomChart('current', 1);
  }

  window.addEventListener('beforeunload', saveSettings);
  loadSettings();
});

  