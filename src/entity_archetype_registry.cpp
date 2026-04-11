#include "entity_archetype.hpp"
#include "entities/altar.hpp"
#include "entities/arrow_trap.hpp"
#include "entities/baseball_bat.hpp"
#include "entities/bat.hpp"
#include "entities/block.hpp"
#include "entities/bomb.hpp"
#include "entities/bow.hpp"
#include "entities/breakaway_container.hpp"
#include "entities/caveman.hpp"
#include "entities/chest.hpp"
#include "entities/damsel.hpp"
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
#include "entities/mod.hpp"
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
#include "entities/shopkeeper.hpp"
#include "entities/sign.hpp"
#include "entities/snake.hpp"
#include "entities/spider_hang.hpp"
#include "entities/stomp_pad.hpp"
#include "entities/teleporter.hpp"
#include "entities/web_cannon.hpp"
#include "entities/pistol.hpp"

#include <cassert>

namespace splonks {

namespace {

std::array<EntityArchetype, kEntityTypeCount> g_entity_archetypes{};
bool g_entity_archetypes_populated = false;

} // namespace

const EntityArchetype& GetEntityArchetype(EntityType type_) {
    assert(g_entity_archetypes_populated && "PopulateEntityArchetypesTable must run before lookup");
    return g_entity_archetypes[EntityTypeIndex(type_)];
}

void PopulateEntityArchetypesTable() {
    g_entity_archetypes[EntityTypeIndex(EntityType::None)] = kNoneArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Player)] = entities::player::kPlayerArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Block)] = entities::block::kBlockArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::GhostBall)] =
        entities::ghost_ball::kGhostBallArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Bat)] = entities::bat::kBatArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Rock)] = entities::rock::kRockArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::MouseTrailer)] =
        entities::mouse_trailer::kMouseTrailerArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::JetPack)] =
        entities::jetpack::kJetPackArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Bomb)] = entities::bomb::kBombArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Gold)] = entities::money::kGoldArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::GoldStack)] =
        entities::money::kGoldStackArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Rope)] = entities::rope::kRopeArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Pot)] =
        entities::breakaway_container::kPotArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Box)] =
        entities::breakaway_container::kBoxArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::StompPad)] =
        entities::stomp_pad::kStompPadArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::BaseballBat)] =
        entities::baseball_bat::kBaseballBatArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::AltarLeft)] =
        entities::altar::kAltarLeftArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::AltarRight)] =
        entities::altar::kAltarRightArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::SacAltarLeft)] =
        entities::sac_altar::kSacAltarLeftArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::SacAltarRight)] =
        entities::sac_altar::kSacAltarRightArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::GoldIdol)] =
        entities::gold_idol::kGoldIdolArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Chest)] = entities::chest::kChestArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Mattock)] =
        entities::mattock::kMattockArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Cape)] = entities::gear_items::kCapeArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Shotgun)] =
        entities::shotgun::kShotgunArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Teleporter)] =
        entities::teleporter::kTeleporterArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Gloves)] =
        entities::gear_items::kGlovesArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Spectacles)] =
        entities::gear_items::kSpectaclesArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::WebCannon)] =
        entities::web_cannon::kWebCannonArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Pistol)] = entities::pistol::kPistolArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Mitt)] = entities::gear_items::kMittArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Paste)] =
        entities::gear_items::kPasteArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::SpringShoes)] =
        entities::gear_items::kSpringShoesArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::SpikeShoes)] =
        entities::gear_items::kSpikeShoesArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Machete)] =
        entities::machete::kMacheteArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::BombBox)] =
        entities::gear_items::kBombBoxArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::BombBag)] =
        entities::gear_items::kBombBagArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Bow)] = entities::bow::kBowArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Compass)] =
        entities::gear_items::kCompassArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Parachute)] =
        entities::gear_items::kParachuteArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::RopePile)] =
        entities::gear_items::kRopePileArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Dice)] = entities::dice::kDiceArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::RubyBig)] =
        entities::ruby_big::kRubyBigArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::EmeraldBig)] =
        entities::emerald_big::kEmeraldBigArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::SapphireBig)] =
        entities::sapphire_big::kSapphireBigArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Shopkeeper)] =
        entities::shopkeeper::kShopkeeperArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Damsel)] = entities::damsel::kDamselArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::SignGeneral)] =
        entities::sign::kSignGeneralArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::SignBomb)] =
        entities::sign::kSignBombArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::SignWeapon)] =
        entities::sign::kSignWeaponArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::SignRare)] =
        entities::sign::kSignRareArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::SignClothing)] =
        entities::sign::kSignClothingArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::SignCraps)] =
        entities::sign::kSignCrapsArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::SignKissing)] =
        entities::sign::kSignKissingArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Lantern)] =
        entities::lantern::kLanternArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::LanternRed)] =
        entities::lantern::kLanternRedArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::GiantTikiHead)] =
        entities::giant_tiki_head::kGiantTikiHeadArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::KaliHead)] =
        entities::kali_head::kKaliHeadArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::ArrowTrap)] =
        entities::arrow_trap::kArrowTrapArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Snake)] = entities::snake::kSnakeArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Caveman)] =
        entities::caveman::kCavemanArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::SpiderHang)] =
        entities::spider_hang::kSpiderHangArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::GiantSpiderHang)] =
        entities::spider_hang::kGiantSpiderHangArchetype;
    g_entity_archetypes[EntityTypeIndex(EntityType::Scarab)] = entities::scarab::kScarabArchetype;
    g_entity_archetypes_populated = true;
}

} // namespace splonks
