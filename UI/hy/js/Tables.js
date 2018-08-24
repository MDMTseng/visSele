var dataSet2 = [
	[1, "x:198.827,y:181.296", "Edinburgh", "5421", "2011/04/25", "$320,800"],
	[1, "x:198.827,y:181.296", "Edinburgh", "5421", "2011/04/25", "$320,800"],
	[1, "x:198.827,y:181.296", "Edinburgh", "5421", "2011/04/25", "$320,800"],
	[1, "x:198.827,y:181.296", "Edinburgh", "5421", "2011/04/25", "$320,800"]
];
var rirle=['a','a','a','a','a','a'];
function preprocData(dataIn){
   var dataOut = [];
   for(id in dataIn){
      if(dataIn.hasOwnProperty(id)){         
         dataOut.push(dataIn[id]);
         dataOut[dataOut.length - 1].Id = id;
      }
   }
   return dataOut;
}
function initDataTables() {

	dataSet = JSON.parse(RXMSG_temp3_json);
	// // rx_array.dataSet
	// // var dataSet = Object.values(RXMSG_temp3.reports[0].reports[0]);

	
	
	
	dataSet=dataSet.reports[0].reports;
	console.log(dataSet);
	// dataSet= preprocData(dataSet);
	// console.log(dataSet);
	


	
	var dTables=$('#table_id1').DataTable({
		"scrollX": "200px",
  "scrollCollapse": true,
		"autoWidth": true,
		"searching": true,
		"ordering": true,
		"paging": true,
		"data":dataSet,
		columns: [
            // { data: "Id", title: "Id" },
            { data: "area", title: "area" },
            { data: "scale", title: "scale" },
            { data: "cx", title: "cx" },
            { data: "cy", title: "cy" },
        

            { data: "isFlipped", title: "isFlipped" },
            { data: "rotate", title: "rotate" }

        ]
		 
	});
	console.log(dTables);
}

function initTabulator() {
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