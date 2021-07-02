(function () {
  var ws = null;
  var connected = false;
  var historyItems = [];

  var serverUrl;
  var connectionStatus;
  var sendMessage;

  var historyList;
  var connectButton;
  var disconnectButton;
  var sendButton;

  var open = function () {
    var url = serverUrl.val();
    ws = new WebSocket(url);
    ws.onopen = onOpen;
    ws.onclose = onClose;
    ws.onmessage = onMessage;
    ws.onerror = onError;

    connectionStatus.text('OPENING ...');
    serverUrl.attr('disabled', 'disabled');
    connectButton.hide();
    disconnectButton.show();
  };

  var close = function () {
    if (ws) {
      console.log('CLOSING ...');
      ws.close();
    }
  };

  var reset = function () {
    connected = false;
    connectionStatus.text('CLOSED');

    serverUrl.removeAttr('disabled');
    connectButton.show();
    disconnectButton.hide();
    sendMessage.attr('disabled', 'disabled');
    sendButton.attr('disabled', 'disabled');
  };

  var clearLog = function () {
    $('#messages').html('');
  };

  var onOpen = function () {
    console.log('OPENED: ' + serverUrl.val());
    connected = true;
    connectionStatus.text('OPENED');
    sendMessage.removeAttr('disabled');
    sendButton.removeAttr('disabled');
  };

  var onClose = function () {
    console.log('CLOSED: ' + serverUrl.val());
    ws = null;
    reset();
  };

  var arrayBuffer2String = function (uint8_arr) {

    // console.log(uint8_arr[0],uint8_arr.byteLength);
    let strLen = "0x" + uint8_arr.reduce((str, ele) => str + ele.toString(16) + ",", "");
    return strLen.substring(0, strLen.length - 1);
  }
  var onMessage = function (event) {

    // if (!(event.data instanceof ArrayBuffer))
    // {
    //   var data = event.data;
    //   addMessage(data);
    //   return;
    // }
    console.log(event);
    var headerArray = event.data.arrayBuffer().then(e => {
      let intarr = new Uint8Array(e);
      addMessage(arrayBuffer2String(intarr));
    });//new Uint8ClampedArray(event.data,0,9);
    console.log(headerArray);
  };

  var onError = function (event) {
    alert(event.type);
  };

  var addMessage = function (data, type) {
    var msg = $('<pre>').text(data);
    if (type === 'SENT') {
      msg.addClass('sent');
    }
    var messages = $('#messages');
    messages.append(msg);

    var msgBox = messages.get(0);
    while (msgBox.childNodes.length > 1000) {
      msgBox.removeChild(msgBox.firstChild);
    }
    msgBox.scrollTop = msgBox.scrollHeight;
  };

  var addToHistoryList = function (item) {
    var addedLi = $('<li>').attr('id', item.id).append(
      $('<a>').attr('href', item.url).attr('data-msg', item.msg).attr('title', item.url + '\n\n' + item.msg).attr('class', 'historyUrl').append(item.url)).append(
        $('<span>').attr('class', 'removeHistory').append("x")).attr('style', 'display: none;').prependTo(historyList);

    addedLi.toggle('slow');
  };

  var loadHistory = function () {
    historyList = $('#history');
    historyItems = JSON.parse(localStorage.getItem('history'));

    if (!historyItems) {
      historyItems = [];
    }

    $.each(historyItems, function (i, item) {
      addToHistoryList(item);
    });
  };

  var removeHistory = function (item) {
    var removeLi = function () {
      $(this).remove();
    };
    for (var i = historyItems.length - 1; i >= 0; i--) {
      if (historyItems[i].url === item.url && historyItems[i].msg === item.msg) {
        var selector = 'li#' + historyItems[i].id;
        $(selector).toggle('slow', removeLi);

        historyItems.splice(i, 1);
      }
    }
  };

  var guid = function () {
    function s4() {
      return Math.floor((1 + Math.random()) * 0x10000)
        .toString(16)
        .substring(1);
    }
    return s4() + s4() + '-' + s4() + '-' + s4() + '-' +
      s4() + '-' + s4() + s4() + s4();
  };

  var saveHistory = function (msg) {
    var item = { 'id': guid(), 'url': serverUrl.val(), 'msg': msg };

    removeHistory(item);

    if (historyItems.length >= 20) {
      historyItems.shift();
      $('#history li:last-child').remove();
    }

    historyItems.push(item);
    localStorage.setItem('history', JSON.stringify(historyItems));

    addToHistoryList(item);
  };

  var clearHistory = function () {
    historyItems = [];
    localStorage.clear();
    historyList.empty();
  };

  WebSocketClient = {
    init: function () {
      serverUrl = $('#serverUrl');
      connectionStatus = $('#connectionStatus');
      sendMessage = $('#sendMessage');
      historyList = $('#history');

      connectButton = $('#connectButton');
      disconnectButton = $('#disconnectButton');
      sendButton = $('#sendButton');

      loadHistory();

      $('#clearHistory').click(function (e) {
        clearHistory();
      });

      connectButton.click(function (e) {
        close();
        open();
      });

      disconnectButton.click(function (e) {
        close();
      });

      sendButton.click(function (e) {
        var msg = $('#sendMessage').val();
        var msgStr = msg;
        if (msg.startsWith("0x")) {
          msg = msg.replaceAll(" ", "").substring(2).split(",");

          let bbuf = new Uint8Array(msg.length);
          msg.forEach((hexStr, idx) => {
            bbuf[idx] = parseInt(msg[idx], 16);
          });
          msg = bbuf;
          msgStr = arrayBuffer2String(msg);
        }



        addMessage(msgStr, 'SENT');
        ws.send(msg);

        saveHistory(msgStr);
      });

      $('#clearMessage').click(function (e) {
        clearLog();
      });

      historyList.delegate('.removeHistory', 'click', function (e) {
        var link = $(this).parent().find('a');
        removeHistory({ 'url': link.attr('href'), 'msg': link.attr('data-msg') });
        localStorage.setItem('history', JSON.stringify(historyItems));
      });

      historyList.delegate('.historyUrl', 'click', function (e) {
        window.haha1 = this;
        serverUrl.val(this.href);
        sendMessage.val(this.dataset.msg);
        e.preventDefault();
      });

      serverUrl.keydown(function (e) {
        if (e.which === 13) {
          connectButton.click();
        }
      });

      var isCtrl;
      sendMessage.keyup(function (e) {
        if (e.which === 17) {
          isCtrl = false;
        }
      }).keydown(function (e) {
        if (e.which === 17) {
          isCtrl = true;
        }
        if (e.which === 13 && isCtrl === true) {
          sendButton.click();
          return false;
        }
      });
    }
  };
})();

var WebSocketClient;

$(function () {
  WebSocketClient.init();
});