#pragma once

#include <assert.h>
#include <stdint.h>

#define __YYDEFINE_EXTENSION_FUNCTIONS__
#include "Extension_Interface.h"
#include "Ref.h"
#include "YYRValue.h"

#define YYEXPORT __declspec(dllexport)

#define stricmp	_stricmp

#ifdef YYXBOX
#include "../../Files/Object/Object_Class.h"
#else
#define	MAX_COMMAND_LINE	8192

#define YYASSERT(...) assert(__VA_ARGS__)

static const int EVENT_OTHER_DIALOG_ASYNC = 63;
static const int EVENT_OTHER_SOCIAL = 70;
static const int EVENT_OTHER_SYSTEM_EVENT = 75;

extern char* g_XboxSCID;
#endif

