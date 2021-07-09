'use strict'

const EventEmitter = require('events').EventEmitter
const addon = require('bindings')('emit_from_cpp')

const emitter = new EventEmitter()

emitter.on('start', () => {
    console.log('### START ...')
})

let preTime=Date.now();
let buffer_RECV=undefined;
let buffer_SEND=undefined;


emitter.on('buffer_RECV', (evt) => {
  buffer_RECV=evt;
})

emitter.on('buffer_SEND', (evt) => {
  buffer_SEND=evt;
})


emitter.on('data', (evt) => {
    let curTime=Date.now();

    console.log(curTime-preTime,buffer_RECV);
    // let i=0;
    // for(i=0;i<1000;i++)
    // {
    //   console.log(buffer_RECV[i]);
    // }
    preTime=curTime;
})

emitter.on('end', () => {
    console.log('### END ###')
})

addon.callEmit(emitter.emit.bind(emitter))
