#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

namespace
{
    // ------------------------------------------------------------------
    // Simple file log (no spdlog setup, so it cannot silently fail)
    // ------------------------------------------------------------------
    std::filesystem::path g_logPath;

    void InitLog()
    {
        auto dir = SKSE::log::log_directory();
        g_logPath = dir ? (*dir / "WerewolfHotkey.log") : std::filesystem::path("WerewolfHotkey.log");
        std::ofstream f(g_logPath, std::ios::trunc);
        if (f.is_open()) {
            f << "WerewolfHotkey log started\n";
        }
    }

    void Log(const std::string& a_msg)
    {
        if (g_logPath.empty()) {
            return;
        }
        std::ofstream f(g_logPath, std::ios::app);
        if (f.is_open()) {
            f << a_msg << '\n';
        }
    }

    void Notify(const char* a_text)
    {
        RE::DebugNotification(a_text);
    }

    // ------------------------------------------------------------------
    // Config (Data/SKSE/Plugins/WerewolfHotkey.ini)
    // ------------------------------------------------------------------
    struct Config
    {
        std::uint32_t hotkey = 0x25;          // DIK_K
        bool toggleBack = true;
        bool debug = true;                    // log every key press + on-screen messages
        RE::FormID transformSpell = 0x92C48;  // Skyrim.esm, VERIFY in xEdit
        RE::FormID revertSpell = 0x0;         // 0 = revert disabled
        RE::FormID werewolfRace = 0xCDD84;    // Skyrim.esm WerewolfBeastRace, VERIFY in xEdit
    };

    Config g_cfg;

    std::string Trim(std::string s)
    {
        const char* ws = " \t\r\n";
        s.erase(0, s.find_first_not_of(ws));
        s.erase(s.find_last_not_of(ws) + 1);
        return s;
    }

    std::uint32_t ParseNum(const std::string& a_value, std::uint32_t a_default)
    {
        if (a_value.empty()) {
            return a_default;
        }
        char* end = nullptr;
        const auto v = std::strtoul(a_value.c_str(), &end, 0);
        return (end && *end == '\0') ? static_cast<std::uint32_t>(v) : a_default;
    }

    void LoadConfig()
    {
        std::ifstream file("Data/SKSE/Plugins/WerewolfHotkey.ini");
        if (!file.is_open()) {
            Log("INI not found, using defaults");
        } else {
            std::string line;
            while (std::getline(file, line)) {
                line = Trim(line);
                if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') {
                    continue;
                }
                const auto eq = line.find('=');
                if (eq == std::string::npos) {
                    continue;
                }
                const auto key = Trim(line.substr(0, eq));
                auto value = line.substr(eq + 1);
                if (const auto c = value.find(';'); c != std::string::npos) {
                    value.erase(c);
                }
                value = Trim(value);

                if (key == "Hotkey") {
                    g_cfg.hotkey = ParseNum(value, g_cfg.hotkey);
                } else if (key == "ToggleBack") {
                    g_cfg.toggleBack = ParseNum(value, 1) != 0;
                } else if (key == "Debug") {
                    g_cfg.debug = ParseNum(value, 1) != 0;
                } else if (key == "TransformSpell") {
                    g_cfg.transformSpell = ParseNum(value, g_cfg.transformSpell);
                } else if (key == "RevertSpell") {
                    g_cfg.revertSpell = ParseNum(value, g_cfg.revertSpell);
                } else if (key == "WerewolfRace") {
                    g_cfg.werewolfRace = ParseNum(value, g_cfg.werewolfRace);
                }
            }
        }

