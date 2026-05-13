#include "ents/alien_boss.hpp"
#include "ents/alien_ship.hpp"
#include "ents/altar.hpp"
#include "ents/ankh.hpp"
#include "ents/arrow_trap.hpp"
#include "ents/ball_and_chain.hpp"
#include "ents/baseball_bat.hpp"
#include "ents/barrier_emitter.hpp"
#include "ents/basic_exit.hpp"
#include "ents/bat.hpp"
#include "ents/block.hpp"
#include "ents/bones.hpp"
#include "ents/bomb.hpp"
#include "ents/boulder.hpp"
#include "ents/bow.hpp"
#include "ents/box.hpp"
#include "ents/caveman.hpp"
#include "ents/ceiling_trap.hpp"
#include "ents/chest.hpp"
#include "ents/cobra.hpp"
#include "ents/craps_table.hpp"
#include "ents/crown.hpp"
#include "ents/crystal_skull.hpp"
#include "ents/damsel.hpp"
#include "ents/debug_moving_light.hpp"
#include "ents/dice.hpp"
#include "ents/door.hpp"
#include "ents/dvdlogo.hpp"
#include "ents/entrance.hpp"
#include "ents/emerald_big.hpp"
#include "ents/flappy_bee.hpp"
#include "ents/flesh_guy.hpp"
#include "ents/frog.hpp"
#include "ents/gear_items.hpp"
#include "ents/ghost_ball.hpp"
#include "ents/giant_tiki_head.hpp"
#include "ents/gold_idol.hpp"
#include "ents/hawkman.hpp"
#include "ents/jaws.hpp"
#include "ents/jetpack.hpp"
#include "ents/kali_head.hpp"
#include "ents/lamp.hpp"
#include "ents/lantern.hpp"
#include "ents/machete.hpp"
#include "ents/mantrap.hpp"
#include "ents/mattock.hpp"
#include "ents/meathead.hpp"
#include "ents/moai.hpp"
#include "ents/money.hpp"
#include "ents/monkey.hpp"
#include "ents/mouse_trailer.hpp"
#include "ents/moving_platform.hpp"
#include "ents/none_spec.hpp"
#include "ents/piranha.hpp"
#include "ents/pistol.hpp"
#include "ents/player.hpp"
#include "ents/pot.hpp"
#include "ents/rock.hpp"
#include "ents/rope.hpp"
#include "ents/ruby_big.hpp"
#include "ents/sac_altar.hpp"
#include "ents/sac_altar_topper.hpp"
#include "ents/sapphire_big.hpp"
#include "ents/scarab.hpp"
#include "ents/shop.hpp"
#include "ents/shopkeeper.hpp"
#include "ents/shotgun.hpp"
#include "ents/sign.hpp"
#include "ents/skeleton.hpp"
#include "ents/snake.hpp"
#include "ents/spider.hpp"
#include "ents/spider_hang.hpp"
#include "ents/stomp_pad.hpp"
#include "ents/store_light.hpp"
#include "ents/teleporter.hpp"
#include "ents/thwomp_trap.hpp"
#include "ents/tomb_lord.hpp"
#include "ents/trap_block.hpp"
#include "ents/ufo.hpp"
#include "ents/vampire.hpp"
#include "ents/web_cannon.hpp"
#include "ents/xoc_block.hpp"
#include "ents/yeti.hpp"
#include "ents/zombie.hpp"
#include "ent/spec.hpp"
#include "graphics.hpp"

#include <cassert>

