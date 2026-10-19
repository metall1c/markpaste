#pragma once

#include <QString>

namespace mp {

// Имя локального сокета для единственного экземпляра. На Unix — путь в каталоге
// пользователя ($XDG_RUNTIME_DIR), а не в общем /tmp; на Windows — имя канала.
QString singleInstanceSocketName(const QString &appId);

} // namespace mp
