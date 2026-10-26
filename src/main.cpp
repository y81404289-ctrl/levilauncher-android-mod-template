#include <llapi/LoggerAPI.h>
#include <llapi/mc/Player.h>
#include <llapi/mc/ItemStack.h>
#include <llapi/mc/Actor.h>
#include <llapi/utils/FileHelper.h>
#include <nlohmann/json.hpp>
#include <dobby.h>

// ==========================================
// 1. CONFIG SYSTEM (MENÜ-SCHALTER)
// ==========================================
namespace ModConfig {
    bool enabled = true;
    const std::string configPath = "plugins/AutoTotem/config.json";

    void load() {
        if (FileHelper::exists(configPath)) {
            try {
                auto data = FileHelper::readAllText(configPath);
                auto json = nlohmann::json::parse(data);
                if (json.contains("enabled")) {
                    enabled = json["enabled"].get<bool>();
                }
            } catch (...) {
                enabled = true; // Falls Datei kaputt, Standard an
            }
        }
    }

    void save() {
        nlohmann::json json;
        json["enabled"] = enabled;
        FileHelper::writeAllText(configPath, json.dump(4));
    }
}

// ==========================================
// 2. AUTO-TOTEM HOOK LOGIK
// ==========================================
Logger logger("AutoTotem");

typedef bool (*PlayerHurt_t)(Player* self, ActorDamageSource const& source, int damage, bool bypassArmor, bool bypassDamageing);
PlayerHurt_t ori_PlayerHurt = nullptr;

bool hook_PlayerHurt(Player* self, ActorDamageSource const& source, int damage, bool bypassArmor, bool bypassDamageing) {
    // Wenn der Schalter im LeviLauncher-Menü auf "AUS" steht, brechen wir sofort ab
    if (!ModConfig::enabled) {
        return ori_PlayerHurt(self, source, damage, bypassArmor, bypassDamageing);
    }

    if (self && self->isPlayer()) {
        int currentHealth = self->getHealth();
        
        // Wenn der Schaden ausreicht, um den Spieler zu töten
        if (currentHealth - damage <= 0) {
            auto& offhandItem = self->getOffhandSlot();
            
            // Prüfen, ob nicht bereits ein Totem in der Schildhand liegt
            if (offhandItem.getRawNameId() != "totem_of_undying") {
                auto& inv = self->getSupplies();
                int containerSize = inv.getContainerSize();
                
                // Inventar frame-perfekt nach einem Totem durchsuchen
                for (int i = 0; i < containerSize; ++i) {
                    auto& item = inv.getItem(i);
                    if (!item.isNull() && item.getRawNameId() == "totem_of_undying") {
                        
                        // Totem ohne Verzögerung in die Schildhand drücken
                        self->setOffhandSlot(item);
                        
                        // Ein Totem aus dem Inventar-Slot abziehen
                        if (item.getCount() > 1) {
                            item.setCount(item.getCount() - 1);
                            inv.setItem(i, item);
                        } else {
                            inv.clearSlot(i);
                        }
                        
                        // Client-Synchronisation erzwingen
                        self->refreshContainer();
                        break; 
                    }
                }
            }
        }
    }
    return ori_PlayerHurt(self, source, damage, bypassArmor, bypassDamageing);
}

// ==========================================
// 3. MAIN HAUPTEINSTIEG (START DER MOD)
// ==========================================
extern "C" [[maybe_unused]] void mod_init() {
    // Konfiguration (Menü-Status) laden
    ModConfig::load();
    
    logger.info("AutoTotem Mod mit UI-Support erfolgreich geladen!");
    
    // Suche nach der Speicheradresse im Spielcode für Android
    void* playerHurtAddr = dlsym(RTLD_DEFAULT, "_ZN6Player4hurtERK17ActorDamageSourceibb");
    if (playerHurtAddr) {
        DobbyHook(playerHurtAddr, (void*)hook_PlayerHurt, (void**)&ori_PlayerHurt);
        logger.info("Hook auf Player::hurt gesetzt - Bereit für frame-perfektes Ausrüsten.");
    } else {
        logger.error("Fehler: Speicheradresse für Player::hurt nicht gefunden!");
    }
}
