const mongoose = require('mongoose');
mongoose.Promise = Promise;
const hitCount = new mongoose.Schema({});
/*
 * This is a simple build demo to show a connection to a database
 *
 * Each time the service is called, it creates a new `hit`, then
 * returns with a total count of hits stored in the database.
 *
 * @returns {number} Number of times this service has been called
 */

let Inspection_With_TS_Schema = new mongoose.Schema(
  {InspectionData:{}},
  { timestamps: true }
  );

mongoose.Promise = global.Promise;

mongoose.connect('mongodb://127.0.0.1:27017/admin?gssapiServiceName=mongodb', { useNewUrlParser: true })
.then(() => {
  console.log('OK')
})
.catch(err =>console.log(err));
mongoose.pluralize(null);
// let db =mongoose.connection.once('open', () => console.log(`Connected to mongo at ${url}`));
let db = mongoose.connection;

let InspectionModel_A = db.model('Machine_A',Inspection_With_TS_Schema);


new InspectionModel_A({InspectionData:{sss:1}}).save()
db.on('open', function (ref) {
  console.log('[O]Connected to mongo server.');
  Object.keys(db.models).forEach((collection) => {
      console.info("Collection=>"+collection);
  });
})





