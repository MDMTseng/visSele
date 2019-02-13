// let fetureListTable=$('#table_id1');
var editor;
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
    // console.log(dataSet);
    for (let i = 0; i < dataSet.length; i++) {
        dataSet[i].cx = Math.round(dataSet[i].cx * 100) / 100;
        dataSet[i].cy = Math.round(dataSet[i].cy * 100) / 100;
        dataSet[i].rotate = Math.round(dataSet[i].rotate * 100) / 100;
    }
    console.log("d1");
    console.log(dataSet);
    dataSet=preprocData(dataSet);
    console.log("d2");
    console.log(dataSet);
    // dataSet= preprocData(dataSet);
    // console.log(dataSet);
    let dTables = $('#table_id1').DataTable({
        "scrollX": "200px",
        "scrollCollapse": true,
        "autoWidth": true,
        "searching": false,
        "ordering": true,
        "paging": false,
        "data": dataSet,
        "select": {
            style:    'os',
            selector: 'td:first-child'
        },
        buttons: [
            { extend: 'create', editor: editor },
            { extend: 'edit',   editor: editor },
            { extend: 'remove', editor: editor }
        ],
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
    // console.log(dTables);
    initChildRow(dTables);
    }
function inlineEditor2() {
    editor = new $.fn.dataTable.Editor({});
}
// function inlineEditor(){
//     editor =new $.fn.dataTable.Editor( {
//         // ajax: "../php/staff.php",
//         "table": "#table_id1",
//         "idsrc":"area",
//         "fields": [ {
//             label: "area",
//             name: "area"
//         }, {
//             label: "scale",
//             name: "scale"
//         }, {
//             label: "cx",
//             name: "cx"
//         }, {
//             label: "cy",
//             name: "cy"
//         }, {
//             label: "isFlipped",
//             name: "isFlipped"
//         }
//         ],
//         select: {
//             style:    'os',
//             selector: 'td:first-child'
//         },
//         buttons: [
//             { extend: "create", editor: editor },
//             { extend: "edit",   editor: editor },
//             { extend: "remove", editor: editor }
//         ]
//     } );
//     $('#table_id1').on( 'click', 'tbody td:not(:first-child)', function (e) {
//         editor.inline( this );
//     } );
// }
function format(d) {
    // `d` is the original data object for the row
    return '<table cellpadding="5" cellspacing="0" border="0" style="float: left; padding-left:50px;">' +
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
    table.on('click', 'td.details-control', function () {
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
