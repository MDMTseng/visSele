import mongoose, { Schema } from 'mongoose';

let FF_Schema = new Schema({
	ID: String,
	FeatureFile_JsonStr: String,
	categories: []
});

let Model_FeatureSets = mongoose.model( 'Collection_FeatureSets',FF_Schema,'Collection_FeatureSets' );

export default Model_FeatureSets;
