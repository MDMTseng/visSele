// GraphQL
import {
	GraphQLString,
	GraphQLList,
	GraphQLNonNull
} from 'graphql';

import T from './Type.js';
import M from './Model.js';

let Query = {
	users: {
		type: new GraphQLList(T),
		descriptions: 'All users info',
		resolve: () => {
			return new M().getDenormalizedUsers();
		}
	},
	user: {
		type: T,
		descriptions: 'User info by id',
		args: {
			id: {
				type: new GraphQLNonNull(GraphQLString)
			}
		},
		resolve: (root, { id }) => {
			return new M().getDenormalizedUserById(id);
		}
	}
};


export default Query;