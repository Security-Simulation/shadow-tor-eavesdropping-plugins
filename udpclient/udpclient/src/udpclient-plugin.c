
/*
 * See LICENSE for licensing information
 */

#include "udpclient.h"

/* functions that interface into shadow */
ShadowFunctionTable shadowlib;

/* our opaque instance of the udpclient node */
UDPClient* udpclientNodeInstance = NULL;

/* shadow is creating a new instance of this plug-in udpclient a node in
 * the simulation. argc and argv are udpclient configured via the XML.
 */
static void udpclientplugin_new(int argc, char* argv[]) {
	/* shadow wants to create a new node. pass this to the lower level
	 * plug-in function that implements this for both plug-in and non-plug-in modes.
	 * also pass along the interface shadow gave us earlier.
	 *
	 * the value of udpclientNodeInstance will be different for every node, because
	 * we did not set it in __shadow_plugin_init__(). this is desirable, because
	 * each node needs its own application state.
	 */
	udpclientNodeInstance = udpclient_new(argc, argv, shadowlib.log);
}

/* shadow is freeing an existing instance of this plug-in that we previously
 * created in udpclientplugin_new()
 */
static void udpclientplugin_free() {
	/* shadow wants to free a node. pass this to the lower level
	 * plug-in function that implements this for both plug-in and non-plug-in modes.
	 */
	udpclient_free(udpclientNodeInstance);
}

/* shadow is notifying us that some descriptors are ready to read/write */
static void udpclientplugin_ready() {
	/* shadow wants to handle some descriptor I/O. pass this to the lower level
	 * plug-in function that implements this for both plug-in and non-plug-in modes.
	 */
	udpclient_ready(udpclientNodeInstance);
}

/* plug-in initialization. this only happens once per plug-in,
 * no matter how many nodes (instances of the plug-in) are configured.
 *
 * whatever state is configured in this function will become the default
 * starting state for each node.
 *
 * the "__shadow_plugin_init__" function MUST exist in every plug-in.
 */
void __shadow_plugin_init__(ShadowFunctionTable* shadowlibFuncs) {
	assert(shadowlibFuncs);

	/* locally store the functions we use to call back into shadow */
	shadowlib = *shadowlibFuncs;

	/*
	 * tell shadow how to call us back when creating/freeing nodes, and
	 * where to call to notify us when there is descriptor I/O
	 */
	int success = shadowlib.registerPlugin(&udpclientplugin_new, &udpclientplugin_free, &udpclientplugin_ready);

	/* we log through Shadow by using the log function it supplied to us */
	if(success) {
		shadowlib.log(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__,
				"successfully registered as plug-in state");
	} else {
		shadowlib.log(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"error registering udpclient plug-in state");
	}
}
