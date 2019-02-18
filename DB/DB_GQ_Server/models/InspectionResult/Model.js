import mongoose, { Schema } from 'mongoose';
import FeatureFile from '../FeatureFile/Model.js';
import GraphQLError from 'graphql';

let IR_Schema = new Schema({
	ID: String,
	IR_RESULT:String
});

function denormalizeUser(user) {
	let resultUser = JSON.parse(JSON.stringify(user));
	let detailedShoppingList = resultUser.shoppingList.map(async (productId) => {
		try {
			return await FeatureFile.findById(productId);
		} catch(err) {
			return err;
		}
	});
	resultUser.shoppingList = detailedShoppingList;
	return resultUser;
}

IR_Schema.methods.getDenormalizedUsers = async function() {
	let users;
	try {
		users = await User.find();
	} catch(err) {
		return err;
	}
	return users.map(denormalizeUser);
};

IR_Schema.methods.getDenormalizedUserById = async function(id) {
	let user;
	try {
		user = await User.findById(id);
	} catch(err) {
		return err;
	}
	return denormalizeUser(user);
};

let User = mongoose.model('InspectionResult', IR_Schema);
let Model_InspectionResult = mongoose.model( 'Collection_InspectionResult',IR_Schema,'Collection_InspectionResult' );

export default Model_InspectionResult;
