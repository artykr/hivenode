/*
  AppContext.h - Library for holding application context properties
*/
  
#ifndef AppContext_h
#define AppContext_h
#define APPCONTEXT_MODULE_VERSION 1

#include "aJSON.h"

class AppContext
{
  public:
    typedef boolean PushNotify(byte moduleId);
    PushNotify *pushNotify;
    aJsonObject **moduleCollection;
    AppContext(aJsonObject **moduleCollection, PushNotify *pushNotify);
};

#endif