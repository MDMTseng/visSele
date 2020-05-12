import * as logX from 'loglevel';
import { UI_SM_EVENT } from 'REDUX_STORE_SRC/actions/UIAct';
let UISEV = UI_SM_EVENT;

//TODO: Move inspection report reducer logic to here
let StateReducer = (state, action) => {

  switch (action.type) {
    case "0":
      break;
  }
  //logX.info(action)
  return state;
}

let uInspReducer = (state = {
  inspReport: undefined,
  reportStatisticState: {
    trackingWindow: [],
    historyReport: [],
    newAddedReport: [],
    statisticValue: undefined,
  },
}, action) => {
  if (action.type === undefined || action.type.includes("@@redux/")) return state;



  var d = new Date();
  //console.log(action);
  let newState = state;
  if (action.type === "ATBundle") {
    newState = action.data.reduce((state, action) => {
      action.date = d;
      return StateReducer(state, action);
    }, newState);

    return newState;
  }
  else {
    action.date = d;
    newState = StateReducer(newState, action);
    //log.debug(newState);
    return newState;
  }
}

export default uInspReducer