const {  gql } = require('apollo-server');
const array_books = [
    {
        title: 'Harry Potter and the Chamber of Secrets',
        author: 'J.K. Rowling',
    },
    {
        title: 'Jurassic Park',
        author: 'Michael Crichton',
    },
];
const array_xids = [{idid:2},{idid:3},{idid:4},{idid:5}];
const array_xlinx=[{no:1}];
let resolvers = {
    Query: {
        books: () => array_books,

    },
};
let typeDefs = gql` 
  type type_Book {
    title: String
    author: String
  }
  
  type Query {
    books: [type_Book]
    title: String
    author: String
   
  }
  
  type Mutation {
    addBook(title: String, author: String): type_Book
  }
`;

module.exports = {typeDefs,resolvers};