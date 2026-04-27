#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace splonks {

using FrameDataId = std::uint32_t;

constexpr FrameDataId kInvalidFrameDataId = 0;
constexpr FrameDataId kFrameDataFnvOffsetBasis32 = 2166136261U;
constexpr FrameDataId kFrameDataFnvPrime32 = 16777619U;

constexpr FrameDataId HashFrameDataIdConstexpr(std::string_view text) {
    FrameDataId hash = kFrameDataFnvOffsetBasis32;
    for (char character : text) {
        hash ^= static_cast<FrameDataId>(static_cast<unsigned char>(character));
        hash *= kFrameDataFnvPrime32;
    }
    return hash;
}

inline FrameDataId HashFrameDataId(const std::string& text) {
    return HashFrameDataIdConstexpr(text);
}

namespace frame_data_ids {

constexpr FrameDataId NoSprite = HashFrameDataIdConstexpr("no_sprite");
constexpr FrameDataId Exit = HashFrameDataIdConstexpr("exit");
constexpr FrameDataId PlayerStanding = HashFrameDataIdConstexpr("player_standing");
constexpr FrameDataId PlayerStandingHolding =
    HashFrameDataIdConstexpr("player_standing_holding");
constexpr FrameDataId PlayerWalking = HashFrameDataIdConstexpr("player_walking");
constexpr FrameDataId PlayerWalkHolding = HashFrameDataIdConstexpr("player_walk_holding");
constexpr FrameDataId PlayerClimbing = HashFrameDataIdConstexpr("player_climbing");
constexpr FrameDataId PlayerDead = HashFrameDataIdConstexpr("player_dead");
constexpr FrameDataId PlayerStunned = HashFrameDataIdConstexpr("player_stunned");
constexpr FrameDataId PlayerHanging = HashFrameDataIdConstexpr("player_hanging");
constexpr FrameDataId PlayerFalling = HashFrameDataIdConstexpr("player_falling");
constexpr FrameDataId Pot = HashFrameDataIdConstexpr("pot");
constexpr FrameDataId Box = HashFrameDataIdConstexpr("box");
constexpr FrameDataId BaseballBatSwing = HashFrameDataIdConstexpr("baseball_bat_swing");
constexpr FrameDataId GoldCoin = HashFrameDataIdConstexpr("gold_coin");
constexpr FrameDataId GoldStack = HashFrameDataIdConstexpr("gold_stack");
constexpr FrameDataId GoldChunk = HashFrameDataIdConstexpr("gold_chunk");
constexpr FrameDataId GoldNugget = HashFrameDataIdConstexpr("gold_nugget");
constexpr FrameDataId GoldBar = HashFrameDataIdConstexpr("gold_bar");
constexpr FrameDataId GoldBars = HashFrameDataIdConstexpr("gold_bars");
constexpr FrameDataId BigGoldStack = HashFrameDataIdConstexpr("big_gold_stack");
constexpr FrameDataId RopeBall = HashFrameDataIdConstexpr("rope_ball");
constexpr FrameDataId UnfoldingRope = HashFrameDataIdConstexpr("unfolding_rope");
constexpr FrameDataId Jetpack = HashFrameDataIdConstexpr("jetpack");
constexpr FrameDataId JetpackBack = HashFrameDataIdConstexpr("jetpack_back");
constexpr FrameDataId JetpackSide = HashFrameDataIdConstexpr("jetpack_side");
constexpr FrameDataId LiveGrenade = HashFrameDataIdConstexpr("live_grenade");
constexpr FrameDataId Grenade = HashFrameDataIdConstexpr("grenade");
constexpr FrameDataId StickyLiveGrenade = HashFrameDataIdConstexpr("sticky_live_grenade");
constexpr FrameDataId StickyGrenade = HashFrameDataIdConstexpr("sticky_grenade");
constexpr FrameDataId HangingBat = HashFrameDataIdConstexpr("hanging_bat");
constexpr FrameDataId FlyingBat = HashFrameDataIdConstexpr("flying_bat");
constexpr FrameDataId DeadBat = HashFrameDataIdConstexpr("dead_bat");
constexpr FrameDataId SpiderHang = HashFrameDataIdConstexpr("spider_hang");
constexpr FrameDataId RageSpiderHang = HashFrameDataIdConstexpr("rage_spider_hang");
constexpr FrameDataId GiantSpiderHang = HashFrameDataIdConstexpr("giant_spider_hang");
constexpr FrameDataId Spider = HashFrameDataIdConstexpr("spider");
constexpr FrameDataId RageSpider = HashFrameDataIdConstexpr("rage_spider");
constexpr FrameDataId GiantSpider = HashFrameDataIdConstexpr("giant_spider");
constexpr FrameDataId Rock = HashFrameDataIdConstexpr("rock");
constexpr FrameDataId Chest = HashFrameDataIdConstexpr("chest");
constexpr FrameDataId ChestOpen = HashFrameDataIdConstexpr("chest_open");
constexpr FrameDataId KeyChest = HashFrameDataIdConstexpr("key_chest");
constexpr FrameDataId KeyChestOpen = HashFrameDataIdConstexpr("key_chest_open");
constexpr FrameDataId ChestKey = HashFrameDataIdConstexpr("chest_key");
constexpr FrameDataId UdjatEye = HashFrameDataIdConstexpr("udjat_eye");
constexpr FrameDataId Dice = HashFrameDataIdConstexpr("dice");
constexpr FrameDataId Damsel = HashFrameDataIdConstexpr("damsel");
constexpr FrameDataId DamselStunned = HashFrameDataIdConstexpr("damsel_stunned");
constexpr FrameDataId DamselDead = HashFrameDataIdConstexpr("damsel_dead");
constexpr FrameDataId DamselWalk = HashFrameDataIdConstexpr("damsel_walk");
constexpr FrameDataId DamselCry = HashFrameDataIdConstexpr("damsel_cry");
constexpr FrameDataId Shopkeeper = HashFrameDataIdConstexpr("shopkeeper");
constexpr FrameDataId Caveman = HashFrameDataIdConstexpr("caveman");
constexpr FrameDataId CavemanWalk = HashFrameDataIdConstexpr("caveman_walk");
constexpr FrameDataId CavemanStunned = HashFrameDataIdConstexpr("caveman_stunned");
constexpr FrameDataId CavemanDead = HashFrameDataIdConstexpr("caveman_dead");
constexpr FrameDataId Bones = HashFrameDataIdConstexpr("bones");
constexpr FrameDataId Skull = HashFrameDataIdConstexpr("skull");
constexpr FrameDataId SkeletonGettingUp = HashFrameDataIdConstexpr("skeleton_getting_up");
constexpr FrameDataId SkeletonWalk = HashFrameDataIdConstexpr("skeleton_walk");
constexpr FrameDataId Snake = HashFrameDataIdConstexpr("snake");
constexpr FrameDataId SnakeWalk = HashFrameDataIdConstexpr("snake_walk");
constexpr FrameDataId SnakeDead = HashFrameDataIdConstexpr("snake_dead");
constexpr FrameDataId Cobra = HashFrameDataIdConstexpr("cobra");
constexpr FrameDataId CobraWalk = HashFrameDataIdConstexpr("cobra_walk");
constexpr FrameDataId CobraDead = HashFrameDataIdConstexpr("cobra_dead");
constexpr FrameDataId CobraSpit = HashFrameDataIdConstexpr("cobra_spit");
constexpr FrameDataId Scarab = HashFrameDataIdConstexpr("scarab");
constexpr FrameDataId CaveBlock = HashFrameDataIdConstexpr("cave_block");
constexpr FrameDataId IceBlock = HashFrameDataIdConstexpr("ice_block");
constexpr FrameDataId JungleBlock = HashFrameDataIdConstexpr("jungle_block");
constexpr FrameDataId TempleBlock = HashFrameDataIdConstexpr("temple_block");
constexpr FrameDataId BossBlock = HashFrameDataIdConstexpr("boss_block");
constexpr FrameDataId HeartUiIcon = HashFrameDataIdConstexpr("heart_ui_icon");
constexpr FrameDataId GrenadeUiIcon = HashFrameDataIdConstexpr("grenade_ui_icon");
constexpr FrameDataId StickyGrenadeUiIcon = HashFrameDataIdConstexpr("sticky_grenade_ui_icon");
constexpr FrameDataId RopeUiIcon = HashFrameDataIdConstexpr("rope_ui_icon");
constexpr FrameDataId GoldIcon = HashFrameDataIdConstexpr("gold_icon");
constexpr FrameDataId ToolSlot1 = HashFrameDataIdConstexpr("tool_slot_1");
constexpr FrameDataId ToolSlot2 = HashFrameDataIdConstexpr("tool_slot_2");
constexpr FrameDataId GoldIdol = HashFrameDataIdConstexpr("gold_idol");
constexpr FrameDataId Mattock = HashFrameDataIdConstexpr("mattock");
constexpr FrameDataId MattockSwing = HashFrameDataIdConstexpr("mattock_swing");
constexpr FrameDataId CapePickup = HashFrameDataIdConstexpr("cape_pickup");
constexpr FrameDataId Shotgun = HashFrameDataIdConstexpr("shotgun");
constexpr FrameDataId Teleporter = HashFrameDataIdConstexpr("teleporter");
constexpr FrameDataId TeleporterBackpack = HashFrameDataIdConstexpr("telepack");
constexpr FrameDataId TeleporterBackpackBack = HashFrameDataIdConstexpr("telepack_back");
constexpr FrameDataId TeleporterBackpackSide = HashFrameDataIdConstexpr("telepack_side");
constexpr FrameDataId Gloves = HashFrameDataIdConstexpr("gloves");
constexpr FrameDataId Spectacles = HashFrameDataIdConstexpr("spectacles");
constexpr FrameDataId WebCannon = HashFrameDataIdConstexpr("webgun");
constexpr FrameDataId Cobweb = HashFrameDataIdConstexpr("cobweb");
constexpr FrameDataId WebBall = HashFrameDataIdConstexpr("webball");
constexpr FrameDataId Pistol = HashFrameDataIdConstexpr("pistol");
constexpr FrameDataId GrenadeBoom = HashFrameDataIdConstexpr("grenade_boom");
constexpr FrameDataId BigExplosion = HashFrameDataIdConstexpr("big_explosion");
constexpr FrameDataId LittleExplosion = HashFrameDataIdConstexpr("little_explosion");
constexpr FrameDataId Spark = HashFrameDataIdConstexpr("spark");
constexpr FrameDataId Pow = HashFrameDataIdConstexpr("pow");
constexpr FrameDataId LittleSmoke = HashFrameDataIdConstexpr("little_smoke");
constexpr FrameDataId BigSmoke = HashFrameDataIdConstexpr("big_smoke");
constexpr FrameDataId BloodBall = HashFrameDataIdConstexpr("blood_ball");
constexpr FrameDataId Sparkle = HashFrameDataIdConstexpr("sparkle");
constexpr FrameDataId Trail = HashFrameDataIdConstexpr("trail");
constexpr FrameDataId BaseballBatTrail = HashFrameDataIdConstexpr("baseball_bat_trail");
constexpr FrameDataId Kiss = HashFrameDataIdConstexpr("kiss");
constexpr FrameDataId LittleBrownShard = HashFrameDataIdConstexpr("little_brown_shard");
constexpr FrameDataId Mitt = HashFrameDataIdConstexpr("mitt");
constexpr FrameDataId Paste = HashFrameDataIdConstexpr("paste");
constexpr FrameDataId SpiderMilk = HashFrameDataIdConstexpr("spider_milk");
constexpr FrameDataId SpringShoes = HashFrameDataIdConstexpr("spring_shoes");
constexpr FrameDataId SpikeShoes = HashFrameDataIdConstexpr("spike_shoes");
constexpr FrameDataId Knife = HashFrameDataIdConstexpr("knife");
constexpr FrameDataId KnifeSwing = HashFrameDataIdConstexpr("knife_swing");
constexpr FrameDataId Machete = HashFrameDataIdConstexpr("machete");
constexpr FrameDataId BombBox = HashFrameDataIdConstexpr("bomb_box");
constexpr FrameDataId BombBag = HashFrameDataIdConstexpr("bomb_bag");
constexpr FrameDataId Bow = HashFrameDataIdConstexpr("bow");
constexpr FrameDataId Compass = HashFrameDataIdConstexpr("compass");
constexpr FrameDataId CompassArrow = HashFrameDataIdConstexpr("compass_arrow");
constexpr FrameDataId PackedParachute = HashFrameDataIdConstexpr("packed_parachute");
constexpr FrameDataId OpenParachute = HashFrameDataIdConstexpr("open_parachute");
constexpr FrameDataId Parachute = PackedParachute;
constexpr FrameDataId RopePile = HashFrameDataIdConstexpr("rope_pile");
constexpr FrameDataId EmeraldBig = HashFrameDataIdConstexpr("emerald_big");
constexpr FrameDataId SapphireBig = HashFrameDataIdConstexpr("sapphire_big");
constexpr FrameDataId RubyBig = HashFrameDataIdConstexpr("ruby_big");
constexpr FrameDataId AltarLeft = HashFrameDataIdConstexpr("altar_left");
constexpr FrameDataId AltarRight = HashFrameDataIdConstexpr("altar_right");
constexpr FrameDataId SacAltarLeft = HashFrameDataIdConstexpr("sac_altar_left");
constexpr FrameDataId SacAltarRight = HashFrameDataIdConstexpr("sac_altar_right");
constexpr FrameDataId SacAltarTopper = HashFrameDataIdConstexpr("sac_altar_topper");
constexpr FrameDataId SacAltarSac = HashFrameDataIdConstexpr("sac_altar_sac");
constexpr FrameDataId Meathead = HashFrameDataIdConstexpr("meathead");
constexpr FrameDataId MeatheadRise = HashFrameDataIdConstexpr("meathead_rise");
constexpr FrameDataId SignGeneral = HashFrameDataIdConstexpr("sign_general");
constexpr FrameDataId SignBomb = HashFrameDataIdConstexpr("sign_bomb");
constexpr FrameDataId SignWeapon = HashFrameDataIdConstexpr("sign_weapon");
constexpr FrameDataId SignRare = HashFrameDataIdConstexpr("sign_rare");
constexpr FrameDataId SignClothing = HashFrameDataIdConstexpr("sign_clothing");
constexpr FrameDataId SignCraps = HashFrameDataIdConstexpr("sign_craps");
constexpr FrameDataId SignKissing = HashFrameDataIdConstexpr("sign_kissing");
constexpr FrameDataId DiceSign = HashFrameDataIdConstexpr("dice_sign");
constexpr FrameDataId StoreLight = HashFrameDataIdConstexpr("store_light");
constexpr FrameDataId StoreLightBroken = HashFrameDataIdConstexpr("store_light_broken");
constexpr FrameDataId Lantern = HashFrameDataIdConstexpr("lantern");
constexpr FrameDataId LanternRed = HashFrameDataIdConstexpr("lantern_red");
constexpr FrameDataId ArrowTrap = HashFrameDataIdConstexpr("arrow_trap");
constexpr FrameDataId Arrow = HashFrameDataIdConstexpr("arrow");
constexpr FrameDataId GiantTikiHead = HashFrameDataIdConstexpr("giant_tiki_head");
constexpr FrameDataId KaliHead = HashFrameDataIdConstexpr("kali_head");
constexpr FrameDataId KaliBody = HashFrameDataIdConstexpr("kali_body");
constexpr FrameDataId BeeFly = HashFrameDataIdConstexpr("bee_fly");
constexpr FrameDataId BeeWalk = HashFrameDataIdConstexpr("bee_walk");
constexpr FrameDataId FleshGuy = HashFrameDataIdConstexpr("fleshguy");
constexpr FrameDataId FleshGuyWalk = HashFrameDataIdConstexpr("fleshguy_walk");
constexpr FrameDataId MeatTileTopper = HashFrameDataIdConstexpr("meat_tile_topper");

} // namespace frame_data_ids

} // namespace splonks
