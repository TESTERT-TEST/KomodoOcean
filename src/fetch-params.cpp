// Copyright (c) 2024-2026 The Komodo developers
// Distributed under the MIT software license.
//
// fetch-params: Downloads Zcash zkSNARK parameters and verifies SHA256 integrity.
// Uses the same directory as zcutil/fetch-params.sh (ZC_GetBaseParamsDir).

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>

#include <boost/filesystem.hpp>
#include <curl/curl.h>

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#ifdef _WIN32
#include <shlobj.h>
#endif

#include <openssl/evp.h>

namespace fs = boost::filesystem;

// Parameters from zcutil/fetch-params.sh
static const char* const DOWNLOAD_URLS[] = {
    "https://z.cash/downloads",
    "https://omega.decker.im/downloads",
    "https://komodoplatform.com/downloads"
};
static const size_t NUM_DOWNLOAD_URLS = sizeof(DOWNLOAD_URLS) / sizeof(DOWNLOAD_URLS[0]);
static const char* const SAPLING_SPEND_NAME = "sapling-spend.params";
static const char* const SAPLING_OUTPUT_NAME = "sapling-output.params";
static const char* const SAPLING_SPROUT_GROTH16_NAME = "sprout-groth16.params";

struct ParamInfo {
    const char* filename;
    const char* expected_sha256_hex;
};

static const ParamInfo PARAMS[] = {
    {SAPLING_SPEND_NAME, "8e48ffd23abb3a5fd9c5589204f32d9c31285a04b78096ba40a79b75677efc13"},
    {SAPLING_OUTPUT_NAME, "2f0ebbcbb9bb0bcffe95a397e7eba89c29eb4dde6191c339db88570e3f3fb0e4"},
    {SAPLING_SPROUT_GROTH16_NAME, "b685d700c60328498fbde589c8c7c484c722b788b265b72af448a5bf0ee55b50"},
};

static const size_t NUM_PARAMS = sizeof(PARAMS) / sizeof(PARAMS[0]);

// ZC_GetBaseParamsDir - same logic as util.cpp
static fs::path ZC_GetBaseParamsDir()
{
#ifdef _WIN32
    {
        char pszPath[MAX_PATH] = "";
        if (SHGetSpecialFolderPathA(NULL, pszPath, CSIDL_APPDATA, true)) {
            return fs::path(pszPath) / "ZcashParams";
        }
    }
    return fs::path("");
#else
    char* pszHome = getenv("HOME");
    fs::path pathRet;
    if (pszHome == NULL || strlen(pszHome) == 0) {
        pathRet = fs::path("/");
    } else {
        pathRet = fs::path(pszHome);
    }
#ifdef MAC_OSX
    pathRet /= "Library/Application Support";
    if (!fs::exists(pathRet)) {
        fs::create_directories(pathRet);
    }
    return pathRet / "ZcashParams";
#else
    return pathRet / ".zcash-params";
#endif
#endif
}

// Parse hex string to bytes, returns false on invalid input
static bool ParseHex(const std::string& hex, unsigned char* out, size_t outlen)
{
    if (hex.size() != outlen * 2) return false;
    for (size_t i = 0; i < outlen; i++) {
        int hi = 0, lo = 0;
        char c = hex[2 * i];
        if (c >= '0' && c <= '9') hi = c - '0';
        else if (c >= 'a' && c <= 'f') hi = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') hi = c - 'A' + 10;
        else return false;
        c = hex[2 * i + 1];
        if (c >= '0' && c <= '9') lo = c - '0';
        else if (c >= 'a' && c <= 'f') lo = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') lo = c - 'A' + 10;
        else return false;
        out[i] = (hi << 4) | lo;
    }
    return true;
}

