// Pure expression engine (infix->postfix + eval). Extracted from MISC_Util.
function extractAtomGroup(exp_str_arr, mark = "$") {
  let lastH = exp_str_arr[exp_str_arr.length - 1];
  let matchD = lastH.match(/\(([^\(\)]+)\)/);
  //let matchD=/\([^\(\)]+\)/g.exec(lastH);
  if (matchD !== null && matchD.length > 0) {
    exp_str_arr[exp_str_arr.length - 1] = matchD[1];

    var newstr = lastH.replace(/\([^\(\)]+\)/, mark);
    exp_str_arr.push(newstr);
  }
  return exp_str_arr;
}
function extractArithGroup(expression, sep = "\\-\\+", mark = "$") {
  let regx_param;
  let regx_opera;

  if (mark == "$") mark += "$";

  regx_param = new RegExp("(.+)[" + sep + "](.+)");
  regx_opera = new RegExp(".+([" + sep + "]).+");

  let matchD = expression.match(regx_param);
  if (matchD == null) {
    return [expression];
  }
  var rep = expression.replace(regx_opera, mark + "$1" + mark);
  return [matchD[1], matchD[2], rep];
}

function extractArith(exp_stack, operator_regexp = "\\-\\+", mark = "$") {
  return exp_stack
    .map(exp => {
      if (operator_regexp == "," && exp.includes(",")) {
        if (exp.includes(",")) {
          let splitArr = exp.split(",");
          let aggStr = mark;
          for (let i = 1; i < splitArr.length; i++) aggStr += "," + mark;

          splitArr.push(aggStr);
          return splitArr;
        }
        return exp;
      }

      let parse_target = operator_regexp;
      let arrayX = extractArithGroup(exp, parse_target, mark);
      while (true) {
        let arrayY = extractArithGroup(arrayX[0], parse_target, mark);
        if (arrayY.length == 1) break;
        arrayX.shift();
        arrayX = arrayY.concat(arrayX);
      }

      return arrayX;
    })
    .flat();
}


