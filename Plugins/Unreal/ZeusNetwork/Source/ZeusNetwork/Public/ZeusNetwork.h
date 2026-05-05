#pragma once

#include "CoreMinimal.h"

#if defined(ZEUSNETWORK_API)
#elif defined(ZEUSNETWORK_EXPORTS)
#define ZEUSNETWORK_API DLLEXPORT
#else
#define ZEUSNETWORK_API DLLIMPORT
#endif
