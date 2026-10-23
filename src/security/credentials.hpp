#pragma once
#include <QString>
namespace tca {
struct Tokens { QString access, refresh; };
class Credentials {
public:
    static Tokens read(const QString &clientId);
    static bool write(const QString &clientId, const Tokens &tokens);
    static bool erase(const QString &clientId);
};
}
