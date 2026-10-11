#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>

namespace
{
    // ------------------------------------------------------------------
    // Config (Data/SKSE/Plugins/WerewolfHotkey.ini)
    // ------------------------------------------------------------------
    struct Config
    {
        std::uint32_t hotkey = 0x25;          // DIK_K
        bool toggleBack = true;               // press again to revert
        RE::FormID transformSpell = 0x92C48;  // Skyrim.esm, VERIFY in xEdit
        RE::FormID revertSpell = 0x0;         // 0 = revert disabled, VERIFY in xEdit
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
        const auto v = std::strtoul(a_value.c_str(), &end, 0);  // accepts 37 or 0x25
        return (end && *end == '\0') ? static_cast<std::uint32_t>(v) : a_default;
    }

    void LoadConfig()
    {
        std::ifstream file("Data/SKSE/Plugins/WerewolfHotkey.ini");
        if (!file.is_open()) {
            spdlog::warn("INI not found, using defaults");
            return;
        }

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
            } else if (key == "TransformSpell") {
                g_cfg.transformSpell = ParseNum(value, g_cfg.transformSpell);
            } else if (key == "RevertSpell") {
                g_cfg.revertSpell = ParseNum(value, g_cfg.revertSpell);
            } else if (key == "WerewolfRace") {
                g_cfg.werewolfRace = ParseNum(value, g_cfg.werewolfRace);
            }
        }

        spdlog::info("Config: hotkey=0x{:X} toggleBack={} transform=0x{:X} revert=0x{:X} race=0x{:X}",
                     g_cfg.hotkey, g_cfg.toggleBack, g_cfg.transformSpell, g_cfg.revertSpell,
                     g_cfg.werewolfRace);
    }

    void SetupLog()
    {
        auto path = SKSE::log::log_directory();
        if (!path) {
            return;
        }
        *path /= "WerewolfHotkey.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        auto log = std::make_shared<spdlog::logger>("global log", std::move(sink));
        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);
        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%H:%M:%S] [%l] %v");
    }

    // ------------------------------------------------------------------
    // Logic
    // ------------------------------------------------------------------
    void CastOnPlayer(RE::FormID a_spellID, const char* a_label)
    {
        auto player = RE::PlayerCharacter::GetSingleton();
        auto spell = RE::TESForm::LookupByID<RE::SpellItem>(a_spellID);
        if (!player || !spell) {
            spdlog::error("{}: spell 0x{:X} not found", a_label, a_spellID);
            return;
        }
        auto caster = player->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
        if (!caster) {
            spdlog::error("{}: no magic caster", a_label);
            return;
        }
        caster->CastSpellImmediate(spell, false, player, 1.0f, false, 0.0f, nullptr);
        spdlog::info("{}: cast spell 0x{:X}", a_label, a_spellID);
    }

    void OnHotkey()
    {
        auto player = RE::PlayerCharacter::GetSingleton();
        auto ui = RE::UI::GetSingleton();
        if (!player || !ui || !player->Is3DLoaded()) {
            return;
        }
        // ignore while in menus / paused / typing in console
        if (ui->GameIsPaused() || ui->IsMenuOpen(RE::Console::MENU_NAME)) {
            return;
        }

        const auto race = player->GetRace();
        const bool isWerewolf = race && race->GetFormID() == g_cfg.werewolfRace;

        if (!isWerewolf) {
            CastOnPlayer(g_cfg.transformSpell, "Transform");
        } else if (g_cfg.toggleBack && g_cfg.revertSpell != 0) {
            CastOnPlayer(g_cfg.revertSpell, "Revert");
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
                    button->GetDevice() != RE::INPUT_DEVICE::kKeyboard ||
                    button->GetIDCode() != g_cfg.hotkey) {
                    continue;
                }
                // run on the game's main thread
                SKSE::GetTaskInterface()->AddTask([]() { OnHotkey(); });
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    void OnMessage(SKSE::MessagingInterface::Message* a_msg)
    {
        if (a_msg->type == SKSE::MessagingInterface::kInputLoaded) {
            if (auto mgr = RE::BSInputDeviceManager::GetSingleton()) {
                mgr->AddEventSink(InputSink::GetSingleton());
                spdlog::info("Input sink registered");
            }
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);
    SetupLog();
    spdlog::info("WerewolfHotkey loaded");
    LoadConfig();
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}
