const {  gql } = require('apollo-server');
const mongoose =require('mongoose');
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
let Inspection_With_TS_Schema = new mongoose.Schema(
    {InspectionData:{}},
    { timestamps: true }
    );
let DefineFile_With_TS_Schema = new mongoose.Schema(
    {DF:{}},
    { timestamps: true }
);

let ItemSchema = new mongoose.Schema({
    Inspection_Result: String,
    createTime: {
        type: Date,
        default: Date.now
    },
    updateTime: {
        type: Date,
        default: Date.now
    }
}, {
    versionKey: false,
    timestamps: { createdAt: 'createTime', updatedAt: 'updateTime' }
});
module.exports = {DefineFile_With_TS_Schema,Inspection_With_TS_Schema,typeDefs,resolvers};