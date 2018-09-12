

export function Num2Str_padding(pad,num)
{
  var str="0000000000000000"+(num);
  return str.substr(-pad);
}
