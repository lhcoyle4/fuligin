#include "game_data.h"

/* =========== STATIC GEOMETRY TABLES =========== */

/* Player ship — five-line arrowhead */
Line ship_lines[] = {
    {{ 0.0f, -12.0f}, {-8.0f,  10.0f}},
    {{-8.0f,  10.0f}, {-3.0f,   6.0f}},
    {{-3.0f,   6.0f}, { 3.0f,   6.0f}},
    {{ 3.0f,   6.0f}, { 8.0f,  10.0f}},
    {{ 8.0f,  10.0f}, { 0.0f, -12.0f}}
};

/* Small/large UFO saucer silhouette */
Line ufo_lines[] = {
    {{-16.0f,   0.0f}, { 16.0f,   0.0f}},
    {{-16.0f,   0.0f}, { -8.0f,   5.0f}},
    {{ -8.0f,   5.0f}, {  8.0f,   5.0f}},
    {{  8.0f,   5.0f}, { 16.0f,   0.0f}},
    {{-16.0f,   0.0f}, { -8.0f,  -5.0f}},
    {{ -8.0f,  -5.0f}, {  8.0f,  -5.0f}},
    {{  8.0f,  -5.0f}, { 16.0f,   0.0f}},
    {{ -8.0f,  -5.0f}, { -4.0f, -10.0f}},
    {{ -4.0f, -10.0f}, {  4.0f, -10.0f}},
    {{  4.0f, -10.0f}, {  8.0f,  -6.0f}}
};

/* Kamikaze UFO — compact arrowhead */
Line kamikaze_lines[] = {
    {{ 12.0f,  0.0f}, {-8.0f, -8.0f}},
    {{ -8.0f, -8.0f}, {-4.0f,  0.0f}},
    {{ -4.0f,  0.0f}, {-8.0f,  8.0f}},
    {{ -8.0f,  8.0f}, {12.0f,  0.0f}}
};

/* Bomber UFO — wider saucer variant */
Line bomber_lines[] = {
    {{-20.0f,  0.0f}, { 20.0f,  0.0f}},
    {{-20.0f,  0.0f}, {-10.0f,  7.0f}},
    {{-10.0f,  7.0f}, { 10.0f,  7.0f}},
    {{ 10.0f,  7.0f}, { 20.0f,  0.0f}},
    {{-20.0f,  0.0f}, {-12.0f, -6.0f}},
    {{-12.0f, -6.0f}, { 12.0f, -6.0f}},
    {{ 12.0f, -6.0f}, { 20.0f,  0.0f}},
    {{-12.0f, -6.0f}, { -6.0f,-12.0f}},
    {{ -6.0f,-12.0f}, {  6.0f,-12.0f}},
    {{  6.0f,-12.0f}, { 12.0f, -6.0f}}
};

/* Home space station — octagon + cross + inner box */
Line home_station_lines[] = {
    {{-22.0f,-52.0f}, { 22.0f,-52.0f}},
    {{ 22.0f,-52.0f}, { 52.0f,-22.0f}},
    {{ 52.0f,-22.0f}, { 52.0f, 22.0f}},
    {{ 52.0f, 22.0f}, { 22.0f, 52.0f}},
    {{ 22.0f, 52.0f}, {-22.0f, 52.0f}},
    {{-22.0f, 52.0f}, {-52.0f, 22.0f}},
    {{-52.0f, 22.0f}, {-52.0f,-22.0f}},
    {{-52.0f,-22.0f}, {-22.0f,-52.0f}},
    {{-52.0f,  0.0f}, {-24.0f,  0.0f}},
    {{ 52.0f,  0.0f}, { 24.0f,  0.0f}},
    {{  0.0f,-52.0f}, {  0.0f,-24.0f}},
    {{  0.0f, 52.0f}, {  0.0f, 24.0f}},
    {{-12.0f,-12.0f}, { 12.0f,-12.0f}},
    {{ 12.0f,-12.0f}, { 12.0f, 12.0f}},
    {{ 12.0f, 12.0f}, {-12.0f, 12.0f}},
    {{-12.0f, 12.0f}, {-12.0f,-12.0f}}
};

/* Eldritch Tendril — Zone 2+ enemy, asymmetric tentacle shape */
Line eldritch_lines[] = {
    {{  0.0f,-16.0f}, { 10.0f, -6.0f}},
    {{ 10.0f, -6.0f}, { 16.0f,  4.0f}},
    {{ 16.0f,  4.0f}, {  8.0f, 16.0f}},
    {{  0.0f,-16.0f}, {-10.0f, -6.0f}},
    {{-10.0f, -6.0f}, {-16.0f,  4.0f}},
    {{-16.0f,  4.0f}, { -8.0f, 16.0f}},
    {{  8.0f, 16.0f}, {  0.0f, 10.0f}},
    {{ -8.0f, 16.0f}, {  0.0f, 10.0f}},
    {{  0.0f,-16.0f}, {  0.0f, -8.0f}},
    {{ -4.0f,  0.0f}, {  4.0f,  0.0f}}
};