namespace splonks {

namespace {

std::array<EntSpec, kEntTypeCount> g_ent_specs{};
bool g_ent_specs_populated = false;

void SetSpec(EntType type_, const EntSpec& spec, const char* debug_name) {
    EntSpec populated = spec;
    populated.debug_name = debug_name;
    g_ent_specs[EntTypeIndex(type_)] = populated;
}

} // namespace

void SyncEntSpecSizesFromAFrame(const Graphics& graphics) {
    assert(g_ent_specs_populated && "PopulateEntSpecsTable must run before sync");

    for (EntSpec& spec : g_ent_specs) {
        if (!spec.render_enabled) {
            continue;
        }

        const AFrameId anim_id = spec.aframe_animator.anim_id;
        if (anim_id == kInvalidAFrameId) {
            continue;
        }

        const AFrame* const aframe = graphics.aframe_db.FindFrame(anim_id, 0);
        if (aframe == nullptr || aframe->pbox.w <= 0 || aframe->pbox.h <= 0) {
            continue;
        }

        spec.size = Vec2::New(static_cast<float>(aframe->pbox.w),
                                   static_cast<float>(aframe->pbox.h));
    }
}

const EntSpec& GetEntSpec(EntType type_) {
    assert(g_ent_specs_populated && "PopulateEntSpecsTable must run before lookup");
    return g_ent_specs[EntTypeIndex(type_)];
}

const char* GetEntTypeName(EntType type_) {
    if (!g_ent_specs_populated) {
        return "Unknown";
    }

    const char* const debug_name = g_ent_specs[EntTypeIndex(type_)].debug_name;
    return debug_name != nullptr ? debug_name : "Unknown";
}

void PopulateEntSpecsTable() {
    if (g_ent_specs_populated) {
        return;
    }

    SetSpec(EntType::None, kNoneSpec, "None");
    SetSpec(EntType::Player, ents::player::kPlayerSpec, "Player");
    SetSpec(EntType::Block, ents::block::kBlockSpec, "Block");
    SetSpec(EntType::GhostBall, ents::ghost_ball::kGhostBallSpec, "GhostBall");
    SetSpec(EntType::BasicExit, ents::basic_exit::kBasicExitSpec, "BasicExit");
    SetSpec(EntType::Entrance, ents::entrance::kEntranceSpec, "Entrance");
    SetSpec(EntType::DvdLogo, ents::dvdlogo::kDvdLogoSpec, "DvdLogo");
    SetSpec(EntType::Bat, ents::bat::kBatSpec, "Bat");
    SetSpec(EntType::Rock, ents::rock::kRockSpec, "Rock");
    SetSpec(EntType::MouseTrailer, ents::mouse_trailer::kMouseTrailerSpec,
                 "MouseTrailer");
    SetSpec(EntType::JetPack, ents::jetpack::kJetPackSpec, "JetPack");
    SetSpec(EntType::Bomb, ents::bomb::kBombSpec, "Bomb");
    SetSpec(EntType::Gold, ents::money::kGoldSpec, "Gold");
    SetSpec(EntType::GoldStack, ents::money::kGoldStackSpec, "GoldStack");
    SetSpec(EntType::GoldChunk, ents::money::kGoldChunkSpec, "GoldChunk");
    SetSpec(EntType::GoldNugget, ents::money::kGoldNuggetSpec, "GoldNugget");
    SetSpec(EntType::GoldBar, ents::money::kGoldBarSpec, "GoldBar");
    SetSpec(EntType::GoldBars, ents::money::kGoldBarsSpec, "GoldBars");
    SetSpec(EntType::Rope, ents::rope::kRopeSpec, "Rope");
    SetSpec(EntType::Pot, ents::pot::kPotSpec, "Pot");
    SetSpec(EntType::Box, ents::box::kBoxSpec, "Box");
    SetSpec(EntType::StompPad, ents::stomp_pad::kStompPadSpec, "StompPad");
    SetSpec(EntType::BaseballBat, ents::baseball_bat::kBaseballBatSpec,
                 "BaseballBat");
    SetSpec(EntType::Altar, ents::altar::kAltarSpec, "Altar");
    SetSpec(EntType::SacAltar, ents::sac_altar::kSacAltarSpec, "SacAltar");
    SetSpec(EntType::GoldIdol, ents::gold_idol::kGoldIdolSpec, "GoldIdol");
    SetSpec(EntType::Chest, ents::chest::kChestSpec, "Chest");
    SetSpec(EntType::KeyChest, ents::chest::kKeyChestSpec, "KeyChest");
    SetSpec(EntType::ChestKey, ents::chest::kChestKeySpec, "ChestKey");
    SetSpec(EntType::UdjatEye, ents::chest::kUdjatEyeSpec, "UdjatEye");
    SetSpec(EntType::Mattock, ents::mattock::kMattockSpec, "Mattock");
    SetSpec(EntType::Cape, ents::gear_items::kCapeSpec, "Cape");
    SetSpec(EntType::Shotgun, ents::shotgun::kShotgunSpec, "Shotgun");
    SetSpec(EntType::Teleporter, ents::teleporter::kTeleporterSpec, "Teleporter");
    SetSpec(EntType::TeleporterBackpack, ents::teleporter::kTeleporterBackpackSpec,
                 "TeleporterBackpack");
    SetSpec(EntType::Gloves, ents::gear_items::kGlovesSpec, "Gloves");
    SetSpec(EntType::Spectacles, ents::gear_items::kSpectaclesSpec, "Spectacles");
    SetSpec(EntType::WebCannon, ents::web_cannon::kWebCannonSpec, "WebCannon");
    SetSpec(EntType::Pistol, ents::pistol::kPistolSpec, "Pistol");
    SetSpec(EntType::Mitt, ents::gear_items::kMittSpec, "Mitt");
    SetSpec(EntType::Paste, ents::gear_items::kPasteSpec, "Paste");
    SetSpec(EntType::SpringShoes, ents::gear_items::kSpringShoesSpec,
                 "SpringShoes");
    SetSpec(EntType::SpikeShoes, ents::gear_items::kSpikeShoesSpec, "SpikeShoes");
    SetSpec(EntType::Machete, ents::machete::kMacheteSpec, "Machete");
    SetSpec(EntType::BombBox, ents::gear_items::kBombBoxSpec, "BombBox");
    SetSpec(EntType::BombBag, ents::gear_items::kBombBagSpec, "BombBag");
    SetSpec(EntType::Bow, ents::bow::kBowSpec, "Bow");
    SetSpec(EntType::Compass, ents::gear_items::kCompassSpec, "Compass");
    SetSpec(EntType::Parachute, ents::gear_items::kParachuteSpec, "Parachute");
    SetSpec(EntType::RopePile, ents::gear_items::kRopePileSpec, "RopePile");
    SetSpec(EntType::Dice, ents::dice::kDiceSpec, "Dice");
    SetSpec(EntType::CrapsTable, ents::craps_table::kCrapsTableSpec,
                 "CrapsTable");
    SetSpec(EntType::RubyBig, ents::ruby_big::kRubyBigSpec, "RubyBig");
    SetSpec(EntType::EmeraldBig, ents::emerald_big::kEmeraldBigSpec, "EmeraldBig");
    SetSpec(EntType::SapphireBig, ents::sapphire_big::kSapphireBigSpec,
                 "SapphireBig");
    SetSpec(EntType::Shop, ents::shop::kShopSpec, "Shop");
    SetSpec(EntType::Shopkeeper, ents::shopkeeper::kShopkeeperSpec, "Shopkeeper");
    SetSpec(EntType::Damsel, ents::damsel::kDamselSpec, "Damsel");
    SetSpec(EntType::SignGeneral, ents::sign::kSignGeneralSpec, "SignGeneral");
    SetSpec(EntType::SignBomb, ents::sign::kSignBombSpec, "SignBomb");
    SetSpec(EntType::SignWeapon, ents::sign::kSignWeaponSpec, "SignWeapon");
    SetSpec(EntType::SignRare, ents::sign::kSignRareSpec, "SignRare");
    SetSpec(EntType::SignClothing, ents::sign::kSignClothingSpec, "SignClothing");
    SetSpec(EntType::SignCraps, ents::sign::kSignCrapsSpec, "SignCraps");
    SetSpec(EntType::SignKissing, ents::sign::kSignKissingSpec, "SignKissing");
    SetSpec(EntType::StoreLight, ents::store_light::kStoreLightSpec, "StoreLight");
    SetSpec(EntType::Lantern, ents::lantern::kLanternSpec, "Lantern");
    SetSpec(EntType::LanternRed, ents::lantern::kLanternRedSpec, "LanternRed");
    SetSpec(EntType::GiantTikiHead, ents::giant_tiki_head::kGiantTikiHeadSpec,
                 "GiantTikiHead");
    SetSpec(EntType::Boulder, ents::boulder::kBoulderSpec, "Boulder");
    SetSpec(EntType::MovingPlatform, ents::moving_platform::kMovingPlatformSpec,
                 "MovingPlatform");
    SetSpec(EntType::SacAltarTopper, ents::sac_altar_topper::kSacAltarTopperSpec,
                 "SacAltarTopper");
    SetSpec(EntType::KaliHead, ents::kali_head::kKaliHeadSpec, "KaliHead");
    SetSpec(EntType::BallAndChainBall, ents::ball_and_chain::kBallAndChainBallSpec,
                 "BallAndChainBall");
    SetSpec(EntType::ArrowTrap, ents::arrow_trap::kArrowTrapSpec, "ArrowTrap");
    SetSpec(EntType::Arrow, ents::arrow_trap::kArrowSpec, "Arrow");
    SetSpec(EntType::Skull, ents::skeleton::kSkullSpec, "Skull");
    SetSpec(EntType::Skeleton, ents::skeleton::kSkeletonSpec, "Skeleton");
    SetSpec(EntType::Snake, ents::snake::kSnakeSpec, "Snake");
    SetSpec(EntType::Cobra, ents::cobra::kCobraSpec, "Cobra");
    SetSpec(EntType::CobraSpit, ents::cobra::kCobraSpitSpec, "CobraSpit");
    SetSpec(EntType::Caveman, ents::caveman::kCavemanSpec, "Caveman");
    SetSpec(EntType::Spider, ents::spider::kSpiderSpec, "Spider");
    SetSpec(EntType::SpiderHang, ents::spider_hang::kSpiderHangSpec, "SpiderHang");
    SetSpec(EntType::RageSpider, ents::spider::kRageSpiderSpec, "RageSpider");
    SetSpec(EntType::RageSpiderHang, ents::spider_hang::kRageSpiderHangSpec,
                 "RageSpiderHang");
    SetSpec(EntType::GiantSpider, ents::spider::kGiantSpiderSpec, "GiantSpider");
    SetSpec(EntType::GiantSpiderHang, ents::spider_hang::kGiantSpiderHangSpec,
                 "GiantSpiderHang");
    SetSpec(EntType::Scarab, ents::scarab::kScarabSpec, "Scarab");
    SetSpec(EntType::Meathead, ents::meathead::kMeatheadSpec, "Meathead");
    SetSpec(EntType::WebBall, ents::web_cannon::kWebBallSpec, "WebBall");
    SetSpec(EntType::Cobweb, ents::web_cannon::kCobwebSpec, "Cobweb");
    SetSpec(EntType::Jaws, ents::jaws::kJawsSpec, "Jaws");
    SetSpec(EntType::CrystalSkull, ents::crystal_skull::kCrystalSkullSpec,
                 "CrystalSkull");
    SetSpec(EntType::Bones, ents::bones::kBonesSpec, "Bones");
    SetSpec(EntType::TrapBlock, ents::trap_block::kTrapBlockSpec, "SquisherBlock");
    SetSpec(EntType::CeilingTrap, ents::ceiling_trap::kCeilingTrapSpec,
                 "CeilingTrap");
    SetSpec(EntType::TombLord, ents::tomb_lord::kTombLordSpec, "TombLord");
    SetSpec(EntType::Door, ents::door::kDoorSpec, "Door");
    SetSpec(EntType::Ankh, ents::ankh::kAnkhSpec, "Ankh");
    SetSpec(EntType::Crown, ents::crown::kCrownSpec, "Crown");
    SetSpec(EntType::XocBlock, ents::xoc_block::kXocBlockSpec, "XocBlock");
    SetSpec(EntType::Moai, ents::moai::kMoaiSpec, "Moai");
    SetSpec(EntType::Moai2, ents::moai::kMoai2Spec, "Moai2");
    SetSpec(EntType::Moai3, ents::moai::kMoai3Spec, "Moai3");
    SetSpec(EntType::MoaiInside, ents::moai::kMoaiInsideSpec, "MoaiInside");
    SetSpec(EntType::Yeti, ents::yeti::kYetiSpec, "Yeti");
    SetSpec(EntType::AlienShip, ents::alien_ship::kAlienShipSpec, "AlienShip");
    SetSpec(EntType::AlienBoss, ents::alien_boss::kAlienBossSpec, "AlienBoss");
    SetSpec(EntType::BarrierEmitter, ents::barrier_emitter::kBarrierEmitterSpec,
                 "BarrierEmitter");
    SetSpec(EntType::Beam, ents::barrier_emitter::kBeamSpec, "Beam");
    SetSpec(EntType::ThwompTrap, ents::thwomp_trap::kThwompTrapSpec,
                 "ThwompTrap");
    SetSpec(EntType::Lamp, ents::lamp::kLampSpec, "Lamp");
    SetSpec(EntType::LampRed, ents::lamp::kLampRedSpec, "LampRed");
    SetSpec(EntType::Sign, ents::sign::kSignSpec, "Sign");
    SetSpec(EntType::Mantrap, ents::mantrap::kMantrapSpec, "Mantrap");
    SetSpec(EntType::Frog, ents::frog::kFrogSpec, "Frog");
    SetSpec(EntType::FireFrog, ents::frog::kFireFrogSpec, "FireFrog");
    SetSpec(EntType::Monkey, ents::monkey::kMonkeySpec, "Monkey");
    SetSpec(EntType::Piranha, ents::piranha::kPiranhaSpec, "Piranha");
    SetSpec(EntType::Zombie, ents::zombie::kZombieSpec, "Zombie");
    SetSpec(EntType::Vampire, ents::vampire::kVampireSpec, "Vampire");
    SetSpec(EntType::Ufo, ents::ufo::kUfoSpec, "Ufo");
    SetSpec(EntType::Hawkman, ents::hawkman::kHawkmanSpec, "Hawkman");
    SetSpec(EntType::FlappyBee, ents::flappy_bee::kFlappyBeeSpec, "FlappyBee");
    SetSpec(EntType::FleshGuy, ents::flesh_guy::kFleshGuySpec, "FleshGuy");
    SetSpec(
        EntType::DebugMovingLight,
        ents::debug_moving_light::kDebugMovingLightSpec,
        "DebugMovingLight"
    );
    g_ent_specs_populated = true;
}

} // namespace splonks
