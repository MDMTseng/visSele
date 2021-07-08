'use strict'

const EventEmitter = require('events').EventEmitter
const addon = require('bindings')('emit_from_cpp')

const emitter = new EventEmitter()

emitter.on('start', () => {
    console.log('### START ...')
})

let preTime=Date.now();
emitter.on('data', (evt) => {
    let curTime=Date.now();
    console.log(curTime-preTime,evt);
    preTime=curTime;
})

emitter.on('end', () => {
    console.log('### END ###')
})

addon.callEmit(emitter.emit.bind(emitter))
