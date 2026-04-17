#include "entity/archetype.hpp"
#include "entities/altar.hpp"
#include "entities/arrow_trap.hpp"
#include "entities/baseball_bat.hpp"
#include "entities/bat.hpp"
#include "entities/block.hpp"
#include "entities/bomb.hpp"
#include "entities/bow.hpp"
#include "entities/box.hpp"
#include "entities/caveman.hpp"
#include "entities/chest.hpp"
#include "entities/damsel.hpp"
#include "entities/dvdlogo.hpp"
#include "entities/dice.hpp"
#include "entities/emerald_big.hpp"
#include "entities/gear_items.hpp"
#include "entities/ghost_ball.hpp"
#include "entities/giant_tiki_head.hpp"
#include "entities/gold_idol.hpp"
#include "entities/jetpack.hpp"
#include "entities/kali_head.hpp"
#include "entities/lantern.hpp"
#include "entities/machete.hpp"
#include "entities/mattock.hpp"
#include "entities/none_archetype.hpp"
#include "entities/money.hpp"
#include "entities/mouse_trailer.hpp"
#include "entities/player.hpp"
#include "entities/rock.hpp"
#include "entities/rope.hpp"
#include "entities/ruby_big.hpp"
#include "entities/sac_altar.hpp"
#include "entities/sapphire_big.hpp"
#include "entities/scarab.hpp"
#include "entities/shotgun.hpp"
#include "entities/shop.hpp"
#include "entities/shopkeeper.hpp"
#include "entities/sign.hpp"
#include "entities/snake.hpp"
#include "entities/spider_hang.hpp"
#include "entities/stomp_pad.hpp"
#include "entities/store_light.hpp"
#include "entities/pot.hpp"
#include "entities/spider.hpp"
#include "entities/teleporter.hpp"
#include "entities/web_cannon.hpp"
#include "entities/pistol.hpp"

#include <cassert>

