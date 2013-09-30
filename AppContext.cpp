#include "AppContext.h"

AppContext::AppContext(aJsonObject **moduleCollection, PushNotify *pushNotify) :
	pushNotify(pushNotify),
	moduleCollection(moduleCollection)
{
}