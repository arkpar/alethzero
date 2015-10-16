/*
    This file is part of web3.js.

    web3.js is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    web3.js is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with web3.js.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file ipcprovider.js
 * @authors:
 *   Fabian Vogelsteller <fabian@ethdev.com>
 * @date 2015
 */

//"use strict";

//var errors = require('./errors');
//var webchannel = require('qrc:///qtwebchannel/qwebchannel.js');

var errorTimeout = function (method, id) {
    var err = {
        "jsonrpc": "2.0",
        "error": {
            "code": -32603, 
			"message": "Request timed out for method  \'" + method + "\'"
        }, 
        "id": id
    };
    return JSON.stringify(err);
};

var QtProvider = function () {
    var _this = this;
    this.responseCallbacks = {};

	this.channel = new QWebChannel(qt.webChannelTransport, function (channel) {
		var transport = channel.objects.transport;
		_this.transport = transport;

		transport.response.connect(function(message) {

			_this._parseResponse(data.toString()).forEach(function(result){

				var id = null;

				// get the id which matches the returned id
				if(Array.isArray(result)) {
					result.forEach(function(load){
						if(_this.responseCallbacks[load.id])
							id = load.id;
					});
				} else {
					id = result.id;
				}

				// fire the callback
				if(_this.responseCallbacks[id]) {
					_this.responseCallbacks[id](null, result);
					delete _this.responseCallbacks[id];
				}
			});
		});
	});
};

/**
Will parse the response and make an array out of it.

@method _parseResponse
@param {String} data
*/
QtProvider.prototype._parseResponse = function(data) {
    var _this = this,
        returnValues = [];
    
    // DE-CHUNKER
	var dechunkedData = data .split('\n');

    dechunkedData.forEach(function(data){

        // prepend the last chunk
        if(_this.lastChunk)
            data = _this.lastChunk + data;

        var result = null;

        try {
            result = JSON.parse(data);
        } catch(e) {

            _this.lastChunk = data;

            // start timeout to cancel all requests
            clearTimeout(_this.lastChunkTimeout);
            _this.lastChunkTimeout = setTimeout(function(){
                _this.timeout();
				throw ("QtProvider: invalid response");
            }, 1000 * 15);

            return;
        }

        // cancel timeout and set chunk to null
        clearTimeout(_this.lastChunkTimeout);
        _this.lastChunk = null;

        if(result)
            returnValues.push(result);
    });

    return returnValues;
};


/**
Get the adds a callback to the responseCallbacks object,
which will be called if a response matching the response Id will arrive.

@method _addResponseCallback
*/
QtProvider.prototype._addResponseCallback = function(payload, callback) {
    var id = payload.id || payload[0].id;
    var method = payload.method || payload[0].method;

    this.responseCallbacks[id] = callback;
    this.responseCallbacks[id].method = method;
};

/**
Timeout all requests when the end/error event is fired

@method _timeout
*/
QtProvider.prototype._timeout = function() {
    for(var key in this.responseCallbacks) {
        if(this.responseCallbacks.hasOwnProperty(key)){
            this.responseCallbacks[key](errorTimeout(this.responseCallbacks[key].method, key));
            delete this.responseCallbacks[key];
        }
    }
};


/**
Check if the current connection is still valid.

@method isConnected
*/
QtProvider.prototype.isConnected = function() {
    var _this = this;

	return !!this.transport;
};


function dosend(transport, data) {
  // Return a new promise.
	return new Promise(function(resolve, reject) {
	// Do the usual XHR stuff
		transport.send(data, function(r) {
			resolve(r);
		});
	});
}

function spawn(generatorFunc) {
  function continuer(verb, arg) {
	var result;
	try {
	  result = generator[verb](arg);
	} catch (err) {
	  return Promise.reject(err);
	}
	if (result.done) {
	  return result.value;
	} else {
	  return Promise.resolve(result.value).then(onFulfilled, onRejected);
	}
  }
  var generator = generatorFunc();
  var onFulfilled = continuer.bind(continuer, "next");
  var onRejected = continuer.bind(continuer, "throw");
  return onFulfilled();
}

QtProvider.prototype.send = function (payload) {

	if(this.transport) {
/*
		var transport = this.transport;
		spawn(function *() {
		  try {
			// 'yield' effectively does an async wait,
			// returning the result of the promise
			var r = yield dosend(transport, payload);
			return JSON.parse(r);
		  }
		  catch (err) {
			throw ("QtProvider: invalid response");
		  }
		});
	}*/

		var result;
		this.transport.send(JSON.stringify(payload), function(r)
		{
			result = r;
		});
		while (result === undefined)
		{
			eval("0");
		}
		try {
			return JSON.parse(result);
		} catch(e) {
			throw ("QtProvider: invalid response");
		}

	} else {
        throw new Error('You tried to send "'+ payload.method +'" synchronously. Synchronous requests are not supported by the IPC provider.');
	}
};

QtProvider.prototype.sendAsync = function (payload, callback) {
    // try reconnect, when connection is gone
	this.transport.sendAsync(JSON.stringify(payload));
    this._addResponseCallback(payload, callback);
};

//module.exports = QtProvider;

