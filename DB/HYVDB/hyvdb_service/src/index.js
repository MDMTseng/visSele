import React from 'react';
import ReactDOM from 'react-dom';
import './index.css';
import App from './App';
import * as serviceWorker from './serviceWorker';
// require('./server/apollo_server.js');
import { DatePicker } from 'antd';

const { RangePicker } = DatePicker;

function onChange(value, dateString) {
    console.log('Selected Time: ', value);
    console.log('Formatted Selected Time: ', dateString);
}

function onOk(value) {
    console.log('onOk: ', value);
}
// ReactDOM.render(<App />, document.getElementById('root'));
ReactDOM.render(
    <div>
        <DatePicker showTime placeholder="Select Time" onChange={onChange} onOk={onOk} />
        <br />
        <RangePicker
            showTime={{ format: 'HH:mm' }}
            format="YYYY-MM-DD HH:mm"
            placeholder={['Start Time', 'End Time']}
            onChange={onChange}
            onOk={onOk}
        />
    </div>,
    document.getElementById('root')
);
// If you want your app to work offline and load faster, you can change
// unregister() to register() below. Note this comes with some pitfalls.
// Learn more about service workers: https://bit.ly/CRA-PWA
serviceWorker.unregister();
