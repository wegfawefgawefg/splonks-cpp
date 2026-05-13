#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace splonks {

using AFrameId = std::uint32_t;

constexpr AFrameId kInvalidAFrameId = 0;
constexpr AFrameId kAFrameFnvOffsetBasis32 = 2166136261U;
constexpr AFrameId kAFrameFnvPrime32 = 16777619U;

constexpr AFrameId HashAFrameIdConstexpr(std::string_view text) {
    AFrameId hash = kAFrameFnvOffsetBasis32;
    for (char character : text) {
        hash ^= static_cast<AFrameId>(static_cast<unsigned char>(character));
        hash *= kAFrameFnvPrime32;
    }
    return hash;
}

inline AFrameId HashAFrameId(const std::string& text) {
    return HashAFrameIdConstexpr(text);
}

namespace aframe_ids {

constexpr AFrameId NoSprite = HashAFrameIdConstexpr("no_sprite");
constexpr AFrameId Exit = HashAFrameIdConstexpr("exit");
constexpr AFrameId Entrance = HashAFrameIdConstexpr("entrance");
constexpr AFrameId PlayerStanding = HashAFrameIdConstexpr("player_standing");
constexpr AFrameId PlayerStandingHolding =
    HashAFrameIdConstexpr("player_standing_holding");
constexpr AFrameId PlayerWalking = HashAFrameIdConstexpr("player_walking");
constexpr AFrameId PlayerWalkHolding = HashAFrameIdConstexpr("player_walk_holding");
constexpr AFrameId PlayerClimbing = HashAFrameIdConstexpr("player_climbing");
constexpr AFrameId PlayerDead = HashAFrameIdConstexpr("player_dead");
constexpr AFrameId PlayerStunned = HashAFrameIdConstexpr("player_stunned");
constexpr AFrameId PlayerHanging = HashAFrameIdConstexpr("player_hanging");
constexpr AFrameId PlayerFalling = HashAFrameIdConstexpr("player_falling");
constexpr AFrameId PlayerDab = HashAFrameIdConstexpr("player_dab");
constexpr AFrameId PlayerBald = HashAFrameIdConstexpr("player_bald");
constexpr AFrameId Pot = HashAFrameIdConstexpr("pot");
constexpr AFrameId Box = HashAFrameIdConstexpr("box");
constexpr AFrameId BaseballBatSwing = HashAFrameIdConstexpr("baseball_bat_swing");
constexpr AFrameId GoldCoin = HashAFrameIdConstexpr("gold_coin");
constexpr AFrameId GoldStack = HashAFrameIdConstexpr("gold_stack");
constexpr AFrameId GoldChunk = HashAFrameIdConstexpr("gold_chunk");
constexpr AFrameId GoldNugget = HashAFrameIdConstexpr("gold_nugget");
constexpr AFrameId GoldBar = HashAFrameIdConstexpr("gold_bar");
constexpr AFrameId GoldBars = HashAFrameIdConstexpr("gold_bars");
constexpr AFrameId BigGoldStack = HashAFrameIdConstexpr("big_gold_stack");
constexpr AFrameId RopeBall = HashAFrameIdConstexpr("rope_ball");
constexpr AFrameId UnfoldingRope = HashAFrameIdConstexpr("unfolding_rope");
constexpr AFrameId Jetpack = HashAFrameIdConstexpr("jetpack");
constexpr AFrameId JetpackBack = HashAFrameIdConstexpr("jetpack_back");
constexpr AFrameId JetpackSide = HashAFrameIdConstexpr("jetpack_side");
constexpr AFrameId LiveGrenade = HashAFrameIdConstexpr("live_grenade");
constexpr AFrameId Grenade = HashAFrameIdConstexpr("grenade");
constexpr AFrameId StickyLiveGrenade = HashAFrameIdConstexpr("sticky_live_grenade");
constexpr AFrameId StickyGrenade = HashAFrameIdConstexpr("sticky_grenade");
constexpr AFrameId HangingBat = HashAFrameIdConstexpr("hanging_bat");
constexpr AFrameId FlyingBat = HashAFrameIdConstexpr("flying_bat");
constexpr AFrameId DeadBat = HashAFrameIdConstexpr("dead_bat");
constexpr AFrameId SpiderHang = HashAFrameIdConstexpr("spider_hang");
constexpr AFrameId RageSpiderHang = HashAFrameIdConstexpr("rage_spider_hang");
constexpr AFrameId GiantSpiderHang = HashAFrameIdConstexpr("giant_spider_hang");
constexpr AFrameId Spider = HashAFrameIdConstexpr("spider");
constexpr AFrameId RageSpider = HashAFrameIdConstexpr("rage_spider");
constexpr AFrameId GiantSpider = HashAFrameIdConstexpr("giant_spider");
constexpr AFrameId Rock = HashAFrameIdConstexpr("rock");
constexpr AFrameId Chest = HashAFrameIdConstexpr("chest");
constexpr AFrameId ChestOpen = HashAFrameIdConstexpr("chest_open");
constexpr AFrameId KeyChest = HashAFrameIdConstexpr("key_chest");
constexpr AFrameId KeyChestOpen = HashAFrameIdConstexpr("key_chest_open");
constexpr AFrameId ChestKey = HashAFrameIdConstexpr("chest_key");
constexpr AFrameId UdjatEye = HashAFrameIdConstexpr("udjat_eye");
constexpr AFrameId Ankh = HashAFrameIdConstexpr("ankh");
constexpr AFrameId Dice = HashAFrameIdConstexpr("dice");
constexpr AFrameId Damsel = HashAFrameIdConstexpr("damsel");
constexpr AFrameId DamselStunned = HashAFrameIdConstexpr("damsel_stunned");
constexpr AFrameId DamselDead = HashAFrameIdConstexpr("damsel_dead");
constexpr AFrameId DamselWalk = HashAFrameIdConstexpr("damsel_walk");
constexpr AFrameId DamselCry = HashAFrameIdConstexpr("damsel_cry");
constexpr AFrameId Shopkeeper = HashAFrameIdConstexpr("shopkeeper");
constexpr AFrameId Caveman = HashAFrameIdConstexpr("caveman");
constexpr AFrameId CavemanWalk = HashAFrameIdConstexpr("caveman_walk");
constexpr AFrameId CavemanStunned = HashAFrameIdConstexpr("caveman_stunned");
constexpr AFrameId CavemanDead = HashAFrameIdConstexpr("caveman_dead");
constexpr AFrameId Bones = HashAFrameIdConstexpr("bones");
constexpr AFrameId Skull = HashAFrameIdConstexpr("skull");
constexpr AFrameId SkeletonGettingUp = HashAFrameIdConstexpr("skeleton_getting_up");
constexpr AFrameId SkeletonWalk = HashAFrameIdConstexpr("skeleton_walk");
constexpr AFrameId Snake = HashAFrameIdConstexpr("snake");
constexpr AFrameId SnakeWalk = HashAFrameIdConstexpr("snake_walk");
constexpr AFrameId SnakeDead = HashAFrameIdConstexpr("snake_dead");
constexpr AFrameId Cobra = HashAFrameIdConstexpr("cobra");
constexpr AFrameId CobraWalk = HashAFrameIdConstexpr("cobra_walk");
constexpr AFrameId CobraDead = HashAFrameIdConstexpr("cobra_dead");
constexpr AFrameId CobraSpit = HashAFrameIdConstexpr("cobra_spit");
constexpr AFrameId Scarab = HashAFrameIdConstexpr("scarab");
constexpr AFrameId CaveBlock = HashAFrameIdConstexpr("cave_block");
constexpr AFrameId IceBlock = HashAFrameIdConstexpr("ice_block");
constexpr AFrameId JungleBlock = HashAFrameIdConstexpr("jungle_block");
constexpr AFrameId TempleBlock = HashAFrameIdConstexpr("temple_block");
constexpr AFrameId BossBlock = HashAFrameIdConstexpr("boss_block");
constexpr AFrameId Water = HashAFrameIdConstexpr("water");
constexpr AFrameId HeartUiIcon = HashAFrameIdConstexpr("heart_ui_icon");
constexpr AFrameId GrenadeUiIcon = HashAFrameIdConstexpr("grenade_ui_icon");
constexpr AFrameId StickyGrenadeUiIcon = HashAFrameIdConstexpr("sticky_grenade_ui_icon");
constexpr AFrameId RopeUiIcon = HashAFrameIdConstexpr("rope_ui_icon");
constexpr AFrameId GoldIcon = HashAFrameIdConstexpr("gold_icon");
constexpr AFrameId ToolSlot1 = HashAFrameIdConstexpr("tool_slot_1");
constexpr AFrameId ToolSlot2 = HashAFrameIdConstexpr("tool_slot_2");
constexpr AFrameId HandSlot = HashAFrameIdConstexpr("hand_slot");
constexpr AFrameId BackSlot = HashAFrameIdConstexpr("back_slot");
constexpr AFrameId GoldIdol = HashAFrameIdConstexpr("gold_idol");
constexpr AFrameId CrystalIdol = HashAFrameIdConstexpr("crystal_idol");
constexpr AFrameId Mattock = HashAFrameIdConstexpr("mattock");
constexpr AFrameId MattockSwing = HashAFrameIdConstexpr("mattock_swing");
constexpr AFrameId CapeClosed = HashAFrameIdConstexpr("cape_item");
constexpr AFrameId CapeOpen = HashAFrameIdConstexpr("cape_open");
constexpr AFrameId Cape = HashAFrameIdConstexpr("cape");
constexpr AFrameId CapeBack = HashAFrameIdConstexpr("cape_back");
constexpr AFrameId CapeBackOpen = HashAFrameIdConstexpr("cape_back_open");
constexpr AFrameId CapeSide = HashAFrameIdConstexpr("cape_side");
constexpr AFrameId CapeSideOpen = HashAFrameIdConstexpr("cape_side_open");
constexpr AFrameId CapeSideBack = HashAFrameIdConstexpr("cape_side_back");
constexpr AFrameId CapePickup = CapeClosed;
constexpr AFrameId Shotgun = HashAFrameIdConstexpr("shotgun");
constexpr AFrameId Teleporter = HashAFrameIdConstexpr("teleporter");
constexpr AFrameId TeleporterBackpack = HashAFrameIdConstexpr("telepack");
constexpr AFrameId TeleporterBackpackBack = HashAFrameIdConstexpr("telepack_back");
constexpr AFrameId TeleporterBackpackSide = HashAFrameIdConstexpr("telepack_side");
constexpr AFrameId Gloves = HashAFrameIdConstexpr("gloves");
constexpr AFrameId Spectacles = HashAFrameIdConstexpr("spectacles");
constexpr AFrameId WebCannon = HashAFrameIdConstexpr("webgun");
constexpr AFrameId Cobweb = HashAFrameIdConstexpr("cobweb");
constexpr AFrameId WebBall = HashAFrameIdConstexpr("webball");
constexpr AFrameId Pistol = HashAFrameIdConstexpr("pistol");
constexpr AFrameId GrenadeBoom = HashAFrameIdConstexpr("grenade_boom");
constexpr AFrameId BigExplosion = HashAFrameIdConstexpr("big_explosion");
constexpr AFrameId LittleExplosion = HashAFrameIdConstexpr("little_explosion");
constexpr AFrameId Spark = HashAFrameIdConstexpr("spark");
constexpr AFrameId Pow = HashAFrameIdConstexpr("pow");
constexpr AFrameId LittleSmoke = HashAFrameIdConstexpr("little_smoke");
constexpr AFrameId BigSmoke = HashAFrameIdConstexpr("big_smoke");
constexpr AFrameId BloodBall = HashAFrameIdConstexpr("blood_ball");
constexpr AFrameId Sparkle = HashAFrameIdConstexpr("sparkle");
constexpr AFrameId Glint = HashAFrameIdConstexpr("glint");
constexpr AFrameId Trail = HashAFrameIdConstexpr("trail");
constexpr AFrameId BaseballBatTrail = HashAFrameIdConstexpr("baseball_bat_trail");
constexpr AFrameId Kiss = HashAFrameIdConstexpr("kiss");
constexpr AFrameId LittleBrownShard = HashAFrameIdConstexpr("little_brown_shard");
constexpr AFrameId Mitt = HashAFrameIdConstexpr("mitt");
constexpr AFrameId MittNoGrav = HashAFrameIdConstexpr("mitt_no_grav");
constexpr AFrameId Paste = HashAFrameIdConstexpr("paste");
constexpr AFrameId SpiderMilk = HashAFrameIdConstexpr("spider_milk");
constexpr AFrameId SpringShoes = HashAFrameIdConstexpr("spring_shoes");
constexpr AFrameId SpikeShoes = HashAFrameIdConstexpr("spike_shoes");
constexpr AFrameId Knife = HashAFrameIdConstexpr("knife");
constexpr AFrameId KnifeSwing = HashAFrameIdConstexpr("knife_swing");
constexpr AFrameId Machete = HashAFrameIdConstexpr("machete");
constexpr AFrameId BombBox = HashAFrameIdConstexpr("bomb_box");
constexpr AFrameId BombBag = HashAFrameIdConstexpr("bomb_bag");
constexpr AFrameId BowLooseEmpty = HashAFrameIdConstexpr("bow_loose_empty");
constexpr AFrameId BowPullEmpty = HashAFrameIdConstexpr("bow_pull_empty");
constexpr AFrameId BowLooseLoaded = HashAFrameIdConstexpr("bow_loose_loaded");
constexpr AFrameId BowPullLoaded = HashAFrameIdConstexpr("bow_pull_loaded");
constexpr AFrameId Bow = BowLooseLoaded;
constexpr AFrameId Compass = HashAFrameIdConstexpr("compass");
constexpr AFrameId CompassArrow = HashAFrameIdConstexpr("compass_arrow");
constexpr AFrameId PackedParachute = HashAFrameIdConstexpr("packed_parachute");
constexpr AFrameId OpenParachute = HashAFrameIdConstexpr("open_parachute");
constexpr AFrameId Parachute = PackedParachute;
constexpr AFrameId RopePile = HashAFrameIdConstexpr("rope_pile");
constexpr AFrameId EmeraldBig = HashAFrameIdConstexpr("emerald_big");
constexpr AFrameId SapphireBig = HashAFrameIdConstexpr("sapphire_big");
constexpr AFrameId RubyBig = HashAFrameIdConstexpr("ruby_big");
constexpr AFrameId AltarLeft = HashAFrameIdConstexpr("altar_left");
constexpr AFrameId AltarRight = HashAFrameIdConstexpr("altar_right");
constexpr AFrameId SacAltarLeft = HashAFrameIdConstexpr("sac_altar_left");
constexpr AFrameId SacAltarRight = HashAFrameIdConstexpr("sac_altar_right");
constexpr AFrameId SacAltarTopper = HashAFrameIdConstexpr("sac_altar_topper");
constexpr AFrameId SacAltarSac = HashAFrameIdConstexpr("sac_altar_sac");
constexpr AFrameId Meathead = HashAFrameIdConstexpr("meathead");
constexpr AFrameId MeatheadRise = HashAFrameIdConstexpr("meathead_rise");
constexpr AFrameId Mantrap = HashAFrameIdConstexpr("mantrap");
constexpr AFrameId MantrapWalk = HashAFrameIdConstexpr("mantrap_walk");
constexpr AFrameId MantrapEat = HashAFrameIdConstexpr("mantrap_eat");
constexpr AFrameId Piranha = HashAFrameIdConstexpr("pirana");
constexpr AFrameId PiranhaSwim = HashAFrameIdConstexpr("pirana_swim");
constexpr AFrameId PiranhaBite = HashAFrameIdConstexpr("pirana_bite");
constexpr AFrameId PiranhaDead = Piranha;
constexpr AFrameId MonkeyStand = HashAFrameIdConstexpr("monkey_stand");
constexpr AFrameId MonkeyDead = HashAFrameIdConstexpr("monkey_dead");
constexpr AFrameId MonkeyHang = HashAFrameIdConstexpr("monkey_hang");
constexpr AFrameId MonkeyDown = HashAFrameIdConstexpr("monkey_down");
constexpr AFrameId SignGeneral = HashAFrameIdConstexpr("sign_general");
constexpr AFrameId SignBomb = HashAFrameIdConstexpr("sign_bomb");
constexpr AFrameId SignWeapon = HashAFrameIdConstexpr("sign_weapon");
constexpr AFrameId SignRare = HashAFrameIdConstexpr("sign_rare");
constexpr AFrameId SignClothing = HashAFrameIdConstexpr("sign_clothing");
constexpr AFrameId SignCraps = HashAFrameIdConstexpr("sign_craps");
constexpr AFrameId SignKissing = HashAFrameIdConstexpr("sign_kissing");
constexpr AFrameId DiceSign = HashAFrameIdConstexpr("dice_sign");
constexpr AFrameId StoreLight = HashAFrameIdConstexpr("store_light");
constexpr AFrameId StoreLightBroken = HashAFrameIdConstexpr("store_light_broken");
constexpr AFrameId Lantern = HashAFrameIdConstexpr("lantern");
constexpr AFrameId LanternRed = HashAFrameIdConstexpr("lantern_red");
constexpr AFrameId ArrowTrap = HashAFrameIdConstexpr("arrow_trap");
constexpr AFrameId Arrow = HashAFrameIdConstexpr("arrow");
constexpr AFrameId IdleTrapDoor = HashAFrameIdConstexpr("idle_trap_door");
constexpr AFrameId ThwompTrap = HashAFrameIdConstexpr("thwomp_trap");
constexpr AFrameId SquisherBlock = HashAFrameIdConstexpr("squisher_block");
constexpr AFrameId BeamEmitter = HashAFrameIdConstexpr("beam_emitter");
constexpr AFrameId Beam = HashAFrameIdConstexpr("beam");
constexpr AFrameId GiantTikiHead = HashAFrameIdConstexpr("giant_tiki_head");
constexpr AFrameId KaliHead = HashAFrameIdConstexpr("kali_head");
constexpr AFrameId KaliBody = HashAFrameIdConstexpr("kali_body");
constexpr AFrameId BeeFly = HashAFrameIdConstexpr("bee_fly");
constexpr AFrameId BeeWalk = HashAFrameIdConstexpr("bee_walk");
constexpr AFrameId FleshGuy = HashAFrameIdConstexpr("fleshguy");
constexpr AFrameId FleshGuyWalk = HashAFrameIdConstexpr("fleshguy_walk");
constexpr AFrameId MeatTileTopper = HashAFrameIdConstexpr("meat_tile_topper");

} // namespace aframe_ids

} // namespace splonks
