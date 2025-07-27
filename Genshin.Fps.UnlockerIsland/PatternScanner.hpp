#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <sstream>
#include <Psapi.h>

#pragma comment(lib, "psapi.lib")

namespace PatternScanner {

    struct RegionInfo {
        uintptr_t base;
        size_t size;
    };

    inline bool IsReadableOrExecutable(DWORD protect) {
        return
            protect == PAGE_EXECUTE_READ ||
            protect == PAGE_EXECUTE_READWRITE ||
            protect == PAGE_EXECUTE_WRITECOPY ||
            protect == PAGE_EXECUTE ||
            protect == PAGE_READONLY ||
            protect == PAGE_READWRITE ||
            protect == PAGE_WRITECOPY;
    }

    inline std::vector<RegionInfo> GetMemoryRegions() {
        std::vector<RegionInfo> regions;
        SYSTEM_INFO sysInfo = {};
        GetSystemInfo(&sysInfo);

        uintptr_t start = reinterpret_cast<uintptr_t>(sysInfo.lpMinimumApplicationAddress);
        uintptr_t end = reinterpret_cast<uintptr_t>(sysInfo.lpMaximumApplicationAddress);

        MEMORY_BASIC_INFORMATION mbi{};
        while (start < end) {
            if (VirtualQuery(reinterpret_cast<void*>(start), &mbi, sizeof(mbi)) == 0)
                break;

            if ((mbi.State == MEM_COMMIT) && IsReadableOrExecutable(mbi.Protect)) {
                regions.push_back({ reinterpret_cast<uintptr_t>(mbi.BaseAddress), mbi.RegionSize });
            }

            start += mbi.RegionSize;
        }
        return regions;
    }

    inline void ParsePattern(const std::string& pattern, std::vector<std::pair<uint8_t, bool>>& parsed) {
        std::istringstream iss(pattern);
        std::string byteStr;

        while (iss >> byteStr) {
            if (byteStr == "?" || byteStr == "??") {
                parsed.emplace_back(0x00, true);  // ͨ���
            }
            else {
                parsed.emplace_back(static_cast<uint8_t>(std::strtoul(byteStr.c_str(), nullptr, 16)), false);
            }
        }
    }

    // ������C++����İ�ȫƥ�亯��������ʹ�� __try ��������ͻ
    inline bool SafeCompare(uint8_t* base, size_t size, const std::vector<std::pair<uint8_t, bool>>* pattern, uintptr_t* result) {
        __try {
            size_t patternSize = pattern->size();
            for (size_t i = 0; i <= size - patternSize; ++i) {
                bool found = true;
                for (size_t j = 0; j < patternSize; ++j) {
                    if (!(*pattern)[j].second && base[i + j] != (*pattern)[j].first) {
                        found = false;
                        break;
                    }
                }
                if (found) {
                    *result = reinterpret_cast<uintptr_t>(&base[i]);
                    return true;
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            // ������Ч�ڴ�
        }
        return false;
    }

    inline uintptr_t Scan(const std::string& pattern) {
        std::vector<std::pair<uint8_t, bool>> parsedPattern;
        ParsePattern(pattern, parsedPattern);
        auto regions = GetMemoryRegions();

        for (const auto& region : regions) {
            uintptr_t found = 0;
            if (SafeCompare(reinterpret_cast<uint8_t*>(region.base), region.size, &parsedPattern, &found)) {
                return found;
            }
        }

        return 0;
    }

    // ������ת��ָ��� call/jmp��E8/E9��
    // instructionAddr: ָ���ַ��E8/E9��ͷ��
    // offset: ��תƫ��λ�ã�E8������ +1��
    // instructionSize: ����ָ���С��E8 xx xx xx xx �� 5 �ֽڣ�
    inline uintptr_t ResolveRelativeAddress(uintptr_t instructionAddr, size_t offset = 1, size_t instructionSize = 5) {
        int32_t rel = *reinterpret_cast<int32_t*>(instructionAddr + offset);
        return instructionAddr + instructionSize + rel;
    }

}