namespace splonks {

namespace {

std::array<EntityArchetype, kEntityTypeCount> g_entity_archetypes{};
bool g_entity_archetypes_populated = false;

void SetArchetype(EntityType type_, const EntityArchetype& archetype, const char* debug_name) {
    EntityArchetype populated = archetype;
    populated.debug_name = debug_name;
    g_entity_archetypes[EntityTypeIndex(type_)] = populated;
}

} // namespace

const EntityArchetype& GetEntityArchetype(EntityType type_) {
    assert(g_entity_archetypes_populated && "PopulateEntityArchetypesTable must run before lookup");
    return g_entity_archetypes[EntityTypeIndex(type_)];
}

const char* GetEntityTypeName(EntityType type_) {
    if (!g_entity_archetypes_populated) {
        return "Unknown";
    }

    const char* const debug_name = g_entity_archetypes[EntityTypeIndex(type_)].debug_name;
    return debug_name != nullptr ? debug_name : "Unknown";
}

void PopulateEntityArchetypesTable() {
    SetArchetype(EntityType::None, kNoneArchetype, "None");
    SetArchetype(EntityType::Player, entities::player::kPlayerArchetype, "Player");
    SetArchetype(EntityType::Block, entities::block::kBlockArchetype, "Block");
    SetArchetype(EntityType::GhostBall, entities::ghost_ball::kGhostBallArchetype, "GhostBall");
    SetArchetype(EntityType::DvdLogo, entities::dvdlogo::kDvdLogoArchetype, "DvdLogo");
    SetArchetype(EntityType::Bat, entities::bat::kBatArchetype, "Bat");
    SetArchetype(EntityType::Rock, entities::rock::kRockArchetype, "Rock");
    SetArchetype(EntityType::MouseTrailer, entities::mouse_trailer::kMouseTrailerArchetype, "MouseTrailer");
    SetArchetype(EntityType::JetPack, entities::jetpack::kJetPackArchetype, "JetPack");
    SetArchetype(EntityType::Bomb, entities::bomb::kBombArchetype, "Bomb");
    SetArchetype(EntityType::Gold, entities::money::kGoldArchetype, "Gold");
    SetArchetype(EntityType::GoldStack, entities::money::kGoldStackArchetype, "GoldStack");
    SetArchetype(EntityType::GoldChunk, entities::money::kGoldChunkArchetype, "GoldChunk");
    SetArchetype(EntityType::GoldNugget, entities::money::kGoldNuggetArchetype, "GoldNugget");
    SetArchetype(EntityType::GoldBar, entities::money::kGoldBarArchetype, "GoldBar");
    SetArchetype(EntityType::GoldBars, entities::money::kGoldBarsArchetype, "GoldBars");
    SetArchetype(EntityType::Rope, entities::rope::kRopeArchetype, "Rope");
    SetArchetype(EntityType::Pot, entities::pot::kPotArchetype, "Pot");
    SetArchetype(EntityType::Box, entities::box::kBoxArchetype, "Box");
    SetArchetype(EntityType::StompPad, entities::stomp_pad::kStompPadArchetype, "StompPad");
    SetArchetype(EntityType::BaseballBat, entities::baseball_bat::kBaseballBatArchetype, "BaseballBat");
    SetArchetype(EntityType::Altar, entities::altar::kAltarArchetype, "Altar");
    SetArchetype(EntityType::SacAltar, entities::sac_altar::kSacAltarArchetype, "SacAltar");
    SetArchetype(EntityType::GoldIdol, entities::gold_idol::kGoldIdolArchetype, "GoldIdol");
    SetArchetype(EntityType::Chest, entities::chest::kChestArchetype, "Chest");
    SetArchetype(EntityType::KeyChest, entities::chest::kKeyChestArchetype, "KeyChest");
    SetArchetype(EntityType::ChestKey, entities::chest::kChestKeyArchetype, "ChestKey");
    SetArchetype(EntityType::UdjatEye, entities::chest::kUdjatEyeArchetype, "UdjatEye");
    SetArchetype(EntityType::Mattock, entities::mattock::kMattockArchetype, "Mattock");
    SetArchetype(EntityType::Cape, entities::gear_items::kCapeArchetype, "Cape");
    SetArchetype(EntityType::Shotgun, entities::shotgun::kShotgunArchetype, "Shotgun");
    SetArchetype(EntityType::Teleporter, entities::teleporter::kTeleporterArchetype, "Teleporter");
    SetArchetype(EntityType::Gloves, entities::gear_items::kGlovesArchetype, "Gloves");
    SetArchetype(EntityType::Spectacles, entities::gear_items::kSpectaclesArchetype, "Spectacles");
    SetArchetype(EntityType::WebCannon, entities::web_cannon::kWebCannonArchetype, "WebCannon");
    SetArchetype(EntityType::Pistol, entities::pistol::kPistolArchetype, "Pistol");
    SetArchetype(EntityType::Mitt, entities::gear_items::kMittArchetype, "Mitt");
    SetArchetype(EntityType::Paste, entities::gear_items::kPasteArchetype, "Paste");
    SetArchetype(EntityType::SpringShoes, entities::gear_items::kSpringShoesArchetype, "SpringShoes");
    SetArchetype(EntityType::SpikeShoes, entities::gear_items::kSpikeShoesArchetype, "SpikeShoes");
    SetArchetype(EntityType::Machete, entities::machete::kMacheteArchetype, "Machete");
    SetArchetype(EntityType::BombBox, entities::gear_items::kBombBoxArchetype, "BombBox");
    SetArchetype(EntityType::BombBag, entities::gear_items::kBombBagArchetype, "BombBag");
    SetArchetype(EntityType::Bow, entities::bow::kBowArchetype, "Bow");
    SetArchetype(EntityType::Compass, entities::gear_items::kCompassArchetype, "Compass");
    SetArchetype(EntityType::Parachute, entities::gear_items::kParachuteArchetype, "Parachute");
    SetArchetype(EntityType::RopePile, entities::gear_items::kRopePileArchetype, "RopePile");
    SetArchetype(EntityType::Dice, entities::dice::kDiceArchetype, "Dice");
    SetArchetype(EntityType::RubyBig, entities::ruby_big::kRubyBigArchetype, "RubyBig");
    SetArchetype(EntityType::EmeraldBig, entities::emerald_big::kEmeraldBigArchetype, "EmeraldBig");
    SetArchetype(EntityType::SapphireBig, entities::sapphire_big::kSapphireBigArchetype, "SapphireBig");
    SetArchetype(EntityType::Shop, entities::shop::kShopArchetype, "Shop");
    SetArchetype(EntityType::Shopkeeper, entities::shopkeeper::kShopkeeperArchetype, "Shopkeeper");
    SetArchetype(EntityType::Damsel, entities::damsel::kDamselArchetype, "Damsel");
    SetArchetype(EntityType::SignGeneral, entities::sign::kSignGeneralArchetype, "SignGeneral");
    SetArchetype(EntityType::SignBomb, entities::sign::kSignBombArchetype, "SignBomb");
    SetArchetype(EntityType::SignWeapon, entities::sign::kSignWeaponArchetype, "SignWeapon");
    SetArchetype(EntityType::SignRare, entities::sign::kSignRareArchetype, "SignRare");
    SetArchetype(EntityType::SignClothing, entities::sign::kSignClothingArchetype, "SignClothing");
    SetArchetype(EntityType::SignCraps, entities::sign::kSignCrapsArchetype, "SignCraps");
    SetArchetype(EntityType::SignKissing, entities::sign::kSignKissingArchetype, "SignKissing");
    SetArchetype(EntityType::StoreLight, entities::store_light::kStoreLightArchetype, "StoreLight");
    SetArchetype(EntityType::Lantern, entities::lantern::kLanternArchetype, "Lantern");
    SetArchetype(EntityType::LanternRed, entities::lantern::kLanternRedArchetype, "LanternRed");
    SetArchetype(EntityType::GiantTikiHead, entities::giant_tiki_head::kGiantTikiHeadArchetype, "GiantTikiHead");
    SetArchetype(EntityType::KaliHead, entities::kali_head::kKaliHeadArchetype, "KaliHead");
    SetArchetype(EntityType::ArrowTrap, entities::arrow_trap::kArrowTrapArchetype, "ArrowTrap");
    SetArchetype(EntityType::Snake, entities::snake::kSnakeArchetype, "Snake");
    SetArchetype(EntityType::Caveman, entities::caveman::kCavemanArchetype, "Caveman");
    SetArchetype(EntityType::Spider, entities::spider::kSpiderArchetype, "Spider");
    SetArchetype(EntityType::SpiderHang, entities::spider_hang::kSpiderHangArchetype, "SpiderHang");
    SetArchetype(EntityType::RageSpider, entities::spider::kRageSpiderArchetype, "RageSpider");
    SetArchetype(EntityType::RageSpiderHang, entities::spider_hang::kRageSpiderHangArchetype, "RageSpiderHang");
    SetArchetype(EntityType::GiantSpider, entities::spider::kGiantSpiderArchetype, "GiantSpider");
    SetArchetype(EntityType::GiantSpiderHang, entities::spider_hang::kGiantSpiderHangArchetype, "GiantSpiderHang");
    SetArchetype(EntityType::Scarab, entities::scarab::kScarabArchetype, "Scarab");
    g_entity_archetypes_populated = true;
}

} // namespace splonks
