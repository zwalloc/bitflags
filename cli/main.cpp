#include <iostream>

#include <bitflags/bitflags.h>
#include <ulib/format.h>
#include <ulib/runtimeerror.h>
#include <filesystem>
#include <futile/futile.h>
#include <ulib/yaml.h>

namespace fs = std::filesystem;

namespace bitflags
{
    fs::path GetHomePath()
    {
        const char *pHomeEnvPath = std::getenv("HOME");
        if (!pHomeEnvPath)
            pHomeEnvPath = std::getenv("USERPROFILE");

        if (!pHomeEnvPath)
            throw ulib::RuntimeError{
                "Your environment does not contain \"HOME\" and \"USERPROFILE\" necessary variables"};

        return pHomeEnvPath;
    }

    fs::path GetBitflagsDir()
    {
        fs::path path = GetHomePath() / ".bitflags";
        if (!fs::exists(path))
            fs::create_directory(path);

        return path;
    }

    using FlagsDB = ulib::list<std::pair<ulib::string, uint64>>;

    class FlagsInfo
    {
    public:
        FlagsInfo(const FlagsDB &db) : mFlagsDB(db) {}

        std::optional<ulib::string_view> GetName(uint64_t value)
        {
            for (auto &item : mFlagsDB)
                if (item.second == value)
                    return item.first;

            return std::nullopt;
        }

    private:
        FlagsDB mFlagsDB;
    };

    FlagsInfo GetFlagsInfo(ulib::string_view type)
    {
        fs::path dbPath = GetBitflagsDir() / (ulib::str(type) + ".yml");
        if (!fs::exists(dbPath))
            throw ulib::RuntimeError{ulib::format("File does not exist: {}", ulib::wstr(dbPath.u8string()))};

        ulib::list<std::pair<ulib::string, uint64>> result;
        auto node = ulib::yaml::parse(futile::open(dbPath).read<ulib::string>());
        if (node.is_map())
        {
            for (auto &obj : node.items())
            {
                // fmt::print("{}: {}\n", obj.name(), obj.value().scalar());
                result.push_back({obj.name(), std::stoull(obj.value().scalar(), nullptr, 0x10)});
            }
        }

        return FlagsInfo{result};
    }

} // namespace bitflags

#define CHECK_BIT(var, pos) ((uint64_t(var)) & (uint64_t(1) << (uint64_t(pos))))

#include <windows.h>

int main(int argc, const char *argv[])
{
    try
    {
        ulib::list<ulib::string_view> args{argv + 1, argv + argc};

        uint64 flags = 0;
        if (args.size() < 1)
            throw ulib::RuntimeError{"Required hex var argument"};

        if (args[0] == "db" || args[0] == "show")
        {
            ShellExecuteW(NULL, L"open", bitflags::GetBitflagsDir().wstring().c_str(), NULL, NULL, SW_SHOWDEFAULT);
            return 0;
        }

        if (args[0] == "types")
        {
            fs::path dir = bitflags::GetBitflagsDir();
            for (auto& path :  fs::directory_iterator{dir})
            {
                ulib::u8string filename = ulib::u8(path.path().filename().wstring());
                if (filename.ends_with(u8".yml"))
                {
                    filename.pop_back();
                    filename.pop_back();
                    filename.pop_back();
                    filename.pop_back();

                    fmt::print("{}\n", filename);
                }
            }

            return 0;
        }

        flags = std::stoull(args[0], nullptr, 0x10);
        // fmt::print("flags: 0x{:X}\n", flags);

        constexpr size_t kBitsCount = 64;

        std::optional<bitflags::FlagsInfo> flagsInfo = std::nullopt;

        if (args.size() >= 2)
        {
            flagsInfo = bitflags::GetFlagsInfo(args[1]);
        }

        auto getFlagName = [&](uint64_t value) -> std::optional<ulib::string_view> {
            if (flagsInfo)
                return flagsInfo->GetName(value);
            return std::nullopt;
        };

        ulib::string result;
        for (size_t i = 0; i != kBitsCount; i++)
        {
            bool latestIteration = i == (kBitsCount - 1);
            bool isDef = CHECK_BIT(flags, i);
            if (isDef)
            {
                uint64_t flagValue = (uint64)std::pow(2, i);
                if (auto flagName = getFlagName(flagValue))
                {
                    fmt::print("bit {}: [{}] 0x{:X}\n", i, *flagName, flagValue);
                    result += ulib::format("{} | ", *flagName);
                }

                else
                {
                    fmt::print("bit {}: 0x{:X}\n", i, flagValue);
                    result += ulib::format("0x{:X} | ", flagValue);
                }
            }
        }

        if (result.ends_with(" | "))
        {
            result.pop_back();
            result.pop_back();
            result.pop_back();
        }

        fmt::print("\n");
        fmt::print("{}\n", result);
    }
    catch (const std::exception &ex)
    {
        fmt::print("Error: {}\n", ex.what());
    }

    return 0;
}
