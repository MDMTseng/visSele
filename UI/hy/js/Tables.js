// let fetureListTable=$('#table_id1');

let dataSet2 = [
    [1, "x:198.827,y:181.296", "Edinburgh", "5421", "2011/04/25", "$320,800"],
    [1, "x:198.827,y:181.296", "Edinburgh", "5421", "2011/04/25", "$320,800"],
    [1, "x:198.827,y:181.296", "Edinburgh", "5421", "2011/04/25", "$320,800"],
    [1, "x:198.827,y:181.296", "Edinburgh", "5421", "2011/04/25", "$320,800"]
];
let rirle = ['a', 'a', 'a', 'a', 'a', 'a'];


function initDataTables() {
    if(RXMSG.IR===null){
        console.log("RXMSG.IR===null");
        return;
    }


    // dataSet = JSON.parse(RXMSG.IR);
    dataSet = RXMSG.IR;
    // // rx_array.dataSet
    // // let dataSet = Object.values(RXMSG_temp3.reports[0].reports[0]);
    dataSet = dataSet.reports[0].reports;
    console.log(dataSet);
    for (let i = 0; i < dataSet.length; i++) {
        dataSet[i].cx = Math.round(dataSet[i].cx * 100) / 100;
        dataSet[i].cy = Math.round(dataSet[i].cy * 100) / 100;
        dataSet[i].rotate = Math.round(dataSet[i].rotate * 100) / 100;
    }
    // dataSet= preprocData(dataSet);
    // console.log(dataSet);
    let dTables = $('#table_id1').DataTable({
        "scrollX": "200px",
        "scrollCollapse": true,
        "autoWidth": true,
        "searching": true,
        "ordering": true,
        "paging": false,
        "data": dataSet,
        columns: [
            {
                "className": 'details-control',
                "orderable": false,
                "data": dataSet,
                "defaultContent": ''
            },
            // { data: "Id", title: "Id" },
            {data: "area", title: "area"},
            {data: "scale", title: "scale"},
            {data: "cx", title: "cx"},
            {data: "cy", title: "cy"},
            // { data: "detectedCircles", title: "detectedCircles" },
            //     { data: "detectedLines", title: "detectedLines" },

            {data: "isFlipped", title: "isFlipped"},
            {data: "rotate", title: "rotate"}

        ]

    });
    console.log(dTables);
    initChildRow(dTables);

}

function format(d) {
    // `d` is the original data object for the row
    return '<table cellpadding="5" cellspacing="0" border="0" style="padding-left:50px;">' +
        '<tr>' +
        '<td>detected Circles:</td>' +
        '<td>' + d.detectedCircles + '</td>' +
        '</tr>' +
        '<tr>' +
        '<td>detected Lines:</td>' +
        '<td>' + d.detectedLines + '</td>' +
        '</tr>' +
        '<tr>' +
        '<td>Extra info:</td>' +
        '<td>(may be images here later x)...</td>' +
        '<td><img src="http://decade.tw/templates/purity_iii/images/logo.png"></td>' +
        '</tr>' +
        '</table>';
}

function initChildRow(table) {
    //todo
    $('#table_id1').on('click', 'td.details-control', function () {
        let tr = $(this).closest('tr');
        let row = table.row(tr);

        if (row.child.isShown()) {
            // This row is already open - close it
            row.child.hide();
            tr.removeClass('shown');
        }
        else {
            // Open this row
            row.child(format(row.data())).show();
            tr.addClass('shown');
        }
    });
}

function preprocData(dataIn) {
    let dataOut = [];
    for (id in dataIn) {
        if (dataIn.hasOwnProperty(id)) {
            dataOut.push(dataIn[id]);
            dataOut[dataOut.length - 1].Id = id;
        }
    }
    return dataOut;
}

function initTabulator() {
    let printIcon = function (cell, formatterParams) { //plain text value
        return "<i class='fa fa-print'></i>";
    };

    //Build Tabulator
    $("#example-table").tabulator({
        height: "311px",
        layout: "fitColumns",
        rowFormatter: function (row) {
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
            cellClick: function (e, cell) {
                alert("Printing row data for: " + cell.getRow().getData().name)
            }
        }, {
            title: "Name",
            field: "name",
            width: 150,
            formatter: function (cell, formatterParams) {
                let value = cell.getValue();
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