console.log("WS server started");
var Msg = '';
var WebSocketServer = require('ws').Server
, wss = new WebSocketServer({port: 8080});
wss.on('connection', function(ws) {
        ws.send('hello');
        ws.on('message', function(message) {
                console.log('--> %s', message);
                ws.send('you sent [' + message + ']');
                console.log('<-- you sent [' + message + ']');
        });
        ws.on('error', function(err) {
			console.log('error', err);
			ws.close();
		});
});
