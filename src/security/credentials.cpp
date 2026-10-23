#include "credentials.hpp"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <windows.h>
#include <wincred.h>

namespace tca {
static std::wstring target(const QString &id)
{
    return (QStringLiteral("TwitchCategoryAssistant/") + QString::fromLatin1(QCryptographicHash::hash(id.toUtf8(), QCryptographicHash::Sha256).toHex())).toStdWString();
}
Tokens Credentials::read(const QString &id)
{
    PCREDENTIALW value = nullptr;
    if (id.isEmpty() || !CredReadW(target(id).c_str(), CRED_TYPE_GENERIC, 0, &value)) return {};
    const auto obj = QJsonDocument::fromJson(QByteArray(reinterpret_cast<char *>(value->CredentialBlob), static_cast<int>(value->CredentialBlobSize))).object();
    SecureZeroMemory(value->CredentialBlob, value->CredentialBlobSize);
    CredFree(value);
    return {obj["access"].toString(), obj["refresh"].toString()};
}
bool Credentials::write(const QString &id, const Tokens &tokens)
{
    auto blob = QJsonDocument(QJsonObject{{"access", tokens.access}, {"refresh", tokens.refresh}}).toJson(QJsonDocument::Compact);
    if (id.isEmpty() || blob.size() > CRED_MAX_CREDENTIAL_BLOB_SIZE) return false;
    auto name = target(id);
    CREDENTIALW cred{};
    cred.Type = CRED_TYPE_GENERIC; cred.TargetName = name.data();
    cred.CredentialBlobSize = static_cast<DWORD>(blob.size());
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(blob.data());
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    const bool ok = CredWriteW(&cred, 0) != FALSE;
    SecureZeroMemory(blob.data(), static_cast<size_t>(blob.size()));
    return ok;
}
bool Credentials::erase(const QString &id)
{
    return id.isEmpty() || CredDeleteW(target(id).c_str(), CRED_TYPE_GENERIC, 0) || GetLastError() == ERROR_NOT_FOUND;
}
}
