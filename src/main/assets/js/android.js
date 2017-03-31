/**
 * JS helpers for the global object and require. This should really be implemented in Ejecta-v8 itself.
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 *
 **/


BG = {
	'ajax': function (data, callback) {
		var decoded = JSON.parse(data);
		callback(decoded);
	}
};

global = {
  'console': console,
  'require': int_require
};

_requireCache = {};

require = function (name) {
  if (_requireCache[name]) {
    return _requireCache[name];
  }

  var sandbox = {};

  // Check if name ends with json
  if (name.indexOf(".json", name.length - 5) !== -1) {
    // Yep, don't really require it
    var res = int_require (name, sandbox);
    _requireCache[name] = res;
    return res;
  }

  for (var k in global) {
    sandbox[k] = global[k];
  }
  sandbox.require = require;
  sandbox.exports = {};
  sandbox.__filename = name;
  sandbox.module = sandbox;
  sandbox.global = sandbox;
  sandbox.root = undefined;
  sandbox.environment = "BGJSContext";

  var res = int_require (name, sandbox);
  _requireCache[name] = sandbox.exports;

  return sandbox.exports;
}
