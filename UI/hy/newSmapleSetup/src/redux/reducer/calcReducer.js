import {DISP_EVE_UI} from '../constant';


export const calcReducer = (state = {ans:0}, action) => {

  switch(action.type)
  {
    case "ADD":
      state.ans += action.data;
    break;
    case "SUB":
      state.ans -= action.data;
    break;
    case "MUL":
      state.ans *= action.data;
    break;
    case "DIV":
      state.ans /= action.data;
    break;
    case "SET":
      state.ans  = action.data;
    break;
  }
  return state;
}



export const userReducer = (state = {}, action) => {

  if (action.type === "USER") {
    state.ID=action.data.ID;
    state.Name=action.data.Name;
    return state;
  }
  if(action.type === "ERROR")
  {
    throw new Error("ERROR act");
  }
  return state;
}
