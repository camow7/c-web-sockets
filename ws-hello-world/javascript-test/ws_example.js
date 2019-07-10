  // Test with the following JavaScript:
  const WebSocket = require('ws');

  const ws = new WebSocket('ws://localhost:2506/echo');
  
  ws.on('open', function open() {
    ws.send('something');
  });
  
  ws.on('message', function incoming(data) {
    console.log(data);
  });