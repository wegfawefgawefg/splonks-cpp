#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace splonks {

using AudioAssetId = std::uint32_t;

constexpr AudioAssetId kInvalidAudioAssetId = 0;
constexpr AudioAssetId kAudioAssetFnvOffsetBasis32 = 2166136261U;
constexpr AudioAssetId kAudioAssetFnvPrime32 = 16777619U;

constexpr AudioAssetId HashAudioAssetIdConstexpr(std::string_view text) {
    AudioAssetId hash = kAudioAssetFnvOffsetBasis32;
    for (char character : text) {
        hash ^= static_cast<AudioAssetId>(static_cast<unsigned char>(character));
        hash *= kAudioAssetFnvPrime32;
    }
    return hash;
}

inline AudioAssetId HashAudioAssetId(const std::string& text) {
    return HashAudioAssetIdConstexpr(text);
}

namespace audio_asset_ids {

constexpr AudioAssetId Title = HashAudioAssetIdConstexpr("title");
constexpr AudioAssetId Playing = HashAudioAssetIdConstexpr("playing");
constexpr AudioAssetId InTheCave = HashAudioAssetIdConstexpr("in_the_cave");
constexpr AudioAssetId Jump = HashAudioAssetIdConstexpr("jump");
constexpr AudioAssetId Step1 = HashAudioAssetIdConstexpr("step1");
constexpr AudioAssetId Step2 = HashAudioAssetIdConstexpr("step2");
constexpr AudioAssetId ClimbMetal1 = HashAudioAssetIdConstexpr("climb_metal1");
constexpr AudioAssetId ClimbMetal2 = HashAudioAssetIdConstexpr("climb_metal2");
constexpr AudioAssetId BatFlap1 = HashAudioAssetIdConstexpr("bat_flap1");
constexpr AudioAssetId BatFlap2 = HashAudioAssetIdConstexpr("bat_flap2");
constexpr AudioAssetId BatSqueak = HashAudioAssetIdConstexpr("bat_squeak");
constexpr AudioAssetId Thud = HashAudioAssetIdConstexpr("thud");
constexpr AudioAssetId GameOver = HashAudioAssetIdConstexpr("game_over");
constexpr AudioAssetId Jetpack1 = HashAudioAssetIdConstexpr("jetpack1");
constexpr AudioAssetId Jetpack2 = HashAudioAssetIdConstexpr("jetpack2");
constexpr AudioAssetId JetpackStart1 = HashAudioAssetIdConstexpr("jetpack_start1");
constexpr AudioAssetId Equip = HashAudioAssetIdConstexpr("equip");
constexpr AudioAssetId Throw = HashAudioAssetIdConstexpr("throw");
constexpr AudioAssetId PistolShoot = HashAudioAssetIdConstexpr("pistol_shoot");
constexpr AudioAssetId PistolHolster = HashAudioAssetIdConstexpr("pistol_holster");
constexpr AudioAssetId PistolUnholster = HashAudioAssetIdConstexpr("pistol_unholster");
constexpr AudioAssetId GunEmpty = HashAudioAssetIdConstexpr("gun_empty");
constexpr AudioAssetId BombExplosion = HashAudioAssetIdConstexpr("bomb_explosion");
constexpr AudioAssetId AnimalCrush1 = HashAudioAssetIdConstexpr("animal_crush1");
constexpr AudioAssetId AnimalCrush2 = HashAudioAssetIdConstexpr("animal_crush2");
constexpr AudioAssetId Gold = HashAudioAssetIdConstexpr("gold");
constexpr AudioAssetId GoldStack = HashAudioAssetIdConstexpr("gold_stack");
constexpr AudioAssetId MoneySmashed = HashAudioAssetIdConstexpr("money_smashed");
constexpr AudioAssetId Ouch1 = HashAudioAssetIdConstexpr("ouch1");
constexpr AudioAssetId PlayerOuch = HashAudioAssetIdConstexpr("player_ouch");
constexpr AudioAssetId BlockDrag1 = HashAudioAssetIdConstexpr("block_drag1");
constexpr AudioAssetId BlockDrag2 = HashAudioAssetIdConstexpr("block_drag2");
constexpr AudioAssetId BlockLand = HashAudioAssetIdConstexpr("block_land");
constexpr AudioAssetId DefaultLand = HashAudioAssetIdConstexpr("default_land");
constexpr AudioAssetId RopeDeploy = HashAudioAssetIdConstexpr("rope_deploy");
constexpr AudioAssetId ClimbRope1 = HashAudioAssetIdConstexpr("climb_rope1");
constexpr AudioAssetId ClimbRope2 = HashAudioAssetIdConstexpr("climb_rope2");
constexpr AudioAssetId StageWin = HashAudioAssetIdConstexpr("stage_win");
constexpr AudioAssetId PotShatter = HashAudioAssetIdConstexpr("pot_shatter");
constexpr AudioAssetId BoxBreak = HashAudioAssetIdConstexpr("box_break");
constexpr AudioAssetId BaseballBatSwing = HashAudioAssetIdConstexpr("baseball_bat_swing");
constexpr AudioAssetId BaseballBatKillHit1 = HashAudioAssetIdConstexpr("baseball_bat_kill_hit1");
constexpr AudioAssetId BaseballBatKillHit2 = HashAudioAssetIdConstexpr("baseball_bat_kill_hit2");
constexpr AudioAssetId BaseballBatKillHit3 = HashAudioAssetIdConstexpr("baseball_bat_kill_hit3");
constexpr AudioAssetId BaseballBatMetalDink1 = HashAudioAssetIdConstexpr("baseball_bat_metal_dink1");
constexpr AudioAssetId BaseballBatBoxSmash = HashAudioAssetIdConstexpr("baseball_bat_box_smash");
constexpr AudioAssetId CavemanNotice = HashAudioAssetIdConstexpr("caveman_notice");
constexpr AudioAssetId CavemanHurt = HashAudioAssetIdConstexpr("caveman_hurt");
constexpr AudioAssetId DamselAmbientCry = HashAudioAssetIdConstexpr("damsel_ambient_cry");
constexpr AudioAssetId DamselHurt = HashAudioAssetIdConstexpr("damsel_hurt");
constexpr AudioAssetId Smooch = HashAudioAssetIdConstexpr("smooch");
constexpr AudioAssetId ChestOpen = HashAudioAssetIdConstexpr("chest_open");
constexpr AudioAssetId Unlock = HashAudioAssetIdConstexpr("unlock");
constexpr AudioAssetId LawsonEnter = HashAudioAssetIdConstexpr("lawson_enter");
constexpr AudioAssetId CashRegister = HashAudioAssetIdConstexpr("cash_register");
constexpr AudioAssetId ShopkeepAnger0 = HashAudioAssetIdConstexpr("shopkeep_anger_0");
constexpr AudioAssetId LightBreak = HashAudioAssetIdConstexpr("light_break");
constexpr AudioAssetId BoulderLatch = HashAudioAssetIdConstexpr("boulder_latch");
constexpr AudioAssetId BoulderHitGround = HashAudioAssetIdConstexpr("boulder_hit_ground");
constexpr AudioAssetId BoulderTileCrash = HashAudioAssetIdConstexpr("boulder_tile_crash");
constexpr AudioAssetId BoulderRoll = HashAudioAssetIdConstexpr("boulder_roll");
constexpr AudioAssetId Sacrifice = HashAudioAssetIdConstexpr("sacrifice");
constexpr AudioAssetId Present = HashAudioAssetIdConstexpr("present");
constexpr AudioAssetId Tube = HashAudioAssetIdConstexpr("tube");
constexpr AudioAssetId Pickaxe = HashAudioAssetIdConstexpr("pickaxe");
constexpr AudioAssetId SuccessfulDig = HashAudioAssetIdConstexpr("successful_dig");
constexpr AudioAssetId UnbreakableHit = HashAudioAssetIdConstexpr("unbreakable_hit");
constexpr AudioAssetId UiCant = HashAudioAssetIdConstexpr("ui_cant");
constexpr AudioAssetId UiConfirm = HashAudioAssetIdConstexpr("ui_confirm");
constexpr AudioAssetId UiCursorMove = HashAudioAssetIdConstexpr("ui_cursor_move");
constexpr AudioAssetId UiLeft = HashAudioAssetIdConstexpr("ui_left");
constexpr AudioAssetId UiRight = HashAudioAssetIdConstexpr("ui_right");
constexpr AudioAssetId UiSuperConfirm = HashAudioAssetIdConstexpr("ui_super_confirm");

} // namespace audio_asset_ids

} // namespace splonks
