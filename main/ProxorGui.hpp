#pragma once

#include "Const.hpp"
#include "ProxorGui_Utils.hpp"
#include "ProxorGui_ConfigItem.hpp"
#include "ProxorGui_DataStore.hpp"

// Switch core support

namespace ProxorGui {
    inline int coreType = CoreType::SING_BOX;

    QString FindCoreAsset(const QString &name);

    QString FindProxorCoreRealPath();

    bool IsAdmin();
} // namespace ProxorGui

#define ROUTES_PREFIX_NAME QStringLiteral("routes_box")
#define ROUTES_PREFIX QString(ROUTES_PREFIX_NAME + "/")