// Compute SHA256 of file with progress (EVP API - compatible with OpenSSL 3.0+)
// filename_for_progress: if non-null, displays verification progress
static bool ComputeFileSHA256(const fs::path& path, unsigned char hash[32],
                              const char* filename_for_progress = NULL)
{
    FILE* f = fopen(path.string().c_str(), "rb");
    if (!f) return false;

    boost::uintmax_t file_size = 0;
    try {
        file_size = fs::file_size(path);
    } catch (...) {
        fclose(f);
        return false;
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) { fclose(f); return false; }

    bool ok = EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    if (ok) {
        unsigned char buf[65536];
        size_t n;
        boost::uintmax_t total_read = 0;
        int last_pct = -1;

        while (ok && (n = fread(buf, 1, sizeof(buf), f)) > 0) {
            ok = EVP_DigestUpdate(ctx, buf, n);
            total_read += n;

            if (filename_for_progress && file_size > 0) {
                int pct = static_cast<int>(100 * total_read / file_size);
                if (pct != last_pct || total_read == file_size) {
                    last_pct = pct;
                    fprintf(stderr, "\r  %s: verifying %.1f%% (%llu / %llu MB)   ",
                            filename_for_progress, 100.0 * total_read / file_size,
                            static_cast<unsigned long long>(total_read) / (1024 * 1024),
                            static_cast<unsigned long long>(file_size) / (1024 * 1024));
                }
            }
        }
        ok = ok && !ferror(f) && EVP_DigestFinal_ex(ctx, hash, NULL);
    }
    EVP_MD_CTX_free(ctx);
    fclose(f);
    return ok;
}

// Compare hash with expected hex string, optionally with progress display
static bool VerifySHA256(const fs::path& path, const char* expected_hex,
                        const char* filename_for_progress = NULL)
{
    const size_t SHA256_LEN = 32;
    unsigned char computed[SHA256_LEN];
    unsigned char expected[SHA256_LEN];

    if (!ComputeFileSHA256(path, computed, filename_for_progress)) return false;
    if (filename_for_progress) {
        try {
            boost::uintmax_t file_size = fs::file_size(path);
            fprintf(stderr, "\r  %s: verifying 100.0%% (%llu / %llu MB)   \n",
                    filename_for_progress,
                    static_cast<unsigned long long>(file_size) / (1024 * 1024),
                    static_cast<unsigned long long>(file_size) / (1024 * 1024));
        } catch (...) {
            fprintf(stderr, "\n");
        }
    }
    if (!ParseHex(expected_hex, expected, SHA256_LEN)) return false;

    return memcmp(computed, expected, SHA256_LEN) == 0;
}

// Progress callback for curl
static int ProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                           curl_off_t /*ultotal*/, curl_off_t /*ulnow*/)
{
    const char* filename = static_cast<const char*>(clientp);
    if (dltotal > 0 && dlnow > 0) {
        double pct = 100.0 * static_cast<double>(dlnow) / static_cast<double>(dltotal);
        fprintf(stderr, "\r  %s: %.1f%% (%llu / %llu MB)   ",
                filename, pct,
                static_cast<unsigned long long>(dlnow) / (1024 * 1024),
                static_cast<unsigned long long>(dltotal) / (1024 * 1024));
    } else if (dlnow > 0) {
        fprintf(stderr, "\r  %s: %llu MB downloaded...   ",
                filename, static_cast<unsigned long long>(dlnow) / (1024 * 1024));
    }
    return 0;
}

// Write callback for curl - write to file
static size_t WriteCallback(void* ptr, size_t size, size_t nmemb, void* userdata)
{
    FILE* f = static_cast<FILE*>(userdata);
    return fwrite(ptr, size, nmemb, f);
}

static bool DownloadFile(const std::string& url, const fs::path& output_path)
{
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize curl" << std::endl;
        return false;
    }

    FILE* f = fopen(output_path.string().c_str(), "wb");
    if (!f) {
        std::cerr << "Failed to open " << output_path << " for writing: " << strerror(errno) << std::endl;
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);  // curl built without SSL; integrity checked via SHA256
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    // Limit to TLS 1.2: in some countries with internet censorship, TLS 1.3 is blocked due to ECH
    // curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_MAX_TLSv1_2);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);  // 10 seconds to establish connection
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 32768L);   // 32 KB/s minimum speed
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);       // abort if below limit for 30 seconds
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
    std::string progress_name = output_path.filename().string();
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, progress_name.c_str());

    CURLcode res = curl_easy_perform(curl);
    fclose(f);

    if (res != CURLE_OK) {
        std::cerr << "Download failed: " << curl_easy_strerror(res) << std::endl;
        fs::remove(output_path);
        curl_easy_cleanup(curl);
        return false;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (http_code != 200) {
        std::cerr << "HTTP error: " << http_code << std::endl;
        fs::remove(output_path);
        return false;
    }

    return true;
}

