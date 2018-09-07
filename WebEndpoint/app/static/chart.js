window.onload=function() {

	var ctx = document.getElementById("datachart").getContext('2d');
	var myChart = new Chart(ctx, {
		type: 'bar',
		responsive: true,
		maintainAspectRatio: true,
		data: {
			labels: chartdata.labels,
			datasets: [{
				label: 'Wind Ø',
				data: chartdata.wind_avg,
				backgroundColor: '#60E060',
				borderWidth: 0,
			},{
				label: 'Wind max',
				data: chartdata.wind_max,
				backgroundColor: '#80FF80',
				borderWidth: 0
			},{
				label: 'Lufttemperatur',
				data: chartdata.t_luft,
				type: 'line',
				fill: false,
				borderColor: '#800000',
				pointRadius: 0,
				yAxisID: 'y2'
			},{
				label: 'Wassertemperatur',
				data: chartdata.t_wasser,
				type: 'line',
				fill: false,
				borderColor: '#000090',
				pointRadius: 0,
				yAxisID: 'y2'
			}
			]
		},
		options: {
			layout: {
				padding: {
					left: 10,
					right: 10,
					top: 0,
					bottom: 0
				}
			},
			scales: {
				yAxes: [{
					id: 'y1',
					position: 'right',
					ticks: {
						beginAtZero: true,
						stepValue: 10,
						max: 50,
						callback: function(value, index, values) {
							return value+" km/h"
						}
					}
				},{
					id: 'y1b',
					position: 'right',
					ticks: {
						autoSkip: false,
						stepValue: 1,
						min: 0,
						max: 50,
						callback: function(value, index, values) {
							if (value==1) return '1 bft';
							if (value==6) return '2 bft';
							if (value==12) return '3 bft';
							if (value==20) return '4 bft';
							if (value==29) return '5 bft';
							if (value==39) return '6 bft';
							if (value==50) return '7 bft';
							return undefined;
						}
					},
					afterBuildTicks: function(scale) {
						console.log(scale);
						scale.ticks = [1,6,12,20,29,39,50];
					},
					gridLines: {
						display: true,
						color: '#80FF80'
					}
				},{
					id: 'y2',
					position: 'left',
					ticks: {
						stepValue: 10,
						min: -10,
						max: 40,
						callback: function(value, index, values) {
							return value+" °C"
						}
					},
					gridLines: {
						display: false
					}
				}],
				xAxes: [{
					stacked: true,
					categoryPercentage: 1.0,
					barPercentage: 1.1,
					ticks: {
						autoSkip: true
					}
				},{
					position: 'top',
					ticks: {
						autoSkip: true,
						minRotation: 0,
						maxRotation: 0,
						callback: function(value, index, values) {
							return chartdata.wind_dir[index];
						}
					},
					gridLines: {
						display: false
					}
				}]
			},
			tooltips: {
				callbacks: {
					label: function(tooltipItem, data) {
						if (tooltipItem.datasetIndex<2) {
							return tooltipItem.yLabel.toString().replace(',','.')+" km/h";
						}
						return tooltipItem.yLabel.toString().replace(',','.')+" °C";
					}
				}
			}
		}
	});
}
