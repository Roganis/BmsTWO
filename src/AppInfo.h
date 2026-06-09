#ifndef APPINFO
#define APPINFO

#include <QtGlobal>

#define APP_NAME "BmsTWO"
#define APP_VERSION_STRING "0.3.0"
#define APP_URL "https://github.com/Roganis/BmsTWO"
#define ORGANIZATION_NAME "ExclusionBms"

#if defined(Q_OS_LINUX)
#define APP_PLATFORM_NAME "Linux"
#elif defined(Q_OS_WIN)
#define APP_PLATFORM_NAME "Windows"
#elif defined(Q_OS_MACOS)
#define APP_PLATFORM_NAME "macOS"
#endif

#define QT_URL "http://www.qt.io/"
#define OGG_VERSION_STRING "Xiph.Org libOgg 1.3.5"
#define OGG_URL "https://www.xiph.org/"
#define VORBIS_URL "https://www.xiph.org/"

#endif // APPINFO
