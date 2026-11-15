#pragma once

#include <QString>

namespace fm {

// Resolves a file name received from a client to an absolute path inside
// the watched directory.
//
// Returns an empty string and sets *error if the name is not a plain file
// name, the file does not exist, or it resolves (e.g. through a symlink) to
// a location outside the directory. This keeps clients from reading
// arbitrary files of the server host.
QString resolveRequestedFile(const QString& directory, const QString& fileName, QString* error);

} // namespace fm