static bool FetchParam(const fs::path& params_dir, const ParamInfo& param)
{
    fs::path output_path = params_dir / param.filename;

    if (fs::exists(output_path)) {
        if (VerifySHA256(output_path, param.expected_sha256_hex, param.filename)) {
            std::cout << param.filename << ": already exists, checksum OK" << std::endl;
            return true;
        }
        std::cerr << param.filename << ": exists but checksum mismatch, re-downloading" << std::endl;
        fs::remove(output_path);
    }

    fs::path dl_path = output_path;
    dl_path += ".dl";

    bool download_ok = false;
    for (size_t url_idx = 0; url_idx < NUM_DOWNLOAD_URLS; url_idx++) {
        std::string url = std::string(DOWNLOAD_URLS[url_idx]) + "/" + param.filename;
        std::cout << "Retrieving: " << url << std::endl;

        if (DownloadFile(url, dl_path)) {
            download_ok = true;
            break;
        }
        if (url_idx + 1 < NUM_DOWNLOAD_URLS) {
            std::cerr << "Download failed, trying next mirror..." << std::endl;
        }
    }

    if (!download_ok) {
        return false;
    }

    try {
        boost::uintmax_t file_size = fs::file_size(dl_path);
        fprintf(stderr, "\r  %s: 100.0%% (%llu / %llu MB) - verifying...   \n",
                param.filename,
                static_cast<unsigned long long>(file_size) / (1024 * 1024),
                static_cast<unsigned long long>(file_size) / (1024 * 1024));
    } catch (...) {
        fprintf(stderr, "\r  %s: 100%% - verifying...   \n", param.filename);
    }

    if (!VerifySHA256(dl_path, param.expected_sha256_hex, param.filename)) {
        std::cerr << "Failed to verify parameter checksums for " << param.filename << "!" << std::endl;
        fs::remove(dl_path);
        return false;
    }

    if (rename(dl_path.string().c_str(), output_path.string().c_str()) != 0) {
        std::cerr << "Failed to rename " << dl_path << " to " << output_path << ": " << strerror(errno) << std::endl;
        fs::remove(dl_path);
        return false;
    }

    std::cout << "Download successful: " << param.filename << std::endl;
    return true;
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "Zcash - fetch-params" << std::endl;
    std::cout << std::endl;
    std::cout << "This program will fetch the Zcash zkSNARK parameters and verify their" << std::endl;
    std::cout << "integrity with SHA256." << std::endl;
    std::cout << std::endl;
    std::cout << "If they already exist locally and checksums match, they will be skipped." << std::endl;
    std::cout << std::endl;

    fs::path params_dir = ZC_GetBaseParamsDir();
    if (params_dir.empty()) {
        std::cerr << "Failed to determine params directory" << std::endl;
        return 1;
    }

    if (!fs::exists(params_dir)) {
        if (!fs::create_directories(params_dir)) {
            std::cerr << "Failed to create directory: " << params_dir << std::endl;
            return 1;
        }
        std::cout << "Created params directory: " << params_dir << std::endl;
        std::cout << "The parameters are ~911MB - plan accordingly for bandwidth." << std::endl;
        std::cout << std::endl;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);

    bool all_ok = true;
    for (size_t i = 0; i < NUM_PARAMS; i++) {
        if (!FetchParam(params_dir, PARAMS[i])) {
            all_ok = false;
            break;
        }
    }

    curl_global_cleanup();

    if (!all_ok) {
        std::cerr << std::endl << "Failed to fetch/verify parameters!" << std::endl;
        return 1;
    }

    std::cout << std::endl << "All parameters ready." << std::endl;
    return 0;
}
