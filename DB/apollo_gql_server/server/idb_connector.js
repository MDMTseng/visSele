// import * from 'idb';
//
// const dbName="iDB_HY";
// const dbPromise = openDB(dbName, 1, {
//     upgrade(db) {
//         db.createObjectStore('keyval');
//     }
// });
//
// const idbKeyval = {
//     async get(key) {
//         return (await dbPromise).get('keyval', key);
//     },
//     async set(key, val) {
//         return (await dbPromise).put('keyval', val, key);
//     },
//     async delete(key) {
//         return (await dbPromise).delete('keyval', key);
//     },
//     async clear() {
//         return (await dbPromise).clear('keyval');
//     },
//     async keys() {
//         return (await dbPromise).getAllKeys('keyval');
//     },
// };
// // async function idbArticles() {
// //     const db = await openDB('Articles', 1, {
// //         upgrade(db) {
// //             // Create a store of objects
// //             const store = db.createObjectStore('articles', {
// //                 // The 'id' property of the object will be the key.
// //                 keyPath: 'id',
// //                 // If it isn't explicitly set, create a value by auto incrementing.
// //                 autoIncrement: true,
// //             });
// //             // Create an index on the 'date' property of the objects.
// //             store.createIndex('date', 'date');
// //         },
// //     });
// //
// //     // Add an article:
// //     await db.add('articles', {
// //         title: 'Article 1',
// //         date: new Date('2019-01-01'),
// //         body: '…',
// //     });
// //
// //     // Add multiple articles in one transaction:
// //     {
// //         const tx = db.transaction('articles', 'readwrite');
// //         tx.store.add({
// //             title: 'Article 2',
// //             date: new Date('2019-01-01'),
// //             body: '…',
// //         });
// //         tx.store.add({
// //             title: 'Article 3',
// //             date: new Date('2019-01-02'),
// //             body: '…',
// //         });
// //         await tx.done;
// //     }
// //
// //     // Get all the articles in date order:
// //     console.log(await db.getAllFromIndex('articles', 'date'));
// //
// //     // Add 'And, happy new year!' to all articles on 2019-01-01:
// //     {
// //         const tx = db.transaction('articles', 'readwrite');
// //         const index = tx.store.index('date');
// //
// //         for await (const cursor of index.iterate(new Date('2019-01-01'))) {
// //             const article = { ...cursor.value };
// //             article.body += ' And, happy new year!';
// //             cursor.update(article);
// //         }
// //
// //         await tx.done;
// //     }
// //     return db;
// // }
// //
// module.exports = {
//     idb:dbPromise,
//     idb_KV:idbKeyval
//     // ,idb_Articles:idbAS
// };