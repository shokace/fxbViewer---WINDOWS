#pragma once
#include <QString>
namespace fxb {
// Writes deterministic synthetic VST2 files; never touches an existing file.
bool createDemoLibrary(const QString &directory, int banks = 192, int chunkBanks = 24, QString *error = nullptr);
}
