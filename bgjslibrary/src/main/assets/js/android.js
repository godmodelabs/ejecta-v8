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

// __dirname = "js/";
_requireCache = {};

require = function (name) {
  if (_requireCache[name]) {
    return _requireCache[name];
  }

  var sandbox = {};
  /* if (__dirname == undefined) {
    sandbox.__dirname = "js/";
  } else {
    sandbox.__dirname = __dirname;
  } */

  // console.log("require dirname before: " + sandbox.__dirname + ", global " +  __dirname);

  // Check if name ends with json
  if (name.indexOf(".json", name.length - 5) !== -1) {
    // Yep, don't really require it
    var res = int_require (name, sandbox);
    _requireCache[name] = res;
    // console.log("require dirname after: " + sandbox.__dirname);
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
  /* if (name == "ajax") {
  	sandbox = {};
  } */


  var res = int_require (name, sandbox);
  _requireCache[name] = sandbox.exports;
  // __dirname = sandbox.__dirname;

  // console.log("require dirname after: " + sandbox.__dirname, __dirname);

  return sandbox.exports;
}
