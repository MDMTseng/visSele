
import jsonp from 'jsonp';
import querystring from 'querystring';

function AjaxGet(URL,resCB,rejCB) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = () => {
    if (xhttp.readyState == 4 ) {
      if(xhttp.status == 200)
      {
        let data=xhttp.responseText;
        data= JSON.parse(data);
        resCB(data);
      }
      else
        rejCB({readyState:xhttp.readyState,status:xhttp.status,URL});
    }
  };
  xhttp.open("GET",URL, true);
  xhttp.send();
}

function jsonpGet(URL,resCB,rejCB) {
  jsonp(URL,  (err, data) => {
    if (err)
    {
      rejCB(err)
      throw err;
    }
    resCB(data);
  });
}
