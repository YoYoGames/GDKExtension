#pragma once

#include <assert.h>
#include <stdint.h>

#define __YYDEFINE_EXTENSION_FUNCTIONS__
#include "Extension_Interface.h"

#ifdef YYXBOX
#include "../../Files/Object/Object_Class.h"
#else
#define YYASSERT(...) assert(__VA_ARGS__)

static const int EVENT_OTHER_DIALOG_ASYNC = 63;
static const int EVENT_OTHER_SYSTEM_EVENT = 75;
#endif

