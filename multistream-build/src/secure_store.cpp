// OBS MultiStream Hub - Creado por Sintacto (@sintacto12)
#include "secure_store.hpp"

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>

std::string protectSecret(const std::string &plain)
{
	if (plain.empty())
		return {};

	DATA_BLOB in{static_cast<DWORD>(plain.size()), reinterpret_cast<BYTE *>(const_cast<char *>(plain.data()))};
	DATA_BLOB out{};
	if (!CryptProtectData(&in, L"obs-multistream-hub", nullptr, nullptr, nullptr, 0, &out))
		return {};

	DWORD len = 0;
	CryptBinaryToStringA(out.pbData, out.cbData, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &len);
	std::string b64(len, '\0');
	if (!CryptBinaryToStringA(out.pbData, out.cbData, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, b64.data(),
				  &len)) {
		LocalFree(out.pbData);
		return {};
	}
	b64.resize(len);
	LocalFree(out.pbData);
	return b64;
}

std::string unprotectSecret(const std::string &b64)
{
	if (b64.empty())
		return {};

	DWORD n = 0;
	if (!CryptStringToBinaryA(b64.c_str(), 0, CRYPT_STRING_BASE64, nullptr, &n, nullptr, nullptr))
		return {};
	std::string bin(n, '\0');
	if (!CryptStringToBinaryA(b64.c_str(), 0, CRYPT_STRING_BASE64, reinterpret_cast<BYTE *>(bin.data()), &n,
				  nullptr, nullptr))
		return {};

	DATA_BLOB in{n, reinterpret_cast<BYTE *>(bin.data())};
	DATA_BLOB out{};
	if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out))
		return {};

	std::string plain(reinterpret_cast<char *>(out.pbData), out.cbData);
	SecureZeroMemory(out.pbData, out.cbData);
	LocalFree(out.pbData);
	return plain;
}

#else
// Este plugin apunta a Windows 10/11. En otras plataformas NO se guardan
// claves (mejor no guardar que guardar sin cifrar).
std::string protectSecret(const std::string &) { return {}; }
std::string unprotectSecret(const std::string &) { return {}; }
#endif