        Log(std::format("Config: hotkey=0x{:X} toggleBack={} debug={} transform=0x{:X} revert=0x{:X} race=0x{:X}",
                        g_cfg.hotkey, g_cfg.toggleBack, g_cfg.debug, g_cfg.transformSpell,
                        g_cfg.revertSpell, g_cfg.werewolfRace));
    }

    // ------------------------------------------------------------------
    // Logic
    // ------------------------------------------------------------------
    void CastOnPlayer(RE::FormID a_spellID, const char* a_label)
    {
        auto player = RE::PlayerCharacter::GetSingleton();
        auto spell = RE::TESForm::LookupByID<RE::SpellItem>(a_spellID);
        if (!player || !spell) {
            Log(std::format("{}: spell 0x{:X} NOT FOUND", a_label, a_spellID));
            if (g_cfg.debug) {
                Notify("WerewolfHotkey: spell not found, check FormID in ini");
            }
            return;
        }
        auto caster = player->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
        if (!caster) {
            Log(std::format("{}: no magic caster", a_label));
            return;
        }
        caster->CastSpellImmediate(spell, false, player, 1.0f, false, 0.0f, nullptr);
        Log(std::format("{}: cast spell 0x{:X}", a_label, a_spellID));
    }

    void OnHotkey()
    {
        Log("Hotkey pressed");
        if (g_cfg.debug) {
            Notify("WerewolfHotkey: key pressed");
        }

        auto player = RE::PlayerCharacter::GetSingleton();
        auto ui = RE::UI::GetSingleton();
        if (!player || !ui || !player->Is3DLoaded()) {
            Log("Skipped: player/UI not ready");
            return;
        }
        if (ui->GameIsPaused() || ui->IsMenuOpen(RE::Console::MENU_NAME)) {
            Log("Skipped: menu/console open");
            return;
        }

        const auto race = player->GetRace();
        const bool isWerewolf = race && race->GetFormID() == g_cfg.werewolfRace;

        if (!isWerewolf) {
            CastOnPlayer(g_cfg.transformSpell, "Transform");
        } else if (g_cfg.toggleBack && g_cfg.revertSpell != 0) {
            CastOnPlayer(g_cfg.revertSpell, "Revert");
        } else {
            Log("Already werewolf, revert disabled");
        }
    }

    class InputSink : public RE::BSTEventSink<RE::InputEvent*>
    {
    public:
        static InputSink* GetSingleton()
        {
            static InputSink instance;
            return &instance;
        }

        RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event,
                                              RE::BSTEventSource<RE::InputEvent*>*) override
        {
            if (!a_event) {
                return RE::BSEventNotifyControl::kContinue;
            }

            for (auto e = *a_event; e; e = e->next) {
                auto button = e->AsButtonEvent();
                if (!button || !button->IsDown() ||
                    button->GetDevice() != RE::INPUT_DEVICE::kKeyboard) {
                    continue;
                }
                if (g_cfg.debug) {
                    Log(std::format("Key down: code {} (0x{:X})", button->GetIDCode(), button->GetIDCode()));
                }
                if (button->GetIDCode() == g_cfg.hotkey) {
                    SKSE::GetTaskInterface()->AddTask([]() { OnHotkey(); });
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    bool g_registered = false;

    void RegisterInput()
    {
        if (g_registered) {
            return;
        }
        if (auto mgr = RE::BSInputDeviceManager::GetSingleton()) {
            mgr->AddEventSink(InputSink::GetSingleton());
            g_registered = true;
            Log("Input sink registered");
        } else {
            Log("BSInputDeviceManager not available yet");
        }
    }

    void OnMessage(SKSE::MessagingInterface::Message* a_msg)
    {
        switch (a_msg->type) {
        case SKSE::MessagingInterface::kInputLoaded:
            Log("Message: InputLoaded");
            RegisterInput();
            break;
        case SKSE::MessagingInterface::kDataLoaded:
            Log("Message: DataLoaded");
            RegisterInput();
            break;
        case SKSE::MessagingInterface::kNewGame:
        case SKSE::MessagingInterface::kPostLoadGame:
            Log("Message: game loaded");
            if (g_cfg.debug) {
                Notify("WerewolfHotkey loaded");
            }
            break;
        default:
            break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);
    InitLog();
    Log("SKSEPluginLoad called");
    LoadConfig();
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}
