var express = require('express');
var router = express.Router();

var dbModel = require('../dbModels/dataModel.js');

// router.get('/list', function(req, res, next) {
//   dbModel.find(function(err, data){
//     if(err){ return console.log(err) }
//     res.render('UserList',{
//       user: data
//     })
//   })
// });

/* GET home page. */
router.get('/', function(req, res, next) {
  res.render('index', { title: 'Exprffesssss' });
});
router.get('/xlinx', function (req, res) {
  res.send('xlinxasdf');
});
module.exports = router;
