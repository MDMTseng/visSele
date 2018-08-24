function initTable() {
	var printIcon = function(cell, formatterParams) { //plain text value
		return "<i class='fa fa-print'></i>";
	};

	//Build Tabulator
	$("#example-table").tabulator({
		height: "311px",
		layout: "fitColumns",
		rowFormatter: function(row) {
			if (row.getData().col == "blue") {
				row.getElement().css({
					"background-color": "#A6A6DF"
				});
			}
		},
		columns: [{
			formatter: "rownum",
			align: "center",
			width: 40
		}, {
			formatter: printIcon,
			width: 40,
			align: "center",
			cellClick: function(e, cell) {
				alert("Printing row data for: " + cell.getRow().getData().name)
			}
		}, {
			title: "Name",
			field: "name",
			width: 150,
			formatter: function(cell, formatterParams) {
				var value = cell.getValue();
				if (value.indexOf("o") > 0) {
					return "<span style='color:red; font-weight:bold;'>" + value + "</span>";
				} else {
					return value;
				}
			}
		}, {
			title: "Progress",
			field: "progress",
			formatter: "progress",
			formatterParams: {
				color: ["#00dd00", "orange", "rgb(255,0,0)"]
			},
			sorter: "number",
			width: 100
		}, {
			title: "Rating",
			field: "rating",
			formatter: "star",
			formatterParams: {
				stars: 6
			},
			align: "center",
			width: 120
		}, {
			title: "Driver",
			field: "car",
			align: "center",
			formatter: "tickCross",
			width: 50
		}, {
			title: "Col",
			field: "col",
			formatter: "color",
			width: 50
		}, {
			title: "Line Wraping",
			field: "lorem",
			formatter: "textarea"
		}, {
			formatter: "buttonCross",
			width: 30,
			align: "center"
		}],
	});
}