export function Exp2PostfixExp(exp_str) {//exp="Math.max(3+Math.tan(5-1/4*3)/3,1+2*3/(4+5)/6)"
  let retA = [exp_str];

  let p_Mark = "$";
  
  //exp="Math.max(3+Math.tan(5-1/4*3)/3,1+2*3/(4+5)/6)"
  // First pass Postfix exp converter
  while (true) {
    let currentSize = retA.length;
    retA = extractAtomGroup(retA, p_Mark);

    if (currentSize == retA.length) break;
  }

  let postExpCalc_Func = {
    default: (key, vals) => {
      if (vals !== undefined) {
        
        //console.log("ori.key",key);
        let skey = key.split("$");
        let newKey =skey.reduce((nk,ele,idx)=>{
          if(idx==0)return ele;
          let repK = ((idx-1)>=vals.length)?"$":vals[idx-1]
          return nk+"(" + repK + ")"+ele;
        },"");
        key=newKey;
        
        // key = key.replace(/\$/g, "#");
        // vals.forEach(val => {
        //   key = key.replace("#", "(" + val + ")");
        // });
        // key = key.replace(/\#/g, "$");
        
//         console.log("...",newKey,key);
      }
      return key;
    }
  };
  //console.log("retA",retA);
  //["5-1/4*3", "4+5", "3+Math.tan$/3,1+2*3/$/6", "Math.max$"]
  retA = retA.map(exp => {//parse piece expression with +-*/... operators 
    exp = exp.replace(/\$/g,"#");
    let a_post_exp = [",","\\-\\+","%","\\*\\/","\\^"].reduce(
      (pexp,exp)=>
       extractArith(pexp, exp, "$"),[exp]);
    //Assamble back with "(" ")"
    let XXY = PostfixExpCalc(a_post_exp, postExpCalc_Func,false);
    
    return XXY[0].replace(/\#/g,"$");
  });
  //console.log("retA",retA);
  //["(5)-(((1)/(4))*(3))", "(4)+(5)", "((3)+((Math.tan$)/(3))),((1)+((((2)*(3))/($))/(6)))", "Math.max$"]
  //Assamble back with "(" ")"
  retA = PostfixExpCalc(retA, postExpCalc_Func);
  // console.log(retA)
  // retA=retA.filter(A=>A!="$")

  retA=retA.flat();
  
  
  //Now we have all the "(" ")" to do the 2nd pass Postfix exp converter

  //console.log("retA",retA);
  //["Math.max(((3)+((Math.tan((5)-(((1)/(4))*(3))))/(3))),((1)+((((2)*(3))/(((4)+(5))))/(6))))"]
  while (true) {
    let currentSize = retA.length;
    retA = extractAtomGroup(retA, p_Mark);

    if (currentSize == retA.length) break;
  }
  //console.log("retA",retA);
  //["3", "5", "1", "4", "$/$", "3", "$*$", "$-$", "Math.tan$", "3", "$/$", "$+$", "1", "2", "3", "$*$", "4", "5", "$+$", "$", "$/$", "6", "$/$", "$+$", "$,$", "Math.max$"]
  return retA;
}

const regex_multi_group = /^\$(,\$)*$/gm;//match  $ or $,$ or $,$,$ or $,$,$,$ ...
export function PostfixExpCalc(postExp, funcSet,handleMultiGroup=true) {
  let valStack = [];
  postExp.forEach(exp => {
    let valPush;
    
    if (exp.includes !== undefined && exp.includes("$")) {
      let groupFlagCount = exp.split("$").length - 1;
      let tmpArr = valStack.splice(valStack.length - groupFlagCount, 9999999);
      if (funcSet[exp] !== undefined) 
      {
        valPush = funcSet[exp](tmpArr.flat());
      }
      else if(handleMultiGroup&&exp.match(regex_multi_group)!=null)
      {
        valPush =tmpArr.flat();
      }
      else
      {
        valPush = funcSet.default(exp, tmpArr.flat());
      }

    } else {
      if (funcSet[exp] !== undefined) {
        valPush = funcSet[exp];
      } else if (funcSet.default !== undefined) {
        valPush = funcSet.default(exp);
      } else {
        valPush = NaN;
      }
    }

    valStack.push(valPush);
  });
  return valStack;
}


function ExpCalcBasic(postExp_,funcSet,fallbackFunctionSet) {
  let postExp=postExp_.filter(exp=>exp!="$")
  funcSet = {
    min$: arr => Math.min(...arr),
    max$: arr => Math.max(...arr),
    "$+$": vals => vals[0] + vals[1],
    "$-$": vals => vals[0] - vals[1],
    "$*$": vals => vals[0] * vals[1],
    "$/$": vals => vals[0] / vals[1],
    "$^$": vals => Math.pow(vals[0] , vals[1]),
    ...funcSet,
    default:fallbackFunctionSet
    //default:_=>false
  };

  return PostfixExpCalc(postExp, funcSet)[0];
}

// What the CORE can evaluate — the only set that matters for a saved def.
//
// judge_CALC (InspectionCore/MatchingEngine/JudgeCALC.cpp) implements exactly
// six operators and returns -2 for anything else. The UI's own evaluator
// implements a SUPERSET (it has $^$), and the editor's validity check runs the
// UI evaluator — so an expression using ^ computes a number on screen, saves
// cleanly, and then makes the core return -2 for every part. That path sets the
// judge status to STATUS_UNSET and never clears it: the measurement is NA
// forever, and nothing says the operator does not exist.
//
// Keep this list in step with JudgeCALC.cpp. It is the contract, not a
// convenience.
export const CORE_CALC_OPS = ['$+$', '$-$', '$*$', '$/$', 'max$', 'min$'];

// A token is an operand (a number, or a [id] reference) rather than an operator.
const isOperand = (t) => !t.includes('$');
// The core's isParamsCache(): '$', '$,$', '$,$,$' … mark how many arguments the
// following function consumes.
const isParamsCache = (t) => /^\$(,\$)*$/.test(t);

// Operators in a compiled expression that the core cannot run. Empty means the
// core can execute it.
export function unsupportedCoreOps(postExp) {
  if (!Array.isArray(postExp)) return [];
  const bad = [];
  postExp.forEach((t) => {
    const tok = String(t);
    if (isOperand(tok) || isParamsCache(tok)) return;
    if (CORE_CALC_OPS.indexOf(tok) >= 0) return;
    if (bad.indexOf(tok) < 0) bad.push(tok);
  });
  return bad;
}
