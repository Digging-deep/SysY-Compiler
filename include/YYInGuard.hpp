
#ifndef SYSY2022_YYIN_GUARD_HPP
#define SYSY2022_YYIN_GUARD_HPP

#include <cstdio>
#include <stdexcept>
#include <string>
#include "llvm/Support/raw_ostream.h"

// 利用RAII管理flex中的yyin，只需将filename作为参数传入即可
// 避免fopen后忘记fclose
class YYInGuard {
private:
    FILE* file;
    FILE* original_yyin;

public:
    explicit YYInGuard(const std::string& filename, const char* mode = "r")
        : file(std::fopen(filename.c_str(), mode)), original_yyin(nullptr) {
        if (!file) {
            llvm::errs() << "Failed to open file: " << filename << "\n";
            exit(EXIT_FAILURE);
        }
        original_yyin = yyin;  // 保存原始 yyin
        yyin = file;           // 设置全局 yyin
    }

    // 禁止拷贝
    YYInGuard(const YYInGuard&) = delete;
    YYInGuard& operator=(const YYInGuard&) = delete;

    // 支持移动
    YYInGuard(YYInGuard&& other) noexcept
        : file(other.file), original_yyin(other.original_yyin) {
        other.file = nullptr;
        other.original_yyin = nullptr;
    }

    YYInGuard& operator=(YYInGuard&& other) noexcept {
        if (this != &other) {
            restore();
            file = other.file;
            original_yyin = other.original_yyin;
            other.file = nullptr;
            other.original_yyin = nullptr;
        }
        return *this;
    }

    ~YYInGuard() {
        restore();
        if (file) {
            std::fclose(file);
        }
    }

    FILE* get() const { return file; }

    void restore() {
        if (original_yyin) {
            yyin = original_yyin;
        }
    }

    explicit operator bool() const { return file != nullptr; }
};

#endif // SYSY2022_YYIN_GUARD_HPP