/* Daemon Sigil — Zone 3+ enemy, angular chaos symbol */
Line daemon_lines[] = {
    {{  0.0f,-20.0f}, { 12.0f, -8.0f}},
    {{ 12.0f, -8.0f}, { 20.0f,  0.0f}},
    {{ 20.0f,  0.0f}, { 12.0f, 12.0f}},
    {{ 12.0f, 12.0f}, {  0.0f, 20.0f}},
    {{  0.0f, 20.0f}, {-12.0f, 12.0f}},
    {{-12.0f, 12.0f}, {-20.0f,  0.0f}},
    {{-20.0f,  0.0f}, {-12.0f, -8.0f}},
    {{-12.0f, -8.0f}, {  0.0f,-20.0f}},
    {{ -8.0f, -8.0f}, {  8.0f, -8.0f}},
    {{  8.0f, -8.0f}, {  8.0f,  8.0f}},
    {{  8.0f,  8.0f}, { -8.0f,  8.0f}},
    {{ -8.0f,  8.0f}, { -8.0f, -8.0f}},
    {{  0.0f,-12.0f}, {  0.0f, 12.0f}},
    {{-12.0f,  0.0f}, { 12.0f,  0.0f}},
    {{ -6.0f, -6.0f}, {  6.0f,  6.0f}},
    {{  6.0f, -6.0f}, { -6.0f,  6.0f}}
};

/* Friendly NPC shield drone — small diamond with inner square hint */
Line npc_drone_lines[] = {
    {{  0.0f,-10.0f}, { 10.0f,  0.0f}},
    {{ 10.0f,  0.0f}, {  0.0f, 10.0f}},
    {{  0.0f, 10.0f}, {-10.0f,  0.0f}},
    {{-10.0f,  0.0f}, {  0.0f,-10.0f}},
    {{ -5.0f, -5.0f}, {  5.0f, -5.0f}},
    {{  5.0f, -5.0f}, {  5.0f,  5.0f}},
    {{  5.0f,  5.0f}, { -5.0f,  5.0f}}
};

/* =========== STRING TABLES =========== */

const char *upgrade_names[] = {
    "TRISKELION BURST", "VOID RICOCHET", "ETHER SHROUD", "OVERCLOCK",
    "ICHOROUS ROUNDS", "AUTODYNE BOOST", "SMALL FORM", "SEEKER ICHORS",
    "PALE SIGHT", "CORRUPTION ROUNDS", "TEMPORAL FOLD", "ORB MAGNET",
    "SOLAR FURY", "WRAITH TWIN", "AFT CANNON", "ORBIT MINE",
    "CRIT CHANCE", "RESTORED VIGOR", "BANE TRAIL", "FISSION SHOT",
    "VORTEX GRENADE", "PHASE SHIFT", "AUTO TURRET", "NOVA SHELL",
    "CHRONICLE BOOST", "OSSIFIED HULL", "THERMAL HULL", "RIFT DISPLACER",
    "BANE WHIP", "RESONANCE CASCADE"
};

const char *upgrade_descs[] = {
    "Three-bolt spread from the prow",
    "Bolts rebound off the void's edge",
    "One absorption per life",
    "30% faster discharge",
    "Bolts pierce through Void Stones",
    "20% faster Autodyne thrust",
    "Reduce ship collision radius",
    "Bolts seek the nearest stone",
    "Reveal hidden Remnant threats",
    "Bolts corrode on contact",
    "Briefly slow the drift of time",
    "Draw chronicle orbs to you",
    "Rage boils when hull is intact",
    "A phantom twin follows your path",
    "Discharge from the aft thruster",
    "Mines orbit the hull",
    "Random bolts strike twice",
    "The Autarch grants another life",
    "Thruster wash scorches Void Stones",
    "Bolts split on first impact",
    "Pulls Void Stones into ruin",
    "Pass through one killing blow",
    "An ancient turret follows you",
    "Hull detonates on impact",
    "Harvest 50% more chronicle",
    "Ancient plating resists damage",
    "Ram Void Stones at full thrust",
    "Double-tap thrust to rift-jump",
    "Thruster trail scorches the drift",
    "Bolts unleash shockwaves"
};

const char *difficulty_names[] = {
    "EASY", "NORMAL", "HARD", "ACE"
};

