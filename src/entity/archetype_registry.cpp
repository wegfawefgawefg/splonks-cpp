#include "entities/alien_boss.hpp"
#include "entities/alien_ship.hpp"
#include "entities/altar.hpp"
#include "entities/ankh.hpp"
#include "entities/arrow_trap.hpp"
#include "entities/ball_and_chain.hpp"
#include "entities/baseball_bat.hpp"
#include "entities/barrier_emitter.hpp"
#include "entities/basic_exit.hpp"
#include "entities/bat.hpp"
#include "entities/block.hpp"
#include "entities/bones.hpp"
#include "entities/bomb.hpp"
#include "entities/boulder.hpp"
#include "entities/bow.hpp"
#include "entities/box.hpp"
#include "entities/caveman.hpp"
#include "entities/ceiling_trap.hpp"
#include "entities/chest.hpp"
#include "entities/cobra.hpp"
#include "entities/craps_table.hpp"
#include "entities/crate.hpp"
#include "entities/crown.hpp"
#include "entities/crystal_skull.hpp"
#include "entities/damsel.hpp"
#include "entities/dice.hpp"
#include "entities/door.hpp"
#include "entities/dvdlogo.hpp"
#include "entities/emerald_big.hpp"
#include "entities/flappy_bee.hpp"
#include "entities/flesh_guy.hpp"
#include "entities/frog.hpp"
#include "entities/gear_items.hpp"
#include "entities/ghost_ball.hpp"
#include "entities/giant_tiki_head.hpp"
#include "entities/gold_idol.hpp"
#include "entities/hawkman.hpp"
#include "entities/jar.hpp"
#include "entities/jaws.hpp"
#include "entities/jetpack.hpp"
#include "entities/kali_head.hpp"
#include "entities/lamp.hpp"
#include "entities/lantern.hpp"
#include "entities/machete.hpp"
#include "entities/mantrap.hpp"
#include "entities/mattock.hpp"
#include "entities/meathead.hpp"
#include "entities/moai.hpp"
#include "entities/money.hpp"
#include "entities/monkey.hpp"
#include "entities/mouse_trailer.hpp"
#include "entities/moving_platform.hpp"
#include "entities/none_archetype.hpp"
#include "entities/piranha.hpp"
#include "entities/pistol.hpp"
#include "entities/player.hpp"
#include "entities/pot.hpp"
#include "entities/rock.hpp"
#include "entities/rope.hpp"
#include "entities/ruby_big.hpp"
#include "entities/sac_altar.hpp"
#include "entities/sac_altar_topper.hpp"
#include "entities/sapphire_big.hpp"
#include "entities/scarab.hpp"
#include "entities/shop.hpp"
#include "entities/shopkeeper.hpp"
#include "entities/shotgun.hpp"
#include "entities/sign.hpp"
#include "entities/skeleton.hpp"
#include "entities/snake.hpp"
#include "entities/spider.hpp"
#include "entities/spider_hang.hpp"
#include "entities/stomp_pad.hpp"
#include "entities/store_light.hpp"
#include "entities/teleporter.hpp"
#include "entities/thwomp_trap.hpp"
#include "entities/tomb_lord.hpp"
#include "entities/trap_block.hpp"
#include "entities/ufo.hpp"
#include "entities/vampire.hpp"
#include "entities/web_cannon.hpp"
#include "entities/xoc_block.hpp"
#include "entities/yeti.hpp"
#include "entities/zombie.hpp"
#include "entity/archetype.hpp"
#include "graphics.hpp"

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

