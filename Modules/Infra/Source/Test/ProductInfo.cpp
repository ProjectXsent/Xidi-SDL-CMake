/***************************************************************************************************
 * Infra
 *   Common infrastructure used by other projects.
 ***************************************************************************************************
 * @file ProductInfo.cpp
 *   Defines the product name and version for the CoreInfraTest executable. Added for the CMake
 *   build: CoreInfra requires every executable that links it to provide these definitions.
 **************************************************************************************************/

#include <string>

#include "Core/ProcessInfo.h"

INFRA_DEFINE_PRODUCT_NAME_FROM_LITERAL(L"CoreInfraTest");
INFRA_DEFINE_PRODUCT_VERSION_FROM_GIT_VERSION_INFO();
