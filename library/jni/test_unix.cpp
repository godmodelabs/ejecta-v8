#include "ClientOSX.h"
#include "BGJSContext.h"
#include "bgjs/modules/AjaxModule.h"

int main() {
	ClientOSX* _client = new ClientOSX();
	BGJSContext& ct= BGJSContext::getInstance();
	ct.setClient(_client);
	ct.createContext();
	ct.registerModule(new AjaxModule());
	ct.load("test.js");
	ct.run();
	ct.tick(500);
	sleep(5);
}
