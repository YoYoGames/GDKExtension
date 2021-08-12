//
// Copyright (C) 2020 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#pragma once

#include <assert.h>
#include <stdint.h>

#define __YYDEFINE_EXTENSION_FUNCTIONS__
#include "Extension_Interface.h"
#include "Ref.h"
#include "YYRValue.h"

#define YYEXPORT __declspec(dllexport)

#define stricmp	_stricmp

#define	MAX_COMMAND_LINE	8192

#define YYASSERT(...) assert(__VA_ARGS__)

static const int EVENT_OTHER_DIALOG_ASYNC = 63;
static const int EVENT_OTHER_SOCIAL = 70;
static const int EVENT_OTHER_SYSTEM_EVENT = 75;

extern char* g_XboxSCID;