void SyncEntityArchetypeSizesFromFrameData(const Graphics& graphics) {
    assert(g_entity_archetypes_populated && "PopulateEntityArchetypesTable must run before sync");

    for (EntityArchetype& archetype : g_entity_archetypes) {
        if (!archetype.render_enabled) {
            continue;
        }

        const FrameDataId animation_id = archetype.frame_data_animator.animation_id;
        if (animation_id == kInvalidFrameDataId) {
            continue;
        }

        const FrameData* const frame_data = graphics.frame_data_db.FindFrame(animation_id, 0);
        if (frame_data == nullptr || frame_data->pbox.w <= 0 || frame_data->pbox.h <= 0) {
            continue;
        }

        archetype.size = Vec2::New(static_cast<float>(frame_data->pbox.w),
                                   static_cast<float>(frame_data->pbox.h));
    }
}

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
    if (g_entity_archetypes_populated) {
        return;
    }

    SetArchetype(EntityType::None, kNoneArchetype, "None");
    SetArchetype(EntityType::Player, entities::player::kPlayerArchetype, "Player");
    SetArchetype(EntityType::Block, entities::block::kBlockArchetype, "Block");
    SetArchetype(EntityType::GhostBall, entities::ghost_ball::kGhostBallArchetype, "GhostBall");
    SetArchetype(EntityType::BasicExit, entities::basic_exit::kBasicExitArchetype, "BasicExit");
    SetArchetype(EntityType::DvdLogo, entities::dvdlogo::kDvdLogoArchetype, "DvdLogo");
    SetArchetype(EntityType::Bat, entities::bat::kBatArchetype, "Bat");
    SetArchetype(EntityType::Rock, entities::rock::kRockArchetype, "Rock");
    SetArchetype(EntityType::MouseTrailer, entities::mouse_trailer::kMouseTrailerArchetype,
                 "MouseTrailer");
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
    SetArchetype(EntityType::BaseballBat, entities::baseball_bat::kBaseballBatArchetype,
                 "BaseballBat");
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
    SetArchetype(EntityType::TeleporterBackpack, entities::teleporter::kTeleporterBackpackArchetype,
                 "TeleporterBackpack");
    SetArchetype(EntityType::Gloves, entities::gear_items::kGlovesArchetype, "Gloves");
    SetArchetype(EntityType::Spectacles, entities::gear_items::kSpectaclesArchetype, "Spectacles");
    SetArchetype(EntityType::WebCannon, entities::web_cannon::kWebCannonArchetype, "WebCannon");
    SetArchetype(EntityType::Pistol, entities::pistol::kPistolArchetype, "Pistol");
    SetArchetype(EntityType::Mitt, entities::gear_items::kMittArchetype, "Mitt");
    SetArchetype(EntityType::Paste, entities::gear_items::kPasteArchetype, "Paste");
    SetArchetype(EntityType::SpringShoes, entities::gear_items::kSpringShoesArchetype,
                 "SpringShoes");
    SetArchetype(EntityType::SpikeShoes, entities::gear_items::kSpikeShoesArchetype, "SpikeShoes");
    SetArchetype(EntityType::Machete, entities::machete::kMacheteArchetype, "Machete");
    SetArchetype(EntityType::BombBox, entities::gear_items::kBombBoxArchetype, "BombBox");
    SetArchetype(EntityType::BombBag, entities::gear_items::kBombBagArchetype, "BombBag");
    SetArchetype(EntityType::Bow, entities::bow::kBowArchetype, "Bow");
    SetArchetype(EntityType::Compass, entities::gear_items::kCompassArchetype, "Compass");
    SetArchetype(EntityType::Parachute, entities::gear_items::kParachuteArchetype, "Parachute");
    SetArchetype(EntityType::RopePile, entities::gear_items::kRopePileArchetype, "RopePile");
    SetArchetype(EntityType::Dice, entities::dice::kDiceArchetype, "Dice");
    SetArchetype(EntityType::CrapsTable, entities::craps_table::kCrapsTableArchetype,
                 "CrapsTable");
    SetArchetype(EntityType::RubyBig, entities::ruby_big::kRubyBigArchetype, "RubyBig");
    SetArchetype(EntityType::EmeraldBig, entities::emerald_big::kEmeraldBigArchetype, "EmeraldBig");
    SetArchetype(EntityType::SapphireBig, entities::sapphire_big::kSapphireBigArchetype,
                 "SapphireBig");
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
    SetArchetype(EntityType::GiantTikiHead, entities::giant_tiki_head::kGiantTikiHeadArchetype,
                 "GiantTikiHead");
    SetArchetype(EntityType::Boulder, entities::boulder::kBoulderArchetype, "Boulder");
    SetArchetype(EntityType::MovingPlatform, entities::moving_platform::kMovingPlatformArchetype,
                 "MovingPlatform");
    SetArchetype(EntityType::SacAltarTopper, entities::sac_altar_topper::kSacAltarTopperArchetype,
                 "SacAltarTopper");
    SetArchetype(EntityType::KaliHead, entities::kali_head::kKaliHeadArchetype, "KaliHead");
    SetArchetype(EntityType::BallAndChainBall, entities::ball_and_chain::kBallAndChainBallArchetype,
                 "BallAndChainBall");
    SetArchetype(EntityType::ArrowTrap, entities::arrow_trap::kArrowTrapArchetype, "ArrowTrap");
    SetArchetype(EntityType::Arrow, entities::arrow_trap::kArrowArchetype, "Arrow");
    SetArchetype(EntityType::Skull, entities::skeleton::kSkullArchetype, "Skull");
    SetArchetype(EntityType::Skeleton, entities::skeleton::kSkeletonArchetype, "Skeleton");
    SetArchetype(EntityType::Snake, entities::snake::kSnakeArchetype, "Snake");
    SetArchetype(EntityType::Cobra, entities::cobra::kCobraArchetype, "Cobra");
    SetArchetype(EntityType::CobraSpit, entities::cobra::kCobraSpitArchetype, "CobraSpit");
    SetArchetype(EntityType::Caveman, entities::caveman::kCavemanArchetype, "Caveman");
    SetArchetype(EntityType::Spider, entities::spider::kSpiderArchetype, "Spider");
    SetArchetype(EntityType::SpiderHang, entities::spider_hang::kSpiderHangArchetype, "SpiderHang");
    SetArchetype(EntityType::RageSpider, entities::spider::kRageSpiderArchetype, "RageSpider");
    SetArchetype(EntityType::RageSpiderHang, entities::spider_hang::kRageSpiderHangArchetype,
                 "RageSpiderHang");
    SetArchetype(EntityType::GiantSpider, entities::spider::kGiantSpiderArchetype, "GiantSpider");
    SetArchetype(EntityType::GiantSpiderHang, entities::spider_hang::kGiantSpiderHangArchetype,
                 "GiantSpiderHang");
    SetArchetype(EntityType::Scarab, entities::scarab::kScarabArchetype, "Scarab");
    SetArchetype(EntityType::Meathead, entities::meathead::kMeatheadArchetype, "Meathead");
    SetArchetype(EntityType::WebBall, entities::web_cannon::kWebBallArchetype, "WebBall");
    SetArchetype(EntityType::Cobweb, entities::web_cannon::kCobwebArchetype, "Cobweb");
    SetArchetype(EntityType::Jaws, entities::jaws::kJawsArchetype, "Jaws");
    SetArchetype(EntityType::CrystalSkull, entities::crystal_skull::kCrystalSkullArchetype,
                 "CrystalSkull");
    SetArchetype(EntityType::Bones, entities::bones::kBonesArchetype, "Bones");
    SetArchetype(EntityType::Jar, entities::jar::kJarArchetype, "Jar");
    SetArchetype(EntityType::TrapBlock, entities::trap_block::kTrapBlockArchetype, "TrapBlock");
    SetArchetype(EntityType::CeilingTrap, entities::ceiling_trap::kCeilingTrapArchetype,
                 "CeilingTrap");
    SetArchetype(EntityType::TombLord, entities::tomb_lord::kTombLordArchetype, "TombLord");
    SetArchetype(EntityType::Door, entities::door::kDoorArchetype, "Door");
    SetArchetype(EntityType::Ankh, entities::ankh::kAnkhArchetype, "Ankh");
    SetArchetype(EntityType::Crown, entities::crown::kCrownArchetype, "Crown");
    SetArchetype(EntityType::XocBlock, entities::xoc_block::kXocBlockArchetype, "XocBlock");
    SetArchetype(EntityType::Moai, entities::moai::kMoaiArchetype, "Moai");
    SetArchetype(EntityType::Moai2, entities::moai::kMoai2Archetype, "Moai2");
    SetArchetype(EntityType::Moai3, entities::moai::kMoai3Archetype, "Moai3");
    SetArchetype(EntityType::MoaiInside, entities::moai::kMoaiInsideArchetype, "MoaiInside");
    SetArchetype(EntityType::Yeti, entities::yeti::kYetiArchetype, "Yeti");
    SetArchetype(EntityType::AlienShip, entities::alien_ship::kAlienShipArchetype, "AlienShip");
    SetArchetype(EntityType::AlienBoss, entities::alien_boss::kAlienBossArchetype, "AlienBoss");
    SetArchetype(EntityType::BarrierEmitter, entities::barrier_emitter::kBarrierEmitterArchetype,
                 "BarrierEmitter");
    SetArchetype(EntityType::ThwompTrap, entities::thwomp_trap::kThwompTrapArchetype,
                 "ThwompTrap");
    SetArchetype(EntityType::Lamp, entities::lamp::kLampArchetype, "Lamp");
    SetArchetype(EntityType::LampRed, entities::lamp::kLampRedArchetype, "LampRed");
    SetArchetype(EntityType::Crate, entities::crate::kCrateArchetype, "Crate");
    SetArchetype(EntityType::Sign, entities::sign::kSignArchetype, "Sign");
    SetArchetype(EntityType::Mantrap, entities::mantrap::kMantrapArchetype, "Mantrap");
    SetArchetype(EntityType::Frog, entities::frog::kFrogArchetype, "Frog");
    SetArchetype(EntityType::FireFrog, entities::frog::kFireFrogArchetype, "FireFrog");
    SetArchetype(EntityType::Monkey, entities::monkey::kMonkeyArchetype, "Monkey");
    SetArchetype(EntityType::Piranha, entities::piranha::kPiranhaArchetype, "Piranha");
    SetArchetype(EntityType::Zombie, entities::zombie::kZombieArchetype, "Zombie");
    SetArchetype(EntityType::Vampire, entities::vampire::kVampireArchetype, "Vampire");
    SetArchetype(EntityType::Ufo, entities::ufo::kUfoArchetype, "Ufo");
    SetArchetype(EntityType::Hawkman, entities::hawkman::kHawkmanArchetype, "Hawkman");
    SetArchetype(EntityType::FlappyBee, entities::flappy_bee::kFlappyBeeArchetype, "FlappyBee");
    SetArchetype(EntityType::FleshGuy, entities::flesh_guy::kFleshGuyArchetype, "FleshGuy");
    g_entity_archetypes_populated = true;
}

} // namespace splonks
