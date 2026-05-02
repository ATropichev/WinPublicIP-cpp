#pragma once
#include <string>

// Синхронный HTTP GET. Возвращает тело ответа или бросает std::runtime_error.
std::string HttpGet(const std::string& url, int timeoutMs = 5000);
