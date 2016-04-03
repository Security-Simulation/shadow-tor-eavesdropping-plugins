/*
 * See LICENSE for licensing information
 */

#include "simpletcp.h" 

void main_log(ShadowLogLevel level, const char* functionName, 
		const char* format, ...) 
{
	va_list variableArguments;
	va_start(variableArguments, format);
	vprintf(format, variableArguments);
	va_end(variableArguments);
	printf("%s", "\n");
}

#define mylog(...) main_log(SHADOW_LOG_LEVEL_INFO, __FUNCTION__, __VA_ARGS__)

void main_createCallback(ShadowPluginCallbackFunc callback, void *data, 
		unsigned int millisecondsDelay) 
{
	usleep(millisecondsDelay * 1000);
	callback(data);
}

ShadowFunctionTable main_functionTable = {
	NULL,
	&main_log,
	&main_createCallback,
	NULL,
};

